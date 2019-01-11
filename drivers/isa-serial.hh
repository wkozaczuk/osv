/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef DRIVERS_ISA_SERIAL_HH
#define DRIVERS_ISA_SERIAL_HH

#include "console-driver.hh"
#include <queue>
#include <osv/pci.hh>
#include <osv/sched.hh>
#include <osv/interrupt.hh>
#include <osv/mutex.h>

namespace console {

class isa_serial_console : public console_driver {
public:
    ~isa_serial_console() {
        if (_writer_buffer) {
            delete _writer_buffer;
        }
    }
    static void early_init();
    virtual void write(const char *str, size_t len);
    virtual void flush() {}
    virtual bool input_ready() override;
    virtual char readch();
    bool output_ready();
private:
    std::unique_ptr<gsi_edge_interrupt> _irq;
    static const u16 ioport = 0x3f8;
    bool _started = false;
    std::queue<char> *_writer_buffer = nullptr;
    sched::thread *_writer_thread;
    mutex _write_mutex;

    virtual void dev_start();
    void enable_interrupt();
    static void putchar(const char ch);
    virtual const char *thread_name() { return "isa-serial-input"; }
    void write();
};

}

#endif
