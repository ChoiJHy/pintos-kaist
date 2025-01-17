/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {

		//==================================================================
		//				Project 1 - Priority Scheduling
		//------------------------------------------------------------------
		/* 	lock을 기다리면서 대기하고 있는 스레드가 여러개인 경우에 lock을 획득할 수 있어졌을 때 
			Priority가 가장 높은 스레드가 먼저 깨어나야 한다. 
			waiters에 스레드가 삽입 될 때 Priority 기준으로 정렬이 되어 삽입되도록 코드를 수정
			semaphore에 추가되는 elem들은 스레드이기 때문에 스레드에서 사용했던 
			CompareThreadByPriority 함수를 그대로 사용 가능하다.*/
		list_insert_ordered(&sema->waiters, &thread_current()->elem, CompareThreadByPriority, NULL);
		//list_push_back (&sema->waiters, &thread_current ()->elem);
		//==================================================================

		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();

	if (!list_empty (&sema->waiters))
	{
		//==================================================================
		//				Project 1 - Priority Scheduling
		//------------------------------------------------------------------
		/* 	waiters 리스트에 있던 동안에 우선순위에 변경이 생겼을 가능성(donation에 의해)을 생각해서
			waiters 리스트를 다시 내림차순으로 정렬해준다. */
		list_sort(&sema->waiters, CompareThreadByPriority, NULL);
		//==================================================================
		thread_unblock (list_entry (list_pop_front (&sema->waiters),
					struct thread, elem));
	}
	sema->value++;
	//==================================================================
	//				Project 1 - Priority Scheduling
	//------------------------------------------------------------------
	/* 	위에서 unblock된 스레드가 현재 running 스레드보다 우선순위가 높을 수 있기 때문에 
		우선순위를 기준으로 CPU선점이 일어나도록 해준다.*/
	ThreadYieldByPriority();
	//==================================================================
	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	//==================================================================
	//				Project 1 - Priority Donation
	//------------------------------------------------------------------
	/*	lock을 요청했을 때 이미 lock을 점유하고 있는 스레드(holder)가 있다면 holder에게 priority를 기부한다.
		= holder의 donations 리스트에 현재 스레드의 donation_elem을 이용하여 들어간다.
		이 때 priority를 기준으로 정렬이 되도록 들어간다.*/
		struct thread* cur_thread = thread_current();
		if(NULL != lock->holder)
		{
			cur_thread->wait_on_lock = lock; // 현재 스레드가 대기하는 lock으로 지정
			// holder의 donations리스트에 priority를 기준으로 정렬되어 들어간다.
			list_insert_ordered(&lock->holder->donations, &cur_thread->donation_elem, CompareDonationsByPriority, NULL);

			//==================================================================
			//				Project 1 - mlfqs
			//------------------------------------------------------------------
			/* mlfqs 에서는 priority를 직접 수정하지 않는다.*/
			if(!thread_mlfqs)
				DonatePriority(); // 현재 스레드의 priority를 holder에게 기부
			//==================================================================
		}
	//==================================================================

	sema_down (&lock->semaphore); // lock을 점유

	//==================================================================
	//				Project 1 - Priority Donation
	//------------------------------------------------------------------
	cur_thread->wait_on_lock = NULL; // lock을 점유했으니 대기하는 lock은 없음
	//==================================================================

	lock->holder = thread_current ();
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	//==================================================================
	//				Project 1 - Priority Donation
	//------------------------------------------------------------------
	/* 	현재 스레드가 lock을 반환할 때 , 이 lock을 요청한 스레드가 있었고 donation을 받았다면
		원래의 priority로 돌아가야 한다. 
		lock을 반환하기 때문에 기부자 목록에 기부를 해준 스레드가 들어있을 필요가 없다.
		donations에서 나에게 기부를 한 스레드를 제거해준다.*/

	//==================================================================
	//				Project 1 - mlfqs
	//------------------------------------------------------------------
	/* mlfqs를 사용할때는 donation관련 기능은 꺼야한다.*/
	if(!thread_mlfqs)
	{
		struct thread* cur_thread = thread_current();

		for(struct list_elem* iter = list_begin(&cur_thread->donations); iter != list_end(&cur_thread->donations);)
		{
			struct thread* donor = list_entry(iter, struct thread, donation_elem);
			if(lock == donor->wait_on_lock)
				iter = list_remove(iter);
			else
				iter = list_next(iter);
		}

		ThreadUpdatePriorityFromDonations();
	}
	//==================================================================

	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);

	//==================================================================
	//				Project 1 - Priority Scheduling
	//------------------------------------------------------------------
	/* cond의 waiters 리스트에 들어있는 세마포어도 우선순위를 기반으로 정렬되게 삽입한다.*/
	list_insert_ordered(&cond->waiters, &waiter.elem, CompareSemaphoreByPriority, NULL);
	//list_push_back (&cond->waiters, &waiter.elem);
	//==================================================================

	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters))
	{
		//==================================================================
		//				Project 1 - Priority Scheduling
		//------------------------------------------------------------------
		/* 깨우기 전에 wait 도중에 Priority가 바뀌었을 수 있으니 다시 정렬 한다. */
		list_sort(&cond->waiters, CompareSemaphoreByPriority, NULL);
		//==================================================================
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
	}
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}


//==================================================================
//				Project 1 - Priority Scheduling
//------------------------------------------------------------------
/* 	condition variables를 사용할 때 우선순위를 기준으로 정렬하기 위한 함수
	thread.h에 구현했던 CompareThreadByPriority를 사용할수 없는 이유는 
	condition variables를 사용하는 함수에서 cond의 waiters 리스트에 넣는 요소가 
	스레드가 아닌 semaphore_elem이기 때문이다. 
	이 때 각 세마포어의 waiters는 스레드의 우선순위를 기반으로 이미 정렬되어있는 상태기 때문에 
	세마포어의 waiters의 가장 앞에 있는 스레드의 우선순위만 비교해주면 된다.*/
bool CompareSemaphoreByPriority(const struct list_elem* l, const struct list_elem* r, void* aux UNUSED)
{
	struct semaphore_elem* sema_l = list_entry(l, struct semaphore_elem, elem);
	struct semaphore_elem* sema_r = list_entry(r, struct semaphore_elem, elem);

	struct list* waiters_l = &(sema_l->semaphore.waiters);
	struct list* waiters_r = &(sema_r->semaphore.waiters);

	struct thread* thread_l = list_entry(list_begin(waiters_l), struct thread, elem);
	struct thread* thread_r = list_entry(list_begin(waiters_r), struct thread, elem);

	return thread_l->priority > thread_r->priority;
}

//==================================================================


//==================================================================
//				Project 1 - Priority Donatnion
//------------------------------------------------------------------
/* Donation_elem을 holder의 donations에 넣을 때 priority를 기준으로 정렬하기 위한 함수 */
bool CompareDonationsByPriority(const struct list_elem* l, const struct list_elem* r, void* aux UNUSED)
{
	return list_entry(l, struct thread, donation_elem)->priority > list_entry(r, struct thread, donation_elem)->priority; 
}

//==================================================================