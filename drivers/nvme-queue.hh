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

#define NVME_PAGESIZE 4096
#define NVME_PAGESHIFT 12

namespace nvme {

class queue_pair;

class queue_pair
{
public:
    queue_pair(
        int driver_id,
        u32 id,
        int qsize,
        pci::device& dev,
        nvme_sq_entry_t* sq_addr,
        u32* sq_doorbell,
        nvme_cq_entry_t* cq_addr,
        u32* cq_doorbell,
        std::map<u32, nvme_ns_t*>& ns
    );

    ~queue_pair();

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

    nvme_sq_entry_t* _sq_addr;
    u32 _sq_head;
    u32 _sq_tail;
    volatile u32* _sq_doorbell;
    bool _sq_full;

    nvme_cq_entry_t* _cq_addr;
    u32 _cq_head;
    u32 _cq_tail;
    volatile u32* _cq_doorbell;
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
        nvme_sq_entry_t* sq_addr,
        u32* sq_doorbell,
        nvme_cq_entry_t* cq_addr,
        u32* cq_doorbell,
        std::map<u32, nvme_ns_t*>& ns
    );
    ~io_queue_pair();

    int self_test();
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
        nvme_sq_entry_t* sq_addr,
        u32* sq_doorbell,
        nvme_cq_entry_t* cq_addr,
        u32* cq_doorbell,
        std::map<u32, nvme_ns_t*>& ns
    );

    std::unique_ptr<nvme_cq_entry_t> _req_res;
    volatile bool new_cq;
    void req_done();
    std::unique_ptr<nvme_cq_entry_t>
    submit_and_return_on_completion(std::unique_ptr<nvme_sq_entry_t> cmd, void* data=nullptr, unsigned int datasize=0);
private:
    sched::thread_handle _req_waiter;
};

}

#endif
