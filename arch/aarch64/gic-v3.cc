/*
 * Copyright (C) 2023 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/mmio.hh>
//#include <osv/sched.hh>

#include "processor.hh"
#include "gic-v3.hh"
#include "arm-clock.hh"

#define isb() ({ asm volatile ("isb"); })

namespace gic {

class gic_v3_driver *gic;

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

u32 gic_v3_redist::read_at_offset(int smp_idx, u32 offset)
{
    return mmio_getl((mmioaddr_t)_base + smp_idx * GICR_STRIDE + offset);
}

void gic_v3_redist::write_at_offset(int smp_idx, u32 offset, u32 value)
{
    mmio_setl((mmioaddr_t)_base + smp_idx * GICR_STRIDE + offset, value);
}

void gic_v3_redist::wait_for_write_complete()
{
    unsigned int val;

    do {
        val = mmio_getl((mmioaddr_t)_base);
    } while (val & GICD_CTLR_WRITE_COMPLETE);
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

/* to be called only from the boot CPU */
void gic_v3_driver::init_dist()
{
    _gicd.disable();

    _nr_irqs = _gicd.read_number_of_interrupts();
    if (_nr_irqs > GIC_MAX_IRQ) {
        _nr_irqs = GIC_MAX_IRQ + 1;
    }

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

    //Enable cpu timer
    if (smp_idx) {
        u32 val = 1UL << (get_timer_irq_id() % GICR_I_PER_ISENABLERn);
        _gicr.write_at_offset(smp_idx, GICR_ISENABLER0, val);
    }
}

void gic_v3_driver::mask_irq(unsigned int irq)
{
    WITH_LOCK(gic_lock) {
        if (irq >= GIC_SPI_BASE) {
            u32 val = 1UL << (irq % GICD_I_PER_ICENABLERn);
            _gicd.write_reg_at_offset((u32)gicd_reg_irq1::GICD_ICENABLER, irq / 8, val);
         } else {
            u32 val = 1UL << (irq % GICR_I_PER_ICENABLERn);
            _gicr.write_at_offset(0, GICR_ICENABLER0, val); //TODO: sched::cpu::current()->id;
         }
    }
}

void gic_v3_driver::unmask_irq(unsigned int irq)
{
    WITH_LOCK(gic_lock) {
        /*if (irq >= GIC_SPI_BASE) {
            uint64_t aff = (uint64_t)get_cpu_affinity();
            uint64_t irouter_val = GIC_AFF_TO_ROUTER(aff, 0);

            write_gicd64(GICD_IROUTER(irq), irouter_val);
            uk_pr_debug("IRQ %d routed to 0x%lx (REG: 0x%lx)\n",
                    irq, aff, irouter_val);
        }*/
        if (irq >= GIC_SPI_BASE) {
            u32 val = 1UL << (irq % GICD_I_PER_ISENABLERn);
            _gicd.write_reg_at_offset((u32)gicd_reg_irq1::GICD_ISENABLER, irq / 8, val);
        } else {
            u32 val = 1UL << (irq % GICR_I_PER_ISENABLERn);
            _gicr.write_at_offset(0, GICR_ISENABLER0, val); //TODO: sched::cpu::current()->id;
        }
    }
}

void gic_v3_driver::set_irq_type(unsigned int id, irq_type type)
{
    WITH_LOCK(gic_lock) {
        if (type == irq_type::IRQ_TYPE_EDGE) {
            auto val = _gicd.read_reg_at_offset((u32)gicd_reg_irq2::GICD_ICFGR, id / 4);
            _gicd.write_reg_at_offset((u32)gicd_reg_irq2::GICD_ICFGR, id / 4, val | 2);
        }
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
            smp_idx = 0;//TODO sched::cpu::current()->id;
        }

        auto mpid = _mpids_by_smpid[smp_idx];
        u64 aff0 = MPIDR_AFF0(mpid);
        sgi_register |= (MPIDR_AFF3(mpid) << ICC_SGIxR_EL1_AFF3_SHIFT) |
                        (MPIDR_AFF2(mpid) << ICC_SGIxR_EL1_AFF2_SHIFT) |
                        (MPIDR_AFF1(mpid) << ICC_SGIxR_EL1_AFF1_SHIFT) |
                        ((aff0 >> 4) << ICC_SGIxR_EL1_RS_SHIFT) | (1 << (aff0 & 0xf));
        /*TODO Check why Unikraft checks these two registers to see if it can send sgi
        control_register_rss = READ_SYS_REG64(ICC_CTLR_EL1) & (1 << 18);
        type_register_rss =  read_gicd32(GICD_TYPER)  & (1 << 26);
        if (control_register_rss == 1 && type_register_rss == 1) {
            ...
        */
    }

    /* Generate interrupt */
    WITH_LOCK(gic_lock) {
        WRITE_SYS_REG64(ICC_SGI1R_EL1, sgi_register);
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

}
