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
 * @brief sys_open_SHELL() opens the file, device, or other kernel object named by the pathname 
 *        provided. The flags argument specifies how to open the file. The optional mode argument 
 *        is only meaningful in Unix and can be ignored. 
 * 
 * ERRORS:
 *      //ENODEV		The device prefix of filename did not exist.
 *      ENOTDIR		A non-final component of filename was not a directory.
 *      //ENOENT		A non-final component of filename did not exist.
 *      //ENOENT		The named file does not exist, and O_CREAT was not specified.
 *      EEXIST		The named file exists, and O_EXCL was specified.
 *      EISDIR		The named object is a directory, and it was to be opened for writing.   
 *      //EMFILE		The process's file table was full, or a process-specific limit on open files was reached.  
 *      //ENFILE		The system file table is full, if such a thing exists, or a system-wide limit on open files was reached.
 *      //ENXIO		The named object is a block device with no mounted filesystem.
 *      ENOSPC		The file was to be created, and the filesystem involved is full.
 *      //EINVAL		flags contained invalid values.
 *      EIO		    A hard I/O error occurred.
 *      //EFAULT		filename was an invalid pointer. 
 * 
 * @param pathname relative or absolute path of the file to open
 * @param openflags how to open the file
 * @param mode can be ignored
 * @param retval file handler of the open file
 * @return zero on success. an error value in case of failure 
 */
#if OPT_SHELL
int sys_open_SHELL(userptr_t pathname, int openflags, mode_t mode, int32_t *retval) {

    /* COPYING PATHNAME TO KERNEL SIDE */
    // this is done for two reasons:
    // 1) security reason
    // 2) vfs_open may destroy the buffer
    char *kbuffer = (char *) kmalloc(strlen((const char *) pathname) * sizeof(char));
    if (kbuffer == NULL) {
        return ENOMEM;
    }
    size_t len;
    int err = copyinstr((const_userptr_t) pathname, kbuffer, PATH_MAX, &len); // may return EFAULT
    if (err) {
        kfree(kbuffer);
        return err;
    }

    /* OPENING WITH VFS UTILITY */
    struct vnode *v;
    err = vfs_open(kbuffer, openflags, mode, &v);   // may return ENOENT, ENXIO, ENODEV
    if (err) {
        kfree(kbuffer);
        return err;
    }
    kfree(kbuffer);

    /* RETRIEVING A FREE POSITION IN THE SYSTEM FILETABLE */
    struct openfile *of = NULL;
    for (int index = 0; index < SYSTEM_OPEN_MAX; index++) {
        if (systemFileTable[index].vn == NULL) {
            of = &systemFileTable[index];
            of->vn = v;
            break;
        }
    }

    /* ASSIGNING OPENFILE TO CURRENT PROCESS FILETABLE */
    int fd = 3;     // skipping STDIN, STDOUT and STDERR
    if (of == NULL) {
        return ENFILE;  // system file table is full
    } else {
        for (; fd < OPEN_MAX; fd++) {
            if (curproc->fileTable[fd] == NULL) {
                curproc->fileTable[fd] = of;
                break;
            }
        }

        if (fd == OPEN_MAX - 1) {
            return EMFILE;  // process file table is full
        }

    }

    /* MANAGING OFFSET */
    // if flag specified O_APPEND, the operation on the file should start at the end
    // otherwise, it should start at the beginning
    if (openflags & O_APPEND) {

            /* RETRIEVING FILE SIZE */
            struct stat filestat;
            err = VOP_STAT(curproc->fileTable[fd]->vn, &filestat);
            if (err) {
                kfree(curproc->fileTable[fd]);
                curproc->fileTable[fd] = NULL;
                return err;
            }
            curproc->fileTable[fd]->offset = filestat.st_size;
    } else {

            /* STARTING FROM THE BEGINNING */
            curproc->fileTable[fd]->offset = 0;
    }

    /* MANAGING REFERENCES */
    curproc->fileTable[fd]->count_refs = 1;

    /* MANAGING MODE */
    switch(openflags & O_ACCMODE){
	    case O_RDONLY:
			curproc->fileTable[fd]->mode_open = O_RDONLY;
			break;
		case O_WRONLY:
			curproc->fileTable[fd]->mode_open = O_WRONLY;
			break;
		case O_RDWR:
			curproc->fileTable[fd]->mode_open = O_RDWR;
			break;
		default:
			vfs_close(curproc->fileTable[fd]->vn);
			kfree(curproc->fileTable[fd]);
			curproc->fileTable[fd] = NULL;
			return EINVAL;
	}

    /* CREATING LOCK ON THIS FILE */
    curproc->fileTable[fd]->lock = lock_create("FILE_LOCK");
    if (curproc->fileTable[fd]->lock == NULL) {
        vfs_close(curproc->fileTable[fd]->vn);
        kfree(curproc->fileTable[fd]);
        curproc->fileTable[fd] = NULL;
        return ENOMEM;
    }

    /* TASK COMPLETED SUCCESSFULLY */
    *retval = fd;
    kprintf("[DEBUG] fd is: %d\n", fd);
    return 0;
}
#endif

