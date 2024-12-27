/*
 * Copyright (C) 2024 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 *
 * The code below is losely based on the implementation of GICv3
 * in the Unikraft project
 * (see https://github.com/unikraft/unikraft/blob/staging/drivers/ukintctlr/gic/gic-v3.c)
 * -----------------------------------------------------------
 * Copyright (c) 2020, OpenSynergy GmbH. All rights reserved.
 *
 * ARM Generic Interrupt Controller support v3 version
 * based on plat/drivers/gic/gic-v2.c:
 *
 * Authors: Wei Chen <Wei.Chen@arm.com>
 *          Jianyong Wu <Jianyong.Wu@arm.com>
 *
 * Copyright (c) 2018, Arm Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <osv/mmio.hh>
#include <osv/irqlock.hh>
#include <osv/sched.hh>
#include <osv/contiguous_alloc.hh>
#include <osv/ilog2.hh>

#include <algorithm>

#include "processor.hh"
#include "gic-v3.hh"
#include "arm-clock.hh"

#define isb() ({ asm volatile ("isb"); })

namespace gic {

void gic_v3_dist::wait_for_write_complete()
{
    unsigned int val;

    do {
        val = read_reg(gicd_reg::GICD_CTLR);
    } while (val & GICD_CTLR_WRITE_COMPLETE);
}

void gic_v3_dist::disable()
{
    write_reg(gicd_reg::GICD_CTLR, 0);
    wait_for_write_complete();
}

void gic_v3_dist::enable()
{
    write_reg(gicd_reg::GICD_CTLR, GICD_CTLR_ARE_NS |
                        GICD_CTLR_ENABLE_G0 | GICD_CTLR_ENABLE_G1NS);
    wait_for_write_complete();
}

//TODO: In real world the formula below = smp_idx * GICR_STRIDE - is too simple.
//Look at https://github.com/zephyrproject-rtos/zephyr/issues/50330 and
//https://github.com/zephyrproject-rtos/zephyr/commit/68b10e8572d9ad4633a8c9f71e9ebf48bb8019ee to
//come up with a better one
u32 gic_v3_redist::read_at_offset(int smp_idx, u32 offset)
{
    return mmio_getl((mmioaddr_t)_base + smp_idx * GICR_STRIDE + offset);
}

u64 gic_v3_redist::read64_at_offset(int smp_idx, u32 offset)
{
    return mmio_getq((mmioaddr_t)_base + smp_idx * GICR_STRIDE + offset);
}

void gic_v3_redist::write_at_offset(int smp_idx, u32 offset, u32 value)
{
    mmio_setl((mmioaddr_t)_base + smp_idx * GICR_STRIDE + offset, value);
}

void gic_v3_redist::write64_at_offset(int smp_idx, u32 offset, u64 value)
{
    mmio_setq((mmioaddr_t)_base + smp_idx * GICR_STRIDE + offset, value);
}

void gic_v3_redist::wait_for_write_complete()
{
    unsigned int val;

    do {
        val = mmio_getl((mmioaddr_t)_base);
    } while (val & GICD_CTLR_WRITE_COMPLETE);
}

mmu::phys gic_v3_redist::rdbase(int smp_idx, bool pta)
{
    if (pta) {
        return (_base + smp_idx * GICR_STRIDE) >> 16;
    } else {
	u64 typer = read64_at_offset(smp_idx, GICR_TYPER);
	return GICR_TYPER_PROC_NUM(typer);
    }
}

static uint32_t get_cpu_affinity(void)
{
    uint64_t mpidr = processor::read_mpidr();

    uint64_t aff = ((mpidr & MPIDR_AFF3_MASK) >> 8) |
        (mpidr & MPIDR_AFF2_MASK) |
        (mpidr & MPIDR_AFF1_MASK) |
        (mpidr & MPIDR_AFF0_MASK);

    return (uint32_t)aff;
}

u64 gic_v3_its::read_reg64(gic_its_reg reg)
{
    return mmio_getq((mmioaddr_t)_base + (u32)reg);
}

u64 gic_v3_its::read_reg64_at_offset(gic_its_reg reg, u32 offset)
{
    return mmio_getq((mmioaddr_t)_base + (u32)reg + offset);
}

void gic_v3_its::write_reg(gic_its_reg reg, u32 value)
{
    mmio_setl((mmioaddr_t)_base + (u32)reg, value);
}

void gic_v3_its::write_reg64(gic_its_reg reg, u64 value)
{
    mmio_setq((mmioaddr_t)_base + (u32)reg, value);
}

void gic_v3_its::write_reg64_at_offset(gic_its_reg reg, u32 offset, u64 value)
{
    mmio_setq((mmioaddr_t)_base + (u32)reg + offset, value);
}

void gic_v3_its::read_type_register()
{
    _typer = read_reg64(gic_its_reg::GICITS_TYPER);
}

#define GIC_ITS_CMD_QUEUE_SIZE  0x10000 //64 KB
//https://developer.arm.com/documentation/102923/0100/ITS/The-command-queue
void gic_v3_its::initialize_cmd_queue()
{
    _cmd_queue = memory::alloc_phys_contiguous_aligned(GIC_ITS_CMD_QUEUE_SIZE, 0x10000); //Queue needs to be 64KB aligned
    memset(_cmd_queue, 0, GIC_ITS_CMD_QUEUE_SIZE);

    u64 cmd_queue_pa = mmu::virt_to_phys(_cmd_queue);
    u64 queue_size_in_pages = GIC_ITS_CMD_QUEUE_SIZE / mmu::page_size;
    //
    //Read https://developer.arm.com/documentation/ddi0601/2024-09/External-Registers/GITS-CBASER--ITS-Command-Queue-Descriptor
    write_reg64(gic_its_reg::GICITS_CBASER, GITS_CBASER_VALID | cmd_queue_pa | (queue_size_in_pages - 1));
    write_reg64(gic_its_reg::GICITS_CWRITER, 0);
}

void gic_v3_its::enqueue_cmd(its_cmd *cmd)
{
    u64 cread = read_reg64(gic_its_reg::GICITS_CREADR);
    u64 cwrite = read_reg64(gic_its_reg::GICITS_CWRITER);
    //
    //Wait until queue is not full
    while (cread == cwrite + sizeof(*cmd)) {
	__asm __volatile("isb sy"); //Hint it is in a busy loop
        cread = read_reg64(gic_its_reg::GICITS_CREADR);
    }

    its_cmd *cmd_to_write = (its_cmd *)(_cmd_queue + cwrite);
    *cmd_to_write = *cmd;

    cwrite += sizeof(*cmd);
    if (cwrite == GIC_ITS_CMD_QUEUE_SIZE) {
        cwrite = 0;
    }

    write_reg64(gic_its_reg::GICITS_CWRITER, cwrite);
}

void gic_v3_its::cmd_mapd(u32 dev_id, u64 itt_pa, u64 itt_size)
{
    its_cmd cmd;
    cmd.data[0] = ((u64)dev_id << 32) | (u32)gic_its_cmd::ITS_CMD_MAPD;
    cmd.data[1] = itt_size;
    cmd.data[2] = ITS_MAPD_V | itt_pa;
    cmd.data[3] = 0;
    enqueue_cmd(&cmd);
}

void gic_v3_its::cmd_mapti(u32 dev_id, int vector, int smp_idx)
{
    its_cmd cmd;
    cmd.data[0] = ((u64)dev_id << 32) | (u32)gic_its_cmd::ITS_CMD_MAPTI;
    u32 event_id = vector - GIC_LPI_INTS_START;
    cmd.data[1] = ((u64)vector << 32) | event_id;
    cmd.data[2] = smp_idx;
    cmd.data[3] = 0;
    enqueue_cmd(&cmd);
}

void gic_v3_its::cmd_inv(u32 dev_id, int vector)
{
    its_cmd cmd;
    cmd.data[0] = ((u64)dev_id << 32) | (u32)gic_its_cmd::ITS_CMD_INV;
    u32 event_id = vector - GIC_LPI_INTS_START;
    cmd.data[1] = event_id;
    cmd.data[2] = cmd.data[3] = 0;
    enqueue_cmd(&cmd);
}

void gic_v3_its::cmd_discard(u32 dev_id, int vector)
{
    its_cmd cmd;
    cmd.data[0] = ((u64)dev_id << 32) | (u32)gic_its_cmd::ITS_CMD_DISCARD;
    u32 event_id = vector - GIC_LPI_INTS_START;
    cmd.data[1] = event_id;
    cmd.data[2] = cmd.data[3] = 0;
    enqueue_cmd(&cmd);
}

void gic_v3_its::cmd_sync(mmu::phys rdbase)
{
    its_cmd cmd;
    cmd.data[0] = (u64)gic_its_cmd::ITS_CMD_SYNC;
    cmd.data[2] = rdbase << 16;
    cmd.data[1] = cmd.data[3] = 0;
    enqueue_cmd(&cmd);
}

void gic_v3_its::cmd_mapc(int smp_idx, mmu::phys rdbase)
{
    its_cmd cmd;
    cmd.data[0] = (u32)gic_its_cmd::ITS_CMD_MAPC;
    cmd.data[1] = 0;
    cmd.data[2] = ITS_MAPC_V | (rdbase << 16) | smp_idx;
    cmd.data[3] = 0;
    enqueue_cmd(&cmd);
}

void gic_v3_driver::init_lpis(int smp_idx)
{
    if (smp_idx == 0) {
        //Identify number of LPIs supported by GIC
        //Read bits 15:11 (num_LPIs) of GICD_TYPER
        u32 typer = _gicd.read_reg(gicd_reg::GICD_TYPER);
        u32 num_lpis = (typer >> 11) & GICD_TYPER_LPI_NUM_MASK;
        if (num_lpis) { //Not-zero
            _msi_vector_num = 1UL << (num_lpis + 1);
            debug_early_u64("Num_lpis ", num_lpis);
        } else { //Determine using the IDBits field
            u32 id_bits = (typer >> 19) & GICD_TYPER_IDBITS_MASK;
            debug_early_u64("Id_bits ", id_bits);
            _msi_vector_num = (1UL << (id_bits + 1)) - GIC_LPI_INTS_START;
        }
        //TODO: Investigate using smaller number of LPIs using GICR_PROPBASER.IDbits
        //Read https://developer.arm.com/documentation/102923/0100/Redistributors/Initial-configuration-of-a-Redistributor
        //and https://developer.arm.com/documentation/ddi0601/2024-09/External-Registers/GICR-PROPBASER--Redistributor-Properties-Base-Address-Register
        //msi_vector_num = std::max(msi_vector_num, 4096);
        debug_early_u64("Number of LPIs: ", _msi_vector_num);

        //Set up LPI configuration table
        void *config_table = memory::alloc_phys_contiguous_aligned(_msi_vector_num, 4096);
        memset(config_table, 0, _msi_vector_num);
        _lpi_config_table = (u8*)config_table;

        u64 id_bits = ilog2_roundup<u64>(_msi_vector_num + GIC_LPI_INTS_START) - 1; //TODO: Double-check this
        debug_early_u64("ID bits: ", id_bits);
        _lpi_prop_base = mmu::virt_to_phys(config_table) | id_bits;

        //Set up LPI pending table
        size_t pending_table_size = (_msi_vector_num + GIC_LPI_INTS_START) / 8;
        void *pending_table = memory::alloc_phys_contiguous_aligned(pending_table_size, 4096);
        memset(pending_table, 0, pending_table_size);

        //Read about PTZ here - https://developer.arm.com/documentation/ddi0601/2024-12/External-Registers/GICR-PENDBASER--Redistributor-LPI-Pending-Table-Base-Address-Register
        _lpi_pend_base = mmu::virt_to_phys(pending_table) | GICR_PENDBASER_PTZ;
    }

    _gicr.write64_at_offset(smp_idx, GICR_PROPBASER, _lpi_prop_base);
    _gicr.write64_at_offset(smp_idx, GICR_PENDBASER, _lpi_pend_base);

    //Enable LPIs
    _gicr.write_at_offset(smp_idx, GICR_CTLR, GICR_CTLR_EnableLPIs);
}

/* to be called only from the boot CPU */
void gic_v3_driver::init_dist()
{
    _gicd.disable();

    _nr_irqs = _gicd.read_number_of_interrupts();
    if (_nr_irqs > GIC_MAX_IRQ) {
        _nr_irqs = GIC_MAX_IRQ + 1;
    }

    debug_early_u64("_nr_irqs: ", _nr_irqs);

    /* Configure all SPIs as non-secure Group 1 */
    for (unsigned int i = GIC_SPI_BASE; i < _nr_irqs; i += GICD_I_PER_IGROUPRn)
        _gicd.write_reg_at_offset((u32)gicd_reg_irq1::GICD_IGROUPR, 4 * (i >> 5), GICD_DEF_IGROUPRn);

    // Send all SPIs to this cpu
    u64 aff = (uint64_t)get_cpu_affinity();
    u64 irouter_val = GIC_AFF_TO_ROUTER(aff, 0);

    for (unsigned int i = GIC_SPI_BASE; i < _nr_irqs; i++)
        _gicd.write_reg64_at_offset(GICD_IROUTER_BASE, i * 8, irouter_val);

    //
    // Set all SPIs to level-sensitive at the start
    for (unsigned int i = GIC_SPI_BASE; i < _nr_irqs; i += GICD_I_PER_ICFGRn)
        _gicd.write_reg_at_offset((u32)gicd_reg_irq2::GICD_ICFGR, i / 4, GICD_ICFGR_DEF_TYPE);

    // Set priority
    for (unsigned int i = GIC_SPI_BASE; i < _nr_irqs; i += GICD_I_PER_IPRIORITYn) {
        _gicd.write_reg_at_offset((u32)gicd_reg_irq8::GICD_IPRIORITYR, i, GICD_IPRIORITY_DEF);
    }

    // Deactivate and disable all SPIs
    for (unsigned int i = GIC_SPI_BASE; i < _nr_irqs; i += GICD_I_PER_ICACTIVERn) {
        _gicd.write_reg_at_offset((u32)gicd_reg_irq1::GICD_ICACTIVER, i / 8, GICD_DEF_ICACTIVERn);
        _gicd.write_reg_at_offset((u32)gicd_reg_irq1::GICD_ICENABLER, i / 8, GICD_DEF_ICENABLERn);
    }

    _gicd.wait_for_write_complete();

    _gicd.enable();
}

