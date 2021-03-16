/*
 * Copyright (C) 2021 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "pl031.hh"
#include <osv/mmu.hh>

#define rtc_reg(ADDR,REG_OFFSET) (*(volatile u32 *)(ADDR+REG_OFFSET))

pl031::pl031(u64 address)
{
    _address = address;
    mmu::linear_map((void *)_address, _address, 0x1000, mmu::page_size,
                    mmu::mattr::dev);
}

uint64_t pl031::wallclock_ns()
{
    if( rtc_reg(_address, RTCPeriphID0) == RTCPeriphID0_val &&
        rtc_reg(_address, RTCPeriphID1) == RTCPeriphID1_val &&
       (rtc_reg(_address, RTCPeriphID2) & RTCPeriphID2_mask) == RTCPeriphID2_val &&
        rtc_reg(_address, RTCPeriphID3) == RTCPeriphID3_val &&
        rtc_reg(_address, RTCPCellID0) == RTCPCellID0_val &&
        rtc_reg(_address, RTCPCellID1) == RTCPCellID1_val &&
        rtc_reg(_address, RTCPCellID2) == RTCPCellID2_val &&
        rtc_reg(_address, RTCPCellID3) == RTCPCellID3_val) {
	//debug_early_u64("pl031: detected at: ", _address);
	u64 seconds = rtc_reg(_address, RTCDR);
	//debug_early_u64("pl031: seconds: ", seconds);
	return seconds * 1000000000;
    } else {
	debug_early("pl031: could no detect!");
	return 0;
    }
}
