/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "mmio-isa-serial.hh"

namespace console {

mmioaddr_t mmio_isa_serial_console::_addr_mmio;
u64 mmio_isa_serial_console::_phys_mmio_address;

u8 isa_serial_console_base::read_byte(int reg) {
    return mmio_getb(mmio_isa_serial_console::_addr_mmio + reg);
};

void isa_serial_console_base::write_byte(u8 val, int reg) {
    mmio_setb(mmio_isa_serial_console::_addr_mmio + reg, val);
};

void mmio_isa_serial_console::early_init(u64 mmio_phys_address)
{
    _phys_mmio_address = mmio_phys_address;
    _addr_mmio = reinterpret_cast<char*>(mmio_phys_address);

    common_early_init();
}

void mmio_isa_serial_console::memory_map()
{
    if (_phys_mmio_address) {
        _addr_mmio = mmio_map(_phys_mmio_address, mmu::page_size);
    }
}

static void remove_property_name_value(char *cmdline, const char *prefix)
{
    char *prefix_pos = strstr(cmdline, prefix);
    if (!prefix_pos)
        return;

    // Find first white-character or null as an end of the property value
    char *next_char_pos = prefix_pos + strlen(prefix);
    while (*next_char_pos && !isspace(*next_char_pos))
        next_char_pos++;

    // Remove conf info part from cmdline by copying over remaining part
    do {
        *prefix_pos = *next_char_pos++;
    } while (*prefix_pos++);
}

#define CONSOLE_PREFIX "console="
#define MMIO_ISA_CONSOLE_PREFIX "earlycon="
void mmio_isa_serial_console::clean_cmdline(char *cmdline)
{
    remove_property_name_value(cmdline, CONSOLE_PREFIX);
    remove_property_name_value(cmdline, MMIO_ISA_CONSOLE_PREFIX);
}

void mmio_isa_serial_console::dev_start() {
    _irq.reset(new spi_interrupt(gic::irq_type::IRQ_TYPE_EDGE, 64,
                                 [&] { return true; },
                                 [&] { _thread->wake(); }));
    enable_interrupt();
}

}