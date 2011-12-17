/*
 * Core thread system.
 */
#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <array.h>
#include <machine/spl.h>
#include <machine/pcb.h>
#include <thread.h>
#include <curthread.h>
#include <scheduler.h>
#include <addrspace.h>
#include <vnode.h>
#include "opt-synchprobs.h"

/* States a thread can be in. */
typedef enum {
	S_RUN,
	S_READY,
	S_SLEEP,
	S_ZOMB,
	S_JOIN,
} threadstate_t;

/* Global variable for the thread currently executing at any given time. */
struct thread *curthread;

/* Table of sleeping threads. */
static struct array *sleepers;

/* List of dead threads to be disposed of. */
static struct array *zombies;

static struct array *joinable;

/* Total number of outstanding threads. Does not count zombies[]. */
static int numthreads;

/*
 * Create a thread. This is used both to create the first thread's 
 * thread structure and to create subsequent threads.
 */

static
struct thread *
thread_create(const char *name)
{
	struct thread *thread = kmalloc(sizeof(struct thread));
	if (thread==NULL) {
		return NULL;
	}
	thread->t_name = kstrdup(name);
	if (thread->t_name==NULL) {
		kfree(thread);
		return NULL;
	}
	thread->t_sleepaddr = NULL;
	thread->t_stack = NULL;
	
	thread->t_vmspace = NULL;

	thread->t_cwd = NULL;
	
	thread->t_pid = kmalloc(sizeof(pid_t));
	thread->parent = kmalloc(sizeof(pid_t));
	thread->t_exit = kmalloc(sizeof(int));
	thread->is_detached = kmalloc(sizeof(int));
	thread->has_exited = kmalloc(sizeof(int));
	thread->children = array_create();
	
	// If you add things to the thread structure, be sure to initialize
	// them here.
	
	return thread;
}

/*
 * Destroy a thread.
 *
 * This function cannot be called in the victim thread's own context.
 * Freeing the stack you're actually using to run would be... inadvisable.
 */
static
void
thread_destroy(struct thread *thread)
{
	assert(thread != curthread);

	// If you add things to the thread structure, be sure to dispose of
	// them here or in thread_exit.

	// These things are cleaned up in thread_exit.
	assert(thread->t_vmspace==NULL);
	assert(thread->t_cwd==NULL);
	
	if (thread->t_stack) {
		kfree(thread->t_stack);
	}
	kfree(thread->t_name);
	
	kfree(thread->t_pid);
	
	kfree(thread->t_pid);
	kfree(thread->parent);
	array_destroy(thread->children);
/*	thread->t_exit = NULL;*/
/*	thread->is_detached = NULL;*/
/*	thread->has_exited = NULL;*/
	kfree(thread->t_exit);
	kfree(thread->is_detached);
	kfree(thread->has_exited);
	kfree(thread);
}


/*
 * Remove zombies. (Zombies are threads/processes that have exited but not
 * been fully deleted yet.)
 */
static
void
exorcise(void)
{
	int i, result;

	assert(curspl>0);
	
	for (i=0; i<array_getnum(zombies); i++) {
		struct thread *z = array_getguy(zombies, i);
		assert(z!=curthread);
		thread_destroy(z);
	}
	result = array_setsize(zombies, 0);
	/* Shrinking the array; not supposed to be able to fail. */
	assert(result==0);
}

/*
 * Kill all sleeping threads. This is used during panic shutdown to make 
 * sure they don't wake up again and interfere with the panic.
 */
static
void
thread_killall(void)
{
	int i, result;

	assert(curspl>0);

	/*
	 * Move all sleepers to the zombie list, to be sure they don't
	 * wake up while we're shutting down.
	 */

	for (i=0; i<array_getnum(sleepers); i++) {
		struct thread *t = array_getguy(sleepers, i);
		kprintf("sleep: Dropping thread %s\n", t->t_name);

		/*
		 * Don't do this: because these threads haven't
		 * been through thread_exit, thread_destroy will
		 * get upset. Just drop the threads on the floor,
		 * which is safer anyway during panic.
		 *
		 * array_add(zombies, t);
		 */
	}

	result = array_setsize(sleepers, 0);
	/* shrinking array: not supposed to fail */
	assert(result==0);
}

