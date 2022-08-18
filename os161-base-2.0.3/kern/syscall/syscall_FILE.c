/**
 * @file syscall_FILE.c
 * @author Davide Arcolini (davide.arcolini@studenti.polito.it)
 * 
 * @brief Contains the definitions of the system calls used in the SHELL project. 
 *        In particular, it contains the definition of the system calls used in the management
 *        of files, such as open(), close(), read(), write(), etc...
 * 
 * @version 0.1
 * @date 2022-08-17
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
#include "syscall_SHELL.h"

#define SYSTEM_OPEN_MAX (10*OPEN_MAX)

#if OPT_SHELL
struct openfile systemFileTable[SYSTEM_OPEN_MAX];
#endif

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
ssize_t sys_write_SHELL(int fd, const void *buf, size_t buflen, int32_t *retval) {

    /* CHECKING FILE DESCRIPTOR */
    if (fd < 0 || fd >= OPEN_MAX) {                                 /* fd should be a valid number                          */
        return EBADF;       
    } else if (curproc->fileTable[fd] == NULL) {                    /* fd should refer to a valid entry in the fileTable    */
        return EBADF;
    } else if (curproc->fileTable[fd]->mode_open == O_RDONLY) {     /* fd should refer to a file allowed to be written      */
        return EBADF;
    }

    /* COPYING BUFFER TO KERNEL SIDE (copyin()) */
    // NB: we are implementing the system call in such a way that it will use a kernel buffer;
    //     this is not strictly necessary, but by doing so, it will exploit the already implemented
    //     uio_kinit() function without the need to properly initialize the uio struct.
    char *kbuffer = (char *) kmalloc(buflen * sizeof(char));
    if (kbuffer == NULL) {
        return ENOMEM;
    } else if (copyin((const_userptr_t) buf, kbuffer, buflen)) {
        kfree(kbuffer);
        return EFAULT;
    }

    /* PERFORMING WRITING (VOP_WRITE()) */
    struct iovec iov;
	struct uio kuio;
    struct openfile *of = curproc->fileTable[fd];
    struct vnode *vn = of->vn;

    lock_acquire(of->lock);
    uio_kinit(&iov, &kuio, kbuffer, buflen, of->offset, UIO_WRITE);
    int error = VOP_WRITE(vn, &kuio);
    if (error) {
        kfree(kbuffer);
        return error;
    }

    /* REPOSITION OF THE OFFSET */
    off_t nbytes = kuio.uio_offset - of->offset;
    *retval = (int32_t) nbytes;
    of->offset = kuio.uio_offset;

    /* FREEING KERNEL BUFFER */
    lock_release(of->lock);
    kfree(kbuffer);

    /* TASK COMPLETED SUCCESSFULLY */
    return 0;
}
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
ssize_t sys_read_SHELL(int fd, const void *buf, size_t buflen, int32_t *retval) {

    /* CHECKING FILE DESCRIPTOR */
    if (fd < 0 || fd >= OPEN_MAX) {                                 /* fd should be a valid number                          */
        return EBADF;       
    } else if (curproc->fileTable[fd] == NULL) {                    /* fd should refer to a valid entry in the fileTable    */
        return EBADF;
    } else if (curproc->fileTable[fd]->mode_open == O_WRONLY) {     /* fd should refer to a file allowed to be read         */
        return EBADF;
    }

    /* PREPARING KERNEL BUFFER */
    char *kbuffer = (char *) kmalloc(buflen * sizeof(char));
    if (kbuffer == NULL) {
        return ENOMEM;
    }

    /* PERFORMING READING (VOP_READ()) */
    struct openfile *of = curproc->fileTable[fd];
    struct iovec iov;
    struct uio kuio;
    struct vnode *vn = of->vn;
    lock_acquire(of->lock);
    uio_kinit(&iov, &kuio, kbuffer, buflen, of->offset, UIO_READ);
    int err = VOP_READ(vn, &kuio);
    if (err) {
        kfree(kbuffer);
        return err;
    }

    /* REPOSITION OF THE OFFSET */
    of->offset = kuio.uio_offset;
    *retval = buflen - kuio.uio_resid;

    /* COPYING BUFFER TO USER SIDE (COPYOUT()) */
    // NB: we are implementing the system call in such a way that it will use a kernel buffer;
    //     this is not strictly necessary, but it will be a choice of design
    if (copyout(kbuffer, (userptr_t) buf, *retval)) {
        kfree(kbuffer);
        return EFAULT;
    }

    /* FREEING KERNEL BUFFER */
    lock_release(of->lock);
    kfree(kbuffer);

    /* TASK COMPLETED SUCCESSFULLY */
    return 0;
}
#endif

/**
 * @brief sys_open_SHELL() opens the file specified by pathname.
 *        If the specified file does not exist, it may optionally (if O_CREAT is specified 
 *        in flags) be created.
 * 
 *        The return value of open() is a file descriptor, a small, nonnegative integer that 
 *        is an index to an entry in the process's table of open file descriptors.
 * 
 * @param pathname relative or absolute path of the file
 * @param openflags optional flags specifying how to open the file (only read, only write, etc...)
 * @param mode specify the access mode to the file (granting access to specific users or groups)
 * @param errp error value in case of failure
 * @return the file descriptor value fd
*/
// #if OPT_SHELL
// int sys_open_SHELL(userptr_t pathname, int openflags, mode_t mode, int *errp) {

//     // int fd;
//     // struct vnode *v;
    
//     // /* OPENING FILE WITH VFS UTILITIES FUNCTIONS */
//     // int result = vfs_open((char *) pathname, openflags, mode, &v);
//     // if (result) {
//     //     errp = ENOENT;
//     //     return -1;
//     // }

//     (void) pathname;
//     (void) openflags;
//     (void) mode;
//     (void *) errp;
//     return -1;

// }
// #endif

/**
 * @brief sys_close_SHELL() closes a file descriptor, so that it no longer refers to any 
 *        file and may be reused.
 *        If fd is the last file descriptor referring to the underlying open file description, 
 *        the resources associated with the open file description are freed.
 * 
 *        It returns zero on success. On error, -1 is returned, and errno is set to indicate 
 *        the error.
 * 
 * @param fd file descriptor
 * @return zero on success
*/
// #if OPT_SHELL
// int sys_close_SHELL(int fd) {

//     (void) fd;
//     return 0;
// }
// #endif