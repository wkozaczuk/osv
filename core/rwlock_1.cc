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

rwlock::rwlock()
    : _readers(0),
      _wowner(nullptr),
      _wrecurse(0)
{ }

rwlock::~rwlock()
{
    assert(_wowner == nullptr);
    assert(_readers == 0);
    //assert(_read_waiters.empty());
    //assert(_write_waiters.empty());
}

static constexpr unsigned LOCK_INDICATOR  = 0x80000000;
static constexpr unsigned WRITE_INDICATOR = 0x40000000;
static constexpr unsigned READER_MASK     = 0x3fffffff;


static constexpr unsigned FREE = 0;
static constexpr unsigned RETRY_THRESHOLD = 10; //Do not retry if single CPU


void rwlock::rlock() {
    unsigned retry = 0;
    //QUESTION: Is there a point here and in wlock() to retry
    //only if the _wowner is running() like spinning mutex does?
    while (true) {
        if (try_rlock())
            return;
        if (retry++ > RETRY_THRESHOLD) {
            retry = 0;
            sched::thread::yield(std::chrono::milliseconds(1));
        }
    }
}

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

void rwlock::runlock() {
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    assert(_wowner == nullptr);

    unsigned prev_readers = readers->fetch_add(-1);

    assert(prev_readers > 0);
    assert(prev_readers < LOCK_INDICATOR);
}

bool rwlock::try_upgrade() {
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    unsigned prev_readers = readers->load();
    if ((prev_readers & READER_MASK) == 1 && (prev_readers & LOCK_INDICATOR) == 0 && (prev_readers & WRITE_INDICATOR) == 0) {
        //TODO: Simply compare to 1
        if (readers->compare_exchange_weak(prev_readers, (WRITE_INDICATOR | LOCK_INDICATOR))) {
            //reinterpret_cast<std::atomic<sched::thread*>*>(&_wowner)->store(sched::thread::current());
            _wowner = sched::thread::current();
            return true;
        }
    }

    return false;
}


void rwlock::wlock() {
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);

    unsigned retry;
    while (true) {
        if (try_wlock() == true)
            break;

        // Ok, the first one did not suceed, lets set the write
        // indicator in order to phase out all readers for the moment
        readers->fetch_or(WRITE_INDICATOR);

        if (retry++ > RETRY_THRESHOLD) {
            retry = 0;
            sched::thread::yield();
        }
    }
}

bool rwlock::try_wlock() {
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);

    uint32_t prev_readers = readers->load();
    if ((prev_readers & READER_MASK) == 0 && (prev_readers & LOCK_INDICATOR) == 0) {
        if (readers->compare_exchange_weak(prev_readers, WRITE_INDICATOR | LOCK_INDICATOR)) {
            // we've won the race
            //reinterpret_cast<std::atomic<sched::thread*>*>(&_wowner)->store(sched::thread::current());
            _wowner = sched::thread::current();
            return true;
        }
    } else if (prev_readers & LOCK_INDICATOR && _wowner == sched::thread::current()) {
        _wrecurse ++;
        return true;
    }
    return false;
}

void rwlock::wunlock() {
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    assert(_readers & (LOCK_INDICATOR | WRITE_INDICATOR));
    assert(_wowner == sched::thread::current());
    //assert(reinterpret_cast<std::atomic<sched::thread*>*>(&_wowner)->load() == sched::thread::current());
    if (_wrecurse > 0) {
        _wrecurse--;
        return;
    } else {
        //reinterpret_cast<std::atomic<sched::thread*>*>(&_wowner)->store(nullptr);
        _wowner = nullptr;
    }
    // This is the acutal unlock;
    readers->fetch_and(~(LOCK_INDICATOR | WRITE_INDICATOR));
}

void rwlock::downgrade()
{
    std::atomic<unsigned> *readers = reinterpret_cast<std::atomic<unsigned>*>(&_readers);
    assert(_wowner == sched::thread::current());
    assert(_readers & (LOCK_INDICATOR | WRITE_INDICATOR));

    _wrecurse = 0;

    while(true) {
        uint32_t prev_readers = readers->load();
        uint32_t next_readers = (prev_readers & ~(LOCK_INDICATOR | WRITE_INDICATOR)) + 1;
        if (readers->compare_exchange_weak(prev_readers, next_readers)) {
            //reinterpret_cast<std::atomic<sched::thread*>*>(&_wowner)->store(nullptr);
            _wowner = nullptr;
            return;
        }
    }
}

bool rwlock::wowned()
{
    return (sched::thread::current() == _wowner);
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
