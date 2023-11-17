/*
 * Copyright (C) 2023 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef ENA_HH
#define ENA_HH

#include <bsd/sys/net/if_var.h>
#include <bsd/sys/net/if.h>

#include "drivers/driver.hh"
#include "drivers/pci-device.hh"

bool ena_probe(pci::device *dev);
int ena_attach(pci::device *dev);

namespace aws {

class ena: public hw_driver {
public:
    explicit ena(pci::device& dev);
    virtual ~ena() {};

    virtual std::string get_name() const { return "ena"; }

    virtual void dump_config(void);
    int transmit(struct mbuf* m_head);

    static hw_driver* probe(hw_device* dev);

    /**
     * Fill the if_data buffer with data from our iface including those that
     * we have gathered by ourselvs (e.g. FP queue stats).
     * @param out_data output buffer
     */
    void fill_stats(struct if_data* out_data) const;

private:
    int _id;
    struct ifnet* _ifn;

    pci::device& _dev;
};

}

#endif
