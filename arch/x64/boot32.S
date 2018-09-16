# Copyright (C) 2013 Cloudius Systems, Ltd.
# Copyright (C) 2018 Waldemar Kozaczuk
#
# This work is open source software, licensed under the terms of the
# BSD license as described in the LICENSE file in the top-level directory.

#define MULTIBOOT_HEADER_MAGIC  0x1badb002

/* Align all boot modules on i386 page (4KB) boundaries. */
#define MULTIBOOT_PAGE_ALIGN    0x00000001

/* Must pass memory information to OS. */
#define MULTIBOOT_MEMORY_INFO   0x00000002

/* This flag indicates the use of the address fields in the header. */
#define MULTIBOOT_AOUT_KLUDGE   0x00010000

#define MULTIBOOT_HEADER_FLAGS  (MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_AOUT_KLUDGE)

#define OSV_MBKERNEL_BASE (OSV_KERNEL_BASE-0x2000)
.text
.code32
//
// Define data that needs to be found in the beginning of the mbloader.elf
// by multiboot loader to properly load kernel as defined by multiboot
// specification at https://www.gnu.org/software/grub/manual/multiboot/multiboot.html.
multiboot_header_start:
    .long MULTIBOOT_HEADER_MAGIC
    .long MULTIBOOT_HEADER_FLAGS
    .long -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
    .long multiboot_header_start        # header_addr
    .long OSV_MBKERNEL_BASE             # load_addr
    .long 0                             # load_end_addr
    .long 0                             # bss_end_addr
    .long start32                       # entry_addr

mb_copy_address = 0x4000
mb_cmd_line_copy_address = 0x3000
mb_mmap_info_copy_address = 0x2000

mb_cmdline_offset = 16
mb_mmap_addr_offset = 48

mb_tsc1_lo_offset = 88
mb_tsc1_hi_offset = (88 + 4)
mb_tsc_disk_lo_offset = (88 + 8)
mb_tsc_disk_hi_offset = (88 + 12)
mb_uncompress_lo_offset = (88 + 16)
mb_uncompress_hi_offset = (88 + 20)

.global start32
start32:
    //
    // The original multiboot info address gets passed in by the multiboot loader
    // in the ebx register. We need to copy the relevant parts of the data to
    // the other place in memory (mb_copy_address) in case the original data
    // gets overwritten by our code.
    //
    // First set target mmap_address and cmdline in the target multiboot_info_type struct.
    movl $mb_copy_address, %eax
    movl $mb_mmap_info_copy_address, mb_mmap_addr_offset(%eax)
    movl $mb_cmd_line_copy_address, mb_cmdline_offset(%eax)
    //
    // Call utils function written in C to copy/transform
    // memory information and command line
    mov $0x7c00, %esp // Needs stack
    pushl $mb_copy_address  // Pass multiboot info source and target address on stack as parameters
    pushl %ebx
    call copy_multiboot_info
    subl $8, %esp // Restore stack
    //
    // Set bootchart fields
    mov $mb_copy_address, %ebx
    rdtsc
    mov %eax, mb_tsc1_lo_offset(%ebx)
    mov %edx, mb_tsc1_hi_offset(%ebx)
    mov %eax, mb_tsc_disk_lo_offset(%ebx)
    mov %edx, mb_tsc_disk_hi_offset(%ebx)
    mov %eax, mb_uncompress_lo_offset(%ebx)
    mov %edx, mb_uncompress_hi_offset(%ebx)

	lea elf64_header, %eax
	jmp *24+elf64_header

.align 4096

elf64_header:
.incbin "loader-stripped.elf"
