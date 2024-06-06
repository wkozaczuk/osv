/*
 * Copyright (C) 2023 Jan Braunwarth
 * Copyright (C) 2024 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <sys/cdefs.h>

#include <vector>
#include <memory>

#include <osv/contiguous_alloc.hh>
#include <osv/bio.h>
#include <osv/trace.hh>
#include <osv/mempool.hh>
#include <osv/align.hh>

#include "nvme-queue.hh"

TRACEPOINT(trace_nvme_io_queue_wake, "nvme%d qid=%d", int, int);
TRACEPOINT(trace_nvme_wait_for_completion_queue_entries, "nvme%d qid=%d,have_elements=%d", int, int, bool);
TRACEPOINT(trace_nvme_completion_queue_not_empty, "nvme%d qid=%d,not_empty=%d", int, int, bool);
TRACEPOINT(trace_nvme_enable_interrupts, "nvme%d qid=%d", int, int);
TRACEPOINT(trace_nvme_disable_interrupts, "nvme%d qid=%d", int, int);

TRACEPOINT(trace_nvme_read, "nvme%d qid=%d cid=%d, bio data=%#x, slba=%d, nlb=%d", int, int , u16, void*, u64, u32);
TRACEPOINT(trace_nvme_write, "nvme%d qid=%d cid=%d, bio data=%#x, slba=%d, nlb=%d", int, int , u16, void*, u64, u32);

TRACEPOINT(trace_nvme_req_done_error, "nvme%d qid=%d, cid=%d, status type=%#x, status code=%#x, bio=%#x", int, int, u16, u8, u8, bio*);
TRACEPOINT(trace_nvme_req_done_success, "nvme%d qid=%d, cid=%d, bio=%#x",int,int, u16, bio*);

TRACEPOINT(trace_nvme_admin_queue_wake, "nvme%d qid=%d",int,int);

TRACEPOINT(trace_nvme_admin_queue_submit, "nvme%d qid=%d, cid=%d",int, int, int);
TRACEPOINT(trace_nvme_admin_req_done_error, "nvme%d qid=%d, cid=%d, status type=%#x, status code=%#x", int, int, u16, u8, u8);
TRACEPOINT(trace_nvme_admin_req_done_success, "nvme%d qid=%d, cid=%d",int,int, u16);

TRACEPOINT(trace_advance_sq_tail_full, "nvme%d qid=%d, sq_tail=%d, sq_head=%d", int, int, int, int);
TRACEPOINT(trace_nvme_wait_for_entry, "nvme%d qid=%d, sq_tail=%d, sq_head=%d", int, int, int, int);

TRACEPOINT(trace_nvme_expand_bios, "nvme%d qid=%d", int, int);

using namespace memory;

namespace nvme {

queue_pair::queue_pair(
    int did,
    u32 id,
    int qsize,
    pci::device &dev,
    u32* sq_doorbell,
    u32* cq_doorbell,
    std::map<u32, nvme_ns_t*>& ns)
      : _id(id)
      ,_driver_id(did)
      ,_qsize(qsize)
      ,_dev(&dev)
      ,_sq(sq_doorbell)
      ,_sq_full(false)
      ,_cq(cq_doorbell)
      ,_cq_phase_tag(1)
      ,_ns(ns)
{
    size_t sq_buf_size = qsize * sizeof(nvme_sq_entry_t);
    _sq._addr = (nvme_sq_entry_t*) alloc_phys_contiguous_aligned(sq_buf_size, mmu::page_size);
    assert(_sq._addr);
    memset(_sq._addr, 0, sq_buf_size);

    size_t cq_buf_size = qsize * sizeof(nvme_cq_entry_t);
    _cq._addr = (nvme_cq_entry_t*) alloc_phys_contiguous_aligned(cq_buf_size, mmu::page_size);
    assert(_cq._addr);
    memset(_cq._addr, 0, cq_buf_size);

    auto prp_lists = (u64**) malloc(sizeof(u64*)*qsize);
    memset(prp_lists,0,sizeof(u64*)*qsize);
    _prp_lists_in_use.push_back(prp_lists);

    assert(!completion_queue_not_empty());
}

queue_pair::~queue_pair()
{
    for(auto vec: _prp_lists_in_use)
        free_phys_contiguous_aligned(vec);

    free_phys_contiguous_aligned(_sq._addr);
    free_phys_contiguous_aligned(_cq._addr);
}

inline void queue_pair::advance_sq_tail()
{
    _sq._tail = (_sq._tail + 1) % _qsize;
    if(_sq._tail == _sq._head) {
        _sq_full = true;
        trace_advance_sq_tail_full(_driver_id, _id, _sq._tail, _sq._head);
    }
}

u16 queue_pair::submit_cmd(nvme_sq_entry_t* cmd)
{
    SCOPE_LOCK(_lock);
    return submit_cmd_without_lock(cmd);
}

u16 queue_pair::submit_cmd_without_lock(nvme_sq_entry_t* cmd)
{
    _sq._addr[_sq._tail] = *cmd;
    advance_sq_tail();
    mmio_setl(_sq._doorbell, _sq._tail);
    return _sq._tail;
}

void queue_pair::wait_for_completion_queue_entries()
{
    sched::thread::wait_until([this] {
        bool have_elements = this->completion_queue_not_empty();
        if (!have_elements) {
            this->enable_interrupts();
            //check if we got a new cqe between completion_queue_not_empty()
            //and enable_interrupts()
            have_elements = this->completion_queue_not_empty();
            if (have_elements) {
                this->disable_interrupts();
            }
        }

        trace_nvme_wait_for_completion_queue_entries(_driver_id,_id,have_elements);
        return have_elements;
    });
}

void queue_pair::map_prps(nvme_sq_entry_t* cmd, void* data, u64 datasize)
{
    // Depending on the datasize, we map PRPs (Physical Region Page) as follows:
    // 0. We always set the prp1 field to the beginning of the data
    // 1. If data falls within single 4K page then we simply set prp2 to 0
    // 2. If data falls within 2 pages then set prp2 to the second 4K-aligned part of data
    // 3. Otherwise, allocate a physically contigous array long enough to hold addresses
    //    of remaining 4K pages of data
    u64 addr = (u64) data;
    cmd->rw.common.prp1 = addr;
    cmd->rw.common.prp2 = 0;

    // Calculate number of 4K pages and therefore number of entries in the PRP
    // list. The 1st entry rw.common.prp1 can be misaligned but every
    // other one needs to be 4K-aligned
    u64 first_page_start = align_down(addr, NVME_PAGESIZE);
    u64 last_page_end = align_up(addr + datasize, NVME_PAGESIZE);
    int num_of_pages = (last_page_end - first_page_start) / NVME_PAGESIZE;

    if (num_of_pages == 2) {
        cmd->rw.common.prp2 = first_page_start + NVME_PAGESIZE; //2nd page start
    } else if (num_of_pages > 2) {
        // Allocate PRP list as the request is larger than 8K
        assert(num_of_pages / 512 == 0); //For now we can only accomodate datasize <= 2MB
        u64* prp_list = (u64*) alloc_page();
        assert(prp_list != nullptr);
        cmd->rw.common.prp2 = mmu::virt_to_phys(prp_list);
        _prp_lists_in_use.at(cmd->rw.common.cid / _qsize)[cmd->rw.common.cid % _qsize] = prp_list;

        // Fill in the PRP list with address of subsequent 4K pages
        addr = first_page_start + NVME_PAGESIZE; //2nd page start
        prp_list[0] = addr;

        for (int i = 1; i < num_of_pages - 1; i++) {
            addr += NVME_PAGESIZE;
            prp_list[i] = addr;
        }
    }
}

nvme_cq_entry_t* queue_pair::get_completion_queue_entry()
{
    if(!completion_queue_not_empty()) {
        return nullptr;
    }

    auto* cqe = &_cq._addr[_cq._head];
    assert(cqe->p == _cq_phase_tag);

    return cqe;
}

inline void queue_pair::advance_cq_head()
{
    if(++_cq._head == _qsize) {
        _cq._head -= _qsize;
        _cq_phase_tag = !_cq_phase_tag;
    }
}

bool queue_pair::completion_queue_not_empty() const
{
    bool a = reinterpret_cast<volatile nvme_cq_entry_t*>(&_cq._addr[_cq._head])->p == _cq_phase_tag;
    trace_nvme_completion_queue_not_empty(_driver_id, _id, a);
    return a;
}

void queue_pair::enable_interrupts()
{
    _dev->msix_unmask_entry(_id);
    trace_nvme_enable_interrupts(_driver_id, _id);
}

void queue_pair::disable_interrupts()
{
    _dev->msix_mask_entry(_id);
    trace_nvme_disable_interrupts(_driver_id, _id);
}

io_queue_pair::io_queue_pair(
    int driver_id,
    int id,
    int qsize,
    pci::device& dev,
    u32* sq_doorbell,
    u32* cq_doorbell,
    std::map<u32, nvme_ns_t*>& ns
    ) : queue_pair(
        driver_id,
        id,
        qsize,
        dev,
        sq_doorbell,
        cq_doorbell,
        ns
    )
{
    auto bios_array = (bio**) malloc(sizeof(bio*) * qsize);
    memset(bios_array, 0, sizeof(bio*) * qsize);
    _pending_bios.push_back(bios_array);
}

io_queue_pair::~io_queue_pair()
{
    for(auto vec : _pending_bios)
        free(vec);
}

int io_queue_pair::make_request(struct bio* bio, u32 nsid = 1)
{
    u64 slba = bio->bio_offset;
    u32 nlb = bio->bio_bcount; //do the blockshift in nvme_driver

    _lock.lock();
    u16 cid = _sq._tail;
    if(_sq_full) {
        //Wait for free entries
        _sq_full_waiter.reset(*sched::thread::current());
        trace_nvme_wait_for_entry(_driver_id, _id, _sq._tail, _sq._head);
        sched::thread::wait_until([this] { return !(this->_sq_full); });
        _sq_full_waiter.clear();
    }
    /* 
    We need to check if there is an outstanding command that uses 
    _sq._tail as command id.
    This happens if 
        1.The SQ is full. Then we just have to wait for an open slot (see above)
        2.the Controller already read a SQE but didnt post a CQE yet.
            This means we could post the command but need a different cid. To still
            use the cid as index to find the corresponding bios we use a matrix 
            adding columns if we need them
    */
    while (_pending_bios.at(cid / _qsize)[cid % _qsize]) {
        cid += _qsize;
        if(_pending_bios.size() <= (cid / _qsize)){
            auto bios_array = (struct bio**) malloc(sizeof(struct bio*) * _qsize);
            memset(bios_array,0,sizeof(struct bio*) * _qsize);
            _pending_bios.push_back(bios_array);
            auto prplists = (u64**) malloc(sizeof(u64*)* _qsize);
            memset(prplists,0,sizeof(u64*)* _qsize);
            _prp_lists_in_use.push_back(prplists);
            trace_nvme_expand_bios(_driver_id, _id);
        }
    }
    _pending_bios.at(cid / _qsize)[cid % _qsize] = bio;

    switch (bio->bio_cmd) {
    case BIO_READ:
        trace_nvme_read(_driver_id, _id, cid, bio->bio_data, slba, nlb);
        submit_read_write_cmd(cid, nsid, NVME_CMD_READ, slba, nlb, (void*)mmu::virt_to_phys(bio->bio_data));
        break;
    
    case BIO_WRITE:
        trace_nvme_write(_driver_id, _id, cid, bio->bio_data, slba, nlb);
        submit_read_write_cmd(cid, nsid, NVME_CMD_WRITE, slba, nlb, (void*)mmu::virt_to_phys(bio->bio_data));
        break;
    
    case BIO_FLUSH:
        submit_flush_cmd(cid, nsid);
        break;
        
    default:
        NVME_ERROR("Operation not implemented\n");
        _lock.unlock();
        return ENOTBLK;
    }
    _lock.unlock();
    return 0;
}