/**
 * @brief the file handle fd is closed.
 * 
 * @param fd file descriptor
 * @return zero on success, an error value in case of failure 
*/
#if OPT_SHELL
int sys_close_SHELL(int fd) {

    /* CHECKING FILE DESCRIPTOR */
    if (fd < 0 || fd >= OPEN_MAX) {                                 /* fd should be a valid number                          */
        return EBADF;       
    } else if (curproc->fileTable[fd] == NULL) {                    /* fd should refer to a valid entry in the fileTable    */
        return EBADF;
    }

    /* REDUCING REFERENCES */
    struct openfile *of = curproc->fileTable[fd];
    lock_acquire(of->lock);
    curproc->fileTable[fd] = NULL;
    if (--of->count_refs > 0) {

        /* THIS FILE IS STILL REFERENCED BY SOME PROCESS */
        lock_release(of->lock);
        return 0;
    } else {

        /* NO MORE PROCESS REFER TO THIS FILE, CLOSING ALSO VNODE */
        struct vnode *vn = of->vn;
        of->vn = NULL;
        vfs_close(vn);
    }

    lock_release(of->lock);
    return 0;
}
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
int sys_remove_SHELL(const char *pathname) {

    /* NOT IMPLEMENTED (YET?) */
    (void) pathname;

    /* TASK COMPLETED SUCCESSFULLY */
    return 0;
}
#endif

/**
 * @brief The current directory of the current process is set to the directory named by pathname. 
 * 
 * @param pathname directory to be set as current.
 * @return zero on success, an error value in case of failure.
 */
#if OPT_SHELL
int sys_chdir_SHELL(const char *pathname) {

    /* SOME ASSERTIONS */
    KASSERT(curthread != NULL);
    KASSERT(curthread->t_proc != NULL);

    /* COPYING PATHNAME FROM USERLAND TO KERNEL LAND */
    // 1) security reason
    // 2) pathname may be corrupted by vfs utilities
    char *kbuffer = (char *) kmalloc(PATH_MAX * sizeof(char));
    if (kbuffer == NULL) {
        return ENOMEM;
    }
    int err = copyinstr((const_userptr_t) pathname, kbuffer, sizeof(kbuffer), NULL);
    if (err) {
        kfree(kbuffer);
        return err;     // may return EFAULT, ENAMETOOLONG
    }

    /* OPEN DIRECTORY POINTED BY PATHANME */
    struct vnode *vn = NULL;
    err = vfs_open(kbuffer, O_RDONLY, 0644, &vn);
    if (err) {
        kfree(kbuffer);
        return err; // may return EINVAL, ENOENT
    }
    kfree(kbuffer);

    /* CHANGE CURRENT DIRECTORY WITH VFS UTILITY */
    err = vfs_setcurdir(vn);
    if (err) {
        vfs_close(vn);
        return err;
    }

    /* CLOSING VNODE */
    vfs_close(vn);

    /* TASK COMPLETED SUCCESSFULLY */
    return 0;
}
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
int sys_getcwd_SHELL(const char *buf, size_t buflen, int32_t *retval) {

    /* SOME ASSERTIONS */
    KASSERT(curthread != NULL);
    KASSERT(curthread->t_proc != NULL);

    /* INITIALIZING DATA FOR READING */
    struct uio u;
    struct iovec iov;
    iov.iov_ubase = (userptr_t) buf;            
	iov.iov_len = buflen;
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_resid = buflen;
	u.uio_offset = 0;
	u.uio_segflg = UIO_USERSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = curthread->t_proc->p_addrspace;

    /* READING CURRENT DIRECTORY WITH VFS UTILITIES */
    int err = vfs_getcwd(&u);
	if (err) {
		return err;
	}

    /* TASK COMPLETED SUCCESSFULLY */
    *retval = buflen - u.uio_resid;
    return 0;
}
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
int sys_dup2_SHELL(int oldfd, int newfd, int32_t *retval) {

    struct openfile *of;

    /* SOME ASSERTION */
    KASSERT(curproc != NULL);

    /* CHECKING INPUT ARGUMENTS */
    if (oldfd < 0 || oldfd >= OPEN_MAX || newfd < 0 || newfd >= OPEN_MAX) {
        return EBADF;   // invalid file handler
    } else if (curproc->fileTable[oldfd] == NULL) {
        return EBADF;   // invalid file handler
    } else if (oldfd == newfd) {
        return 0;       // using dup2 to clone a file handle onto itself has no effect
    } 

    /* CHECKING WHETHER newfd REFERS TO AN OPEN FILE */
    if (curproc->fileTable[newfd] != NULL) {
        
        /* newfd REFERS TO AN OPEN FILE --> CLOSE IT */
        of = curproc->fileTable[newfd];
        lock_acquire(of->lock);
        curproc->fileTable[newfd] = NULL;
        if (--of->count_refs == 0) {

            /* NO MORE PROCESS REFER TO THIS FILE, CLOSING ALSO VNODE */
            struct vnode *vn = of->vn;
            of->vn = NULL;
            vfs_close(vn);
        }
        of = NULL;
        lock_release(of->lock);
    }

    /* INCREMENTIG COUNTING REFERENCES */
    of = curproc->fileTable[oldfd];
    lock_acquire(of->lock);
    of->count_refs++;
    lock_release(of->lock);

    /* ASSIGNING TO NEW FILE DESCRIPTOR */
    curproc->fileTable[newfd] = of;     // i.e.     curproc->fileTable[newfd] = curproc->fileTable[oldfd];

    /* TASK COMPLETED SUCCESSFULLY */
    *retval = newfd;
    return 0;
}
#endif