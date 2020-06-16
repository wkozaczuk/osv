/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "mmio-isa-serial.hh"

namespace console {

mmioaddr_t mmio_isa_serial_console::_addr_mmio;

std::function<u8 (const int&)> isa_serial_console_base::read_byte = [](const int& reg) {
    return mmio_getb(mmio_isa_serial_console::_addr_mmio + reg);
};

std::function<void (const u8&, const int&)> isa_serial_console_base::write_byte = [](const u8& val, const int& reg) {
    mmio_setb(mmio_isa_serial_console::_addr_mmio + reg, val);
};

void mmio_isa_serial_console::early_init(u64 mmio_phys_address)
{
    _addr_mmio = reinterpret_cast<char*>(mmio_phys_address);

    common_early_init( [](const int& reg) {
        return mmio_getb(_addr_mmio + reg);
    }, [](const u8& val, const int& reg) {
        mmio_setb(_addr_mmio + reg, val);
    });
}

void mmio_isa_serial_console::map(u64 mmio_phys_address)
{
    _addr_mmio = mmio_map(mmio_phys_address, mmu::page_size);
}

void mmio_isa_serial_console::dev_start() {
    //TODO: Figure out which interrupt and what kind to use
    //_irq.reset(new sgi_edge_interrupt(4, [&] { _thread->wake(); }));
    enable_interrupt();
}

}