#define __STRINGIFY(x) #x
#define READ_SYS_REG32(reg)                                       ({ \
    u32 v;                                                           \
    asm volatile("mrs %0, " __STRINGIFY(reg) : "=r"(v) :: "memory"); \
    v;                                                               \
})

#define WRITE_SYS_REG32(reg, v)                                            ({ \
    asm volatile("msr " __STRINGIFY(reg) ", %0" :: "r"((u32)(v)) : "memory"); \
})

#define READ_SYS_REG64(reg)                                       ({ \
    u64 v;                                                           \
    asm volatile("mrs %0, " __STRINGIFY(reg) : "=r"(v) :: "memory"); \
    v;                                                               \
})

#define WRITE_SYS_REG64(reg, v)                                            ({ \
    asm volatile("msr " __STRINGIFY(reg) ", %0" :: "r"((u64)(v)) : "memory"); \
})

void gic_v3_driver::init_redist(int smp_idx)
{
    //Grab current cpu mpid and store it in the array
    _mpids_by_smpid[smp_idx] = processor::read_mpidr();

    /* Wake up CPU redistributor */
    u32 val = _gicr.read_at_offset(smp_idx, GICR_WAKER);
    val &= ~GICR_WAKER_ProcessorSleep;
    _gicr.write_at_offset(smp_idx, GICR_WAKER, val);

    /* Poll GICR_WAKER.ChildrenAsleep */
    do {
        val = _gicr.read_at_offset(smp_idx, GICR_WAKER);
    } while ((val & GICR_WAKER_ChildrenAsleep));

    /* Set PPI and SGI to a default value */
    for (unsigned int i = 0; i < GIC_SPI_BASE; i += GICD_I_PER_IPRIORITYn)
        _gicr.write_at_offset(smp_idx, GICR_IPRIORITYR4(i), GICD_IPRIORITY_DEF);

    /* Deactivate SGIs and PPIs as the state is unknown at boot */
    _gicr.write_at_offset(smp_idx, GICR_ICACTIVER0, GICD_DEF_ICACTIVERn);

    /* Disable all PPIs */
    _gicr.write_at_offset(smp_idx, GICR_ICENABLER0, GICD_DEF_PPI_ICENABLERn);

    /* Configure SGIs and PPIs as non-secure Group 1 */
    _gicr.write_at_offset(smp_idx, GICR_IGROUPR0, GICD_DEF_IGROUPRn);

    /* Enable all SGIs */
    _gicr.write_at_offset(smp_idx, GICR_ISENABLER0, GICD_DEF_SGI_ISENABLERn);

    /* Wait for completion */
    _gicr.wait_for_write_complete();

    /* Enable system register access */
    val = READ_SYS_REG32(ICC_SRE_EL1);
    val |= 0x7;
    WRITE_SYS_REG32(ICC_SRE_EL1, val);
    isb();

    /* No priority grouping */
    WRITE_SYS_REG32(ICC_BPR1_EL1, 0);

    /* Set priority mask register */
    WRITE_SYS_REG32(ICC_PMR_EL1, 0xff);

    /* EOI drops priority, DIR deactivates the interrupt (mode 1) */
    WRITE_SYS_REG32(ICC_CTLR_EL1, GICC_CTLR_EL1_EOImode_drop);

    /* Enable Group 1 interrupts */
    WRITE_SYS_REG32(ICC_IGRPEN1_EL1, 1);

    isb();

    //Enable cpu timer on secondary CPU
    if (smp_idx) {
        u32 val = 1UL << (get_timer_irq_id() % GICR_I_PER_ISENABLERn);
        _gicr.write_at_offset(smp_idx, GICR_ISENABLER0, val);
    }
}

