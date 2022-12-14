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
#include <mips/trapframe.h>
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
 * @brief The name of the file referred to by pathname is removed from the filesystem. 
 *        The actual file itself is not removed until no further references to it exist, 
 *        whether those references are on disk or in memory.
 * 
 * @param pathname specify an existing file.
 * @return zero on success, an error value in case of failure.
 */
#if OPT_SHELL
int sys_remove_SHELL(const char *pathname);
#endif

/**
 * @brief The current directory of the current process is set to the directory named by pathname. 
 * 
 * @param pathname directory to be set as current.
 * @return zero on success, an error value in case of failure.
 */
#if OPT_SHELL
int sys_chdir_SHELL(const char *pathname);
#endif

/**
 * @brief The name of the current directory is computed and stored in buf, an area of size buflen. 
 *        The length of data actually stored, which must be non-negative, is returned. 
 * 
 * @param buf buffer to store the result
 * @param buflen length of the buffer in which to store the result
 * @param retval the length of data actually stored
 * @return zero on success, an error value in case of failure 
 */
#if OPT_SHELL
int sys_getcwd_SHELL(const char *buf, size_t buflen, int32_t *retval);
#endif

/**
 * @brief Alters the current seek position of the file handle fd, seeking to a new position based on pos and whence. 
 * 
 *         If whence is
 * 
 *                  SEEK_SET, the new position is pos.
 *                  SEEK_CUR, the new position is the current position plus pos.
 *                  SEEK_END, the new position is the position of end-of-file plus pos.
 *                  anything else, lseek fails. 
 * 
 * @param fd file handle
 * @param pos signed quantity indicating the offset to add
 * @param whence flag indicating the operation to perform
 * @param retval_low32 new seek position of the file (lower bytes)
 * @param retval_upp32 new seek position of the file (upper bytes)
 * @return zero on success, and error value in case of failure
 */
#if OPT_SHELL
int sys_lseek_SHELL(int fd, off_t pos, int whence, int32_t *retval_low32, int32_t *retval_upp32);
#endif

/**
 * @brief dup2 clones the file handle oldfd onto the file handle newfd. If newfd names an open file, 
 *        that file is closed. 
 * 
 * @param oldfd old file descriptor
 * @param newfd new file descriptor
 * @param retval new file descriptor
 * @return zero on success, an error value on failure 
 */
#if OPT_SHELL
int sys_dup2_SHELL(int oldfd, int newfd, int32_t *retval);
#endif






















/**
 * @brief Retrieve the PID of the current process
 * 
 * @param retval PID of the current process
 * @return zero on success
 */
#if OPT_SHELL
int sys_getpid_SHELL(pid_t *retval);
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
int sys_waitpid_SHELL(pid_t pid, int *status, int options, int32_t *retval);
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
int sys_fork_SHELL(struct trapframe *ctf, pid_t *retval);
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
int sys_execv_SHELL(const char *pathname, char *argv[]);
#endif

#if OPT_SHELL
/* Setup function for exec. */
void exec_bootstrap(void);
#endif

#endif /* _SYSCALL_SHELL_H_ */