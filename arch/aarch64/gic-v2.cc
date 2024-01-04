
void gic_v2_dist::disable()
{
    unsigned int gicd_ctlr = read_reg(gicd_reg::GICD_CTLR);
    gicd_ctlr &= ~1;
    write_reg(gicd_reg::GICD_CTLR, gicd_ctlr);
}

/* to be called only from the boot CPU */
void gic_v2_driver::init_dist(int smp_idx)
{
    _gicd.disable();

    _nr_irqs = _gicd.read_number_of_interrupts();
    if (_nr_irqs > GIC_MAX_IRQ) {
        _nr_irqs = GIC_MAX_IRQ + 1;
    }

    // Send all SPIs to all cpus
    for (unsigned int i = GIC_SPI_BASE; i < this->nr_irqs; i += 4) {
        this->gicd.write_reg_raw((u32)gicd_reg_irq8::GICD_ITARGETSR, i,
                                     mask); //TODO All
    }

    // Set all SPIs to level-sensitive at the start
    for (unsigned int i = GIC_SPI_BASE; i < this->nr_irqs; i += 16)
        this->gicd.write_reg_raw((u32)gicd_reg_irq2::GICD_ICFGR, i / 4, 0);

    // Set priority
    for (unsigned int i = GIC_SPI_BASE; i < this->nr_irqs; i += 4) {
        this->gicd.write_reg_raw((u32)gicd_reg_irq8::GICD_IPRIORITYR, i,
                                 0xc0c0c0c0);
    }

    // Deactivate and disable all SPIs
    for (unsigned int i = GIC_SPI_BASE; i < this->nr_irqs; i += 32) {
        this->gicd.write_reg_raw((u32)gicd_reg_irq1::GICD_ICACTIVER, i / 8,
                                 0xffffffff); //TODO
        this->gicd.write_reg_raw((u32)gicd_reg_irq1::GICD_ICENABLER, i / 8,
                                 0xffffffff);
    }

    _gicd.enable();
}
