/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <mutex>
#include <osv/sched.hh>
#include <osv/clock.hh>
#include <osv/rwlock.h>
#include <osv/export.h>

using namespace sched;

rwlock::rwlock()
    : _readers(0)
{}

rwlock::~rwlock()
{
    assert(_readers == 0);
    assert(_read_waiters.empty());
}

static constexpr unsigned LOCK_INDICATOR  = 0x80000000;
static constexpr unsigned WRITE_INDICATOR = 0x40000000;
static constexpr unsigned READER_MASK     = 0x3fffffff;

static constexpr unsigned RETRY_THRESHOLD = 0; //Do not retry if single CPU

//Possibly DONE
bool rwlock::try_rlock() {
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    if (_readers < WRITE_INDICATOR) {
        uint32_t prev_readers = readers->fetch_add(1);
        if (prev_readers < LOCK_INDICATOR) {
            return true;
        }
        readers->fetch_add(-1);
    }
    return false;
}

using namespace osv::clock::literals;

//Possibly DONE except for the problematic scenario explained before wait_until()
void rwlock::rlock() {
    unsigned retry = 0;
    while (true) {
        if (try_rlock())
            return;

        if (retry++ > RETRY_THRESHOLD) {
            retry = 0;
            //
            //Wait up until try_rlock() succeeds
            read_waiter waiter(thread::current());
            //
            //Here is a scenario: we are the only reader to try and there is
            //an active writer (that is why try_rlock() ABOVE failed) and we try again
            //and fail in the wait_until() below. But then right after the writer calls
            //wunlock(), removes the LOCK and WRITE indicator, and tries to pop
            //any items from the _read_waiters which is empty, BEFORE we have a chance
            //to push ours before going to sleep - who will wake us up?
            bool rlocked = false;
            auto now = osv::clock::uptime::now();
            sched::timer tmr(*thread::current());
            tmr.set(now + 1_ms);
            thread::wait_until( [this, &waiter, &rlocked, &tmr] { 
                if (this->try_rlock()) {
                    rlocked = true;
                    return true;
                } else if (tmr.expired()) {
                    return true;
                } else {
                    //Because timer did not expire, it is either 1st time
                    //this executes or we were woken by a writer after sleeping
                    waiter.woken = false;
                    this->_read_waiters.push(&waiter);
                    return false;
                }
            });
            //Cancel timer if not expired
            if (!tmr.expired()) {
                tmr.cancel();
            }
            //Remove all read waiters including us (maybe in vain) if we were not woken by a writer
            if (!waiter.woken) {
                while (true) {
                    read_waiter *owaiter = _read_waiters.pop();
                    if (!owaiter) {
                        break;
                    }
                    if (owaiter != &waiter) {
                        owaiter->woken = true;
                        owaiter->t->wake();
                    }
                }
            }
            //We may have gotten woken up and running after failing try_rlock()
            //and before going to sleep() due to a race with wunlock()
            if (rlocked) {
                 return;
            }
        }
    }
}

//Possibly DONE
void rwlock::runlock() {
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);

    unsigned prev_readers = readers->fetch_add(-1);

    assert(prev_readers > 0);
    assert(prev_readers < LOCK_INDICATOR);

    //Wake potential writer if any (no other active writer) if we are the last one
    if ((prev_readers & READER_MASK) == 1 && (prev_readers & WRITE_INDICATOR) && (prev_readers & LOCK_INDICATOR) == 0) {
        //Wake the _wmtx owner - pending writer - if not null
        auto pending_writer = _wmtx.get_owner();
        if (pending_writer) {
            pending_writer->wake();
        }
    }
}

//Possibly DONE
bool rwlock::try_upgrade() {
    if (!_wmtx.try_lock()) {
        return false;
    }
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    unsigned prev_readers = readers->load();
    if (prev_readers == 1) { // LOCK_INDICATOR and WRITE_INDICATOR are off 
        if (readers->compare_exchange_weak(prev_readers, (WRITE_INDICATOR | LOCK_INDICATOR))) {
            // we've won the race
            return true;
        }
    }
    //We either were not the only reader or have lost the race with a new reader or writer
    //(is writer a possibility after changing wlock()?)
    _wmtx.unlock();
    return false;
}

//Possibly DONE
bool rwlock::internal_try_wlock(std::atomic<unsigned> *readers) {
    uint32_t prev_readers = readers->load();
    if ((prev_readers & READER_MASK) == 0 && (prev_readers & LOCK_INDICATOR) == 0) {
        if (readers->compare_exchange_weak(prev_readers, WRITE_INDICATOR | LOCK_INDICATOR)) {
            // we've won the race
            return true;
        }
    } else if (prev_readers & LOCK_INDICATOR && _wmtx.owned()) {
        return true;
    }
    return false;
}

