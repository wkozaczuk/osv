/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * Copyright (C) 2014 Huawei Technologies Duesseldorf GmbH
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/drivers_config.h>
#include <osv/kernel_config_logger_debug.h>
#include "arch-setup.hh"
#include <osv/sched.hh>
#include <osv/mempool.hh>
#include <osv/elf.hh>
#include <osv/types.h>
#include <string.h>
#include <osv/boot.hh>
#include <osv/debug.hh>
#include <osv/commands.hh>
#if CONF_drivers_xen
#include <osv/xen.hh>
#endif

#include "arch-mmu.hh"
#include "arch-dtb.hh"
#include "gic-v2.hh"
#include "gic-v3.hh"

#include "drivers/console.hh"
#include "drivers/pl011.hh"
#include "drivers/acpi.hh"
#include "early-console.hh"
#if CONF_drivers_pci
#include <osv/pci.hh>
#endif
#include "drivers/mmio-isa-serial.hh"

#include <alloca.h>
#include "drivers/acpi.hh"

#include <osv/kernel_config_networking_stack.h>

char *cmdline;

void setup_temporary_phys_map()
{
    // duplicate 1:1 mapping into the lower part of phys_mem
    u64 *pt_ttbr0 = reinterpret_cast<u64*>(processor::read_ttbr0());
    for (auto&& area : mmu::identity_mapped_areas) {
        auto base = reinterpret_cast<void*>(get_mem_area_base(area));
        pt_ttbr0[mmu::pt_index(base, 3)] = pt_ttbr0[0];
    }
    mmu::flush_tlb_all();
}

#if CONF_drivers_pci
#define DEV_BASE_PCIE_MMIO_ADDR    0x10000000
#define DEV_BASE_PCIE_MMIO_SIZE    0x2eff0000
#define DEV_BASE_PCIE_PIO_ADDR     0x3eff0000
#define DEV_BASE_PCIE_PIO_SIZE     0x10000
void arch_setup_pci()
{
    u64 ecam_addr = acpi::get_pci_ecam();
    pci::set_pci_ecam(ecam_addr != 0);

    /* linear_map [TTBR0 - PCI config space] */
    u64 pci_cfg = ecam_addr;
    size_t pci_cfg_len = 0x01000000; //TODO: This is what is in QEMU, where exactly can we get it from ACPI or is it a default?
    /*if (!dtb_get_pci_cfg(&pci_cfg, &pci_cfg_len)) {
        return;
    }*/

    pci::set_pci_cfg(pci_cfg, pci_cfg_len);
    pci_cfg = pci::get_pci_cfg(&pci_cfg_len);
    mmu::linear_map((void *)pci_cfg, (mmu::phys)pci_cfg, pci_cfg_len,
		    "pci_cfg", mmu::page_size, mmu::mattr::dev);

    /* linear_map [TTBR0 - PCI I/O and memory ranges] */
    u64 ranges[2]; size_t ranges_len[2];
    /*if (!dtb_get_pci_ranges(ranges, ranges_len, 2)) {
        abort("arch-setup: failed to get PCI ranges.\n");
    }*/

    ranges[0] = DEV_BASE_PCIE_PIO_ADDR; //See if that is a standard place or maybe it is in the same place as 
    ranges_len[0] = DEV_BASE_PCIE_PIO_SIZE;

    ranges[1] = DEV_BASE_PCIE_MMIO_ADDR;
    ranges_len[1] = DEV_BASE_PCIE_MMIO_SIZE;

    pci::set_pci_io(ranges[0], ranges_len[0]);
    pci::set_pci_mem(ranges[1], ranges_len[1]);
    ranges[0] = pci::get_pci_io(&ranges_len[0]);
    ranges[1] = pci::get_pci_mem(&ranges_len[1]);
    mmu::linear_map((void *)ranges[0], (mmu::phys)ranges[0], ranges_len[0],
                    "pci_io", mmu::page_size, mmu::mattr::dev);
    mmu::linear_map((void *)ranges[1], (mmu::phys)ranges[1], ranges_len[1],
                    "pci_mem", mmu::page_size, mmu::mattr::dev);
}
#endif