//https://developer.arm.com/documentation/102923/0100/ITS/The-sizes-and-layout-of-Collection-and-Device-tables
//"The location and size of the Collection and Device tables is configured
// using the GITS_BASERn registers. Software must allocate memory for
// these tables and configure the GITS_BASERn registers before enabling the ITS."
void gic_v3_driver::init_its_device_or_collection_table(int idx)
{
    //Read https://developer.arm.com/documentation/ddi0601/2024-09/External-Registers/GITS-BASER-n---ITS-Table-Descriptors
    debug_early_u64("ITS table ", idx);
    u32 offset = idx * 8;
    u64 base = _gits.read_reg64_at_offset(gic_its_reg::GICITS_BASER, offset);

    u64 type = GITS_TABLE_TYPE(base); //Bits [58:56]
    if (type != GITS_TABLE_DEVICES_TYPE && type != GITS_TABLE_COLLECTIONS_TYPE) {
        return;
    }

    debug_early_u64("-> base:", base);
    debug_early_u64("-> type:", type);
    //
    //"Software can allocate a flat (single level) table or two-level tables."
    //We allocate a flat table
    u64 page_size_type = GITS_PAGE_SIZE(base); //Bits [9:8]
    debug_early_u64("-> page_size_type:", page_size_type);
    u64 table_size = page_size_type == GITS_TABLE_PAGE_SIZE_4K ? 0x1000 :
	           (page_size_type == GITS_TABLE_PAGE_SIZE_16K ? 0x4000 : 0x10000);

    if (type == GITS_TABLE_DEVICES_TYPE) {
        //TODO: Calculate maximum devices count and save it somewhere
    }

    void *table = memory::alloc_phys_contiguous_aligned(table_size, table_size);
    memset(table, 0, table_size);

    u64 table_pa = mmu::virt_to_phys(table);
    debug_early_u64("-> allocated at phys:", table_pa);
    base = (base & ~GITS_TABLE_BASE_PA_MASK) | table_pa;
    debug_early_u64("-> new base:", base);
    _gits.write_reg64_at_offset(gic_its_reg::GICITS_BASER, offset, GITS_BASER_VALID | base);
}

