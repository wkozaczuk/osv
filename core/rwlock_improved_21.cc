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
static constexpr unsigned IND_READ_MASK        = 0xffff0000;
static constexpr unsigned WAITING_READERS_MASK = 0x0000ffff;
static constexpr unsigned READER_LOCK_INC      = 0x00010000;

static constexpr unsigned RETRY_THRESHOLD = 0; //Do not retry if single CPU

//Possibly DONE
bool rwlock::try_rlock()
{
    if (_readers < WRITE_INDICATOR) {
        std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
        unsigned prev_readers = readers->load(std::memory_order_acquire);
        if (readers->compare_exchange_weak(prev_readers, prev_readers + READER_LOCK_INC, std::memory_order_acq_rel)) {
            return true;
        }
    }
    return false;
}

//Possibly DONE
void rwlock::rlock()
{
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
  
    //Try to acquire a lock
    //The loop will stop once a new writer enters wlock() past mutex and sets WRITE_INDICATOR 
    //or writer has already locked it before so LOCK indicator is set as well
    //It may also race with wunlock() which removes WRITE_INDICATOR
    //The loop will compete with other rlocks() or runlocks()
    unsigned prev_readers = readers->load(std::memory_order_acquire);
    while (prev_readers < WRITE_INDICATOR) {
        if (readers->compare_exchange_weak(prev_readers, prev_readers + READER_LOCK_INC, std::memory_order_acq_rel)) {
            return;
        }
    }

    //We stopped because of WRITE_INDICATOR so let us add ourselves to the pending readers
    //This may race with wunlock() which removes WRITE_INDICATOR
    //1) if we win (are first) - wunlock() will see correct pending number
    //2) if we lose (are 2nd) - we will see WRITE_INDICATOR off (the while condition true)
    prev_readers = readers->fetch_add(1, std::memory_order_acq_rel);
    while (prev_readers < WRITE_INDICATOR) {
        if (readers->compare_exchange_weak(prev_readers, prev_readers + READER_LOCK_INC - 1, std::memory_order_acq_rel)) {
            return;
        }
    }

    //We have bumped pending and WRITE_INDICATOR is on
    //Let us wait until wunlock() or downgrade() wakes
    lockfree::linked_item<thread*> read_waiter(thread::current());
    _read_waiters.push(&read_waiter);
    thread::wait_until( [&read_waiter] {
        return read_waiter.value == nullptr;
    });
}

//Possibly DONE
void rwlock::runlock()
{
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);

    unsigned prev_readers = readers->fetch_add(-READER_LOCK_INC, std::memory_order_acq_rel);

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

//Possibly DONE
bool rwlock::try_upgrade()
{
    if (!_wmtx.try_lock()) {
        return false;
    }

    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);

    unsigned prev_readers = readers->load(std::memory_order_acquire);
    if ((prev_readers & IND_READ_MASK) == READER_LOCK_INC) { // LOCK_INDICATOR and WRITE_INDICATOR are off 
        if (readers->compare_exchange_weak(prev_readers, WRITE_INDICATOR | LOCK_INDICATOR | (prev_readers & WAITING_READERS_MASK), std::memory_order_acq_rel)) {
            // we've won the race
            return true;
        }
    }

    //We either were not the only reader or have lost the race with a new reader or writer
    _wmtx.unlock();
    return false;
}

//Possibly DONE
bool rwlock::try_wlock()
{
    if (!_wmtx.try_lock()) {
        return false;
    }

    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    unsigned prev_readers = readers->load(std::memory_order_acquire);
    if ((prev_readers & READER_MASK) == 0 && (prev_readers & LOCK_INDICATOR) == 0) {
        if (readers->compare_exchange_weak(prev_readers, WRITE_INDICATOR | LOCK_INDICATOR | (prev_readers & WAITING_READERS_MASK), std::memory_order_acq_rel)) {
            // we've won the race
            return true;
        }
    } else if (prev_readers & LOCK_INDICATOR && _wmtx.owned()) {
        return true;
    }

    //We have lost the race with a new reader or writer
    _wmtx.unlock();
    return false;
}