struct efi_memory_descriptor {
        u32 type;
        u32 pad;
        u64 physical_start;
        u64 virtual_start;
        u64 pages;
        u64 attributes;
};

enum efi_memory_type {
        EFI_RESERVED_MEMORY_TYPE,
        EFI_LOADER_CODE,
        EFI_LOADER_DATA,
        EFI_BOOT_SERVICES_CODE,
        EFI_BOOT_SERVICES_DATA,
        EFI_RUNTIME_SERVICES_CODE,
        EFI_RUNTIME_SERVICES_DATA,
        EFI_CONVENTIAL_MEMORY,
        EFI_UNUSABLE_MEMORY,
        EFI_ACPI_RECLAIM_MEMORY,
        EFI_ACPI_MEMORY_NVS,
        EFI_MEMORY_MAPPED_IO,
        EFI_MEMORY_MAPPED_IO_PORT_SPACE,
        EFI_PAL_CODE,
        EFI_PERSISTENT_MEMORY,
        EFI_MAX_MEMORY_TYPE,
};

void *memory_map = 0;
size_t mmap_size;
size_t mmap_descriptor_size = 0;

u64 phys_start = 0;
size_t phys_size = 0;

static void acpi_discover_memory()
{
    debug_early_u64("memory map      : ", (u64)memory_map);
    debug_early_u64("memory map size : ", mmap_size);
    debug_early_u64("desc size       : ", mmap_descriptor_size);

    u32 i = 0;
    u32 desc_num = mmap_size / mmap_descriptor_size;

    u64 range_start = 0;
    size_t range_size = 0;
    size_t total_size = 0;
    u64 last_phys = 0;
    u64 phys_end = 0;

    //TODO: Should we sort or assume sorted?
    //TODO: Truncate kernel ELF from any memory range 
    for (; i < desc_num; i++) {
        struct efi_memory_descriptor* desc = (struct efi_memory_descriptor*)(memory_map + i * mmap_descriptor_size);
        u32 type = desc->type;
	if (type != EFI_LOADER_CODE && type != EFI_LOADER_DATA && type != EFI_BOOT_SERVICES_CODE && type != EFI_BOOT_SERVICES_DATA && type != EFI_CONVENTIAL_MEMORY) continue;
	
	if (!range_start) {
            phys_start = range_start = desc->physical_start;
	    range_size = desc->pages * 4096;
	} else {
	    if ((range_start + range_size) == desc->physical_start) {
		range_size += (desc->pages * 4096);
            } else {
		if (desc->physical_start <= last_phys) {
			debug_early("UNSORTED range\n");
		}
                debug_early_u64("Range start: ", range_start);
                debug_early_u64("Range size:  ", range_size);
                mmu::free_initial_memory_range(range_start, range_size);

                range_start = desc->physical_start;
	        range_size = desc->pages * 4096;
	    }
        }
	last_phys = desc->physical_start;
	phys_end = desc->physical_start + (desc->pages * 4096);
	total_size += (desc->pages * 4096);
    }

    //phys_size = (phys_end - phys_start) + 0x40000000;
    phys_size = phys_end - phys_start;
    debug_early_u64("Last range start: ", range_start);
    debug_early_u64("Last range size:  ", range_size);
    debug_early_u64("Total size:       ", total_size);
    debug_early_u64("Phys start:       ", phys_start);
    debug_early_u64("Phys size:        ", phys_size);
}