//https://developer.arm.com/documentation/102923/0100/ITS/Initial-configuration-of-an-ITS
void gic_v3_driver::init_its(int smp_idx)
{
    if (smp_idx == 0) {
        _gits.read_type_register();

        //Initialize the Device and Collection tables
        for (int table_idx = 0; table_idx < GITS_TABLE_NUM_MAX; table_idx++) {
            init_its_device_or_collection_table(table_idx);
        }

        //Initialize command queue
        _gits.initialize_cmd_queue();

        // Enable ITS
        _gits.write_reg(gic_its_reg::GICITS_CTLR, GITS_CTLR_ENABLED);
    }

    //Init per cpu
    mmu::phys rdbase = _gicr.rdbase(smp_idx, _gits.is_typer_pta());
    WITH_LOCK(gic_lock) {
        _gits.cmd_mapc(smp_idx, rdbase);
    }
}

#define GIC_LPI_ENABLE  0x01
void gic_v3_driver::mask_irq(unsigned int irq)
{
    WITH_LOCK(gic_lock) {
        if (irq >= GIC_LPI_INTS_START) {
            _lpi_config_table[irq - GIC_LPI_INTS_START] |= ~GIC_LPI_ENABLE;
        } else if (irq >= GIC_SPI_BASE) {
            u32 val = 1UL << (irq % GICD_I_PER_ICENABLERn);
            _gicd.write_reg_at_offset((u32)gicd_reg_irq1::GICD_ICENABLER, 4 * (irq >> 5), val);
        } else {
            u32 val = 1UL << (irq % GICR_I_PER_ICENABLERn);
            _gicr.write_at_offset(sched::cpu::current()->id, GICR_ICENABLER0, val);
        }
    }
}

