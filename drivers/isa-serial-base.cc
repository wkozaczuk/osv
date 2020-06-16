/*
 * Copyright (C) 2020 Waldemar Kozaczuk.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "isa-serial-base.hh"

namespace console {

void isa_serial_console_base::common_early_init(
        const std::function<u8 (const int&)> _read_byte,
        const std::function<void (const u8&, const int&)> _write_byte)
{
    read_byte = _read_byte;
    write_byte = _write_byte;

    // Set the UART speed to to 115,200 bps, This is done by writing 1,0 to
    // Divisor Latch registers, but to access these we need to temporarily
    // set the Divisor Latch Access Bit (DLAB) on the LSR register, because
    // the UART has fewer ports than registers...
    write_byte(lcr::LEN_8BIT | lcr::DLAB, regs::LCR);
    write_byte(1, regs::DLL);
    write_byte(0, regs::DLM);
    write_byte(lcr::LEN_8BIT, regs::LCR);

    //  interrupt threshold
    write_byte(0, regs::FCR);

    // disable interrupts
    write_byte(0, regs::IER);

    // Most physical UARTs need the MCR AUX_OUTPUT_2 bit set to 1 for
    // interrupts to be generated. QEMU doesn't bother checking this
    // bit, but interestingly VMWare does, so we must set it.
    write_byte(mcr::AUX_OUTPUT_2, regs::MCR);
}

void isa_serial_console_base::write(const char *str, size_t len)
{
    while (len-- > 0)
        putchar(*str++);
}

bool isa_serial_console_base::input_ready()
{
    u8 val = read_byte(regs::LSR);
    // On VMWare hosts without a serial port, this register always
    // returns 0xff.  Just ignore it instead of spinning incessantly.
    return (val != 0xff && (val & lsr::RECEIVE_DATA_READY));
}

char isa_serial_console_base::readch()
{
    u8 val;
    char letter;

    do {
        val = read_byte(regs::LSR);
    } while (!(val & (lsr::RECEIVE_DATA_READY | lsr::OVERRUN | lsr::PARITY_ERROR | lsr::FRAME_ERROR)));

    letter = read_byte(0);

    return letter;
}

void isa_serial_console_base::putchar(const char ch)
{
    u8 val;

    do {
        val = read_byte(regs::LSR);
    } while (!(val & lsr::TRANSMIT_HOLD_EMPTY));

    write_byte(ch, 0);
}

void isa_serial_console_base::enable_interrupt()
{
    // enable interrupts
    write_byte(1, regs::IER);
}

}
