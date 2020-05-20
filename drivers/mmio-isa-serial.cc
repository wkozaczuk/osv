/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "mmio-isa-serial.hh"

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

mmioaddr_t mmio_isa_serial_console::_addr_mmio;

void mmio_isa_serial_console::early_init()
{
    u64 address = 0x40001000; //TODO: Should parse from early console boot command line
    u64 size = 4096;

    _addr_mmio = mmio_map(address, size);

    // Set the UART speed to to 115,200 bps, This is done by writing 1,0 to
    // Divisor Latch registers, but to access these we need to temporarily
    // set the Divisor Latch Access Bit (DLAB) on the LSR register, because
    // the UART has fewer ports than registers...
    mmio_setb(_addr_mmio + (int)regs::LCR, lcr::LEN_8BIT | lcr::DLAB);
    mmio_setb(_addr_mmio + (int)regs::DLL, 1);
    mmio_setb(_addr_mmio + (int)regs::DLM, 0);
    mmio_setb(_addr_mmio + (int)regs::LCR, lcr::LEN_8BIT);

    //  interrupt threshold
    mmio_setb(_addr_mmio + (int)regs::FCR, 0);

    // disable interrupts
    mmio_setb(_addr_mmio + (int)regs::IER, 0);

    // Most physical UARTs need the MCR AUX_OUTPUT_2 bit set to 1 for
    // interrupts to be generated. QEMU doesn't bother checking this
    // bit, but interestingly VMWare does, so we must set it.
    mmio_setb(_addr_mmio + (int)regs::MCR, mcr::AUX_OUTPUT_2);
}

void mmio_isa_serial_console::write(const char *str, size_t len)
{
    while (len-- > 0)
        putchar(*str++);
}

bool mmio_isa_serial_console::input_ready()
{
    u8 val = mmio_getb(_addr_mmio + (int)regs::LSR);
    // On VMWare hosts without a serial port, this register always
    // returns 0xff.  Just ignore it instead of spinning incessantly.
    return (val != 0xff && (val & lsr::RECEIVE_DATA_READY));
}

char mmio_isa_serial_console::readch()
{
    u8 val;
    char letter;

    do {
        val = mmio_getb(_addr_mmio + (int)regs::LSR);
    } while (!(val & (lsr::RECEIVE_DATA_READY | lsr::OVERRUN | lsr::PARITY_ERROR | lsr::FRAME_ERROR)));

    letter = mmio_getb(_addr_mmio);

    return letter;
}

void mmio_isa_serial_console::putchar(const char ch)
{
    u8 val;

    do {
        val = mmio_getb(_addr_mmio + (int)regs::LSR);
    } while (!(val & lsr::TRANSMIT_HOLD_EMPTY));

    mmio_setb(_addr_mmio, ch);
}

void mmio_isa_serial_console::enable_interrupt()
{
    // enable interrupts
    mmio_setb(_addr_mmio + (int)regs::IER, 1);
}

void mmio_isa_serial_console::dev_start() {
    //_irq.reset(new sgi_edge_interrupt(4, [&] { _thread->wake(); }));
    enable_interrupt();
}

}