/*
 * Shut down the other threads in the thread system when a panic occurs.
 */
void
thread_panic(void)
{
	assert(curspl > 0);

	thread_killall();
	scheduler_killall();
}

/*
 * Thread initialization.
 */
struct thread *
thread_bootstrap(void)
{
	struct thread *me;

	/* Create the data structures we need. */
	sleepers = array_create();
	if (sleepers==NULL) {
		panic("Cannot create sleepers array\n");
	}

	zombies = array_create();
	if (zombies==NULL) {
		panic("Cannot create zombies array\n");
	}
	
	joinable = array_create();
	if (joinable == NULL) {
	  panic("Cannot create joinable array\n");
	}
	/*
	 * Create the thread structure for the first thread
	 * (the one that's already running)
	 */
	me = thread_create("<boot/menu>");
	if (me==NULL) {
		panic("thread_bootstrap: Out of memory\n");
	}

	/*
	 * Leave me->t_stack NULL. This means we're using the boot stack,
	 * which can't be freed.
	 */
	/* Initialize the first thread's pcb */
	md_initpcb0(&me->t_pcb);

	/* Set curthread */
	curthread = me;
	*(curthread->t_pid) = 1;
	*(curthread->parent) = 0;
	
	/* Number of threads starts at 1 */
	numthreads = 1;

	/* Done */
	return me;
}

/*
 * Thread final cleanup.
 */
void
thread_shutdown(void)
{
	array_destroy(sleepers);
	sleepers = NULL;
	array_destroy(zombies);
	zombies = NULL;
	array_destroy(joinable);
	joinable = NULL;
	// Don't do this - it frees our stack and we blow up
	//thread_destroy(curthread);
}

/*
 * Create a new thread based on an existing one.
 * The new thread has name NAME, and starts executing in function FUNC.
 * DATA1 and DATA2 are passed to FUNC.
 */
