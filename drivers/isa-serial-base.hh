/*
 * Copyright (C) 2020 Waldemar Kozaczuk.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef DRIVERS_ISA_SERIAL_BASE_HH
#define DRIVERS_ISA_SERIAL_BASE_HH

#include "console-driver.hh"
#include <osv/pci.hh>
#include <osv/sched.hh>
#include <osv/interrupt.hh>

namespace console {

// UART registers, offsets to ioport:
enum regs {
    IER = 1,    // Interrupt Enable Register
    FCR = 2,    // FIFO Control Register
    LCR = 3,    // Line Control Register
    MCR = 4,    // Modem Control Register
    LSR = 5,    // Line Control Register
    MSR = 6,    // Modem Status Register
    SCR = 7,    // Scratch Register
    DLL = 0,    // Divisor Latch LSB Register
    DLM = 1,    // Divisor Latch MSB Register
};

enum lcr {
    // When bit 7 (DLAB) of LCR is set to 1, the two registers 0 and 1
    // change their meaning and become two bytes controlling the baud rate
    DLAB     = 0x80,    // Divisor Latch Access Bit in LCR register
    LEN_8BIT = 3,
};

// Various bits of the Line Status Register
enum lsr {
    RECEIVE_DATA_READY  = 0x1,
    OVERRUN             = 0x2,
    PARITY_ERROR        = 0x4,
    FRAME_ERROR         = 0x8,
    BREAK_INTERRUPT     = 0x10,
    TRANSMIT_HOLD_EMPTY = 0x20,
    TRANSMIT_EMPTY      = 0x40,
    FIFO_ERROR          = 0x80,
};

// Various bits of the Modem Control Register
enum mcr {
    DTR                 = 0x1,
    RTS                 = 0x2,
    AUX_OUTPUT_1        = 0x4,
    AUX_OUTPUT_2        = 0x8,
    LOOPBACK_MODE       = 0x16,
};

class isa_serial_console_base : public console_driver {
public:
    virtual void write(const char *str, size_t len);
    virtual void flush() {}
    virtual bool input_ready() override;
    virtual char readch();
protected:
    static void common_early_init(const std::function<u8 (const int&)> _read_byte,
            const std::function<void (const u8&, const int&)> _write_byte);
    void enable_interrupt();
private:
    static std::function<u8 (const int&)> read_byte;
    static std::function<void (const u8&, const int&)> write_byte;
    void putchar(const char ch);
};
}

#endif
