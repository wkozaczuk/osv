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

    u16 submit_cmd(nvme_sq_entry_t* cmd);

    virtual void req_done() {};
    void wait_for_completion_queue_entries();
    bool completion_queue_not_empty() const;

    void enable_interrupts();
    void disable_interrupts();

    u32 _id;
protected:
    void advance_sq_tail();
    void advance_cq_head();

    void map_prps(nvme_sq_entry_t* cmd, void* data, u64 datasize);

    u16 submit_cmd_without_lock(nvme_sq_entry_t* cmd);

    nvme_cq_entry_t* get_completion_queue_entry();

    int _driver_id;

    // Length of the CQ and SQ
    // Admin queue is 8 entries long, therefore occupies 640 bytes (8 * (64 + 16))
    // I/O queue is normally 256 entries long, therefore occupies 20K (256 * (64 + 16))
    u32 _qsize;

    pci::device* _dev;

    // Submission Queue (SQ) - each entry is 64 bytes in size
    queue<nvme_sq_entry_t> _sq;
    bool _sq_full;
    sched::thread_handle _sq_full_waiter;

    // Completion Queue (CQ) - each entry is 16 bytes in size
    queue<nvme_cq_entry_t> _cq;
    int _cq_phase_tag;

    // Map of namespaces (for now there would normally be one entry keyed by 1)
    std::map<u32, nvme_ns_t*> _ns;

    // PRP stands for Physical Region Page and is used to specify locations in
    // physical memory for data tranfers. In essence, they are arrays of physical
    // addresses of pages to read from or write to data.
    // We use _prp_lists_in_use to track those PRP arrays as we allocate
    // them as needed and free them once the read or write request is done.
    std::vector<u64**> _prp_lists_in_use;

    mutex _lock;
};

// Pair of SQ and CQ queues used for reading from and writing to (I/O)
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
    u16 submit_read_write_cmd(u16 cid, u32 nsid, int opc, u64 slba, u32 nlb, void* data);
    u16 submit_flush_cmd(u16 cid, u32 nsid);

    // Vector of arrays of pointers to struct bio used to track bio associated
    // with given command. The scheme (similar to the one used with _prp_lists_in_use)
    // to generate 16-bit 'cid' is - _sq._tail + N * qsize - where N is typically 0 and
    // is equal to a row in _pending_bios and _sq._tail is equal to a column.
    // Given cid, 
    std::vector<struct bio**> _pending_bios;
};

// Pair of SQ and CQ queues used for setting up/configuring controller
// like creating I/O queues
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
    nvme_cq_entry_t submit_and_return_on_completion(nvme_sq_entry_t* cmd, void* data = nullptr, unsigned int datasize = 0);
private:
    sched::thread_handle _req_waiter;
    nvme_cq_entry_t _req_res;
    volatile bool new_cq;
};

}

#endif