//Possibly DONE
bool rwlock::try_wlock() {
    if (!_wmtx.try_lock()) {
        return false;
    }
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    if (internal_try_wlock(readers)) {
        return true; 
    }
    //We have lost the race with a new reader or writer
    //(is writer a possibility after changing wlock()?)
    _wmtx.unlock();
    return false;
}

//Possibly DONE
void rwlock::wlock() {
    //Lock the writer mutex which may obviusly go sleep
    _wmtx.lock();
    //At this point we are still a potential writer and the one that is the 1st from all the
    //pending ones if any
    
    //Lets set the write indicator in order to phase out the current readers and block new ones
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    readers->fetch_or(WRITE_INDICATOR);
    
    //Now retry to set lock indicator in a loop 
    unsigned retry = 0;
    while (true) {
        if (internal_try_wlock(readers)) {
            return;
        }

        if (retry++ > RETRY_THRESHOLD) {
            retry = 0;
            //Go to sleep
            //TODO: May need to add timer as well like with rlock to cover rare case
            //when we never get woken up by last reader (runlock) - is it possible?
            thread::wait_until( [this, readers] { return this->internal_try_wlock(readers); });
            return; 
        }
    }
}

//Possibly DONE
void rwlock::wunlock() {
    assert(_readers & (LOCK_INDICATOR | WRITE_INDICATOR));

    //If we are recursed then simply unlock and return
    if (_wmtx.getdepth() > 1) {
        return _wmtx.unlock();
    }

    //Allow waiting readers to acquire a lock before new writer comes in
    //or 1st in line wakes from the the sleep
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    readers->fetch_and(~(LOCK_INDICATOR | WRITE_INDICATOR));

    //Wake the waiting readers which many should be able to acquire a lock
    //TODO: To minimize sending 100s of IPI wakeups to other CPUs we may
    //use a new thread::wake_many() method that would do similar logic
    //wake_impl() does for one - set status to waking, but only set need_reschedule
    //once and send IPI wake up once for each relevant target CPU 
    while (true) {
        read_waiter *waiter = _read_waiters.pop();
        if (!waiter) {
            break;
        }
        waiter->woken = true;
        waiter->t->wake();
    }

    _wmtx.unlock();
}

//Possibly DONE
void rwlock::downgrade()
{
    assert(_readers & (LOCK_INDICATOR | WRITE_INDICATOR));

    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);

    //Remove LOCK and WRITE indicators but increment readers
    while(true) {
        uint32_t prev_readers = readers->load();
        uint32_t next_readers = (prev_readers & ~(LOCK_INDICATOR | WRITE_INDICATOR)) + 1;
        if (readers->compare_exchange_weak(prev_readers, next_readers)) {
            break;
        }
    }
    //Wake the waiting readers
    while (true) {
        read_waiter *waiter = _read_waiters.pop();
        if (!waiter) {
            break;
        }
        waiter->woken = true;
        waiter->t->wake();
    }

    //Unlock all the way down
    for (int depth = _wmtx.getdepth(); depth > 0; depth--) {
        _wmtx.unlock();
    }
}

bool rwlock::wowned()
{
    return _wmtx.owned();
}

bool rwlock::has_readers()
{
    return _readers;
}

OSV_LIBSOLARIS_API
void rwlock_init(rwlock_t* rw)
{
    new (rw) rwlock;
}

OSV_LIBSOLARIS_API
void rwlock_destroy(rwlock_t* rw)
{
    rw->~rwlock();
}

OSV_LIBSOLARIS_API
void rw_rlock(rwlock_t* rw)
{
    rw->rlock();
}

OSV_LIBSOLARIS_API
void rw_wlock(rwlock_t* rw)
{
    rw->wlock();
}

OSV_LIBSOLARIS_API
int rw_try_rlock(rwlock_t* rw)
{
    return rw->try_rlock();
}

OSV_LIBSOLARIS_API
int rw_try_wlock(rwlock_t* rw)
{
    return rw->try_wlock();
}

OSV_LIBSOLARIS_API
void rw_runlock(rwlock_t* rw)
{
    rw->runlock();
}

OSV_LIBSOLARIS_API
void rw_wunlock(rwlock_t* rw)
{
    rw->wunlock();
}

OSV_LIBSOLARIS_API
int rw_try_upgrade(rwlock_t* rw)
{
    return rw->try_upgrade();
}

OSV_LIBSOLARIS_API
void rw_downgrade(rwlock_t* rw)
{
    rw->downgrade();
}

OSV_LIBSOLARIS_API
int rw_wowned(rwlock_t* rw)
{
    return rw->wowned();
}

int rw_has_readers(rwlock_t* rw)
{
    return rw->has_readers();
}
