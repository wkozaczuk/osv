/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <mutex>
#include <osv/sched.hh>
#include <osv/rwlock.h>
#include <osv/export.h>

using namespace sched;

rwlock::rwlock()
    : _pending_writers(0),
      _readers(0)
{}

rwlock::~rwlock()
{
    assert(_readers == 0);
    assert(_pending_writers == 0);
    assert(_read_waiters.empty());
}

static constexpr unsigned LOCK_INDICATOR  = 0x80000000;
static constexpr unsigned WRITE_INDICATOR = 0x40000000;
static constexpr unsigned READER_MASK     = 0x3fffffff;

static constexpr unsigned RETRY_THRESHOLD = 0;//10; //Do not retry if single CPU

//Possibly DONE with one TODO
bool rwlock::try_rlock() {
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    if (_readers < WRITE_INDICATOR) {
        //TODO: I think it should be a CAS operation
        uint32_t prev_readers = readers->fetch_add(1);
        if (prev_readers < LOCK_INDICATOR) {
            return true;
        }
        readers->fetch_add(-1);
    }
    return false;
}

//Possibly DONE except for the CAS loop and other to do
void rwlock::rlock() {
    unsigned retry = 0;
    while (true) {
        if (try_rlock())
            return;

        if (retry++ > RETRY_THRESHOLD) {
            retry = 0;
            //Add to the _read_waiters
            //
            //Here is a scenario: we are the only reader to try and there is
            //an active writer (that is why try_rlock() failed) and we get preempted
            //at this moment; meanwhile the writer calls wunlock() which pops and unlocks
            //all readers from _read_waiters; we resume and outselves below and the wait()
            //succeeds and we cannot return because we would leave invalid item in the list (stack)
            lockfree::linked_item<thread*> read_waiter(thread::current());
            _read_waiters.push(&read_waiter);
            //
            //Wait up until try_rlock() succeeded
            //TODO: If try_rlock() failed AFTER waking up by wunlock() or another rlock()
            //it means we were popped from _read_waiters so we need to push ourlseves again - how
            //do we determine this? counter? allow one wait and continue try again
            thread::wait_until( [this] { return this->try_rlock(); }); //|| 1ms.expired()); //TODO Timer and flag to see if timer or internal_try_wlock
            //We do not know if were woken and then try_rlock() succeeded or it succeded before we went to sleep
            //So let us iterate over all readers so we remove ourselves if we are in
            //TODO: Add CAS while loop to lock _waking_readers so we are the only ones popping
            while (true) {
                lockfree::linked_item<thread*> *read_waiter = _read_waiters.pop();
                if (!read_waiter) {
                    break;
                }
                if (read_waiter->value != thread::current()) { //Do not wake ourself
                    read_waiter->value->wake();
                }
            }
            return;
        }
    }
}

//Possibly DONE expect for waking the _wmtx owner
void rwlock::runlock() {
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);

    unsigned prev_readers = readers->fetch_add(-1);

    assert(prev_readers > 0);
    assert(prev_readers < LOCK_INDICATOR);

    //Wake potential writer if any (no other active writer) if we are the last one
    if ((prev_readers & READER_MASK) == 1 && (prev_readers & WRITE_INDICATOR) && (prev_readers & LOCK_INDICATOR) == 0) {
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
    //if ((prev_readers & READER_MASK) == 1 && (prev_readers & LOCK_INDICATOR) == 0 && (prev_readers & WRITE_INDICATOR) == 0) {
    if (prev_readers == 1) { // LOCK_INDICATOR and WRITE_INDICATOR are off 
        if (readers->compare_exchange_weak(prev_readers, (WRITE_INDICATOR | LOCK_INDICATOR))) {
            // we've won the race
            return true;
        }
    }
    //We either were not the only reader or have lost the race with a new reader or writer
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
    _wmtx.unlock();
    return false;
}

//Possibly DONE except for the wait_until()
void rwlock::wlock() {
    //Lets set the write indicator in order to phase out the current readers and block new ones
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    readers->fetch_or(WRITE_INDICATOR);

    //Increment pending writers so that wunlock() knows whether to notify waiting readers
    std::atomic<unsigned> *pending_writers = reinterpret_cast<std::atomic<unsigned>*>(&_pending_writers);
    pending_writers->fetch_add(1); //TODO relax ordering to acq_rel maybe 

    //Lock the writer mutex which may obviusly go sleep
    _wmtx.lock();
    //At this point we are still a potential writer and the one that is the 1st from all the
    //pending ones if any
    
    //Now retry to set lock indicator in a loop 
    unsigned retry = 0;
    while (true) {
        if (internal_try_wlock(readers)) {
            pending_writers->fetch_add(-1); //No longer pending
            return;
        }

        readers->fetch_or(WRITE_INDICATOR); //Is it necessary - see wunlock()

        if (retry++ > RETRY_THRESHOLD) { //TODO: It should be so when RETRY_THRESHOLD = 0 it should try once
            retry = 0;
            //Go to sleep
            thread::wait_until( [this, readers] { return this->internal_try_wlock(readers); }); //|| 1ms.expired()); //TODO Timer and flag to see if timer or internal_try_wlock
            return; 
        }
    }
}

//Possibly DONE except for the _waking_readers if necessary
void rwlock::wunlock() {
    assert(_readers & (LOCK_INDICATOR | WRITE_INDICATOR));

    //If we are recursed then simply unlock and return
    if (_wmtx.getdepth() > 1) {
        return _wmtx.unlock();
    }

    //Now more difficult part
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    std::atomic<unsigned> *pending_writers = reinterpret_cast<std::atomic<unsigned>*>(&_pending_writers);
    if (pending_writers->load() == 0) {
        readers->fetch_and(~(LOCK_INDICATOR | WRITE_INDICATOR)); //May race with wlock() loop
        //Wake the waiting readers
        //TODO: Possibly use _waking_readers atomic flag to enforce one _read_waiters consumer
        //if rlock() also consumes it
        //TODO: Possibly disable preemption if rlock() consumes _read_waiters
        while (true) {
            lockfree::linked_item<thread*> *read_waiter = _read_waiters.pop();
            if (!read_waiter) {
                break;
            }
            read_waiter->value->wake();
        }
    } else {
        readers->fetch_and(~LOCK_INDICATOR);
    }

    _wmtx.unlock();
}

//Still in flux - not sure how it should work
void rwlock::downgrade()
{
    assert(_readers & (LOCK_INDICATOR | WRITE_INDICATOR));

    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);

    //TODO: Like wunlock() needs to wake readers
    //Maybe do the same as lines 191 - 209
    while(true) {
        uint32_t prev_readers = readers->load();
        uint32_t next_readers = (prev_readers & ~(LOCK_INDICATOR | WRITE_INDICATOR)) + 1;
        if (readers->compare_exchange_weak(prev_readers, next_readers)) {
            return;
        }
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
