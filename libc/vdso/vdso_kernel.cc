#include <time.h>
#include <sys/time.h>
#include "arch.hh"
#include "arch-tls.hh"

namespace sched {
void set_fsbase(u64 v);
}

class app_kernel_tls_switcher {
    thread_control_block *_kernel_tcb;
public:
    app_kernel_tls_switcher() {
        asm volatile ( "movq %%gs:0, %0\n\t" : "=r"(_kernel_tcb));
        
        //Switch to kernel tcb if app tcb present
        if (_kernel_tcb->app_tcb && !_kernel_tcb->kernel_tcb_counter) {
            arch::irq_disable();
            sched::set_fsbase(reinterpret_cast<u64>(_kernel_tcb->self));
        }
    }

    ~app_kernel_tls_switcher() {
        //Restore app tcb
        if (_kernel_tcb->app_tcb && !_kernel_tcb->kernel_tcb_counter) {
            sched::set_fsbase(reinterpret_cast<u64>(_kernel_tcb->app_tcb));
            arch::irq_enable();
        }
    }
};

extern "C" __attribute__((__visibility__("default")))
time_t __vdso_kernel_time(time_t *tloc)
{
    app_kernel_tls_switcher _tls_switch;
    return time(tloc);
}

extern "C" __attribute__((__visibility__("default")))
int __vdso_kernel_gettimeofday(struct timeval *tv, struct timezone *tz)
{
    app_kernel_tls_switcher _tls_switch;
    return gettimeofday(tv, tz);
}

extern "C" __attribute__((__visibility__("default")))
int __vdso_kernel_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    app_kernel_tls_switcher _tls_switch;
    return clock_gettime(clk_id, tp);
}
