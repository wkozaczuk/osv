/*
 * Copyright (C) 2023 Jan Braunwarth
 * Copyright (C) 2024 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef NVME_QUEUE_H
#define NVME_QUEUE_H

#include "drivers/pci-device.hh"
#include "drivers/nvme-structs.h"

#include <osv/bio.h>

#define nvme_tag "nvme"
#define nvme_d(...)    tprintf_d(nvme_tag, __VA_ARGS__)
#define nvme_i(...)    tprintf_i(nvme_tag, __VA_ARGS__)
#define nvme_w(...)    tprintf_w(nvme_tag, __VA_ARGS__)
#define nvme_e(...)    tprintf_e(nvme_tag, __VA_ARGS__)

#define NVME_ERROR(...) nvme_e(__VA_ARGS__)

#define NVME_PAGESIZE  4096
#define NVME_PAGESHIFT 12

namespace nvme {

// Template to specify common elements of the submission and completion
// queue as described in the chapter 4.1 of the NVMe specification (see
// "https://www.nvmexpress.org/wp-content/uploads/NVM-Express-1_1a.pdf")
// The type T argument would be either nvme_sq_entry_t or nvme_cq_entry_t.
//
// The _tail, used by the producer, specifies the 0-based index of
// the next free slot to place new entry into the array _addr. After
// placing new entry, the _tail should be incremented - if it exceeds
// queue size, the it should roll to 0.
//
// The _head, used by the consumer, specifies the 0-based index of
// the entry to be fetched of the queue _addr. Likewise, the _head is
// incremented after, and if exceeds queue size, it should roll to 0.
//
// The queue is considered empty, if _head == _tail.
// The queue is considered full, if _head == (_tail + 1)
//
// The _doorbool points to the address where _tail of the submission
// queue is written to. For completion queue, it points to the address
// where the _head value is written to.
template<typename T>
struct queue {
    queue(u32* doorbell) :
        _addr(nullptr), _doorbell(doorbell), _head(0), _tail(0) {}
    T* _addr;
    volatile u32* _doorbell;
    u32 _head;
    u32 _tail;
};

// Pair of submission queue and completion queue - SQ and CQ.
// The work in tandem and share the same size.
class queue_pair
{
public:
    queue_pair(
        int driver_id,
        u32 id,
        int qsize,
        pci::device& dev,
        u32* sq_doorbell,
        u32* cq_doorbell,
        std::map<u32, nvme_ns_t*>& ns
    );

    ~queue_pair();

    u64 sq_phys_addr() { return (u64) mmu::virt_to_phys((void*) _sq._addr); }
    u64 cq_phys_addr() { return (u64) mmu::virt_to_phys((void*) _cq._addr); }

    u16 submit_cmd(std::unique_ptr<nvme_sq_entry_t> cmd);

    virtual void req_done() {};
    void wait_for_completion_queue_entries();
    bool completion_queue_not_empty() const;

    void enable_interrupts();
    void disable_interrupts();

    u32 _id;
protected:
    int _driver_id;

    u32 _qsize;
    pci::device* _dev;

    queue<nvme_sq_entry_t> _sq;
    bool _sq_full;

    queue<nvme_cq_entry_t> _cq;
    int _cq_phase_tag;

    std::map<u32, nvme_ns_t*> _ns;

    std::vector<u64**> _prp_lists_in_use;

    mutex _lock;
    sched::thread_handle _waiter;

    void advance_sq_tail();
    int map_prps(u16 cid, void* data, u64 datasize, u64* prp1, u64* prp2);

    u16 submit_cmd_without_lock(std::unique_ptr<nvme_sq_entry_t> cmd);

    std::unique_ptr<nvme_cq_entry_t> get_completion_queue_entry();
};

class io_queue_pair : public queue_pair {
public:
    io_queue_pair(
        int driver_id,
        int id,
        int qsize,
        pci::device& dev,
        u32* sq_doorbell,
        u32* cq_doorbell,
        std::map<u32, nvme_ns_t*>& ns
    );
    ~io_queue_pair();

    int make_request(struct bio* bio, u32 nsid);
    void req_done();
private:
    std::vector<struct bio**> _pending_bios;
    int submit_rw(u16 cid, void* data, u64 slba, u32 nlb, u32 nsid, int opc);
    int submit_flush();
};

class admin_queue_pair : public queue_pair {
public:
    admin_queue_pair(
        int driver_id,
        int id,
        int qsize,
        pci::device& dev,
        u32* sq_doorbell,
        u32* cq_doorbell,
        std::map<u32, nvme_ns_t*>& ns
    );

    void req_done();
    std::unique_ptr<nvme_cq_entry_t>
    submit_and_return_on_completion(std::unique_ptr<nvme_sq_entry_t> cmd, void* data = nullptr, unsigned int datasize = 0);
private:
    sched::thread_handle _req_waiter;
    std::unique_ptr<nvme_cq_entry_t> _req_res;
    volatile bool new_cq;
};

}

#endif