void io_queue_pair::req_done()
{
    nvme_cq_entry_t* cqe = nullptr;
    while (true)
    {
        wait_for_completion_queue_entries();
        trace_nvme_io_queue_wake(_driver_id,_id);
        while ((cqe = get_completion_queue_entry())) {
            u16 cid = cqe->cid;
            if (cqe->sct != 0 || cqe->sc != 0) {
                trace_nvme_req_done_error(_driver_id,_id, cid, cqe->sct, cqe->sc, _pending_bios.at(cid / _qsize)[cid % _qsize]);
                if(_pending_bios.at(cid / _qsize)[cid % _qsize])
                    biodone(_pending_bios.at(cid / _qsize)[cid % _qsize],false);
                NVME_ERROR("I/O queue: cid=%d, sct=%#x, sc=%#x, bio=%#x, slba=%llu, nlb=%llu\n", cqe->cid, cqe->sct, 
                    cqe->sc,_pending_bios.at(cid / _qsize)[cid % _qsize],
                    cqe->sc,_pending_bios.at(cid / _qsize)[cid % _qsize]->bio_offset,
                    cqe->sc,_pending_bios.at(cid / _qsize)[cid % _qsize]->bio_bcount);
            } else {
                trace_nvme_req_done_success(_driver_id, _id, cid, _pending_bios.at(cid / _qsize)[cid % _qsize]);
                if(_pending_bios.at(cid / _qsize)[cid % _qsize])
                    biodone(_pending_bios.at(cid / _qsize)[cid % _qsize],true);
            }

            _pending_bios.at(cid / _qsize)[cid % _qsize] = nullptr;
            if (_prp_lists_in_use.at(cid / _qsize)[cid % _qsize]) {
                free_page(_prp_lists_in_use.at(cid / _qsize)[cid % _qsize]);
                _prp_lists_in_use.at(cid / _qsize)[cid % _qsize] = nullptr;
            }
            _sq._head = cqe->sqhd; //update sq_head

            advance_cq_head();
        }
        mmio_setl(_cq._doorbell, _cq._head);
        //TODO: Maybe check if cqe->sqhd <> old _sq._head to be sure the _head advanced
        //TODO: Maybe check and disable full in the loop above
        if (_sq_full) { //wake up the requesting thread in case the submission queue was full before
            _sq_full = false;
            if (_sq_full_waiter)
                _sq_full_waiter.wake_from_kernel_or_with_irq_disabled();
        }
    }
}

