/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef ARCH_HH_
#define ARCH_HH_

#include "processor.hh"
#include "msr.hh"

#include <osv/barrier.hh>

// namespace arch - architecture independent interface for architecture
//                  dependent operations (e.g. irq_disable vs. cli)

namespace mmu {
extern unsigned __thread irq_counter;
}

namespace sched {
extern unsigned __thread preempt_counter;
}

namespace arch {

#define CACHELINE_ALIGNED __attribute__((aligned(64)))
#define INSTR_SIZE_MIN 1
#define ELF_IMAGE_START OSV_KERNEL_BASE

inline void ensure_next_stack_page() {
    //long offset = - ((unsigned)(!sched::preempt_counter && !mmu::irq_counter) << 12);
    //long stack_pointer;
    //asm volatile("movq %%rsp, %0\n" : "=r"(stack_pointer));
    //stack_pointer -= offset;
    //char i;
    //asm volatile("movb 0(%1), %0\n" : "=r"(i) : "m"(stack_pointer));
    //asm volatile("movb %1(%%rsp), %0" : "=r"(i) : "m"(offset));

    /*
    if (!sched::preempt_counter && !mmu::irq_counter) {
        char i;
        asm volatile("movb -4096(%%rsp), %0" : "=r"(i));
    }*/

    if (sched::preempt_counter || mmu::irq_counter) {
        return;
    }

    char i;
    asm volatile("movb -4096(%%rsp), %0" : "=r"(i));
}

inline void irq_disable()
{
    ensure_next_stack_page();
    ++mmu::irq_counter;
    processor::cli();
}

__attribute__((no_instrument_function))
inline void irq_disable_notrace();

inline void irq_disable_notrace()
{
    processor::cli_notrace();
}

inline void irq_enable()
{
    processor::sti();
    barrier();
    --mmu::irq_counter;
}

inline void wait_for_interrupt()
{
    processor::sti_hlt();
    barrier();
    --mmu::irq_counter;
}

inline void halt_no_interrupts()
{
    processor::cli_hlt();
}

class irq_flag {
public:
    // need to clear the red zone when playing with the stack. also, can't
    // use "m" constraint as it might be addressed relative to %rsp
    void save() {
        asm volatile("sub $128, %%rsp; pushfq; popq %0; add $128, %%rsp" : "=r"(_rflags));
    }
    void restore() {
        asm volatile("sub $128, %%rsp; pushq %0; popfq; add $128, %%rsp" : : "r"(_rflags));
    }
    bool enabled() const {
        return _rflags & processor::rflags_if;
    }
private:
    unsigned long _rflags;
};

class irq_flag_notrace {
public:
    // need to clear the red zone when playing with the stack. also, can't
    // use "m" constraint as it might be addressed relative to %rsp
    __attribute__((no_instrument_function)) void save();
    __attribute__((no_instrument_function)) void restore();
    __attribute__((no_instrument_function)) bool enabled() const;
private:
    unsigned long _rflags;
};

inline void irq_flag_notrace::save()
{
    ensure_next_stack_page();
    ++mmu::irq_counter;
    asm volatile("sub $128, %%rsp; pushfq; popq %0; add $128, %%rsp" : "=r"(_rflags));
}

inline void irq_flag_notrace::restore()
{
    asm volatile("sub $128, %%rsp; pushq %0; popfq; add $128, %%rsp" : : "r"(_rflags));
    --mmu::irq_counter;
}

inline bool irq_flag_notrace::enabled() const
{
    return _rflags & processor::rflags_if;
}

inline bool irq_enabled()
{
    irq_flag f;
    f.save();
    return f.enabled();
}

extern bool tls_available() __attribute__((no_instrument_function));

inline bool tls_available()
{
    unsigned a, d;
    asm("rdmsr" : "=a"(a), "=d"(d) : "c"(msr::IA32_FS_BASE));
    // don't call rdmsr, since we don't want function instrumentation
    return a != 0 || d != 0;
}

}

#endif /* ARCH_HH_ */