void gic_v3_driver::unmask_irq(unsigned int irq)
{
    WITH_LOCK(gic_lock) {
        if (irq >= GIC_LPI_INTS_START) {
           _lpi_config_table[irq - GIC_LPI_INTS_START] |= GIC_LPI_ENABLE;
        } else if (irq >= GIC_SPI_BASE) {
            u32 val = 1UL << (irq % GICD_I_PER_ISENABLERn);
            _gicd.write_reg_at_offset((u32)gicd_reg_irq1::GICD_ISENABLER, 4 * (irq >> 5), val);
        } else {
            u32 val = 1UL << (irq % GICR_I_PER_ISENABLERn);
            _gicr.write_at_offset(sched::cpu::current()->id, GICR_ISENABLER0, val);
        }
    }
}

void gic_v3_driver::set_irq_type(unsigned int id, irq_type type)
{
    //SGIs are always treated as edge-triggered so ignore call for these
    if (id < GIC_PPI_BASE) {
        return;
    }

    WITH_LOCK(gic_lock) {
        auto offset = 4 * ((id) >> 4);
        auto val = _gicd.read_reg_at_offset((u32)gicd_reg_irq2::GICD_ICFGR, offset);
        u32 oldmask = (val >> ((id % GICD_I_PER_ICFGRn) * 2)) & GICD_ICFGR_MASK;

        u32 newmask = oldmask;
        if (type == irq_type::IRQ_TYPE_LEVEL) {
            newmask &= ~GICD_ICFGR_TRIG_MASK;
            newmask |= GICD_ICFGR_TRIG_LVL;
        } else if (type == irq_type::IRQ_TYPE_EDGE) {
            newmask &= ~GICD_ICFGR_TRIG_MASK;
            newmask |= GICD_ICFGR_TRIG_EDGE;
        }

        //Check if nothing changed
        if (newmask == oldmask)
            return;

        // Update to new type
        val &= (~(GICD_ICFGR_MASK << (id % GICD_I_PER_ICFGRn) * 2));
        val |= (newmask << (id % GICD_I_PER_ICFGRn) * 2);
        _gicd.write_reg_at_offset((u32)gicd_reg_irq2::GICD_ICFGR, offset, val);
    }
}

