/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef DRIVERS_MMIO_ISA_SERIAL_HH
#define DRIVERS_MMIO_ISA_SERIAL_HH

#include "console-driver.hh"
#include <osv/pci.hh>
#include <osv/sched.hh>
#include <osv/interrupt.hh>
#include <osv/mmio.hh>

namespace console {

class mmio_isa_serial_console : public console_driver {
public:
    static void early_init();
    virtual void write(const char *str, size_t len);
    virtual void flush() {}
    virtual bool input_ready() override;
    virtual char readch();

private:
    //std::unique_ptr<sgi_edge_interrupt> _irq;
    static mmioaddr_t _addr_mmio;

    virtual void dev_start();
    void enable_interrupt();
    static void putchar(const char ch);
    virtual const char *thread_name() { return "mmio-isa-serial-input"; }
};

}

#endif
