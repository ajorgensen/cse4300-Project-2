/*
 * Synchronization primitives.
 * See synch.h for specifications of the functions.
 */

#include <types.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *namearg, int initial_count)
{
	struct semaphore *sem;

	assert(initial_count >= 0);

	sem = kmalloc(sizeof(struct semaphore));
	if (sem == NULL) {
		return NULL;
	}

	sem->name = kstrdup(namearg);
	if (sem->name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->count = initial_count;
	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	spl = splhigh();
	assert(thread_hassleepers(sem)==0);
	splx(spl);

	/*
	 * Note: while someone could theoretically start sleeping on
	 * the semaphore after the above test but before we free it,
	 * if they're going to do that, they can just as easily wait
	 * a bit and start sleeping on the semaphore after it's been
	 * freed. Consequently, there's not a whole lot of point in 
	 * including the kfrees in the splhigh block, so we don't.
	 */

	kfree(sem->name);
	kfree(sem);
}

void 
P(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	assert(in_interrupt==0);

	spl = splhigh();
	while (sem->count==0) {
		thread_sleep(sem);
	}
	assert(sem->count>0);
	sem->count--;
	splx(spl);
}

void
V(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);
	spl = splhigh();
	sem->count++;
	assert(sem->count>0);
	thread_wakeup(sem);
	splx(spl);
}

////////////////////////////////////////////////////////////
//
// Lock.


////////////////////////////////////////////////////////////////////////////////////////
// the code for acquiring/releasing locks and cv's is based on code listed online located here: http://www.cdf.toronto.edu/~csc369h/winter/stg/lectures/ossynch_winter11-4.pdf
////////////////////////////////////////////////////////////////////////////////////////

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(struct lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->name = kstrdup(name);
	if (lock->name == NULL) {
		kfree(lock);
		return NULL;
	}
	
	// add stuff here as needed
	
	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	// add stuff here as needed
	
	kfree(lock->name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	// Write this
	int spl; //variable for the priority level
	
	assert (lock != NULL); //can't do anything if the lock is null
  assert(!lock_do_i_hold(lock)); //don't acquire a lock if we already have it
  spl = splhigh(); //set priority level to HIGH
  
  while (lock->owner != NULL) {
    thread_sleep(lock);
  }
  
  assert(lock->owner == NULL); //make sure the lock has no owner
  lock->owner = curthread; //set the lock's owner to the thread that is currently executing
  splx(spl); //set priority level to spl

}

void
lock_release(struct lock *lock)
{
	// Write this
	int spl; //variable for priority level
	assert(lock != NULL); //can't do anything if lock is null
	assert(lock_do_i_hold(lock)); //make sure that we have the lock before trying to release
	
	spl = splhigh(); //set priority level to HIGH
	lock->owner = NULL; //set lock owner to null
	thread_wakeup(lock); //wakeup all threads waiting on the lock
	splx(spl); //set priority level to spl

}

int
lock_do_i_hold(struct lock *lock)
{
	// Write this

	return (lock->owner == curthread); //compare the lock's owner member to the curthread
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(struct cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->name = kstrdup(name);
	if (cv->name==NULL) {
		kfree(cv);
		return NULL;
	}
	
	// add stuff here as needed
	
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	// add stuff here as needed
	
	kfree(cv->name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	// Write this
	int spl;
	
	//make sure the arguments are not null
	assert(lock);
	assert(cv);
	//assert that the current thread actually has the lock
	assert(lock_do_i_hold(lock));
	
	//set the priority level to high, release the lock, put the thread to sleep on the cv address, and re-acquire the lock
	spl = splhigh();
	lock_release(lock);
	thread_sleep(cv);
	lock_acquire(lock);
	splx(spl);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	// Write this
	int spl;
	
	//make sure the args are not null
  assert(cv);
  assert(lock);
  //ensure that the current thread has the lock
  assert(lock_do_i_hold(lock));
  
  //set priority to high, wake the first available thread waiting on cv
  spl = splhigh();
  thread_wakeone(cv);
  splx(spl);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	// Write this
	int spl;
	
	//make sure args are not null
	assert(cv);
	assert(lock);
	//ensure the current thread has the lock
	assert(lock_do_i_hold(lock));
	
	//set priority to high, wake all threads waiting on cv
	spl = splhigh();
	thread_wakeup(cv);
	splx(spl);
}
