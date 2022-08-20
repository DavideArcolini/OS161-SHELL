/*
 * Copyright (c) 2013
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Process support.
 *
 * There is (intentionally) not much here; you will need to add stuff
 * and maybe change around what's already present.
 *
 * p_lock is intended to be held when manipulating the pointers in the
 * proc structure, not while doing any significant work with the
 * things they point to. Rearrange this (and/or change it to be a
 * regular lock) as needed.
 *
 * Unless you're implementing multithreaded user processes, the only
 * process that will have more than one thread is the kernel process.
 */

#include <types.h>
#include <spl.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>

/* INCLUDES FOR CONSOLE INITIALIZATION */
#include <synch.h>
#include <kern/fcntl.h>
#include <vfs.h>

/*
 * The process for the kernel; this holds all the kernel-only threads.
 */
struct proc *kproc;

/**
 * @brief The process table stuct stores an array of user processes, each identified by
 * 		  a specific PID
 */
#if OPT_SHELL
#define PROC_MAX 100				/* maximum number of allowed running process 	*/
static struct _processTable {
	bool is_active;					/* table is active and ready to use 			*/
	struct proc *proc[PROC_MAX+1];	/* [0] not used, PID >= 1 						*/
	pid_t last_pid;					/* last PID used in the table 					*/
	struct spinlock lk;				/* lock for this table 							*/
} processTable;
#endif

/**
 * @brief Return the process associated to the given PID
 * 
 * @param pid pid of the process to retrieve
 * @return struct proc* process associated to the pid
 */
#if OPT_SHELL
struct proc *proc_search(pid_t pid) {

	/* CHECKING PID CONSTRAINTS */
	if (pid <= 0 || pid > PROC_MAX) {
		kprintf("[ERROR] invalid pid\n");
		return NULL;
	}

	/* RETRIEVING PROCESS BASED ON THE INDEX PID */
	struct proc *proc = processTable.proc[pid];
	if (proc->p_pid != pid) {
		return NULL;
	}

	/* TASK COMPLETED SUCCESSFULLY */
	return proc;
}
#endif

/**
 * @brief For any given process, the first file descriptors (0, 1, and 2) are 
 * 		  considered to be standard input (stdin), standard output (stdout), and
 * 		  standard error (stderr). These file descriptors should start out attached 
 * 		  to the console device ("con:").
 * 
 * @param name name that will be assigned to the lock
 * @return int 
 */
#if OPT_SHELL
static int console_init(const char *lock_name, struct proc *proc, int fd, int flag) {

	/* ASSIGNMENT OF THE CONSOLE NAME */
	char *con = kstrdup("con:");
	if (con == NULL) {
		return -1;
	}

	/* ALLOCATING SPACE IN THE FILETABLE */
	proc->fileTable[fd] = (struct openfile *) kmalloc(sizeof(struct openfile));
	if (proc->fileTable[fd] == NULL) {
		kfree(con);
		return -1;
	}

	/* OPENING ASSOCIATED FILE */
	int err = vfs_open(con, O_RDONLY, 0, &proc->fileTable[fd]->vn);
	if (err) {
		kfree(con);
		kfree(proc->fileTable[fd]);
		return -1;
	}
	kfree(con);

	/* INITIALIZATION OF VALUES */
	proc->fileTable[fd]->offset = 0;
	proc->fileTable[fd]->lock = lock_create(lock_name);
	if (proc->fileTable[fd]->lock == NULL) {
		vfs_close(proc->fileTable[fd]->vn);
		kfree(proc->fileTable[fd]);
		return -1;
	}
	proc->fileTable[fd]->count_refs = 1;
	proc->fileTable[fd]->mode_open = flag;

	return 0;
}
#endif

/**
 * @brief Add the given process to the process table and manage the PID initialization.
 * 
 * @param proc newly created process
 * @param name name of the process
 * @return the pid of the process created, -1 on failure
 */
#if OPT_SHELL
static int proc_init(struct proc *proc, const char *name) {

	/* ACQUIRING THE SPINLOCK */
	spinlock_acquire(&processTable.lk);
	proc->p_pid = -1;

	/* SEARCH FREE INDEX IN THE TABLE USING CIRCULAR STRATEGY */
	int index = processTable.last_pid + 1;
	index = (index > PROC_MAX) ? 1 : index;		// skipping [0] (kernel process)
	while (index != processTable.last_pid) {
		if (processTable.proc[index] == NULL) {
			processTable.proc[index] = proc;
			processTable.last_pid = index;
			proc->p_pid = index;
			break;
		}
		index++;
		index = (index > PROC_MAX) ? 1 : index;
	}

	/* RELEASING THE SPINLOCK */
	spinlock_release(&processTable.lk);
	if (proc->p_pid <= 0) {
		kprintf("[ERROR] process initialization failed...\n");
		return proc->p_pid;
	}

	/* PROCESS STATUS INITIALIZATION */
	proc->p_status = 0;

	/* PROCESS CV AND LOCK INITIALIZATION */
	proc->p_cv = cv_create(name);
  	proc->p_locklock = lock_create(name);
	if (proc->p_cv == NULL || proc->p_locklock == NULL) {
		return -1;
	}

	/* TASK COMPLETED SUCCESSFULLY */
	return proc->p_pid;
}
#endif

