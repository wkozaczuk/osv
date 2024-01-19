/*
 * Copyright (C) 2013-2014 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

// linux syscalls

#include <osv/debug.hh>
#define BOOST_NO_STD_LOCALE 1
//#include <boost/format.hpp>
#include <osv/sched.hh>
#include <osv/mutex.h>
#include <osv/waitqueue.hh>
#include <osv/stubbing.hh>
#include <osv/export.h>
#include <osv/trace.hh>
#include <memory>

#include <syscall.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statx.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/unistd.h>
#include <sys/random.h>
#include <sys/vfs.h>
#include <sys/uio.h>
#include <sys/epoll.h>
#include <sys/sysinfo.h>
#include <sys/sendfile.h>
#include <sys/prctl.h>
#include <sys/timerfd.h>
#include <sys/resource.h>
#include <sys/shm.h>
#include <termios.h>
#include <poll.h>
#ifdef __x86_64__
#include "tls-switch.hh"
#endif

#include <unordered_map>

#include <musl/src/internal/ksigaction.h>

extern "C" int eventfd2(unsigned int, int);

extern "C" OSV_LIBC_API long gettid()
{
    return sched::thread::current()->id();
}

// We don't expect applications to use the Linux futex() system call (it is
// normally only used to implement higher-level synchronization mechanisms),
// but unfortunately gcc's C++ runtime uses a subset of futex in the
// __cxa__guard_* functions, which safeguard the concurrent initialization
// of function-scope static objects. We only implement here this subset.
// The __cxa_guard_* functions only call futex in the rare case of contention,
// in fact so rarely that OSv existed for a year before anyone noticed futex
// was missing. So the performance of this implementation is not critical.
static std::unordered_map<void*, waitqueue> queues;
static mutex queues_mutex;

#define FUTEX_BITSET_MATCH_ANY  0xffffffff

int futex(int *uaddr, int op, int val, const struct timespec *timeout,
        int *uaddr2, uint32_t val3)
{
    switch (op & FUTEX_CMD_MASK) {
    case FUTEX_WAIT_BITSET:
        if (val3 != FUTEX_BITSET_MATCH_ANY) {
            abort("Unimplemented futex() operation %d\n", op);
        }

    case FUTEX_WAIT:
        WITH_LOCK(queues_mutex) {
            if (*uaddr == val) {
                waitqueue &q = queues[uaddr];
                if (timeout) {
                    sched::timer tmr(*sched::thread::current());
                    if ((op & FUTEX_CMD_MASK) == FUTEX_WAIT_BITSET) {
                        // If FUTEX_WAIT_BITSET we need to interpret timeout as an absolute
                        // time point. If futex operation FUTEX_CLOCK_REALTIME is set we will use
                        // real-time clock otherwise we will use monotonic clock
                        if (op & FUTEX_CLOCK_REALTIME) {
                            tmr.set(osv::clock::wall::time_point(std::chrono::seconds(timeout->tv_sec) +
                                                                 std::chrono::nanoseconds(timeout->tv_nsec)));
                        } else {
                            tmr.set(osv::clock::uptime::time_point(std::chrono::seconds(timeout->tv_sec) +
                                                                   std::chrono::nanoseconds(timeout->tv_nsec)));
                        }
                    } else {
                        tmr.set(std::chrono::seconds(timeout->tv_sec) +
                                std::chrono::nanoseconds(timeout->tv_nsec));
                    }
                    sched::thread::wait_for(queues_mutex, tmr, q);
                    // FIXME: testing if tmr was expired isn't quite right -
                    // we could have had both a wakeup and timer expiration
                    // racing. It would be more correct to check if we were
                    // waken by a FUTEX_WAKE. But how?
                    if (tmr.expired()) {
                        errno = ETIMEDOUT;
                        return -1;
                    }
                } else {
                    q.wait(queues_mutex);
                }
                return 0;
            } else {
                errno = EWOULDBLOCK;
                return -1;
            }
        }
    case FUTEX_WAKE:
        if(val < 0) {
            errno = EINVAL;
            return -1;
        }

        WITH_LOCK(queues_mutex) {
            auto i = queues.find(uaddr);
            if (i != queues.end()) {
                int waken = 0;
                while( (val > waken) && !(i->second.empty()) ) {
                    i->second.wake_one(queues_mutex);
                    waken++;
                }
                if(i->second.empty()) {
                    queues.erase(i);
                }
                return waken;
            }
        }
        return 0;
    default:
        abort("Unimplemented futex() operation %d\n", op);
    }
}

TRACEPOINT(trace_syscall_futex, "%d <= %p %d %d %p %p %d", int, int *, int, int, const struct timespec *, int *, uint32_t);

#define SYSCALL0(fn) case (__NR_##fn): do { long ret = fn(); trace_syscall_##fn(ret); return ret; } while (0)

#define SYSCALL1(fn, __t1)             \
        case (__NR_##fn): do {         \
        va_list args;                  \
        __t1 arg1;                     \
        va_start(args, number);        \
        arg1 = va_arg(args, __t1);     \
        va_end(args);                  \
        auto ret = fn(arg1);           \
        trace_syscall_##fn(ret, arg1); \
        return ret;                    \
        } while (0)

#define SYSCALL2(fn, __t1, __t2)            \
        case (__NR_##fn): do {              \
        va_list args;                       \
        __t1 arg1;                          \
        __t2 arg2;                          \
        va_start(args, number);             \
        arg1 = va_arg(args, __t1);          \
        arg2 = va_arg(args, __t2);          \
        va_end(args);                       \
        auto ret = fn(arg1, arg2);          \
        trace_syscall_##fn(ret, arg1, arg2);\
        return ret;                         \
        } while (0)

#define SYSCALL3(fn, __t1, __t2, __t3)             \
        case (__NR_##fn): do {                     \
        va_list args;                              \
        __t1 arg1;                                 \
        __t2 arg2;                                 \
        __t3 arg3;                                 \
        va_start(args, number);                    \
        arg1 = va_arg(args, __t1);                 \
        arg2 = va_arg(args, __t2);                 \
        arg3 = va_arg(args, __t3);                 \
        va_end(args);                              \
        auto ret = fn(arg1, arg2, arg3);           \
        trace_syscall_##fn(ret, arg1, arg2, arg3); \
        return ret;                                \
        } while (0)

#define SYSCALL4(fn, __t1, __t2, __t3, __t4)             \
        case (__NR_##fn): do {                           \
        va_list args;                                    \
        __t1 arg1;                                       \
        __t2 arg2;                                       \
        __t3 arg3;                                       \
        __t4 arg4;                                       \
        va_start(args, number);                          \
        arg1 = va_arg(args, __t1);                       \
        arg2 = va_arg(args, __t2);                       \
        arg3 = va_arg(args, __t3);                       \
        arg4 = va_arg(args, __t4);                       \
        va_end(args);                                    \
        auto ret = fn(arg1, arg2, arg3, arg4);           \
        trace_syscall_##fn(ret, arg1, arg2, arg3, arg4); \
        return ret;                                      \
        } while (0)

#define SYSCALL5(fn, __t1, __t2, __t3, __t4, __t5)             \
        case (__NR_##fn): do {                                 \
        va_list args;                                          \
        __t1 arg1;                                             \
        __t2 arg2;                                             \
        __t3 arg3;                                             \
        __t4 arg4;                                             \
        __t5 arg5;                                             \
        va_start(args, number);                                \
        arg1 = va_arg(args, __t1);                             \
        arg2 = va_arg(args, __t2);                             \
        arg3 = va_arg(args, __t3);                             \
        arg4 = va_arg(args, __t4);                             \
        arg5 = va_arg(args, __t5);                             \
        va_end(args);                                          \
        auto ret = fn(arg1, arg2, arg3, arg4, arg5);           \
        trace_syscall_##fn(ret, arg1, arg2, arg3, arg4, arg5); \
        return ret;                                            \
        } while (0)

#define SYSCALL6(fn, __t1, __t2, __t3, __t4, __t5, __t6)             \
        case (__NR_##fn): do {                                       \
        va_list args;                                                \
        __t1 arg1;                                                   \
        __t2 arg2;                                                   \
        __t3 arg3;                                                   \
        __t4 arg4;                                                   \
        __t5 arg5;                                                   \
        __t6 arg6;                                                   \
        va_start(args, number);                                      \
        arg1 = va_arg(args, __t1);                                   \
        arg2 = va_arg(args, __t2);                                   \
        arg3 = va_arg(args, __t3);                                   \
        arg4 = va_arg(args, __t4);                                   \
        arg5 = va_arg(args, __t5);                                   \
        arg6 = va_arg(args, __t6);                                   \
        va_end(args);                                                \
        auto ret = fn(arg1, arg2, arg3, arg4, arg5, arg6);           \
        trace_syscall_##fn(ret, arg1, arg2, arg3, arg4, arg5, arg6); \
        return ret;                                                  \
        } while (0)

OSV_LIBC_API long syscall(long number, ...)
{
    // Save FPU state and restore it at the end of this function
    sched::fpu_lock fpu;
    SCOPE_LOCK(fpu);

    switch (number) {
    SYSCALL6(futex, int *, int, int, const struct timespec *, int *, uint32_t);
    }

    debug_always("syscall(): unimplemented system call %d\n", number);
    errno = ENOSYS;
    return -1;
}
long __syscall(long number, ...)  __attribute__((alias("syscall")));

#ifdef __x86_64__
// In x86-64, a SYSCALL instruction has exactly 6 parameters, because this is the number of registers
// alloted for passing them (additional parameters *cannot* be passed on the stack). So we can get
// 7 arguments to this function (syscall number plus its 6 parameters). Because in the x86-64 ABI the
// seventh argument is on the stack, we must pass the arguments explicitly to the syscall() function
// and can't just call it without any arguments and hope everything will be passed on
extern "C" long syscall_wrapper(long number, long p1, long p2, long p3, long p4, long p5, long p6)
#endif
#ifdef __aarch64__
// In aarch64, the first 8 parameters to a procedure call are passed in the x0-x7 registers and
// the parameters of syscall call (SVC intruction) in are passed in x0-x5 registers and syscall number
// in x8 register before. To avoid shuffling the arguments around we make syscall_wrapper()
// accept the syscall parameters as is but accept the syscall number as the last 7th argument which
// the code in entry.S arranges.
extern "C" long syscall_wrapper(long p1, long p2, long p3, long p4, long p5, long p6, long number)
#endif
{
#ifdef __x86_64__
    // Switch TLS register if necessary
    arch::tls_switch tls_switch;
#endif

    int errno_backup = errno;
    // syscall and function return value are in rax
    auto ret = syscall(number, p1, p2, p3, p4, p5, p6);
    int result = -errno;
    errno = errno_backup;
    if (ret < 0 && ret >= -4096) {
        return result;
    }
    return ret;
}
