/*
 * Copyright (C) 2018 Waldemar Kozaczuk, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/types.h>
#include "multiboot.h"

#define MULTIBOOT_MMAP_PRESENT_FLAG (0x1 << 6)
#define MULTIBOOT_MEM_PRESENT_FLAG  (0x1 << 0)
#define SINGLE_KB_IN_BYTES (1024)
#define SINGLE_MB_ADDRESS (1024 * SINGLE_KB_IN_BYTES)
#define SPACE (' ')
//
// The copy_multiboot_info is called from assembly code in multiboot.S
// where source and target multiboot_info_type are passed in.
extern "C" void copy_multiboot_info(void *source, void *target) {
    //
    // Copy the relevant fields from original multiboot
    // info struct to the target one
    multiboot_info_type* multiboot_info_source = (multiboot_info_type*)source;
    multiboot_info_type* multiboot_info_target = (multiboot_info_type*)target;
    //
    // Copy the flags just in case it will be useful in future
    multiboot_info_target->flags = multiboot_info_source->flags;
    //
    // Most multiboot loaders (including QEMU) pass memory information
    // in both mmap_* and mem_upper/mem_lower fields but we prefer the former
    // as we do not need to change anything in arch-setup.cc. However some multiboot
    // loaders like HyperKit pass information in mem_upper/mem_lower fields only
    // which we need to translate to mmap_* structures.
    //
    // The arch_setup_free_memory() expects memory information
    // in the mmap format. So either copy the mmap_* data if passed in ...
    if( multiboot_info_source->flags & MULTIBOOT_MMAP_PRESENT_FLAG) {
        char *mmap_source = (char *)multiboot_info_source->mmap_addr;
        char *mmap_target = (char *)multiboot_info_target->mmap_addr;
        auto mmap_bytes_to_copy = multiboot_info_source->mmap_length;
        multiboot_info_target->mmap_length = mmap_bytes_to_copy;

        while( mmap_bytes_to_copy > 0 ) {
            *(mmap_target++) = *(mmap_source++);
            mmap_bytes_to_copy--;
        }
    }
    // ... or translate mem_* data into mmap_*
    else if( multiboot_info_source->flags & MULTIBOOT_MEM_PRESENT_FLAG) {
        multiboot_info_target->mmap_length = 2 * sizeof(struct e820ent);

        e820ent *lower = (e820ent*)(multiboot_info_target->mmap_addr);
        lower->ent_size = sizeof(struct e820ent) - 4;
        lower->addr = 0;
        lower->size = multiboot_info_source->mem_lower * SINGLE_KB_IN_BYTES;
        lower->type = 1;

        e820ent *upper = ((e820ent*)multiboot_info_target->mmap_addr) + 1;
        upper->ent_size = sizeof(struct e820ent) - 4;
        upper->addr = SINGLE_MB_ADDRESS;
        upper->size = multiboot_info_source->mem_upper * SINGLE_KB_IN_BYTES;
        upper->type = 1;
    }
    //
    // Multiboot loaders concatenate path of the kernel with extra arguments (OSv cmdline)
    // with a space in between. So we need to find first space as the beginning of
    // the real cmdline
    char *source_cmdline = (char*)multiboot_info_source->cmdline;
    while (*source_cmdline && *source_cmdline != SPACE)
        source_cmdline++;
    //
    // Copy command line
    char *target_cmdline = (char*)multiboot_info_target->cmdline;
    while (*source_cmdline)
        *(target_cmdline++) = *(source_cmdline++);
}