void gic_v3_driver::send_sgi(sgi_filter filter, int smp_idx, unsigned int vector)
{
    assert(smp_idx < max_sgi_cpus);
    assert(vector <= 0x0f);

    //Set vector number in the bits [27:24] - 16 possible values
    u64 sgi_register = vector << ICC_SGIxR_EL1_INTID_SHIFT;

    if (filter == sgi_filter::SGI_TARGET_ALL_BUT_SELF) {
        sgi_register |= ICC_SGIxR_EL1_IRM;
    } else {
        if (filter == sgi_filter::SGI_TARGET_SELF) {
            smp_idx = sched::cpu::current()->id;
        }

        auto mpid = _mpids_by_smpid[smp_idx];
        u64 aff0 = MPIDR_AFF0(mpid);
        sgi_register |= (MPIDR_AFF3(mpid) << ICC_SGIxR_EL1_AFF3_SHIFT) |
                        (MPIDR_AFF2(mpid) << ICC_SGIxR_EL1_AFF2_SHIFT) |
                        (MPIDR_AFF1(mpid) << ICC_SGIxR_EL1_AFF1_SHIFT) |
                        ((aff0 >> 4) << ICC_SGIxR_EL1_RS_SHIFT) | (1 << (aff0 & 0xf));
    }

    //We disable interrupts before taking a lock to prevent scenarios
    //when interrupt arrives after gic_lock is taken and interrupt handler
    //ends up calling send_sgi() (nested example) and stays spinning forever
    //in attempt to take a lock again
    /* Generate interrupt */
    irq_save_lock_type irq_lock;
    WITH_LOCK(irq_lock) {
        WITH_LOCK(gic_lock) {
            WRITE_SYS_REG64(ICC_SGI1R_EL1, sgi_register);
        }
    }
}