//Possibly DONE
void rwlock::wlock()
{
    //Lock the writer mutex which may obviusly go sleep
    _wmtx.lock();
    _writer_wait = true;
    //At this point we are still a potential writer and the one that is the 1st from all the
    //pending ones if any
    
    //Lets set the write indicator in order to phase out the current readers and block new ones
    //This may race with the runlock() of the last reader
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    unsigned prev_readers = readers->fetch_or(WRITE_INDICATOR, std::memory_order_acq_rel);
    //1) If we lost (the fetch above was 2nd), then prev_reader should have 0 readers and
    //   the runlock() will not see WRITE_INDICATOR and therefore will not try to wake us
    //   and _writer_wait will stay true
    //   If the compare_exchange_weak() fails because of the race with rlocks() - change value -
    //   then we will hit wait() that will stay stuck forever - _writer_wait = true
    //2) If we won, then prev_readers will have READER_LOCK_INC (non zero) and runlock() will
    //   will see WRITE_INDICATOR so it should try to wake us and set _writer_wait to false.
    //   And the if below in the 'for' loop will never enter and we will hit wait() which will
    //   proceed without waiting because _writer_wait = false

    //Check recursive
    if (prev_readers & LOCK_INDICATOR && _wmtx.owned()) {
        return;
    }

    //Try to set LOCK_INDICATOR if no active readers and no LOCK_INDICATOR set already (is LOCK_INDICATOR necessary)
    prev_readers = prev_readers | WRITE_INDICATOR;
    while ((prev_readers & READER_MASK) == 0 && (prev_readers & LOCK_INDICATOR) == 0) {
        if (readers->compare_exchange_weak(prev_readers, WRITE_INDICATOR | LOCK_INDICATOR | (prev_readers & WAITING_READERS_MASK), std::memory_order_acq_rel)) {
            // we've won the race
            return;
        }
    }

    //There were some active readers
    //Wait for last active reader to wake us
    thread::wait_until( [this] { 
        return !this->_writer_wait;
    });

    readers->fetch_or(LOCK_INDICATOR | WRITE_INDICATOR, std::memory_order_acq_rel);
}

void rwlock::wake_waiting_readers(std::atomic<unsigned> *readers, unsigned waiting_readers)
{
    //TODO: To minimize sending 100s of IPI wakeups to other CPUs we may
    //use a new thread::wake_many() method that would do similar logic
    //wake_impl() does for one - set status to waking, but only set need_reschedule
    //once and send IPI wake up once for each relevant target CPU 
    while (waiting_readers) {
        lockfree::linked_item<thread*> *read_waiter = _read_waiters.pop();
        if (!read_waiter) {
            waiting_readers = readers->load(std::memory_order_acquire) & WAITING_READERS_MASK;
            continue;
        }
        waiting_readers = readers->fetch_add(READER_LOCK_INC - 1, std::memory_order_acq_rel) & WAITING_READERS_MASK; //lock - pending
        thread *t = read_waiter->value;
        read_waiter->value = nullptr;
        t->wake();
    }
}

//Possibly DONE
void rwlock::wunlock()
{
    assert(_readers & (LOCK_INDICATOR | WRITE_INDICATOR));

    //If we are recursed then simply unlock and return
    if (_wmtx.getdepth() > 1) {
        return _wmtx.unlock();
    }

    //Allow waiting readers to acquire a lock before new writer comes in
    //or 1st in line wakes from the the sleep
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    unsigned waiting_readers = readers->fetch_and(~(LOCK_INDICATOR | WRITE_INDICATOR), std::memory_order_acq_rel) & WAITING_READERS_MASK;

    //Wake the waiting readers which many should be able to acquire a lock
    wake_waiting_readers(readers, waiting_readers);

    _wmtx.unlock();
}

//Re-examine
void rwlock::downgrade()
{
    assert(_readers & (LOCK_INDICATOR | WRITE_INDICATOR));

    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);

    //Remove LOCK and WRITE indicators but increment readers
    unsigned prev_readers = readers->load(std::memory_order_acquire);
    while(true) {
        unsigned next_readers = (prev_readers & ~(LOCK_INDICATOR | WRITE_INDICATOR)) + READER_LOCK_INC;
        if (readers->compare_exchange_weak(prev_readers, next_readers, std::memory_order_acq_rel)) {
            break;
        }
    }
    //
    //Wake the waiting readers which many should be able to acquire a lock
    wake_waiting_readers(readers, readers->load(std::memory_order_acquire) & WAITING_READERS_MASK);

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