/**
 * @brief manage the process table when a process is destroyed.
 * 
 * @param proc the process that will be destroyed.
 * @return 0 on sucess, any other value on failure.
 */
static int proc_deinit(struct proc *proc) {
#if OPT_SHELL
	/* ACQUIRING THE SPINLOCK */
	spinlock_acquire(&processTable.lk);

	/* ACQUIRING PROCESS PID */
	int index = proc->p_pid;
	if (index <= 0 || index > PROC_MAX) {
		return -1;
	}

	/* RELEASING ENTRY IN PROCESS TABLE */
	processTable.proc[index] = NULL;

	/* PROCESS CV AND LOCK DESTROY */
	cv_destroy(proc->p_cv);
  	lock_destroy(proc->p_locklock);

	/* RELEASING THE SPINLOCK */
	spinlock_release(&processTable.lk);

	/* TASK COMPLETED SUCCESSFULLY */
	return 0;
#endif
}

/*
 * Create a proc structure.
 */
static
struct proc *
proc_create(const char *name)
{
	struct proc *proc;

	proc = kmalloc(sizeof(*proc));
	if (proc == NULL) {
		return NULL;
	}
	proc->p_name = kstrdup(name);
	if (proc->p_name == NULL) {
		kfree(proc);
		return NULL;
	}

	proc->p_numthreads = 0;
	spinlock_init(&proc->p_lock);

	/* VM fields */
	proc->p_addrspace = NULL;

	/* VFS fields */
	proc->p_cwd = NULL;

#if OPT_SHELL

	/**
	 * @brief Zeroing out the block of memory used by the process fileTable (i.e.
	 * 		  initializing the struct).
	 */
	bzero(proc->fileTable, OPEN_MAX * sizeof(struct openfile*));

	/* ADD PROCESS TO THE PROCESS TABLE */
	if (strcmp(name, "[kernel]") != 0 && proc_init(proc, name) <= 0) {
		kfree(proc);
		return NULL;
	}

	kprintf("[DEBUG] process created with PID: %d.\n", proc->p_pid);
#endif

	return proc;
}

/*
 * Destroy a proc structure.
 *
 * Note: nothing currently calls this. Your wait/exit code will
 * probably want to do so.
 */
void
proc_destroy(struct proc *proc)
{
	/*
	 * You probably want to destroy and null out much of the
	 * process (particularly the address space) at exit time if
	 * your wait/exit design calls for the process structure to
	 * hang around beyond process exit. Some wait/exit designs
	 * do, some don't.
	 */

	KASSERT(proc != NULL);
	KASSERT(proc != kproc);

	/*
	 * We don't take p_lock in here because we must have the only
	 * reference to this structure. (Otherwise it would be
	 * incorrect to destroy it.)
	 */

	/* VFS fields */
	if (proc->p_cwd) {
		VOP_DECREF(proc->p_cwd);
		proc->p_cwd = NULL;
	}

	/* VM fields */
	if (proc->p_addrspace) {
		/*
		 * If p is the current process, remove it safely from
		 * p_addrspace before destroying it. This makes sure
		 * we don't try to activate the address space while
		 * it's being destroyed.
		 *
		 * Also explicitly deactivate, because setting the
		 * address space to NULL won't necessarily do that.
		 *
		 * (When the address space is NULL, it means the
		 * process is kernel-only; in that case it is normally
		 * ok if the MMU and MMU- related data structures
		 * still refer to the address space of the last
		 * process that had one. Then you save work if that
		 * process is the next one to run, which isn't
		 * uncommon. However, here we're going to destroy the
		 * address space, so we need to make sure that nothing
		 * in the VM system still refers to it.)
		 *
		 * The call to as_deactivate() must come after we
		 * clear the address space, or a timer interrupt might
		 * reactivate the old address space again behind our
		 * back.
		 *
		 * If p is not the current process, still remove it
		 * from p_addrspace before destroying it as a
		 * precaution. Note that if p is not the current
		 * process, in order to be here p must either have
		 * never run (e.g. cleaning up after fork failed) or
		 * have finished running and exited. It is quite
		 * incorrect to destroy the proc structure of some
		 * random other process while it's still running...
		 */
		struct addrspace *as;

		if (proc == curproc) {
			as = proc_setas(NULL);
			as_deactivate();
		}
		else {
			as = proc->p_addrspace;
			proc->p_addrspace = NULL;
		}
		as_destroy(as);
	}

	KASSERT(proc->p_numthreads == 0);
	spinlock_cleanup(&proc->p_lock);

#if OPT_SHELL
	if (proc_deinit(proc) != 0) {
		panic("[ERROR] some errors occurred in the management of the process table\n");
	}
#endif

	kfree(proc->p_name);
	kfree(proc);
}

