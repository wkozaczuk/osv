//
// Created by wkozaczuk on 5/18/17.
//

#include <osv/prio.hh>
#include <osv/irqlock.hh>
#include <osv/mutex.h>
#include "clock.hh"
#include "rtc.hh"

uint64_t hyperv_tc64_rdmsr(void);

class hypervclock : public clock {
public:
    hypervclock();
    virtual s64 time() __attribute__((no_instrument_function));
    virtual s64 uptime() override __attribute__((no_instrument_function));
    virtual s64 boot_time() override __attribute__((no_instrument_function));
private:
    uint64_t _wall;
    uint64_t _boot_count;
};

hypervclock::hypervclock()
{
    // If we ever need another rtc user, it should be global. But
    // we should really, really avoid it. So let it local.
    auto r = new rtc();

    // In theory we should disable NMIs, but on virtual hardware, we can
    // relax that (This is specially true given our current NMI handler,
    // which will just halt us forever.
    irq_save_lock_type irq_lock;
    WITH_LOCK(irq_lock) {
        _wall = r->wallclock_ns();
        _boot_count = hyperv_tc64_rdmsr();
    };
}

s64 hypervclock::time()
{
    return _wall + (hyperv_tc64_rdmsr() - _boot_count) * 100;
}

s64 hypervclock::uptime()
{
    return (hyperv_tc64_rdmsr() - _boot_count) * 100;
}

s64 hypervclock::boot_time()
{
    // The following is time()-uptime():
    return _wall;
}

void __attribute__((constructor(init_prio::clock))) hyperv_init()
{
    if (processor::features().hyperv_clocksource) {
        clock::register_clock(new hypervclock);
    }
}
