/*
 * Copyright (C) 2018 Waldemar Kozaczuk, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */
#ifndef MULTIBOOT_H
#define MULTIBOOT_H

// This structure and e820ent below is defined per multiboot specification
// described at https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
struct multiboot_info_type {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
    u32 drives_length;
    u32 drives_addr;
    u32 config_table;
    u32 boot_loader_name;
    u32 apm_table;
    u32 vbe_control_info;
    u32 vbe_mode_info;
    u16 vbe_mode;
    u16 vbe_interface_seg;
    u16 vbe_interface_off;
    u16 vbe_interface_len;
} __attribute__((packed));

//
// This structure e820ent is referenced when compiling to 32-bit and 64-bit machine code.
// Therefore we need to define this type explicitly instead of using standard u64
// so that in both situations the compiler calculates offsets of relevant fields
// and size of e820ent struct correctly.
typedef unsigned long long multiboot_uint64_t;

struct e820ent {
    u32 ent_size;
    multiboot_uint64_t addr;
    multiboot_uint64_t size;
    u32 type;
} __attribute__((packed));

#endif //MULTIBOOT_H
