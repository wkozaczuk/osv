/*
 * Copyright (C) 2020 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef OSV_COUNTERS_HH
#define OSV_COUNTERS_HH

namespace arch {

union counters {
    struct {
        unsigned preempt;
        unsigned irq;
    };
    uint64_t any_is_on;
};

extern counters __thread irq_preempt_counters;
constexpr unsigned irq_counter_default_init_value = 11;
constexpr unsigned irq_counter_lazy_stack_init_value = 1;

}

#endif //OSV_COUNTERS_HH