static void detect_kernel_elf()
{
    mmu::mem_addr = 0x40000000;
    memory::phys_mem_size = 0x80000000;

    register u64 edata;
    asm volatile ("adrp %0, .edata" : "=r"(edata));

    /* import from loader.cc and core/mmu.cc */
    extern elf::Elf64_Ehdr *elf_header;
    extern size_t elf_size;
    extern void *elf_start;
    extern u64 kernel_vm_shift;

    mmu::elf_phys_start = reinterpret_cast<void *>(elf_header);
    debug_early_u64("elf phys   : ", (u64)mmu::elf_phys_start);
    debug_early_u64("vm_shift   : ", kernel_vm_shift);
    elf_start = mmu::elf_phys_start + kernel_vm_shift;
    elf_size = (u64)edata - (u64)elf_start;

    /* remove amount of memory used for ELF from avail memory */
    mmu::phys addr = (mmu::phys)mmu::elf_phys_start + elf_size;
    memory::phys_mem_size -= addr - mmu::mem_addr;
}

extern bool opt_pci_disabled;
void arch_setup_free_memory()
{
    setup_temporary_phys_map();
    detect_kernel_elf();
    acpi_discover_memory();

    /* import from loader.cc */
    extern size_t elf_size;
    extern elf::Elf64_Ehdr* elf_header;

    mmu::phys before_range_start = mmu::mem_addr;
    size_t before_range_size = (mmu::phys)elf_header -mmu::mem_addr - 0x10000;
    debug_early_u64("before_range_start: ", before_range_start);
    debug_early_u64("before_range_size:  ", before_range_size);
    //mmu::free_initial_memory_range(before_range_start, before_range_size);

    mmu::phys after_range_start = (mmu::phys)elf_header + elf_size;
    //mmu::phys start;
    size_t phys_memory_size = 0x40000000;//dtb_get_phys_memory(&start);
    size_t after_range_size = phys_memory_size - before_range_size - elf_size;
    debug_early_u64("after_range_start : ", after_range_start);
    debug_early_u64("after_range_size:   ", after_range_size);

    mmu::phys addr = (mmu::phys)elf_header + elf_size;
    debug_early_u64("addr              : ", addr);
    debug_early_u64("phys_mem_size:      ", memory::phys_mem_size);
    //mmu::free_initial_memory_range(after_range_start, after_range_size);

    /* linear_map [TTBR1] */
    for (auto&& area : mmu::identity_mapped_areas) {
        auto base = reinterpret_cast<void*>(get_mem_area_base(area));
        mmu::linear_map(base + phys_start, phys_start, phys_size,
            area == mmu::mem_area::main ? "main" :
            area == mmu::mem_area::page ? "page" : "mempool");
    }

    /* linear_map [TTBR0 - boot, DTB and ELF] */
    /* physical memory layout - relative to the 2MB-aligned address PA stored in mmu::mem_addr
       PA +     0x0 - PA + 0x80000: boot
       PA + 0x80000 - PA + 0x90000: DTB copy
       PA + 0x90000 -       [addr]: kernel ELF */
    //mmu::linear_map((void *)(OSV_KERNEL_VM_BASE - 0x80000), (mmu::phys)mmu::mem_addr,
    //                addr - mmu::mem_addr, "kernel"); //Direct QEMU works
    debug_early_u64("OSV_KERNEL_VM_BASE   :", OSV_KERNEL_VM_BASE);
    debug_early_u64("elf_header - 0x10000 :", ((mmu::phys)elf_header) - 0x10000);
    mmu::linear_map((void *)(OSV_KERNEL_VM_BASE - 0x80000), ((mmu::phys)elf_header) - 0x90000, //Both direct QEMU and efi works
                    elf_size + 0x90000, "kernel");
    debug_early_u64("OSV_KERNEL_VM_BASE + size :", OSV_KERNEL_VM_BASE + elf_size + 0x10000);

    if (console::PL011_Console::active) {
        // linear_map [TTBR0 - UART]
        addr = (mmu::phys)console::aarch64_console.pl011.get_base_addr();
        mmu::linear_map((void *)addr, addr, 0x1000, "pl011", mmu::page_size,
                        mmu::mattr::dev);
    }

#if CONF_drivers_cadence
//    if (console::Cadence_Console::active) {
//        // linear_map [TTBR0 - UART]
//        addr = (mmu::phys)console::aarch64_console.cadence.get_base_addr();
//        mmu::linear_map((void *)addr, addr, 0x1000, "cadence", mmu::page_size,
//                        mmu::mattr::dev);
//    }
#endif

    // get rid of the command line, before memory is unmapped
    console::mmio_isa_serial_console::clean_cmdline(cmdline);
    osv::parse_cmdline(cmdline);

#if CONF_drivers_mmio
    //dtb_collect_parsed_mmio_virtio_devices(); //TODO
#endif

    //mmu::free_initial_memory_range(before_range_start, before_range_size);
    //mmu::free_initial_memory_range(after_range_start, after_range_size);
    mmu::switch_to_runtime_page_tables();

    console::mmio_isa_serial_console::memory_map();
    debug_early("arch_setup_free_memory: end\n");

    acpi::early_init();
    //
    //Locate GICv2 or GICv3 information in DTB and construct corresponding GIC driver
    //and map relevant physical memory
    u64 dist, redist, cpuif;
    size_t dist_len, redist_len, cpuif_len;
    if (acpi::get_gic_v3(&dist, &dist_len, &redist, &redist_len)) {
        gic::gic = new gic::gic_v3_driver(dist, redist);
        /* linear_map [TTBR0 - GIC REDIST] */
        mmu::linear_map((void *)redist, (mmu::phys)redist, redist_len, "gic_redist", mmu::page_size,
                        mmu::mattr::dev);
	debug_early("Enabled GIC3\n");
    } else if (acpi::get_gic_v2(&dist, &dist_len, &cpuif, &cpuif_len)) {
        gic::gic = new gic::gic_v2_driver(dist, cpuif);
        /* linear_map [TTBR0 - GIC CPUIF] */
        mmu::linear_map((void *)cpuif, (mmu::phys)cpuif, cpuif_len, "gic_cpuif", mmu::page_size,
                        mmu::mattr::dev);
	debug_early("Enabled GIC2\n");
    } else {
        abort("arch-setup: failed to get GICv3 nor GiCv2 information from dtb.\n");
    }
    /* linear_map [TTBR0 - GIC DIST] */
    mmu::linear_map((void *)dist, (mmu::phys)dist, dist_len, "gic_dist", mmu::page_size,
                    mmu::mattr::dev);

#if CONF_drivers_pci
    if (!opt_pci_disabled) {
        arch_setup_pci();
    }
#endif
}