int
thread_fork(const char *name, 
	    void *data1, unsigned long data2,
	    void (*func)(void *, unsigned long),
	    pid_t **ret)
{
	struct thread *newguy;
	int s, result;
	pid_t pid;

	/* Allocate a thread */
	newguy = thread_create(name);
	if (newguy==NULL) {
		return ENOMEM;
	}

	/* Allocate a stack */
	newguy->t_stack = kmalloc(STACK_SIZE);
	if (newguy->t_stack==NULL) {
		kfree(newguy->t_name);
		kfree(newguy);
		return ENOMEM;
	}
	
	/* Allocate for the children */
/*	newguy->children = array_create();*/

	/* stick a magic number on the bottom end of the stack */
	newguy->t_stack[0] = 0xae;
	newguy->t_stack[1] = 0x11;
	newguy->t_stack[2] = 0xda;
	newguy->t_stack[3] = 0x33;

	/* Inherit the current directory */
	if (curthread->t_cwd != NULL) {
		VOP_INCREF(curthread->t_cwd);
		newguy->t_cwd = curthread->t_cwd;
	}

	/* Set up the pcb (this arranges for func to be called) */
	md_initpcb(&newguy->t_pcb, newguy->t_stack, data1, data2, func);

	/* Interrupts off for atomicity */
	s = splhigh();

	/*
	 * Make sure our data structures have enough space, so we won't
	 * run out later at an inconvenient time.
	 */
	result = array_preallocate(sleepers, numthreads+1);
	if (result) {
		goto fail;
	}
	result = array_preallocate(zombies, numthreads+1);
	if (result) {
		goto fail;
	}
	
	result = array_preallocate(joinable, numthreads+1);
	if (result) {
	  goto fail;
	}
	
	result = array_preallocate(curthread->children, array_getnum(curthread->children)+1);
	if (result) {
	  goto fail;
	}
	
	/* Do the same for the scheduler. */
	result = scheduler_preallocate(numthreads+1);
	if (result) {
		goto fail;
	}

	/* Make the new thread runnable */
	result = make_runnable(newguy);
	if (result != 0) {
		goto fail;
	}

	/*
	 * Increment the thread counter. This must be done atomically
	 * with the preallocate calls; otherwise the count can be
	 * temporarily too low, which would obviate its reason for
	 * existence.
	 */
	numthreads++;

	/* Done with stuff that needs to be atomic */
	splx(s);

	/*
	 * Return new thread structure if it's wanted.  Note that
	 * using the thread structure from the parent thread should be
	 * done only with caution, because in general the child thread
	 * might exit at any time.
	 */
	
	/*
	* If the parent wants the child's pid, return the pid of the child and add it to the children array.
	* Otherwise immediately detach the thread.
	*/
	if (ret != NULL) {
/*		*ret = newguy;*/
/*    kprintf("parent cares about me");*/
/*    kprintf("t_exit == %d\n is_detached == %d\n has_exited == %d\n", *(curthread->t_pid), *(curthread->parent), *(curthread->t_exit), *(curthread->is_detached), *(curthread->has_exited));*/
    pid_t c_pid = *(curthread->t_pid);
    pid = c_pid*2;
    int i, next_child = 0;
    for(i=0; i<array_getnum(curthread->children); i++) {
      struct thread *t = array_getguy(curthread->children, i);
      if (t == NULL) {
        next_child = i;
        break;
      }
    }
    assert(newguy->is_detached != NULL);
    *(newguy->is_detached) = 0;
    assert(newguy->has_exited != NULL);
    *(newguy->has_exited) = 0;
    assert(newguy->t_exit != NULL);
    *(newguy->t_exit) = 0;
    pid = pid + next_child;
    assert(newguy->t_pid != NULL);
    *(newguy->t_pid) = pid;
    assert(newguy->parent != NULL);
    *(newguy->parent) = *(curthread->t_pid);
    array_add(curthread->children, newguy);
		*ret = &pid;
	}
	else {
/*	  kprintf("parent hates me");*/
	  *(newguy->is_detached) = 0;
	  *(newguy->has_exited) = 0;
	}

	return 0;

 fail:
	splx(s);
	if (newguy->t_cwd != NULL) {
		VOP_DECREF(newguy->t_cwd);
	}
	kfree(newguy->t_stack);
	kfree(newguy->t_name);
	kfree(newguy->t_pid);
	kfree(newguy->parent);
	kfree(newguy->is_detached);
	kfree(newguy->has_exited);
	kfree(newguy->t_exit);
/*	newguy->is_detached = NULL;*/
/*	newguy->has_exited = NULL;*/
/*	newguy->t_exit = NULL;*/
	kfree(newguy->is_detached);
	kfree(newguy->has_exited);
	array_destroy(newguy->children);
/*	kfree(newguy->children);*/
	kfree(newguy->t_exit);
	kfree(newguy);

	return result;
}

/*
 * High level, machine-independent context switch code.
 */
