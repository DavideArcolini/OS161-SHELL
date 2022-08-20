/**
 * @file syscall_PROC.c
 * @author Davide Arcolini (davide.arcolini@studenti.polito.it)
 * 
 * @brief Contains the definitions of the system calls used in the SHELL project. 
 *        In particular, it contains the definition of the system calls used in the management
 *        of process, such as fork(), execv(), waitpid(), exit(), etc...
 * 
 * @version 0.1
 * @date 2022-08-18
 * 
 * @copyright Copyright (c) 2022
 * 
*/

#include <types.h>
#include <proc.h>
#include <current.h>
#include <vnode.h>
#include <vfs.h>
#include <uio.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <copyinout.h>
#include <limits.h>
#include <kern/unistd.h>
#include <endian.h>
#include <stat.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <kern/wait.h>
#include <mips/trapframe.h>
#include "syscall_SHELL.h"


/**
 * @brief Retrieve the PID of the current process
 * 
 * @param retval PID of the current process
 * @return zero on success
 */
#if OPT_SHELL
int sys_getpid_SHELL(pid_t *retval) {

    /* RETRIEVING PID */
    KASSERT(curproc != NULL);
    *retval = curproc->p_pid;

    /* getpid() DOES NOT FAIL.  */
    return 0;   
}
#endif

/**
 * @brief Wait for the process specified by pid to exit, and return an encoded exit status 
 *        in the integer pointed to by status. If that process has exited already, waitpid 
 *        returns immediately. If that process does not exist, waitpid fails.
 * 
 * @param pid pid of the process to wait
 * @param status exit status of the process to wait
 * @param options not implemented
 * @return zero on success, an error value in case of failure.
 */
#if OPT_SHELL
int sys_waitpid_SHELL(pid_t pid, int *status, int options) {

    /* CHECKING PID */
    if (pid == curproc->p_pid) {
        kprintf("[ERROR] this process is attempting to wait for itself\n");
        return ECHILD;
    }

    /* CHECKING OPTIONS */
    if (options != 0) {
        kprintf("[ERROR] invalid options parameter\n");
        return EINVAL;
    }

    /* RETRIEVING PROCESS */
    struct proc *proc = proc_search(pid);
    if (proc == NULL) {
        kprintf("[ERROR] process not found\n");
        return ESRCH;
    }

    /* WAITING TERMINATION OF THE PROCESS */
    lock_acquire(proc->p_locklock);
    cv_wait(proc->p_cv, proc->p_locklock);
    lock_release(proc->p_locklock);

    /* ASSIGNING RETURN STATUS */
    *status = proc->p_status;
    if (status == NULL) {
        return EFAULT;
    }

    /* TASK COMPLETED SUCCESSFULLY */
    proc_destroy(proc);
    return 0;
}
#endif


/**
 * @brief Cause the current process to exit.
 * 
 * @param exitcode reported back to other process(es) via the waitpid() call.
 * @return does not return.
 */
#if OPT_SHELL
void sys_exit_SHELL(int exitcode) {

    /* RETRIEVING STATUS OF THE CURRENT PROCESS */
    struct proc *proc = curproc;
    proc->p_status = _MKWAIT_EXIT(exitcode);    /* exitcode & 0xff */

    /* REMOVING THREAD BEFORE SIGNALLING DUE TO RACE CONDITIONS */
    proc_remthread(curthread);        /* remove thread from current process */

    /* SIGNALLING THE TERMINATION OF THE PROCESS */
    lock_acquire(proc->p_locklock);
    cv_signal(proc->p_cv, proc->p_locklock);
    lock_release(proc->p_locklock);

    /* MAIN THREAD TERMINATES HERE. BYE BYE */
    thread_exit();

    /* WAIT! YOU SHOULD NOT HAPPEN TO BE HERE */
    panic("[!] Wait! You should not be here. Some errors happened during thread_exit()...\n");
}
#endif

/**
 * @brief It duplicates the currently running process. The two copies are identical, except 
 *        that one (the "new" one, or "child"), has a new, unique process id, and in the 
 *        other (the "parent") the process id is unchanged. 
 * 
 *        The two processes do not share memory or open file tables; this state is copied into 
 *        the new process, and subsequent modification in one process does not affect the other.
 * 
 *        However, the file handle objects the file tables point to are shared, so, for instance, 
 *        calls to lseek in one process can affect the other. 
 * 
 * @param ctf trapframe of the process
 * @param retval PID of the newly created process.
 * @return zero on success, an error value in case of failure. 
 */
#if OPT_SHELL
int sys_fork_SHELL(struct trapframe *ctf, pid_t *retval) {

    /* ASSERTING CURRENT PROCESS TO ACTUALLY EXIST */
    KASSERT(curproc != NULL);

    /* CHECKING SPACE AVAILABILITY IN PROCESS TABLE */
    int index = find_valid_pid();
    if (index <= 0) {
        return ENPROC;  /* There are already too many processes on the system. */
    }

    /* CREATING NEW RUNNABLE PROCESS */
    struct proc *newproc = proc_create_runprogram(curproc->p_name);
    if (newproc == NULL) {
        return ENOMEM;  /* Sufficient virtual memory for the new process was not available. */
    }

    /* COPYING ADDRESS SPACE */
    int err = as_copy(curproc->p_addrspace, &(newproc->p_addrspace));
    if (err) {
        proc_destroy(newproc);
        return err;
    }

    /* COPYING PARENT'S TRAPFRAME */
    struct trapframe *tf_child = (struct trapframe *) kmalloc(sizeof(struct trapframe));
    if(tf_child == NULL){
        proc_destroy(newproc);
        return ENOMEM; 
    }
    memcpy(tf_child, ctf, sizeof(struct trapframe));

    /* TO BE DONE: linking parent/child, so that child terminated on parent exit */

    /* ADDING NEW PROCESS TO THE PROCESS TABLE */
    err = proc_add((pid_t) index, newproc);
    if (err == -1) {
        return ENOMEM;
    }

    /* CALLING THREAD FORK() AND START NEW THREAD ROUTINE */
    err = thread_fork(
        curthread->t_name,                  /* same name as the parent  */
        newproc,                            /* newly created process    */      
        call_enter_forked_process,          /* routine to start         */
        (void *) tf_child,                  /* child trapframe          */
        (unsigned long) 0                   /* unused                   */
    );

    if (err) {
        proc_remove((pid_t) index);
        proc_destroy(newproc);
        kfree(tf_child);
        return err;
    }

    /* TASK COMPLETED SUCCESSFULLY */
    *retval = newproc->p_pid;
    return 0;
}
#endif