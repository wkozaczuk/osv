/*
 * Copyright (C) 2020 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef OSV_COUNTERS_HH
#define OSV_COUNTERS_HH

namespace arch {

// Both preempt and irq counters are co-located next to each other in this
// union by design to let compiler optimize ensure_next_stack_page() so
// that it can check if any counter is non-zero by checking single 64-bit any_is_on field
union counters {
    struct data {
        unsigned preempt;
        u16 irq;
        u16 lazy_stack_flags;

        inline void set_lazy_flags_bit(unsigned nr, bool v) {
            lazy_stack_flags &= ~(u64(1) << nr);
            lazy_stack_flags |= u64(v) << nr;
        }
    } _data;
    uint64_t lazy_stack_no_pre_fault;
};

extern counters __thread irq_preempt_counters;

class lazy_stack_flags_guard {
    unsigned _flag_nr;
public:
    lazy_stack_flags_guard(unsigned flag_nr) {
        _flag_nr = flag_nr;
        irq_preempt_counters._data.set_lazy_flags_bit(_flag_nr, true);
    }
    ~lazy_stack_flags_guard() {
        irq_preempt_counters._data.set_lazy_flags_bit(_flag_nr, false);
    }
};

enum {
    lazy_stack_disable = 0,
    lazy_stack_mmu_disable = 1,
    lazy_stack_fault_disable = 2,
};

}

#endif //OSV_COUNTERS_HH
