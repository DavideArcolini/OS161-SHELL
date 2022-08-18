/**
 * @file syscall_SHELL.h
 * @author Davide Arcolini (davide.arcolini@studenti.polito.it)
 * 
 * @brief Contains the declarations of the system calls used in the SHELL project. 
 *        For the sake of clarity and separation of concerns, this new header file has been 
 *        created, separatedly from syscall.h, which contains the original system calls of the 
 *        operating system.
 * 
 * @version 0.1
 * @date 2022-08-17
 * 
 * @copyright Copyright (c) 2022
 * 
*/

#ifndef _SYSCALL_SHELL_H_
#define _SYSCALL_SHELL_H_

#include <types.h>
#include <lib.h>
#include "opt-shell.h"

/**
 * @brief struct defining a pointer to a specific vnode.
 *        Used to build an array of pointers to vnodes (i.e. files) opened for
 *        a given user process.
*/
struct openfile {
    struct vnode *vn;           /* Pointer to the vnode storing the file                                                */
    off_t offset;               /* Define the current offset for the file                                               */
    int mode_open;              /* Define the opening mode for the current file (i.e., read-only, write-only, etc...)   */
    unsigned int count_refs;    /* Count the number of processes which have currently opened this file                  */
    struct lock *lock;          /* Define the lock for this open file                                                   */
};


/**
 * @brief sys_write_SHELL() writes up to buflen bytes to the file specified by fd, 
 *        at the location in the file specified by the current seek position of the 
 *        file, taking the data from the space pointed to by buf. The file must be 
 *        open for writing.
 * 
 *        The current seek position of the file is advanced by the number of bytes written.
 *        Each write (or read) operation is atomic relative to other I/O to the same file. 
 * 
 * @param fd destination file
 * @param buf source buffer
 * @param buflen number of bytes to be written
 * @param retval actual number of bytes written
 * @return zero on success. an error value in case of failure
*/
#if OPT_SHELL
ssize_t sys_write_SHELL(int fd, const void *buf, size_t buflen, int32_t *retval);
#endif

/**
 * @brief sys_read_SHELL() reads up to buflen bytes from the file specified by fd, at the 
 *        location in the file specified by the current seek position of the file, and 
 *        stores them in the space pointed to by buf. The file must be open for reading.
 *  
 *        The current seek position of the file is advanced by the number of bytes read. 
 * 
 * @param fd source file
 * @param buf destination buffer
 * @param count number of bytes to be read
 * @return zero on success. an error value in case of failure
*/
#if OPT_SHELL
ssize_t sys_read_SHELL(int fd, const void *buf, size_t buflen, int32_t *retval);
#endif


/**
 * @brief sys_open_SHELL() opens the file, device, or other kernel object named by the pathname 
 *        provided. The flags argument specifies how to open the file. The optional mode argument 
 *        is only meaningful in Unix and can be ignored. 
 * 
 * @param pathname relative or absolute path of the file to open
 * @param openflags how to open the file
 * @param mode can be ignored
 * @param retval file handler of the open file
 * @return zero on success. an error value in case of failure 
 */
#if OPT_SHELL
int sys_open_SHELL(userptr_t pathname, int openflags, mode_t mode, int32_t *retval);
#endif

/**
 * @brief the file handle fd is closed.
 * 
 * @param fd file descriptor
 * @return zero on success, an error value in case of failure 
*/
#if OPT_SHELL
int sys_close_SHELL(int fd);
#endif

/**
 * @brief Cause the current process to exit.
 * 
 * @param exitcode reported back to other process(es) via the waitpid() call.
 * @return does not return.
 */
#if OPT_SHELL
void sys_exit_SHELL(int exitcode);
#endif

#endif /* _SYSCALL_SHELL_H_ */