/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
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
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
        struct semaphore *sem;

        sem = kmalloc(sizeof(*sem));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
        sem->sem_count = initial_count;

        return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

void
P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
        }
        KASSERT(sem->sem_count > 0);
        sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

        sem->sem_count++;
        KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
        struct lock *lock;

        /* STRUCTURE INIT */
        lock = kmalloc(sizeof(*lock));
        if (lock == NULL) {
                return NULL;
        }

        /* ASSIGN A NAME TO THE LOCK */
        lock->lk_name = kstrdup(name);
        if (lock->lk_name == NULL) {
                kfree(lock);
                return NULL;
        }

	HANGMAN_LOCKABLEINIT(&lock->lk_hangman, lock->lk_name);

#if OPT_SHELL
        /* CREATING THE NEW WAITING CHANNEL AND CHECKING */
        lock->lk_wchan = wchan_create(lock->lk_name);
        if (lock->lk_wchan == NULL) {
                kfree(lock->lk_name);
                kfree(lock);
                return NULL;
        }

        /* INITALIZATION OF THE OWNER AND THE SPINLOCK */
        lock->lk_owner = NULL; // at the beginning, no thread owns this lock
        spinlock_init(&lock->lk_lock);
#endif

        return lock;
}

void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);

#if OPT_SHELL
        /* CLEANING UP USED STUFF */
        spinlock_cleanup(&lock->lk_lock);
        wchan_destroy(lock->lk_wchan);
#endif

        kfree(lock->lk_name);
        kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	/* Call this (atomically) before waiting for a lock */
	//HANGMAN_WAIT(&curthread->t_hangman, &lock->lk_hangman);

#if OPT_SHELL

        /* BE SURE THAT LOCK EXISTS*/
        KASSERT(lock != NULL);

        /* BE SURE THAT THE CURRENT THREAD DOES NOT ALREADY OWN THE LOCK */
        KASSERT(lock_do_i_hold(lock) == false);

        /* BE SURE THE CURRENT THREAD HAS INTERRUPTS DISABLED */
        KASSERT(curthread->t_in_interrupt == false);

        /* ATTEMPT TO ACQUIRE THE SPINLOCK, OTHERWISE SLEEP */
        spinlock_acquire(&lock->lk_lock); 
        while (lock->lk_owner != NULL) {
	        wchan_sleep(lock->lk_wchan, &lock->lk_lock);
        }

        /* GET LOCK OWNERSHIP */
        KASSERT(lock->lk_owner == NULL);
        lock->lk_owner = curthread;

        /* RELEASING SPINLOCK */
        spinlock_release(&lock->lk_lock);

#else 
        (void)lock;  // suppress warning until code gets written
#endif
	/* Call this (atomically) once the lock is acquired */
	//HANGMAN_ACQUIRE(&curthread->t_hangman, &lock->lk_hangman);
}

void
lock_release(struct lock *lock)
{
	/* Call this (atomically) when the lock is released */
	//HANGMAN_RELEASE(&curthread->t_hangman, &lock->lk_hangman);

#if OPT_SHELL
        /* BE SURE THAT LOCK EXISTS*/
        KASSERT(lock != NULL);

        /* BE SURE THAT THE CURRENT THREAD OWNS THE LOCK */
        KASSERT(lock_do_i_hold(lock) == true);

        /* ACQUIRE THE LOCK AND CLEAR THE OWNERSHIP */
        spinlock_acquire(&lock->lk_lock);
        lock->lk_owner = NULL;

        /* SIGNALLING THREADS WAITING FOR THE LOCK */
        wchan_wakeone(lock->lk_wchan, &lock->lk_lock);

        /* RELEASING THE LOCK */
        spinlock_release(&lock->lk_lock);
#else
        (void)lock;  // suppress warning until code gets written
#endif
}

