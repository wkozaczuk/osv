#ifndef NVME_DRIVER_H
#define NVME_DRIVER_H

#include "drivers/nvme-structs.h"
#include "drivers/driver.hh"
#include "drivers/pci-device.hh"
#include <osv/mempool.hh>
#include <osv/interrupt.hh>
#include <osv/msi.hh>
#include "drivers/nvme-queue.hh"
#include <vector>
#include <memory>
#include <map>

/*bdev block cache will not be used if enabled*/
#define NVME_DIRECT_RW_ENABLED 0

#define NVME_QUEUE_PER_CPU_ENABLED 1

//Volatile Write Cache
#define NVME_VWC_ENABLED 1

//checks for all active namespaces instead of just ns 1
#define NVME_CHECK_FOR_ADDITIONAL_NAMESPACES 1

#define NVME_ADMIN_QUEUE_SIZE 8

/*Will be lower if the device doesnt support the
specified queue size */ 
#define NVME_IO_QUEUE_SIZE 256

namespace nvme {

class io_queue_pair;
class admin_queue_pair;

class nvme_driver : public hw_driver {
public:
    explicit nvme_driver(pci::device& dev);
    virtual ~nvme_driver() {};

    virtual std::string get_name() const { return "nvme"; }

    virtual void dump_config();

    int make_request(struct bio* bio, u32 nsid=1);
    static hw_driver* probe(hw_device* dev);

    int set_feature();
    int get_feature();

    int set_number_of_queues(u16 num, u16* ret);
    int set_interrupt_coalescing(u8 threshold, u8 time);

    int get_interrupt_coalescing();

    int create_io_queue(int qid, int qsize=NVME_IO_QUEUE_SIZE, bool pin_t=false, sched::cpu* cpu = NULL, int qprio = 2);
    
    bool register_interrupt(unsigned int iv,unsigned int qid,bool pin_t=false, sched::cpu* cpu = NULL);

    int shutdown();

    std::map<u32, nvme_ns_t*> _ns_data;
    
private:
    int identify_controller();
    int identify_namespace(u32 ns);
    int identify_active_namespaces(u32 start);

    void create_admin_queue();
    void register_admin_interrupts();

    void init_controller_config();

    void enable_controller();
    void disable_controller();
    int wait_for_controller_ready_change(int ready);

    void parse_pci_config();

    nvme_controller_reg_t* _control_reg;
    
    //maintains the nvme instance number for multiple adapters
    static int _instance;
    int _id;

    std::vector<std::unique_ptr<msix_vector>> _msix_vectors;
    bool msix_register(unsigned iv,
        // high priority ISR
        std::function<void ()> isr,
        // bottom half
        sched::thread *t,
        // set affinity of the vector to the cpu running t
        bool assign_affinity=false);

    std::unique_ptr<admin_queue_pair> _admin_queue;

    std::vector<std::unique_ptr<io_queue_pair>> _io_queues;
    u32 _doorbell_stride;

    std::unique_ptr<nvme_identify_ctlr_t> _identify_controller;

    pci::device& _dev;
    interrupt_manager _msi;

    pci::bar *_bar0 = nullptr;
};

}
#endif