unsigned int gic_v3_driver::ack_irq(void)
{
    uint32_t irq;

    irq = READ_SYS_REG32(ICC_IAR1_EL1);
    asm volatile ("dsb sy");

    return irq;
}

void gic_v3_driver::end_irq(unsigned int irq)
{
    /* Lower the priority */
    WRITE_SYS_REG32(ICC_EOIR1_EL1, irq);
    isb();

    /* Deactivate */
    WRITE_SYS_REG32(ICC_DIR_EL1, irq);
    isb();
}

u32 gic_v3_driver::pci_device_id(pci::function* dev)
{
    u8 bus, device, function;
    dev->get_bdf(bus, device, function);
    return (((u32)bus) << 8) | (((u32)device) << 3) | (u32)function;
}

void gic_v3_driver::map_msi_irq(unsigned int vector, pci::function* dev, u32 target_cpu)
{
    WITH_LOCK(gic_lock) {
        u32 device_id = pci_device_id(dev);

        //Read https://developer.arm.com/documentation/102923/0100/ITS/Mapping-an-interrupt-to-a-Redistributor

        //Check if there is an Interrupt Translation Table (ITT) for this device
        //If not create it and map it
        auto dev_itt = _itt_by_device_id.find(device_id);
        if (dev_itt == _itt_by_device_id.end()) {
            u64 entries_num = 1ull << ilog2_roundup<u64>(_msi_vector_num); 
            u64 itt_size = entries_num * (_gits.itt_entry_size() + 1);
            itt_size = std::max(itt_size, (u64)256);

            void *itt = memory::alloc_phys_contiguous_aligned(itt_size, 256);
            memset(itt, 0, itt_size);
            _itt_by_device_id.insert(std::make_pair(device_id, itt));

            u64 itt_pa = mmu::virt_to_phys(itt);
            _gits.cmd_mapd(device_id, itt_pa, ilog2_roundup<u64>(entries_num) - 1);
        }

        //Map event ID to collection ID
        _gits.cmd_mapti(device_id, vector, target_cpu);
        _gits.cmd_inv(device_id, vector);

        //Sync redistributor
        mmu::phys rdbase = _gicr.rdbase(target_cpu, _gits.is_typer_pta());
        _gits.cmd_sync(rdbase);
    }
}

void gic_v3_driver::unmap_msi_irq(unsigned int vector, pci::function* dev)
{
    WITH_LOCK(gic_lock) {
        u32 device_id = pci_device_id(dev);

        _gits.cmd_discard(device_id, vector);
        _gits.cmd_inv(device_id, vector);
        //TODO: Issue CMD_SYNC but needs to know rdbase which needs cpu
    }
}

}
