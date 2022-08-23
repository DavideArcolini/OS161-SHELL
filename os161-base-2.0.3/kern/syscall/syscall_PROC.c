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
#include <syscall.h>
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
int sys_waitpid_SHELL(pid_t pid, int *status, int options, int32_t *retval) {

    /* CHECKING ARGUMENTS */
    if (pid == curproc->p_pid) {
        return ECHILD;
    } else if (options != WNOHANG) {
        *status = 0;
        *retval = pid;
        return 0;
    } else if (status == NULL) {
        return EFAULT;
    }

    /* RETRIEVING PROCESS */
    struct proc *proc = proc_search(pid);
    if (proc == NULL) {
        return ESRCH;
    }

    if (proc->p_numthreads == 0) {
        *status = proc->p_status;
        *retval = proc->p_pid;
        proc_destroy(proc);
        return 0;
    }

    /* WAITING TERMINATION OF THE PROCESS */
    lock_acquire(proc->p_locklock);
    cv_wait(proc->p_cv, proc->p_locklock);
    lock_release(proc->p_locklock);

    /* ASSIGNING RETURN STATUS */
    *status = proc->p_status;
    *retval = proc->p_pid;
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
    memmove(tf_child, ctf, sizeof(struct trapframe));

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
        proc_destroy(newproc);
        kfree(tf_child);
        return err;
    }

    /* TASK COMPLETED SUCCESSFULLY */
    *retval = newproc->p_pid;      // parent return pid of child
    return 0;
}
#endif

/**
 * @brief Replaces the currently executing program with a newly loaded program image. This occurs 
 *        within one process; the process id is unchanged. 
 * 
 * @param pathname pathname of the program to run 
 * @param argv an array of 0-terminated strings. The array itself should be terminated by a NULL pointer. 
 * @return zero on success, and error value in case of failure 
 */
#if OPT_SHELL
int sys_execv_SHELL(const char *pathname, char *argv[]) {

    /* CHECKING INPUT ARGUMENTS */
    if (pathname == NULL) {
        return EFAULT;      // One of the args is an invalid pointer.
    }

    /* COPYING PATHNAME FROM USER LAND TO KERNEL LAND */
    // 1) security reason
    // 2) pathname may be corrupted during its use
    char *kpathname = (char *) kmalloc(PATH_MAX * sizeof(char));
    if (kpathname == NULL) {
        return ENOMEM;
    }
    int err = copyinstr((const_userptr_t) pathname, kpathname, PATH_MAX, NULL);
    if (err) {
        kfree(kpathname);
        return err;
    }

    /* COUNTING NUMBER OF ARGUMENTS */
    int args;
    for (args = 0; argv[args]; args++);
    if (args >= ARG_MAX) {
        kfree(kpathname);
        return E2BIG;       // too many arguments
    }

    /* COPYING ARGUMENTS FROM USER LAND TO KERNEL LAND */
    char **kargv = (char **) kmalloc(args * sizeof(char *));
    if (kargv == NULL) {
        kfree(kpathname);
        return ENOMEM;
    }
    for (int i = 0; i < args; i++) {
        kargv[i] = (char *) kmalloc((strlen(argv[i]+1)) * sizeof(char));
        if (kargv[i] == NULL) {
            for (int j = 0; j < i; j++) {
                kfree(kargv[j]);
            }
            kfree(kpathname);
            kfree(kargv);
            return ENOMEM;
        }

        /* PERFORMING COPY WITH copyinstr() */
        err = copyinstr((const_userptr_t) argv[i], kargv[i], strlen(argv[i])+1, NULL);
        if (err) {
            for (int j = 0; j < i; j++) {
                kfree(kargv[j]);
            }
            kfree(kpathname);
            kfree(kargv);
            return err;
        }
    }

    /* OPENING THE FILE */
    struct vnode *vn = NULL;
    err = vfs_open(kpathname, O_RDONLY, 0, &vn);
    if (err) {
        for (int j = 0; j < args; j++) {
            kfree(kargv[j]);
        }
        kfree(kpathname);
        kfree(kargv);
        return err;
    }

    /* CREATING NEW ADDRESS SPACE */
    struct addrspace *newas = as_create();
    if (newas == NULL) {
        vfs_close(vn);
        for (int j = 0; j < args; j++) {
            kfree(kargv[j]);
        }
        kfree(kpathname);
        kfree(kargv);
        return ENOMEM;
    }

    /* SWITCH TO IT AND ACTIVATED IT */
    struct addrspace *oldas = proc_setas(newas);
    as_destroy(oldas);
    as_activate();

    /* LOAD THE EXECUTABLE */
    vaddr_t entrypoint;
    err = load_elf(vn, &entrypoint);
    if (err) {
        vfs_close(vn);
        for (int j = 0; j < args; j++) {
            kfree(kargv[j]);
        }
        kfree(kpathname);
        kfree(kargv);
        return err;
    }

    /* DONE WITH THE FILE NOW */
    vfs_close(vn);

    /* DEFINE THE USER STACK IN THE ADDRESS SPACE */
    vaddr_t stackptr;
    err = as_define_stack(newas, &stackptr);
    if (err) {
        for (int j = 0; j < args; j++) {
            kfree(kargv[j]);
        }
        kfree(kpathname);
        kfree(kargv);
        return err;
    }


    /* COPYING BACK ARGS FROM KERNEL LAND TO USER LAND */
    size_t arg_length = 0;
    size_t pad_length = 0;
    vaddr_t copy_addr = stackptr;
    for (int i = 0; i < args; i++) {

        /* RETRIEVING LENGTH AND PADDING */
        arg_length = strlen(kargv[i]) + 1;                              // +1 due to \0 at the end which need to be added
        pad_length = (arg_length % 4 == 0) ? 0 : 4 - (arg_length % 4);  // test

        /* COMPUTING ADDRESS FOR COPYING */
        copy_addr -= arg_length;
        copy_addr -= pad_length;

        /* ACTUAL COPY OF THE ARGUMENT */
        err = copyout(kargv[i], (userptr_t) copy_addr, arg_length);
        if (err) {
            for (int j = 0; j < args; j++) {
                kfree(kargv[j]);
            }
            kfree(kpathname);
            kfree(kargv);
            return err;
        }
    }
    // char *null_pointer = NULL;
    // copy_addr -= 4;
    // err = copyout(null_pointer, (userptr_t) copy_addr, 4);
    // if (err) {
    //     return err;
    // }

    /* WRAP TO USER MODE */
    enter_new_process(
        (int) args,
        (userptr_t) stackptr,
        NULL,
        stackptr,
        entrypoint
    );

    /* SHOULD NOT GET HERE */
    return EINVAL;
}
#endif