/*
 * Copyright (C) 2013 Nodalink, SARL.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */
#ifndef _OSV_DRIVER_ACPI_HH_
#define _OSV_DRIVER_ACPI_HH_

#include "acpi.h"

namespace acpi {

extern uint64_t pvh_rsdp_paddr;

void init();
bool is_enabled();

u32 get_timer_irq();
u64 get_spcr_addr(u8 &type);

void find_mcfg(std::function<bool(u64 addr, u16 segment, u8 bus_start, u8 bus_end)> mcfg_fun);
void parse_madt(std::function<void(u8 type, void *p)> consume_fun);
}

#endif //!_OSV_DRIVER_ACPI_HH_