static
void
mi_switch(threadstate_t nextstate)
{
	struct thread *cur, *next;
	int result;
	
	/* Interrupts should already be off. */
	assert(curspl>0);

	if (curthread != NULL && curthread->t_stack != NULL) {
		/*
		 * Check the magic number we put on the bottom end of
		 * the stack in thread_fork. If these assertions go
		 * off, it most likely means you overflowed your stack
		 * at some point, which can cause all kinds of
		 * mysterious other things to happen.
		 */
		assert(curthread->t_stack[0] == (char)0xae);
		assert(curthread->t_stack[1] == (char)0x11);
		assert(curthread->t_stack[2] == (char)0xda);
		assert(curthread->t_stack[3] == (char)0x33);
	}
	
	/* 
	 * We set curthread to NULL while the scheduler is running, to
	 * make sure we don't call it recursively (this could happen
	 * otherwise, if we get a timer interrupt in the idle loop.)
	 */
	if (curthread == NULL) {
		return;
	}
	cur = curthread;
	curthread = NULL;

	/*
	 * Stash the current thread on whatever list it's supposed to go on.
	 * Because we preallocate during thread_fork, this should not fail.
	 */

	if (nextstate==S_READY) {
		result = make_runnable(cur);
	}
	else if (nextstate==S_SLEEP) {
		/*
		 * Because we preallocate sleepers[] during thread_fork,
		 * this should never fail.
		 */
		result = array_add(sleepers, cur);
	}
	else if (nextstate == S_JOIN) {
	  result = array_add(joinable, cur);
	}
	else {
		assert(nextstate==S_ZOMB);
		result = array_add(zombies, cur);
	}
	assert(result==0);

	/*
	 * Call the scheduler (must come *after* the array_adds)
	 */

	next = scheduler();

	/* update curthread */
	curthread = next;
	
	/* 
	 * Call the machine-dependent code that actually does the
	 * context switch.
	 */
	md_switch(&cur->t_pcb, &next->t_pcb);
	
	/*
	 * If we switch to a new thread, we don't come here, so anything
	 * done here must be in mi_threadstart() as well, or be skippable,
	 * or not apply to new threads.
	 *
	 * exorcise is skippable; as_activate is done in mi_threadstart.
	 */

	exorcise();

	if (curthread->t_vmspace) {
		as_activate(curthread->t_vmspace);
	}
}

/*
 * Cause the current thread to exit.
 *
 * We clean up the parts of the thread structure we don't actually
 * need to run right away. The rest has to wait until thread_destroy
 * gets called from exorcise().
 */
void
thread_exit(void)
{
	if (curthread->t_stack != NULL) {
		/*
		 * Check the magic number we put on the bottom end of
		 * the stack in thread_fork. If these assertions go
		 * off, it most likely means you overflowed your stack
		 * at some point, which can cause all kinds of
		 * mysterious other things to happen.
		 */
		assert(curthread->t_stack[0] == (char)0xae);
		assert(curthread->t_stack[1] == (char)0x11);
		assert(curthread->t_stack[2] == (char)0xda);
		assert(curthread->t_stack[3] == (char)0x33);
	}

	splhigh();

	if (curthread->t_vmspace) {
		/*
		 * Do this carefully to avoid race condition with
		 * context switch code.
		 */
		struct addrspace *as = curthread->t_vmspace;
		curthread->t_vmspace = NULL;
		as_destroy(as);
	}

	if (curthread->t_cwd) {
		VOP_DECREF(curthread->t_cwd);
		curthread->t_cwd = NULL;
	}

	assert(numthreads>0);
	numthreads--;
	int i;
	for (i=0; i<array_getnum(curthread->children); i++) {
	  struct thread *t = array_getguy(curthread->children, i);
	  pid_t pid = *(t->t_pid);
	  thread_detach(pid);
	}
	//  When a joinable thread terminates, its thread descriptor (i.e. thread id) and exit status must be retained until another thread performs thread_join or thread_detach on it.
	if (*(curthread->is_detached) == 0) {
	  mi_switch(S_JOIN);
	}
	*(curthread->has_exited) = 1;
	mi_switch(S_ZOMB);

	panic("Thread came back from the dead!\n");
}

/*
 * Yield the cpu to another process, but stay runnable.
 */
void
thread_yield(void)
{
	int spl = splhigh();

	/* Check sleepers just in case we get here after shutdown */
	assert(sleepers != NULL);

	mi_switch(S_READY);
	splx(spl);
}

