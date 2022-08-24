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

#ifndef _PROC_H_
#define _PROC_H_

/*
 * Definition of a process.
 *
 * Note: curproc is defined by <current.h>.
 */

#include <spinlock.h>
#include "syscall_SHELL.h"		// to use openfile

#if OPT_SHELL
#include <limits.h>				// to use OPEN_MAX
#endif

struct addrspace;
struct thread;
struct vnode;

/**
 * @brief The child_list structure stores the pid of each children of a process and a 
 * pointer to the next children
 */
#if OPT_SHELL
struct child_list {
	pid_t child_pid;				/* pid of the child							*/
	struct child_list* next_child;	/* next_child						        */
};
#endif


/*
 * Process structure.
 *
 * Note that we only count the number of threads in each process.
 * (And, unless you implement multithreaded user processes, this
 * number will not exceed 1 except in kproc.) If you want to know
 * exactly which threads are in the process, e.g. for debugging, add
 * an array and a sleeplock to protect it. (You can't use a spinlock
 * to protect an array because arrays need to be able to call
 * kmalloc.)
 *
 * You will most likely be adding stuff to this structure, so you may
 * find you need a sleeplock in here for other reasons as well.
 * However, note that p_addrspace must be protected by a spinlock:
 * thread_switch needs to be able to fetch the current address space
 * without sleeping.
 */
struct proc {
	char *p_name;			/* Name of this process */
	struct spinlock p_lock;		/* Lock for this structure */
	unsigned p_numthreads;		/* Number of threads in this process */

	/* VM */
	struct addrspace *p_addrspace;	/* virtual address space */

	/* VFS */
	struct vnode *p_cwd;		/* current working directory */

#if OPT_SHELL
	int p_status;				/* current process status 	*/
	pid_t p_pid;				/* current process PID		*/
	pid_t parent_pid;			/* parent process PID		*/
	struct child_list* children_list;  /*list of children          */
	struct cv *p_cv;			/* used for waitpid() syscall */
	struct lock *p_locklock;	/* used for waitpid() syscall */

	/**
	 * @brief file table of this specific process. Each process can have at maximum 
	 * 		  OPEN_MAX files opened in the table.
	 * 
	 * 		  This struct will be initialized in proc_create() and freed in proc_destroy().
	 */
	struct openfile *fileTable[OPEN_MAX];
#endif
};

/* This is the process structure for the kernel and for kernel-only threads. */
extern struct proc *kproc;

/* Call once during system startup to allocate data structures. */
void proc_bootstrap(void);

/* Create a fresh process for use by runprogram(). */
struct proc *proc_create_runprogram(const char *name);

/* Destroy a process. */
void proc_destroy(struct proc *proc);

/* Attach a thread to a process. Must not already have a process. */
int proc_addthread(struct proc *proc, struct thread *t);

/* Detach a thread from its process. */
void proc_remthread(struct thread *t);

/* Fetch the address space of the current process. */
struct addrspace *proc_getas(void);

/* Change the address space of the current process, and return the old one. */
struct addrspace *proc_setas(struct addrspace *);

/**
 * @brief Return the process associated to the given PID
 * 
 * @param pid pid of the process to retrieve
 * @return struct proc* process associated to the pid
 */
#if OPT_SHELL
struct proc *proc_search(pid_t pid);
#endif

/**
 * @brief Starts the new generated thread
 * 
 * @param tfv trapframe of the new thread.
 * @param dummy not used.
 */
#if OPT_SHELL
void call_enter_forked_process(void *tfv, unsigned long dummy);
#endif

/**
 * @brief Search in the process table a valid PID for an eventual
 * 		  new process.
 * 
 * @return PID on success, an error value in case of failure.
 */
#if OPT_SHELL
int find_valid_pid(void);
#endif

/**
 * @brief Add the given process to the process table, at the given index.
 * 
 * @param pid index in the table (PID)
 * @param proc process to be added
 * 
 * @return zero on success, an error value in case of failure
 */
#if OPT_SHELL
int proc_add(pid_t pid, struct proc *proc);
#endif

/**
 * @brief Remove the process associated to the given pid from the process 
 * 		  table.
 * 
 * @param pid pid of the process.
 */
#if OPT_SHELL
void proc_remove(pid_t pid);
#endif

/**
 * @brief Adds a new child to the child_list of a parent process
 * 
 * @param proc parent process.
 * @param child_pid pid of the child process.
 * 
 * @return 0 on success, -1 in case of failure
 */
#if OPT_SHELL
int add_new_child(struct proc* proc, pid_t child_pid);
#endif

/**
 * @brief Destroys the child_list of a parent process which is being destroyed.
 * Sets the childrens's parent pid to -1, the "root" process. 
 * 
 * @param proc parent process.
 * 
 * @return 0 on success, -1 in case of failure
 */
#if OPT_SHELL
int destroy_child_list(struct proc* proc);
#endif

/**
 * @brief Removes the child (which is being destroyed) from the child list of its parent process.
 * 
 * @param proc parent process.
 * @param child_pid pid of the child process.
 * 
 * @return 0 on success, -1 in case of failure
 */
#if OPT_SHELL
int remove_child_from_list(struct proc* proc, pid_t child_pid);
#endif

/**
 * @brief Checks if the process with pid child_pid is a son of the parent process.
 * 
 * @param proc parent process.
 * @param child_pid pid of the child process.
 * 
 * @return 0 on success, -1 in case of failure
 */
#if OPT_SHELL
int is_child(struct proc* proc, pid_t child_pid);
#endif 


#endif /* _PROC_H_ */
