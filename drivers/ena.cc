/*
 * Copyright (C) 2023 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <sys/cdefs.h>

#include "drivers/ena.hh"
#include "drivers/pci-device.hh"

#include <osv/aligned_new.hh>

#include <bsd/sys/net/ethernet.h>

extern bool opt_maxnic;
extern int maxnic;

namespace aws {

#define ena_tag "ena"
#define ena_d(...)   tprintf_d(ena_tag, __VA_ARGS__)
#define ena_i(...)   tprintf_i(ena_tag, __VA_ARGS__)
#define ena_w(...)   tprintf_w(ena_tag, __VA_ARGS__)
#define ena_e(...)   tprintf_e(ena_tag, __VA_ARGS__)

static int if_ioctl(struct ifnet* ifp, u_long command, caddr_t data)
{
    return 0;
}

static void if_qflush(struct ifnet* ifp)
{
}

static int if_transmit(struct ifnet* ifp, struct mbuf* m_head)
{
    return 0;
}

static void if_init(void* xsc)
{
}

static void if_getinfo(struct ifnet* ifp, struct if_data* out_data)
{
    ena* _ena = (ena*)ifp->if_softc;

    // First - take the ifnet data
    memcpy(out_data, &ifp->if_data, sizeof(*out_data));

    // then fill the internal statistics we've gathered
    _ena->fill_stats(out_data);
}

void ena::fill_stats(struct if_data* out_data) const
{
    assert(!out_data->ifi_oerrors && !out_data->ifi_obytes && !out_data->ifi_opackets);
    /*out_data->ifi_ipackets += _rxq[0].stats.rx_packets;
    out_data->ifi_ibytes   += _rxq[0].stats.rx_bytes;
    out_data->ifi_iqdrops  += _rxq[0].stats.rx_drops;
    out_data->ifi_ierrors  += _rxq[0].stats.rx_csum_err;
    out_data->ifi_opackets += _txq[0].stats.tx_packets;
    out_data->ifi_obytes   += _txq[0].stats.tx_bytes;
    out_data->ifi_oerrors  += _txq[0].stats.tx_err + _txq[0].stats.tx_drops;

    out_data->ifi_iwakeup_stats = _rxq[0].stats.rx_wakeup_stats;
    out_data->ifi_owakeup_stats = _txq[0].stats.tx_wakeup_stats;*/
}

ena::ena(pci::device &dev)
    : _dev(dev)
{
    if_initname(_ifn, "eth", _id);
    _ifn->if_mtu = ETHERMTU;
    _ifn->if_softc = static_cast<void*>(this);
    _ifn->if_flags = IFF_BROADCAST | IFF_MULTICAST;
    _ifn->if_ioctl = if_ioctl;
    _ifn->if_transmit = if_transmit;
    _ifn->if_qflush = if_qflush;
    _ifn->if_init = if_init;
    _ifn->if_getinfo = if_getinfo;
    //IFQ_SET_MAXLEN(&_ifn->if_snd, VMXNET3_MAX_TX_NDESC);


    _ifn->if_capabilities = IFCAP_RXCSUM | IFCAP_TXCSUM;
    _ifn->if_capabilities |= IFCAP_TSO4;
    _ifn->if_capabilities |= IFCAP_LRO;
    _ifn->if_hwassist = CSUM_TCP | CSUM_UDP | CSUM_TSO;
    _ifn->if_capenable = _ifn->if_capabilities | IFCAP_HWSTATS;
}

void ena::dump_config(void)
{
    u8 B, D, F;
    _dev.get_bdf(B, D, F);

    _dev.dump_config();
    ena_d("%s [%x:%x.%x] vid:id= %x:%x", get_name().c_str(),
        (u16)B, (u16)D, (u16)F,
        _dev.get_vendor_id(),
        _dev.get_device_id());
}

int ena::transmit(struct mbuf *m_head)
{
    //return _txq[0].transmit(m_head);
    return 0;
}

hw_driver* ena::probe(hw_device* dev)
{
    try {
        if (auto pci_dev = dynamic_cast<pci::device*>(dev)) {
            pci_dev->dump_config();
            if (ena_probe(pci_dev)) {
                if (opt_maxnic && maxnic-- <= 0) {
                    return nullptr;
                } else {
                    return aligned_new<ena>(*pci_dev);
                }
            }
        }
    } catch (std::exception& e) {
        ena_e("Exception on device construction: %s", e.what());
    }
    return nullptr;
}

}