/*
 * Yield the cpu to another process, and go to sleep, on "sleep
 * address" ADDR. Subsequent calls to thread_wakeup with the same
 * value of ADDR will make the thread runnable again. The address is
 * not interpreted. Typically it's the address of a synchronization
 * primitive or data structure.
 *
 * Note that (1) interrupts must be off (if they aren't, you can
 * end up sleeping forever), and (2) you cannot sleep in an 
 * interrupt handler.
 */
void
thread_sleep(const void *addr)
{
	// may not sleep in an interrupt handler
	assert(in_interrupt==0);
	
	curthread->t_sleepaddr = addr;
	mi_switch(S_SLEEP);
	curthread->t_sleepaddr = NULL;
}

/*
 * Wake up one or more threads who are sleeping on "sleep address"
 * ADDR.
 */
void
thread_wakeup(const void *addr)
{
	int i, result;
	
	// meant to be called with interrupts off
	assert(curspl>0);
	
	// This is inefficient. Feel free to improve it.
	
	for (i=0; i<array_getnum(sleepers); i++) {
		struct thread *t = array_getguy(sleepers, i);
		if (t->t_sleepaddr == addr) {
			
			// Remove from list
			array_remove(sleepers, i);
			
			// must look at the same sleepers[i] again
			i--;

			/*
			 * Because we preallocate during thread_fork,
			 * this should never fail.
			 */
			result = make_runnable(t);
			assert(result==0);
		}
	}
}

/*
 * Wake the first thread in sleepers array waiting on the addr
 */
void
thread_wakeone(const void *addr)
{
  int i, result;
	
	// meant to be called with interrupts off
	assert(curspl>0);
	
	struct thread *t = array_getguy(sleepers, i);
	if (t->t_sleepaddr == addr) {
	  array_remove(sleepers, 0);
	  result = make_runnable(t);
	  assert(result == 0);
	}
}

/*
 * Return nonzero if there are any threads who are sleeping on "sleep address"
 * ADDR. This is meant to be used only for diagnostic purposes.
 */
int
thread_hassleepers(const void *addr)
{
	int i;
	
	// meant to be called with interrupts off
	assert(curspl>0);
	
	for (i=0; i<array_getnum(sleepers); i++) {
		struct thread *t = array_getguy(sleepers, i);
		if (t->t_sleepaddr == addr) {
			return 1;
		}
	}
	return 0;
}

/*
 * New threads actually come through here on the way to the function
 * they're supposed to start in. This is so when that function exits,
 * thread_exit() can be called automatically.
 */
void
mi_threadstart(void *data1, unsigned long data2, 
	       void (*func)(void *, unsigned long))
{
	/* If we have an address space, activate it */
	if (curthread->t_vmspace) {
		as_activate(curthread->t_vmspace);
	}

	/* Enable interrupts */
	spl0();

#if OPT_SYNCHPROBS
	/* Yield a random number of times to get a good mix of threads */
	{
		int i, n;
		n = random()%161 + random()%161;
		for (i=0; i<n; i++) {
			thread_yield();
		}
	}
#endif
	
	/* Call the function */
	func(data1, data2);

	/* Done. */
	thread_exit();
}

int is_thread_running()
{
	int s;
	int n;

	s = splhigh();
	n = numthreads;
	splx(s);
	return(n!=1);
}

//
//
//
//
// STUFF BELOW FOR PART C OF PROJECT 5/6
//
//
//
//

