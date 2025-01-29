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

#define SERIAL_16550_COMPATIBLE 0x00
#define SERIAL_16550_SUBSET     0x01
#define SERIAL_ARM_PL011        0x03
#define SERIAL_16550_WITH_GAS   0x12

bool is_serial_16550(u8 spcr_type);

u32 get_timer_irq();
u64 get_spcr_addr(u8 &type);

int get_cpus_count();
void get_cpus_mpids(u64 *mpids, int n);

void find_mcfg(std::function<bool(u64 addr, u16 segment, u8 bus_start, u8 bus_end)> mcfg_fun);
void parse_madt(std::function<void(u8 type, void *p)> consume_fun);

bool get_gic_v2(u64 *dist, size_t *dist_len, u64 *cpu, size_t *cpu_len);
bool get_gic_v3(u64 *dist, size_t *dist_len, u64 *redist, size_t *redist_len);

u64 get_pci_ecam();
}

#endif //!_OSV_DRIVER_ACPI_HH_