u16 io_queue_pair::submit_read_write_cmd(u16 cid, u32 nsid, int opc, u64 slba, u32 nlb, void* data)
{
    nvme_sq_entry_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    
    cmd.rw.common.cid = cid;
    cmd.rw.common.opc = opc;
    cmd.rw.common.nsid = nsid;
    cmd.rw.slba = slba;
    cmd.rw.nlb = nlb - 1;

    u32 datasize = nlb << _ns[nsid]->blockshift;
    map_prps(&cmd, data, datasize);
        
    return submit_cmd_without_lock(&cmd);
}

u16 io_queue_pair::submit_flush_cmd(u16 cid, u32 nsid)
{
    nvme_sq_entry_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.vs.common.opc = NVME_CMD_FLUSH;
    cmd.vs.common.nsid = nsid;
    cmd.vs.common.cid = cid;

    return submit_cmd_without_lock(&cmd);
}

admin_queue_pair::admin_queue_pair(
    int driver_id,
    int id,
    int qsize,
    pci::device& dev,
    u32* sq_doorbell,
    u32* cq_doorbell,
    std::map<u32, nvme_ns_t*>& ns
    ) : queue_pair(
        driver_id,
        id,
        qsize,
        dev,
        sq_doorbell,
        cq_doorbell,
        ns
) {}

