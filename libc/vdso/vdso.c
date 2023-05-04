//#include "libc.h"
#include <time.h>
#include <sys/time.h>
#include <osv/types.h>

__attribute__((__visibility__("default")))
time_t __vdso_time(time_t *tloc)
{
    ulong* kernel_tcb;
    asm volatile("" : : : "memory");
    asm volatile ( "movq %%gs:0, %0\n\t" : "=r"(kernel_tcb));
    asm volatile("" : : : "memory");

    if (!kernel_tcb[5]) { //kernel_tcb_counter
       //Set kernel tcb
       asm volatile("wrfsbase %0" : : "r"(kernel_tcb[0])); 
    }
    kernel_tcb[5]++;

    time_t res = time(tloc);

    kernel_tcb[5]--;
    if (!kernel_tcb[5]) { //kernel_tcb_counter
       //Set app tcb
       asm volatile("wrfsbase %0" : : "r"(kernel_tcb[4])); // app tcb
    }
    return res;
}

__attribute__((__visibility__("default")))
int __vdso_gettimeofday(struct timeval *tv, struct timezone *tz)
{
    ulong* kernel_tcb;
    asm volatile("" : : : "memory");
    asm volatile ( "movq %%gs:0, %0\n\t" : "=r"(kernel_tcb));
    asm volatile("" : : : "memory");

    if (!kernel_tcb[5]) { //kernel_tcb_counter
       //Set kernel tcb
       asm volatile("wrfsbase %0" : : "r"(kernel_tcb[0])); 
    }
    kernel_tcb[5]++;

    int res = gettimeofday(tv, tz);

    kernel_tcb[5]--;
    if (!kernel_tcb[5]) { //kernel_tcb_counter
       //Set app tcb
       asm volatile("wrfsbase %0" : : "r"(kernel_tcb[4])); // app tcb
    }

    return res;
}

__attribute__((__visibility__("default")))
int __vdso_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    ulong* kernel_tcb;
    asm volatile("" : : : "memory");
    asm volatile ( "movq %%gs:0, %0\n\t" : "=r"(kernel_tcb));
    asm volatile("" : : : "memory");

    if (!kernel_tcb[5]) { //kernel_tcb_counter
       //Set kernel tcb
       asm volatile("wrfsbase %0" : : "r"(kernel_tcb[0])); 
    }
    kernel_tcb[5]++;

    int res = clock_gettime(clk_id, tp);

    kernel_tcb[5]--;
    if (!kernel_tcb[5]) { //kernel_tcb_counter
       //Set app tcb
       asm volatile("wrfsbase %0" : : "r"(kernel_tcb[4])); // app tcb
    }

    return res;
}
