/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef IRQLOCK_HH_
#define IRQLOCK_HH_

#include "arch.hh"

namespace mmu {
extern unsigned __thread read_stack_page_ahead_counter;
}

namespace arch {
inline void read_next_stack_page();
}

class irq_lock_type {
public:
    static void lock() {
       arch::read_next_stack_page();
       arch::irq_disable();
    }
    static void unlock() {
       arch::irq_enable();
       barrier();
       --mmu::read_stack_page_ahead_counter;
    }
};

class irq_save_lock_type {
public:
    void lock();
    void unlock();
private:
    arch::irq_flag _flags;
};


inline void irq_save_lock_type::lock()
{
    _flags.save();
    arch::read_next_stack_page();
    arch::irq_disable();
}

inline void irq_save_lock_type::unlock()
{
    _flags.restore();
    barrier();
    --mmu::read_stack_page_ahead_counter;
}

extern irq_lock_type irq_lock;

#endif /* IRQLOCK_HH_ */