/*
 * Create the process structure for the kernel.
 */
void
proc_bootstrap(void)
{

	/* KERNEL PROCESS INITIALIZATION AND CREATION */
	kproc = proc_create("[kernel]");
	if (kproc == NULL) {
		panic("proc_create for kproc failed\n");
	}

	/* USER PROCESS INITIALIZATION (TABLE) */
#if OPT_SHELL
	spinlock_init(&processTable.lk);	/* lock initialization 								*/
	processTable.proc[0] = kproc;		/* registering kernel process in the process table 	*/
	KASSERT(processTable.proc[0] != NULL);
	for (int i = 1; i <= PROC_MAX; i++) {
		processTable.proc[i] = NULL;
	}
	processTable.is_active = true;		/* activating the process table 					*/
	processTable.last_pid = 0;			/* last used PID 									*/
#endif
}

/*
 * Create a fresh proc for use by runprogram.
 *
 * It will have no address space and will inherit the current
 * process's (that is, the kernel menu's) current directory.
 */
struct proc *
proc_create_runprogram(const char *name)
{
	struct proc *newproc;

	newproc = proc_create(name);
	if (newproc == NULL) {
		return NULL;
	}

	/* VM fields */

	newproc->p_addrspace = NULL;

	/* VFS fields */

#if OPT_SHELL
	/* CONSOLE INITIALIZATION FOR STDIN, STDOUT AND STDERR */
	if (console_init("STDIN", newproc, 0, O_RDONLY) == -1) {
		return NULL;
	} else if (console_init("STDOUT", newproc, 1, O_WRONLY) == -1) {
		return NULL;
	} else if (console_init("STDERR", newproc, 2, O_WRONLY) == -1) {
		return NULL;
	}
#endif

	/*
	 * Lock the current process to copy its current directory.
	 * (We don't need to lock the new process, though, as we have
	 * the only reference to it.)
	 */
	spinlock_acquire(&curproc->p_lock);
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		newproc->p_cwd = curproc->p_cwd;
	}
	spinlock_release(&curproc->p_lock);

	return newproc;
}

/*
 * Add a thread to a process. Either the thread or the process might
 * or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
int
proc_addthread(struct proc *proc, struct thread *t)
{
	int spl;

	KASSERT(t->t_proc == NULL);

	spinlock_acquire(&proc->p_lock);
	proc->p_numthreads++;
	spinlock_release(&proc->p_lock);

	spl = splhigh();
	t->t_proc = proc;
	splx(spl);

	return 0;
}

/*
 * Remove a thread from its process. Either the thread or the process
 * might or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
void
proc_remthread(struct thread *t)
{
	struct proc *proc;
	int spl;

	proc = t->t_proc;
	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	KASSERT(proc->p_numthreads > 0);
	proc->p_numthreads--;
	spinlock_release(&proc->p_lock);

	spl = splhigh();
	t->t_proc = NULL;
	splx(spl);
}

/*
 * Fetch the address space of (the current) process.
 *
 * Caution: address spaces aren't refcounted. If you implement
 * multithreaded processes, make sure to set up a refcount scheme or
 * some other method to make this safe. Otherwise the returned address
 * space might disappear under you.
 */
struct addrspace *
proc_getas(void)
{
	struct addrspace *as;
	struct proc *proc = curproc;

	if (proc == NULL) {
		return NULL;
	}

	spinlock_acquire(&proc->p_lock);
	as = proc->p_addrspace;
	spinlock_release(&proc->p_lock);
	return as;
}

/*
 * Change the address space of (the current) process. Return the old
 * one for later restoration or disposal.
 */
struct addrspace *
proc_setas(struct addrspace *newas)
{
	struct addrspace *oldas;
	struct proc *proc = curproc;

	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	oldas = proc->p_addrspace;
	proc->p_addrspace = newas;
	spinlock_release(&proc->p_lock);
	return oldas;
}