void admin_queue_pair::req_done()
{   
    nvme_cq_entry_t* cqe = nullptr;
    while (true)
    {
        wait_for_completion_queue_entries();
        trace_nvme_admin_queue_wake(_driver_id,_id);
        while ((cqe = get_completion_queue_entry())) {
            u16 cid = cqe->cid;
            if (cqe->sct != 0 || cqe->sc != 0) {
                trace_nvme_admin_req_done_error(_driver_id, _id, cid, cqe->sct, cqe->sc);
                NVME_ERROR("Admin queue cid=%d, sct=%#x, sc=%#x\n",cid,cqe->sct,cqe->sc);
            } else {
                trace_nvme_admin_req_done_success(_driver_id, _id, cid);
            }
            
            if (_prp_lists_in_use.at(cid / _qsize)[cid % _qsize]) {
                free_page(_prp_lists_in_use.at(cid / _qsize)[cid % _qsize]);
                _prp_lists_in_use.at(cid / _qsize)[cid % _qsize] = nullptr;
            }
            _sq._head = cqe->sqhd; //update sq_head
            _req_res = *cqe; //save the cqe so that the requesting thread can return it

            advance_cq_head();
        }
        mmio_setl(_cq._doorbell, _cq._head);
        
        /*Wake up the thread that requested the admin command*/
        new_cq = true;
        _req_waiter.wake_from_kernel_or_with_irq_disabled();
    }
}

nvme_cq_entry_t
admin_queue_pair::submit_and_return_on_completion(nvme_sq_entry_t* cmd, void* data, unsigned int datasize)
{
    _lock.lock();
    
    _req_waiter.reset(*sched::thread::current());
    
    //for now admin cid = sq_tail
    u16 cid = _sq._tail;
    cmd->rw.common.cid = cid;

    if(data != nullptr && datasize > 0) {
        map_prps(cmd, data, datasize);
    }
    
    trace_nvme_admin_queue_submit(_driver_id, _id, cid);
    submit_cmd_without_lock(cmd);
    
    sched::thread::wait_until([this] { return this->new_cq; });
    _req_waiter.clear();
    
    new_cq = false;
    if (_prp_lists_in_use.at(0)[cid]) {
        free(_prp_lists_in_use.at(0)[cid]);
    }
    
    _lock.unlock();
    return _req_res;
}
}
