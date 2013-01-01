#ifndef EXCEPTIONS_HH
#define EXCEPTIONS_HH

#include <stdint.h>

struct exception_frame {
    typedef unsigned long ulong;
    typedef unsigned short u16;
    ulong r15;
    ulong r14;
    ulong r13;
    ulong r12;
    ulong r11;
    ulong r10;
    ulong r9;
    ulong r8;
    ulong rbp;
    ulong rdi;
    ulong rsi;
    ulong rdx;
    ulong rcx;
    ulong rbx;
    ulong rax;
    u16 error_code;
    ulong rip;
    ulong cs;
    ulong rflags;
    ulong rsp;
    ulong ss;
};

class interrupt_descriptor_table {
public:
    interrupt_descriptor_table();
    void load_on_cpu();
private:
    typedef uint8_t u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
    typedef uint64_t u64;
    enum {
        type_intr_gate = 14,
    };
    enum {
        s_special = 0,
    };
    struct idt_entry {
        u16 offset0;
        u16 selector;
        u8 ist : 3;
        u8 res0 : 5;
        u8 type : 4;
        u8 s : 1;
        u8 dpl : 2;
        u8 p : 1;
        u16 offset1;
        u32 offset2;
        u32 res1;
    } __attribute__((aligned(16)));
    void add_entry(unsigned vec, void (*handler)());
    idt_entry _idt[256];
};

extern interrupt_descriptor_table idt;

extern "C" {
    void page_fault(exception_frame* ef);
}

#endif