void arch_setup_tls(void *tls, const elf::tls_data& info)
{
    struct thread_control_block *tcb;
    memset(tls, 0, sizeof(*tcb) + info.size);

    tcb = (thread_control_block *)tls;
    tcb[0].tls_base = &tcb[1];

    memcpy(&tcb[1], info.start, info.filesize);
    asm volatile ("msr tpidr_el0, %0; msr tpidr_el1, %0; isb; " :: "r"(tcb) : "memory");

    /* check that the tls variable preempt_counter is correct */
    assert(sched::get_preempt_counter() == 1);
}

void arch_init_premain()
{
}

#include "drivers/driver.hh"
#if CONF_drivers_virtio
#include "drivers/virtio.hh"
#include "drivers/virtio-mmio.hh"
#endif
#if CONF_drivers_virtio_rng
#include "drivers/virtio-rng.hh"
#endif
#if CONF_drivers_virtio_blk
#include "drivers/virtio-blk.hh"
#endif
#if CONF_networking_stack
#if CONF_drivers_virtio_net
#include "drivers/virtio-net.hh"
#endif
#endif
#if CONF_drivers_virtio_fs
#include "drivers/virtio-fs.hh"
#endif

void arch_init_drivers()
{
    extern boot_time_chart boot_time;

#if CONF_drivers_pci
    if (!opt_pci_disabled) {
        /*int irqmap_count = dtb_get_pci_irqmap_count(); //With acpi (only) when on QEMU with ACPI and MSI off
        if (irqmap_count > 0) {
            u32 mask = dtb_get_pci_irqmask();
            u32 *bdfs = (u32 *)alloca(sizeof(u32) * irqmap_count);
            int *irqs  = (int *)alloca(sizeof(int) * irqmap_count);
            if (!dtb_get_pci_irqmap(bdfs, irqs, irqmap_count)) {
                abort("arch-setup: failed to get PCI irqmap.\n");
            }
            pci::set_pci_irqmap(bdfs, irqs, irqmap_count, mask);
        }*/

//#if CONF_logger_debug
        //pci::dump_pci_irqmap();
//#endif

        // Enumerate PCI devices
        size_t pci_cfg_len;
        if (pci::get_pci_cfg(&pci_cfg_len)) {
            pci::pci_device_enumeration();
            boot_time.event("pci enumerated");
        }
    }
#endif

#if CONF_drivers_mmio
    // Register any parsed virtio-mmio devices
    virtio::register_mmio_devices(device_manager::instance());
#endif

    // Initialize all drivers
    hw::driver_manager* drvman = hw::driver_manager::instance();
#if CONF_drivers_virtio_rng
    drvman->register_driver(virtio::rng::probe);
#endif
#if CONF_drivers_virtio_blk
    drvman->register_driver(virtio::blk::probe);
#endif
#if CONF_networking_stack
#if CONF_drivers_virtio_net
    drvman->register_driver(virtio::net::probe);
#endif
#endif
#if CONF_drivers_virtio_fs
    drvman->register_driver(virtio::fs::probe);
#endif
    boot_time.event("drivers probe");
    drvman->load_all();
    drvman->list_drivers();
}

