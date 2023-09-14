/*
 * Copyright (C) 2023 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef TLS_SWITCH_HH
#define TLS_SWITCH_HH

#include "arch.hh"
#include "arch-tls.hh"

namespace sched {
    void set_fsbase(u64 v);
}

namespace arch {

//TODO: Possibly use inline
class tls_switch_on_exception_stack {
    thread_control_block *_kernel_tcb;
public:
    tls_switch_on_exception_stack() {
        asm volatile ( "movq %%gs:16, %0\n\t" : "=r"(_kernel_tcb));
        
        if (_kernel_tcb->app_tcb) {
            //Switch to kernel tcb if app tcb present and on app tcb
            if (!_kernel_tcb->kernel_tcb_counter) {
                sched::set_fsbase(reinterpret_cast<u64>(_kernel_tcb->self));
            }
            _kernel_tcb->kernel_tcb_counter++;
        }
    }

    ~tls_switch_on_exception_stack() {
        if (_kernel_tcb->app_tcb) {
            _kernel_tcb->kernel_tcb_counter--;
            if (!_kernel_tcb->kernel_tcb_counter) {
                //Restore app tcb
                sched::set_fsbase(reinterpret_cast<u64>(_kernel_tcb->app_tcb));
            }
        }
    }
};

class tls_switch_on_syscall_stack {
    thread_control_block *_kernel_tcb;
public:
    tls_switch_on_syscall_stack() {
        asm volatile ( "movq %%gs:16, %0\n\t" : "=r"(_kernel_tcb));
        
        if (_kernel_tcb->app_tcb) {
            //Switch to kernel tcb if app tcb present and on app tcb
            arch::irq_disable();
            if (!_kernel_tcb->kernel_tcb_counter) {
                sched::set_fsbase(reinterpret_cast<u64>(_kernel_tcb->self));
            }
            _kernel_tcb->kernel_tcb_counter++;
            arch::irq_enable();
        }
    }

    ~tls_switch_on_syscall_stack() {
        if (_kernel_tcb->app_tcb) {
            arch::irq_disable();
            _kernel_tcb->kernel_tcb_counter--;
            if (!_kernel_tcb->kernel_tcb_counter) {
                //Restore app tcb
                sched::set_fsbase(reinterpret_cast<u64>(_kernel_tcb->app_tcb));
            }
            arch::irq_enable();
        }
    }
};

class tls_switch_on_user_stack {
    thread_control_block *_kernel_tcb;
public:
    tls_switch_on_user_stack() {
        asm volatile ( "movq %%gs:16, %0\n\t" : "=r"(_kernel_tcb));
        
        //Switch to kernel tcb if app tcb present and on app tcb
        if (_kernel_tcb->app_tcb && !_kernel_tcb->kernel_tcb_counter) {
            //To avoid page faults on user stack when interrupts are disabled
            arch::ensure_next_stack_page();
            arch::irq_disable();
            sched::set_fsbase(reinterpret_cast<u64>(_kernel_tcb->self));
        }
    }

    ~tls_switch_on_user_stack() {
        //Restore app tcb
        if (_kernel_tcb->app_tcb && !_kernel_tcb->kernel_tcb_counter) {
            sched::set_fsbase(reinterpret_cast<u64>(_kernel_tcb->app_tcb));
            arch::irq_enable();
        }
    }
};

}

#endif