bool
lock_do_i_hold(struct lock *lock)
{
        bool res = true;

#if OPT_SHELL

        /* CHECKING WHETHER THE CURRENT THREAD IS ALREADY THE OWNER OF THE LOCK */
        spinlock_acquire(&lock->lk_lock);
	res = (lock->lk_owner == curthread);
	spinlock_release(&lock->lk_lock);
#else 
        (void)lock;  // suppress warning until code gets written
#endif
        return res; 
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(*cv));
        if (cv == NULL) {
                return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
                kfree(cv);
                return NULL;
        }

#if OPT_SHELL
        /* WAITCHANNEL INITIALIZATION */
        cv->cv_wchan = wchan_create(cv->cv_name);
        if (cv->cv_wchan == NULL) {
                kfree(cv->cv_name);
                kfree(cv);
                return NULL;
        }

        /* SPINLOCK INITIALIZATION */
        spinlock_init(&cv->cv_lock);
#endif

        return cv;
}

void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);

#if OPT_SHELL
        spinlock_cleanup(&cv->cv_lock);
        wchan_destroy(cv->cv_wchan);
#endif

        kfree(cv->cv_name);
        kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
#if OPT_SHELL
        /* ASSERT LOCK AND CONDITION VARIABLE TO EXIST */
        KASSERT(cv != NULL);
        KASSERT(lock != NULL);

        /* ASSERT CURRENT THREAD TO ACTUALLY BE THE OWNER OF THE LOCK */
        KASSERT(lock_do_i_hold(lock));

        /* ACQUIRING THE LOCK AND GET TO SLEEP */
        spinlock_acquire(&cv->cv_lock);

        /* 
         * G.Cabodi - 2019: spinlock already owned as atomic lock_release+wchan_sleep needed 
        */

	lock_release(lock);
	wchan_sleep(cv->cv_wchan,&cv->cv_lock);
	spinlock_release(&cv->cv_lock);
        
	/* 
         * G.Cabodi - 2019: spinlock already  released to avoid ownership while (possibly) going 
         * to wait state in lock_acquire. Atomicity wakeup+lock_acquire not guaranteed (but not 
         * necessary!) 
        */
	lock_acquire(lock);
#endif
        (void)cv;    // suppress warning until code gets written
        (void)lock;  // suppress warning until code gets written
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
#if OPT_SHELL
        /* ASSERT LOCK AND CONDITION VARIABLE TO EXIST */
        KASSERT(cv != NULL);
        KASSERT(lock != NULL);

        /* ASSERT CURRENT THREAD TO ACTUALLY BE THE OWNER OF THE LOCK */
        KASSERT(lock_do_i_hold(lock));

	/* 
         * G.Cabodi - 2019: here the spinlock is NOT required, as no atomic operation 
         * has to be done. The spinlock is just acquired because needed by wakeone 
        */

        /* ACQUIRING THE LOCK AND WAKING UP THE FIRST THREAD IN THE LIST */
	spinlock_acquire(&cv->cv_lock);
	wchan_wakeone(cv->cv_wchan,&cv->cv_lock);
	spinlock_release(&cv->cv_lock);
#endif
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
#if OPT_SHELL
        /* ASSERT LOCK AND CONDITION VARIABLE TO EXIST */
        KASSERT(cv != NULL);
        KASSERT(lock != NULL);

        /* ASSERT CURRENT THREAD TO ACTUALLY BE THE OWNER OF THE LOCK */
        KASSERT(lock_do_i_hold(lock));

	/* 
         * G.Cabodi - 2019: here the spinlock is NOT required, as no atomic operation 
         * has to be done. The spinlock is just acquired because needed by wakeone 
        */

        /* ACQUIRING THE LOCK AND WAKING UP THE FIRST THREAD IN THE LIST */
	spinlock_acquire(&cv->cv_lock);
	wchan_wakeall(cv->cv_wchan,&cv->cv_lock);
	spinlock_release(&cv->cv_lock);
#endif
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
}
