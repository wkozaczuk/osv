/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <lockfree/mutex.hh>
#include <osv/trace.hh>
#include <osv/sched.hh>
#include <osv/wait_record.hh>
#include <osv/wait_record.hh>
#include <osv/export.h>
#include <osv/preempt-lock.hh>
#include <osv/clock.hh>

extern bool smp_allocator;

namespace lockfree {

TRACEPOINT(trace_mutex_lock, "%p", mutex *);
TRACEPOINT(trace_mutex_lock_success, "%p, step=%d", mutex *, int);
TRACEPOINT(trace_mutex_lock_wait, "%p", mutex *);
TRACEPOINT(trace_mutex_lock_wake, "%p", mutex *);
TRACEPOINT(trace_mutex_try_lock, "%p, success=%d", mutex *, bool);
TRACEPOINT(trace_mutex_unlock, "%p count=%d", mutex *, int);
TRACEPOINT(trace_mutex_send_lock, "%p, wr=%p, flavor=%d", mutex *, wait_record *, int);
TRACEPOINT(trace_mutex_receive_lock, "%p", mutex *);
TRACEPOINT(trace_mutex_spun_times, "%p, success=%d, times=%d, fholder=%p, lholder=%p, count=%d, time=%ld", mutex *, bool, int, sched::thread*, sched::thread*, int, u64);

//With new change to stop spinning after failed handoff, the 20 seems to be sweet spot
constexpr unsigned int SPIN_MAX = 20; //20 Seems best, 10 is kind of on a line

void mutex::lock()
{
    trace_mutex_lock(this);

    sched::thread *current = sched::thread::current();

    preempt_lock.lock();
    auto _count = count.fetch_add(1, std::memory_order_acquire);
    if (_count == 0 ) {
    //if (count.fetch_add(1, std::memory_order_acquire) == 0) {
        // Uncontended case (no other thread is holding the lock, and no
        // concurrent lock() attempts). We got the lock.
        // Setting count=1 already got us the lock; we set owner and depth
        // just for implementing a recursive mutex.
        owner.store(current, std::memory_order_relaxed);
        depth = 1;
        trace_mutex_lock_success(this, 1);
        preempt_lock.unlock();
        return;
    }

    // If we're here the mutex was already locked, but we're implementing
    // a recursive mutex so it's possible the lock holder is us - in which
    // case we need to increment depth instead of waiting.
    if (owner.load(std::memory_order_relaxed) == current) {
        count.fetch_add(-1, std::memory_order_relaxed);
        ++depth;
        trace_mutex_lock_success(this, 2);
        preempt_lock.unlock();
        return;
    }

    // If we're still here, the lock is owned by a different thread. Before
    // going to sleep, let's try to spin for a bit, to avoid the context
    // switch costs when the critical section is very short. We're spinning
    // without putting ourselves in the waitqueue first, so when the lock
    // holder unlock()s, we expect to see a handoff.
    // FIXME: do not spin when there is just one CPU.
    // FIXME: change or parametrize the silly "100" number.
    // Waldek: number of spinning iterations could be passed as a constructor
    // parameter - the default could be 0 or a constant that can be changed 
    // as a kernel build parameter (the default constant value would be 0
    // - 'spinning off'). Hopefully default 0 would not change current behavior.
    // Also the constructor should always ignore the value if #cpus = 1 and set it to 0 
    // Also consider build parameter to enable/disable mutex spinning
    // We could change rwlock to construct mutex with the spin parameter > 0 (=10?)
    // Alternative would be to create a template
    //
    // If someone is on the waitqueue, there's no point in continuing to
    // spin, as it will be woken first, and no handoff will be done.
//#ifdef ALE
//The 1000 and my "holder" if seems to be improving 2 pinned to ~150, and 4 to 340
    //if (_count < 2) {
    {
    //bool reset_spinning = false;
    auto t = clock::get()->time();
    //SCOPE_LOCK(preempt_lock);
    int c = SPIN_MAX; //100 lowers apart cont mutex with 2 cores, 10 sometimes
    sched::thread* holder1 = nullptr;
    sched::thread* holder = nullptr;
    for (; c && waitqueue.empty(); c--) {
        // If the lock holder got preempted, we would better be served
        // by going to sleep and let it run again, than futile spinning.
        // Especially if the lock holder wants to run on this CPU.
        barrier(); // trying. didn't help
        holder = owner.load(std::memory_order_relaxed);
	if (c == SPIN_MAX) {
             holder1 = holder;
             /*bool not_spinning = false;
             if (!spinning.compare_exchange_strong(not_spinning, true)) {
                 break;
             }
             reset_spinning = true;*/
	     //if (!holder1) break;
	     //if (!holder1) c = 10; //Breaks whole spinning
	}
        // FIXME: what if thread exits while we want to check if it's
        // running? as soon as we find the owner above, it might already
        // be gone... Switch to using detached_thread as owner?
        // Waldek: the running() method is using _detached_state so are we OK?
        // How exactly _detached_state would help us in the case the thread is terminated
        //
        // Waldek: This probably should be : if (holder && !holder->running()) {
        // meaning if there is still a lock holder and got preempted
        // then do not spin. In the original version we would not attempt to spin
        // if there was NOT a lock holder which is exactly when we should spin
        if (holder && !holder->running()) {
        //if (!holder || !holder->running()) { //SEEMS to be wrong
            break;
        }
        // If the lock holder is on the same CPU as us, better go to sleep
        // and let it run than to futily spin
        //
        // Waldek: I think this is never possible - this code is running
        // on this cpu so there could not be an owner (if there is one)
        // on the same cpu that is also running
        /*if (holder && holder->tcpu() == sched::cpu::current()) {
            break;
        }*/
	//if (!holder) {
        auto old_handoff = handoff.load();
        //Could this be "compare_exchange_weak"? because we are looping
        //if (old_handoff && handoff.compare_exchange_strong(old_handoff, 0U)) {
        if (old_handoff) {
	if (handoff.compare_exchange_strong(old_handoff, 0U)) {
            owner.store(current, std::memory_order_relaxed);
            depth = 1;
            trace_mutex_spun_times(this, true, c, holder1, holder, _count, clock::get()->time() - t);
            //spinning.store(false, std::memory_order_relaxed);
            preempt_lock.unlock();
            return;
	} else break; //Other spinning won
        }
	//}
	//if (!holder && c > 5) c = 5;
#ifdef __x86_64__
        __asm __volatile("pause");
#endif
#ifdef __aarch64__
        __asm __volatile("isb sy");
#endif
    }
    //if (reset_spinning)
    //    spinning.store(false, std::memory_order_relaxed);
    trace_mutex_spun_times(this, false, SPIN_MAX - c, holder1, holder, _count, clock::get()->time() - t);
    }
//#endif
    preempt_lock.unlock();

    // If we're here still here the lock is owned by a different thread.
    // Put this thread in a waiting queue, so it will eventually be woken
    // when another thread releases the lock.
    // Note "waiter" is on the stack, so we must not return before making sure
    // it was popped from waitqueue (by another thread or by us.)
    wait_record waiter(current);
    waitqueue.push(&waiter);

    // The "Responsibility Hand-Off" protocol where a lock() picks from
    // a concurrent unlock() the responsibility of waking somebody up:
    auto old_handoff = handoff.load();
    if (old_handoff) {
         if (!waitqueue.empty()){
            if (handoff.compare_exchange_strong(old_handoff, 0U)) {
                // Note the explanation above about no concurrent pop()s also
                // explains why we can be sure waitqueue is still not empty.
                wait_record *other = waitqueue.pop();
                assert(other);
                if (other->thread() != current) {
                    // At this point, waiter.thread() must be != 0, otherwise
                    // it means someone has already woken us up, breaking the
                    // handoff protocol which decided we should be the ones to
                    // wake somebody up. Note that right after other->wake()
                    // below, waiter.thread() may become 0: the thread we woke
                    // can call unlock() and decide to wake us up.
                    assert(waiter.thread());
                    other->wake();
                } else {
                    // got the lock ourselves
                    assert(other == &waiter);
                    owner.store(current, std::memory_order_relaxed);
                    depth = 1;
                    trace_mutex_lock_success(this, 3);
                    return;
                }
            }
        }
    }

    for (int c = 2 * SPIN_MAX; c; c--) {
        if (waiter.woken()) {
            owner.store(current, std::memory_order_relaxed);
            depth = 1;
            return;
        }
        barrier(); // trying. didn't help
        sched::thread* holder = owner.load(std::memory_order_relaxed);
        if (holder && !holder->running()) {
            break;
        }
        asm volatile ("pause");
    }

    // Wait until another thread pops us from the wait queue and wakes us up.
    trace_mutex_lock_wait(this);
    waiter.wait();
    trace_mutex_lock_wake(this);
    owner.store(current, std::memory_order_relaxed);
    depth = 1;
}

// send_lock() is used for implementing a "wait morphing" technique, where
// the wait_record of a thread currently waiting on a condvar is put to wait
// on a mutex, without waking it up first. This avoids unnecessary context
// switches that happen if the thread is woken up just to try and grab the
// mutex and go to sleep again.
//
// send_lock(wait_record) is similar to lock(), but instead of taking the lock
// for the current thread, it takes the lock for another thread which is
// currently waiting on the given wait_record. This wait_record will be woken
// when the lock becomes available - either during the send_lock() call, or
// sometime later when the lock holder unlock()s it. It is assumed that the
// waiting thread DOES NOT hold the mutex at the time of this call, so the
// thread should relinquish the lock before putting itself on wait_record.
void mutex::send_lock(wait_record *wr)
{
    trace_mutex_send_lock(this, wr, 1);
    if (count.fetch_add(1, std::memory_order_acquire) == 0) {
        // Uncontended case (no other thread is holding the lock, and no
        // concurrent lock() attempts). We got the lock for wr, so wake it.
	//morphing = true;
        wr->wake();
        return;
    }

    // If we can't grab the lock for wr now, we push it in the wait queue,
    // so it will eventually be woken when another thread releases the lock.
    waitqueue.push(wr);

    // Like in lock(), we incremented "count" before pushing anything on to
    // the queue so a concurrent unlock() may not have found anybody to wake.
    // So we must also do the "Responsibility Hand-Off" protocol to help the
    // concurrent unlock() return without waiting for us to push.
    auto old_handoff = handoff.load();
    if (old_handoff) {
        if (!waitqueue.empty()){
            if (handoff.compare_exchange_strong(old_handoff, 0U)) {
                wait_record *other = waitqueue.pop();
                assert(other);
	        //morphing = true;
                other->wake();
            }
        }
    }
}

// A variant of send_lock() with the following differences:
//  - the mutex must be held by the caller
//  - we do nothing if the thread we're sending the lock to is already
//    waiting (and return false)
bool mutex::send_lock_unless_already_waiting(wait_record *wr)
{
    trace_mutex_send_lock(this, wr, 2);
    assert(owned());
    // count could not have been zero, so no need to test for it
    count.fetch_add(1, std::memory_order_acquire);

    for (auto& x : waitqueue) {
        if (wr->thread() == x.thread()) {
            count.fetch_add(-1, std::memory_order_relaxed);
            return false;
        }
    }
    // Queue the wait record, to be woken by an eventual unlock().
    waitqueue.push(wr);

    // No need for the Responsibility Hand-Off protocol, since we're holding
    // the lock - no concurrent unlock can be happening.

    //morphing = true;
    return true;
}



// A thread waking up knowing it received a lock from send_lock() as part of
// a "wait morphing" protocol, must call receive_lock() to complete the lock's
// adoption by this thread.
// TODO: It is unfortunate that receive_lock() needs to exist at all. If we
// changed the code so that an unlock() always sets owner and depth for the
// thread it wakes, we wouldn't need this function.
void mutex::receive_lock()
{
    trace_mutex_receive_lock(this);
    owner.store(sched::thread::current(), std::memory_order_relaxed);
    depth = 1;
}

bool mutex::try_lock()
{
    sched::thread *current = sched::thread::current();
    int zero = 0;
    if (count.compare_exchange_strong(zero, 1, std::memory_order_acquire)) {
        // Uncontended case. We got the lock.
        owner.store(current, std::memory_order_relaxed);
        depth = 1;
        trace_mutex_try_lock(this, true);
        return true;
    }

    // We're implementing a recursive mutex -lock may still succeed if
    // this thread is the one holding it.
    if (owner.load(std::memory_order_relaxed) == current) {
        ++depth;
        trace_mutex_try_lock(this, true);
        return true;
    }

    // The lock is taken, and we're almost ready to give up (return
    // false), but the last chance is if we can accept a handoff - and if
    // we do, we got the lock.
    auto old_handoff = handoff.load();
    if(old_handoff && handoff.compare_exchange_strong(old_handoff, 0U)) {
        count.fetch_add(1, std::memory_order_relaxed);
        owner.store(current, std::memory_order_relaxed);
        depth = 1;
        trace_mutex_try_lock(this, true);
        return true;
    }

    trace_mutex_try_lock(this, false);
    return false;
}

void mutex::unlock()
{
    //trace_mutex_unlock(this);

    // We assume unlock() is only ever called when this thread is holding
    // the lock. The following assertions don't seem to add any measurable
    // performance penalty, so we leave them in.
    assert(owner.load(std::memory_order_relaxed) == sched::thread::current());
    assert(depth!=0);
    if (--depth)
        return; // recursive mutex still locked.

    // Disable preemption before we set owner=null, so that we don't context
    // switch to a different thread who might end up spinning on this mutex.
    // Alternatives to this change include remembering the lock holder's cpu
    // (makes the mutex larger...), stopping spinning when !holder (makes
    // "apart" ctxsw benchmark slower).
    SCOPE_LOCK(preempt_lock);

    // We can't return with owner==current, otherwise a later lock() in the
    // same thread will think it's a recursive lock, while actually another
    // thread is in the middle of acquiring the lock and has set count>0.
    // So we need to zero owner, and we must do it *now*, before decrementing
    // count (as soon as count goes down to 0, we released the lock and some
    // other thread might alread set its own owner).
    owner.store(nullptr, std::memory_order_relaxed);

    // If there is no waiting lock(), we're done. This is the easy case :-)
    int _count = count.fetch_add(-1, std::memory_order_release);
    trace_mutex_unlock(this, _count);
    if (_count == 1) {
	//morphing = false;
        return;
    }

    // Otherwise there is at least one concurrent lock(). Awaken one if
    // it's waiting on the waitqueue, otherwise use the RHO protocol to
    // have the lock() responsible for waking someone up.
    // TODO: it's not completely clear to me why more than two
    // iterations can ever be needed. If the loop continues it means
    // another lock() queued itself - but if it did, wouldn't the next
    // iteration just pop and return?
    while(true) {
        wait_record *other = waitqueue.pop();
        if (other) {
            assert(other->thread() != sched::thread::current()); // this thread isn't waiting, we know that :(
	    //morphing = false;
            other->wake();
            return;
        }
        // Some concurrent lock() is in progress (we know this because of
        // count) but it hasn't yet put itself on the wait queue.
        if (++sequence == 0U) ++sequence;  // pick a number, but not 0
        auto ourhandoff = sequence;
        handoff.store(ourhandoff);
        // If the queue is empty, the concurrent lock() is before adding
        // itself, and therefore will definitely find our handoff later.
        if (waitqueue.empty()) {
	    //morphing = false;
            return;
	}
        // A thread already appeared on the queue, let's try to take the
        // handoff ourselves and awaken it. If somebody else already took
        // the handoff, great, we're done - they are responsible now.
        if (!handoff.compare_exchange_strong(ourhandoff, 0U)) {
	    //morphing = false;
            return;
	}
    }
}

bool mutex::owned() const
{
    return owner.load(std::memory_order_relaxed) == sched::thread::current();
}
/*
sched::thread *mutex::get_owner()
{
    return owner.load(std::memory_order_relaxed);
}*/
}

// For use in C, which can't access namespaces or methods. Note that for
// performance, we use "flatten" which basically creates a copy of each
// function.
// TODO: We can avoid this copy by make these C functions the only copy of the
// functions, and make the methods inline, calling these functions with
// "this" as a parameter. But this would be un-C++-like :(
extern "C" {
OSV_LIBSOLARIS_API __attribute__((flatten)) void lockfree_mutex_lock(void *m) {
    static_cast<lockfree::mutex *>(m)->lock();
}
OSV_LIBSOLARIS_API __attribute__ ((flatten)) void lockfree_mutex_unlock(void *m) {
    static_cast<lockfree::mutex *>(m)->unlock();
}
OSV_LIBSOLARIS_API __attribute__ ((flatten)) bool lockfree_mutex_try_lock(void *m) {
    return static_cast<lockfree::mutex *>(m)->try_lock();
}
OSV_LIBSOLARIS_API __attribute__ ((flatten)) bool lockfree_mutex_owned(void *m) {
    return static_cast<lockfree::mutex *>(m)->owned();
}
}