void arch_init_early_console()
{
    console::mmio_isa_serial_console::_phys_mmio_address = 0;

#if CONF_drivers_xen
    if (is_xen()) {
        new (&console::aarch64_console.xen) console::XEN_Console();
        console::arch_early_console = console::aarch64_console.xen;
        return;
    }
#endif

    u8 spcr_type = 0;
    u64 spsc_addr = 0;//acpi::get_spcr_addr(spcr_type);

    //int irqid;
    //u64 mmio_serial_address = dtb_get_mmio_serial_console(&irqid);
    //if (mmio_serial_address) {
    if (acpi::is_serial_16550(spcr_type) && spsc_addr) {
        console::mmio_isa_serial_console::early_init(spsc_addr);

        new (&console::aarch64_console.isa_serial) console::mmio_isa_serial_console();
        //console::aarch64_console.isa_serial.set_irqid(irqid);
        console::arch_early_console = console::aarch64_console.isa_serial;
        return;
    }

#if CONF_drivers_cadence
/*    mmio_serial_address = dtb_get_cadence_uart(&irqid);
    if (mmio_serial_address) {
        new (&console::aarch64_console.cadence) console::Cadence_Console();
        console::arch_early_console = console::aarch64_console.cadence;
        console::aarch64_console.cadence.set_base_addr(mmio_serial_address);
        console::aarch64_console.cadence.set_irqid(irqid);
        console::Cadence_Console::active = true;
        return;
    }*/
#endif

    new (&console::aarch64_console.pl011) console::PL011_Console();
    console::arch_early_console = console::aarch64_console.pl011;
    console::PL011_Console::active = true;
    //u64 addr = dtb_get_uart(&irqid);
    //if (!addr) {
        // keep using default addresses
    //    return;
    //}

    //if (spsc_addr)
    //    console::aarch64_console.pl011.set_base_addr(spsc_addr);
    //console::aarch64_console.pl011.set_irqid(irqid);
}

bool arch_setup_console(std::string opt_console)
{
    if (opt_console.compare("pl011") == 0) {
        console::console_driver_add(&console::arch_early_console);
    } else if (opt_console.compare("all") == 0) {
        console::console_driver_add(&console::arch_early_console);
    } else {
        return false;
    }
    return true;
}