int thread_join(pid_t pid, int *status)
{
  struct thread *t;
  struct array *curthread_children;
  int is_parent = 0;
  int i, j, k = -1;
  int spl;
  //find the thread with t_pid == pid
  
  //return error if the curthread is the same as the thread trying to be joined
  if (*(curthread->t_pid) == pid)
    return EDEADLK;
  
  //check the curthreads children for t_pid == pid and parent == curthread->t_pid, return an error if the thread joining pid is not the parent
  curthread_children = curthread->children;
  for (i=0; i<array_getnum(curthread_children); i++) {
    struct thread *t1 = array_getguy(curthread_children, i);
    if (*(t1->t_pid) == pid && *(t1->parent) == *(curthread->t_pid)) {
      is_parent = 1;
      t = t1;
      j = i;
    }
  }
  if (is_parent == 0)
    return EINVAL;
    
  //check all other data structures for the thread (this shouldnt happen if curthread != parent)
  for (i=0; i<array_getnum(sleepers); i++) {
    struct thread *t1 = array_getguy(sleepers, i);
    if (*(t1->t_pid) == pid && t == NULL) {
      t = t1;
    }
  }
  for (i=0; i<array_getnum(zombies); i++) {
    struct thread *t1 = array_getguy(zombies, i);
    if (*(t1->t_pid) == pid && t == NULL) {
      t = t1;
    }
  }
  for (i=0; i<array_getnum(joinable); i++) {
    struct thread *t1 = array_getguy(joinable, i);
    if (*(t1->t_pid) == pid && t == NULL) {
      t = t1;
      k = i;
    }
  }
  //no thread found anywhere, we return an error
  if (t == NULL)
    return ESRCH;
  
  //return an error if the thread has been detached
  if (*(t->is_detached) == 1)
    return EINVAL;

  //make the joining thread runnable and suspend execution of curthread
  if (*(t->has_exited) == 0) {
    spl = splhigh();
    make_runnable(t);
/*    thread_yield();*/
    splx(spl);
  }
  else {
/*    spl=splhigh();*/
  }
  
  
  
  if (status != NULL)
    *(status) = *(t->t_exit);
/*    return 0;*/
    //status = &exit_status;
    
 /* Return: On success, the exit status of thread pid is stored in the location pointed to by status, and 0 is the return value.*/
  array_remove(curthread->children, j);
  if (k > 0)
    array_remove(joinable, k);
  return 0;
}
int thread_detach(pid_t pid)
{
  struct thread *t;
  struct array *curthread_children;
  int is_parent = 0;
  int i, j, k = -1;

  //check the curthreads children for t_pid == pid and parent == curthread->t_pid, return an error if the thread joining pid is not the parent
  curthread_children = curthread->children;
  for (i=0; i<array_getnum(curthread_children); i++) {
    struct thread *t1 = array_getguy(curthread_children, i);
    if (*(t1->t_pid) == pid && *(t1->parent) == *(curthread->t_pid)) {
      is_parent = 1;
      t = t1;
      j = i;
    }
  }
  if (is_parent == 0)
    return EINVAL;
    
  //check all other data structures for the thread (this shouldnt happen if curthread != parent)
  for (i=0; i<array_getnum(sleepers); i++) {
    struct thread *t1 = array_getguy(sleepers, i);
    if (*(t1->t_pid) == pid && t == NULL) {
      t = t1;
    }
  }
  for (i=0; i<array_getnum(zombies); i++) {
    struct thread *t1 = array_getguy(zombies, i);
    if (*(t1->t_pid) == pid && t == NULL) {
      t = t1;
    }
  }
  for (i=0; i<array_getnum(joinable); i++) {
    struct thread *t1 = array_getguy(joinable, i);
    if (*(t1->t_pid) == pid && t == NULL) {
      t = t1;
      k = i;
    }
  }
  //no thread found anywhere, we return an error
  if (t == NULL)
    return ESRCH;
  
  //return an error if the thread has been detached
  if (*(t->is_detached) == 1)
    return EINVAL;
  
  array_remove(curthread_children, j);
  if (k > 0)
    array_remove(joinable, k);
  if (*(t->has_exited) == 1) {
    //reclaim thread descriptor
  }
  else {
    //discard exit status and descriptor when pid terminates
  }
  //mark thread as detached
  *(t->is_detached) = 1;

  return 0;
}
