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