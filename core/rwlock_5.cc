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
    : _readers(0)
{}

rwlock::~rwlock()
{
    assert(_readers == 0);
    assert(_read_waiters.empty());
}

static constexpr unsigned LOCK_INDICATOR       = 0x80000000;
static constexpr unsigned WRITE_INDICATOR      = 0x40000000;
static constexpr unsigned READER_MASK          = 0x3fff0000;
static constexpr unsigned WAITING_READERS_MASK = 0x0000ffff;
static constexpr unsigned READER_LOCK_INC      = 0x00010000;

static constexpr unsigned RETRY_THRESHOLD = 0; //Do not retry if single CPU

//Possibly DONE
bool rwlock::try_rlock() {
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    if (_readers < WRITE_INDICATOR) {
        uint32_t prev_readers = readers->fetch_add(READER_LOCK_INC);
        if (prev_readers < LOCK_INDICATOR) {
            return true;
        }
        readers->fetch_add(-READER_LOCK_INC);
    }
    return false;
}

//Possibly DONE except for the problematic scenario explained before wait_until()
void rwlock::rlock() {
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    /*rif (_readers < WRITE_INDICATOR) {
        uint32_t prev_readers = readers->fetch_add(READER_LOCK_INC + 1); //Lock + pending
        //if (prev_readers < LOCK_INDICATOR) {
        if (prev_readers < WRITE_INDICATOR) {
            readers->fetch_add(-1); //- pending
            return;
        } else {
            readers->fetch_add(-READER_LOCK_INC); //-locked 
        }
    } else {
        readers->fetch_add(1); //+ pending
    }*/
  
    //Try to acquire a lock
    //The loop will stop once a new writer enters wlock() past mutex and sets WRITE_INDICATOR 
    //or writer has already locked it before so LOCK indicator is set as well
    //It may also race with wunlock() which removes WRITE_INDICATOR
    //The loop will compete with other rlocks() or runlocks()
    unsigned prev_readers = readers->load();
    while (prev_readers < WRITE_INDICATOR) {
        if (readers->compare_exchange_weak(prev_readers, prev_readers + READER_LOCK_INC)) {
            return;
        }
    }

    //We stopped because of WRITE_INDICATOR
    //This below may race with wunlock() which removes WRITE_INDICATOR
    //1) we win (are first) - wunlock() will see correct pending number
    //2) we lose (are 2nd) - we will see WRITE_INDICATOR
    prev_readers = readers->fetch_add(1);
    while (prev_readers < WRITE_INDICATOR) {
        if (readers->compare_exchange_weak(prev_readers, prev_readers + READER_LOCK_INC - 1)) {
            return;
        }
    }

    //We have bumped pending and WRITE_INDICATOR is on
    lockfree::linked_item<thread*> read_waiter(thread::current());
    _read_waiters.push(&read_waiter);
    thread::wait_until( [&read_waiter] {
        return read_waiter.value == nullptr;
    });
}

//Possibly DONE
void rwlock::runlock() {
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);

    unsigned prev_readers = readers->fetch_add(-READER_LOCK_INC);

    assert(prev_readers > 0);
    assert(prev_readers < LOCK_INDICATOR);

    //Wake potential writer if any (no other active writer) if we are the last one
    if ((prev_readers & READER_MASK) == READER_LOCK_INC && (prev_readers & WRITE_INDICATOR) && (prev_readers & LOCK_INDICATOR) == 0) {
        //Wake the _wmtx owner - pending writer - if not null
        auto pending_writer = _wmtx.get_owner();
        if (pending_writer) {
            _writer_wait = false;
            pending_writer->wake();
        }
    }
}

//Possibly DONE (not sure if there is better)
bool rwlock::try_upgrade() {
    if (!_wmtx.try_lock()) {
        return false;
    }
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    unsigned prev_readers = readers->load();
    //Maybe it should be a while loop
    if ((prev_readers & READER_MASK) == READER_LOCK_INC) { // LOCK_INDICATOR and WRITE_INDICATOR are off 
        if (readers->compare_exchange_weak(prev_readers, (WRITE_INDICATOR | LOCK_INDICATOR) | (prev_readers & WAITING_READERS_MASK))) {
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
        if (readers->compare_exchange_weak(prev_readers, WRITE_INDICATOR | LOCK_INDICATOR) | (prev_readers & WAITING_READERS_MASK)) {
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
    _writer_wait = true;
    //At this point we are still a potential writer and the one that is the 1st from all the
    //pending ones if any
    
    //Lets set the write indicator in order to phase out the current readers and block new ones
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    uint32_t prev_readers = readers->fetch_or(WRITE_INDICATOR);

    //Check recursive
    if (prev_readers & LOCK_INDICATOR && _wmtx.owned()) {
        return;
    }

    //Try to set LOCK_INDICATOR if no active readers and no LOCK_INDICATOR set already (why?)
    for (int i = 0; i < 2; i++ ) {
        if ((prev_readers & READER_MASK) == 0 && (prev_readers & LOCK_INDICATOR) == 0) {
            if (readers->compare_exchange_weak(prev_readers, WRITE_INDICATOR | LOCK_INDICATOR) | (prev_readers & WAITING_READERS_MASK)) {
                // we've won the race
                return;
            }
        }
    }

    //There were some active readers
    //Wait for last active reader to wake us
    //How do we coordinate with the last reader?
    //thread::wait_until( [this, readers] { return this->internal_try_wlock(readers); });
    /*bool locked = false;
    thread::wait_until( [&locked] { 
        if (locked) {
            return true;
        } else {
            locked = true;
            return false;
        }
    });*/
    
    thread::wait_until( [this] { 
        return !this->_writer_wait;
    });

    readers->fetch_or(LOCK_INDICATOR | WRITE_INDICATOR);
/*
    //Now retry to set lock indicator in a loop 
    unsigned retry = 0;
    while (true) {
        if (internal_try_wlock(readers)) {
            return;
        }

        if (retry++ > RETRY_THRESHOLD) {
            retry = 0;
            //Go to sleep
            thread::wait_until( [this, readers] { return this->internal_try_wlock(readers); });
            return; 
        }
    }*/
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
    unsigned waiting_readers = readers->fetch_and(~(LOCK_INDICATOR | WRITE_INDICATOR)) & WAITING_READERS_MASK;

    //Wake the waiting readers which many should be able to acquire a lock
    //TODO: To minimize sending 100s of IPI wakeups to other CPUs we may
    //use a new thread::wake_many() method that would do similar logic
    //wake_impl() does for one - set status to waking, but only set need_reschedule
    //once and send IPI wake up once for each relevant target CPU 
    /*
    for (; waiting_readers > 0; waiting_readers--) {
        while (true) {
            lockfree::linked_item<thread*> *read_waiter = _read_waiters.pop();
            if (!read_waiter) {
                continue;
            }
            readers->fetch_add(READER_LOCK_INC - 1); //lock - pending
            thread *t = read_waiter->value;
            read_waiter->value = nullptr;
            t->wake();
            break;
        }
    }*/
    while (waiting_readers) {
            lockfree::linked_item<thread*> *read_waiter = _read_waiters.pop();
            if (!read_waiter) {
                waiting_readers = readers->load() & WAITING_READERS_MASK;
                continue;
            }
            waiting_readers = readers->fetch_add(READER_LOCK_INC - 1) & WAITING_READERS_MASK; //lock - pending
            thread *t = read_waiter->value;
            read_waiter->value = nullptr;
            t->wake();
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
        lockfree::linked_item<thread*> *read_waiter = _read_waiters.pop();
        if (!read_waiter) {
            break;
        }
        read_waiter->value->wake();
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
