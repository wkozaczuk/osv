/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/drivers_config.h>
#include <map>
#include <memory>

extern "C" {
    #include "acpi.h"
    #include "acpiosxf.h"
    #include "acpixf.h"
}
#include <stdlib.h>
#include <osv/mmu.hh>
#include <osv/sched.hh>
#include <osv/shutdown.hh>
#include <osv/align.hh>
#if CONF_drivers_xen
#include <osv/xen.hh>
#endif

#include <osv/debug.h>
#include <osv/mutex.h>
#include <osv/semaphore.hh>

#include "drivers/console.hh"
#include <osv/pci.hh>
#include <osv/interrupt.hh>

#include <osv/prio.hh>
#include "acpi.hh"

#define acpi_tag "acpi"
#define acpi_d(...)   tprintf_d(acpi_tag, __VA_ARGS__)
#define acpi_i(...)   tprintf_i(acpi_tag, __VA_ARGS__)
#define acpi_w(...)   tprintf_w(acpi_tag, __VA_ARGS__)
#define acpi_e(...)   tprintf_e(acpi_tag, __VA_ARGS__)

ACPI_STATUS AcpiOsInitialize()
{
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate()
{
    return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer()
{
    ACPI_SIZE rsdp;
    if (acpi::pvh_rsdp_paddr) {
        rsdp = acpi::pvh_rsdp_paddr;
    } else {
        auto st = AcpiFindRootPointer(&rsdp);
        if (ACPI_FAILURE(st)) {
            abort();
        }
    }
    return rsdp;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *InitVal,
        ACPI_STRING *NewVal)
{
    *NewVal = nullptr;
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable,
        ACPI_TABLE_HEADER **NewTable)
{
    *NewTable = nullptr;
    return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable,
    ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength)
{
    *NewAddress = 0;
    *NewTableLength = 0;
    return AE_OK;
}

// Note: AcpiOsCreateLock requires a lock which can be used for mutual
// exclusion of a resources between multiple threads *AND* interrupt handlers.
// Normally, this requires a spinlock (which disables interrupts), to ensure
// that while a thread is using the protected resource, an interrupt handler
// with the same context as the thread doesn't use it.
// However, in OSV, interrupt handlers are run in ordinary threads, so the
// mutual exclusion of an ordinary "mutex" is enough.
ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle)
{
    *OutHandle = new mutex();
    return AE_OK;
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
    reinterpret_cast<mutex *>(Handle) -> lock();
    return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
    reinterpret_cast<mutex *>(Handle) -> unlock();;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle)
{
    delete reinterpret_cast<mutex *>(Handle);
}

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits,
        UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle)
{
    // Note: we ignore MaxUnits.
    *OutHandle = new semaphore(InitialUnits);
    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
    if (!Handle)
        return AE_BAD_PARAMETER;
    delete reinterpret_cast<semaphore *>(Handle);
    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle,
        UINT32 Units, UINT16 Timeout)
{
    if (!Handle)
        return AE_BAD_PARAMETER;
    semaphore *sem = reinterpret_cast<semaphore *>(Handle);
    switch(Timeout) {
    case ACPI_DO_NOT_WAIT:
        return sem->trywait(Units) ? AE_OK : AE_TIME;
    case ACPI_WAIT_FOREVER:
        sem->wait(Units);
        return AE_OK;
    default:
        sched::timer timer(*sched::thread::current());
        timer.set(std::chrono::milliseconds(Timeout));
        return sem->wait(Units, &timer) ? AE_OK : AE_TIME;
    }
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units)
{
    if (!Handle)
        return AE_BAD_PARAMETER;
    semaphore *sem = reinterpret_cast<semaphore *>(Handle);
    sem->post(Units);
    return AE_OK;
}

void *AcpiOsAllocate(ACPI_SIZE Size)
{
    return malloc(Size);
}

void AcpiOsFree(void *Memory)
{
    free(Memory);
}

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length)
{
    uint64_t _where = align_down(Where, mmu::huge_page_size);
    size_t map_size = align_up(Length + Where - _where, mmu::huge_page_size);
    
    mmu::linear_map(mmu::phys_to_virt(_where), _where, map_size, "acpi");
    return mmu::phys_to_virt(Where);
}

void AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Size)
{
    // Unmap is a no-op and leaves the mapppings in place because the amount of
    // mapped ACPI memory is limited, and we don't track whether what it maps
    // was previously mapped (so unmap can poke a hole where a previous mapping
    // existed, even before ACPI).
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void *LogicalAddress,
        ACPI_PHYSICAL_ADDRESS *PhysicalAddress)
{
    *PhysicalAddress = mmu::virt_to_phys(LogicalAddress);
    return AE_OK;
}

#if 0
ACPI_STATUS AcpiOsCreateCache (
    char                    *CacheName,
    UINT16                  ObjectSize,
    UINT16                  MaxDepth,
    ACPI_CACHE_T            **ReturnCache);

ACPI_STATUS
AcpiOsDeleteCache (
    ACPI_CACHE_T            *Cache);

ACPI_STATUS
AcpiOsPurgeCache (
    ACPI_CACHE_T            *Cache);

void *
AcpiOsAcquireObject (
    ACPI_CACHE_T            *Cache);

ACPI_STATUS
AcpiOsReleaseObject (
    ACPI_CACHE_T            *Cache,
    void                    *Object);

#endif

/*
 * Interrupt handlers
 */

namespace osv {

class acpi_interrupt {
public:
    acpi_interrupt(unsigned gsi, ACPI_OSD_HANDLER sr, void* ctxt)
        : _service_routine(sr)
        , _context(ctxt)
        , _stopped(false)
        , _counter(0)
        , _thread(sched::thread::make([this] { process_interrupts(); }))
#ifdef __aarch64__
        , _intr(gic::irq_type::IRQ_TYPE_EDGE, gsi, [this] { _counter.fetch_add(1); return true; },//return this->ack_irq(); },
                                                   [this] { _thread->wake_with_irq_disabled(); })
#else
        , _intr(gsi, [this] { _counter.fetch_add(1); _thread->wake_with_irq_disabled(); })
#endif
    {
        _thread->start();
    }
    ~acpi_interrupt() {
        _stopped.store(true);
        _thread->wake();
        _thread->join();
    }
private:
    void process_interrupts() {
        uint64_t local_counter = 0;
        while (!_stopped.load()) {
            sched::thread::wait_for(
                    [=] { return _stopped.load(); },
                    [=] { return local_counter != _counter.load(); });
            if (local_counter != _counter.load()) {
                local_counter = _counter.load();
                _service_routine(_context);
            }
        }
    }
private:
    ACPI_OSD_HANDLER _service_routine;
    void* _context;
    std::atomic<bool> _stopped;
    std::atomic<uint64_t> _counter;
    std::unique_ptr<sched::thread> _thread;
#ifdef __aarch64__
    spi_interrupt _intr;
#else
    gsi_edge_interrupt _intr;
#endif
};

std::map<UINT32, std::unique_ptr<acpi_interrupt>> acpi_interrupts;
}

ACPI_STATUS
AcpiOsInstallInterruptHandler(
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine,
    void                    *Context)
{
    if (ServiceRoutine == nullptr) {
        return AE_BAD_PARAMETER;
    }

    if (osv::acpi_interrupts.count(InterruptNumber)) {
        return AE_ALREADY_EXISTS;
    }

    osv::acpi_interrupts[InterruptNumber] = std::unique_ptr<osv::acpi_interrupt>(
        new osv::acpi_interrupt(InterruptNumber, ServiceRoutine, Context));

    return AE_OK;
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler(
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine)
{
    if (ServiceRoutine == nullptr) {
        return AE_BAD_PARAMETER;
    }

    if (!osv::acpi_interrupts.count(InterruptNumber)) {
        return AE_NOT_EXIST;
    }

    osv::acpi_interrupts.erase(InterruptNumber);
    return AE_OK;
}

ACPI_THREAD_ID AcpiOsGetThreadId()
{
    return reinterpret_cast<uintptr_t>(sched::thread::current());
}

ACPI_STATUS AcpiOsExecute(
    ACPI_EXECUTE_TYPE       Type,
    ACPI_OSD_EXEC_CALLBACK  Function,
    void                    *Context)
{
    return AE_NOT_IMPLEMENTED;
}

void AcpiOsWaitEventsComplete()
{
    // FIXME: ?
}

void AcpiOsSleep(UINT64 Milliseconds)
{
    sched::thread::sleep(std::chrono::milliseconds(Milliseconds));
}

ACPI_STATUS AcpiOsEnterSleep(UINT8 sleep_state, UINT32 rega_value, UINT32 regb_value)
{
    return AE_OK;
}

void AcpiOsStall(UINT32 Microseconds)
{
    // spec says to spin, but...
    sched::thread::sleep(std::chrono::microseconds(Microseconds));
}

#ifdef __x86_64__
#include "processor.hh"
using namespace processor;
#else
#include "arch-pci.hh"
using namespace pci; 
#endif

ACPI_STATUS AcpiOsReadPort(
    ACPI_IO_ADDRESS         Address,
    UINT32                  *Value,
    UINT32                  Width)
{
    switch (Width) {
    case 8:
        *Value = inb(Address);
        break;
    case 16:
        *Value = inw(Address);
        break;
    case 32:
        *Value = inl(Address);
        break;
    default:
        return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(
    ACPI_IO_ADDRESS         Address,
    UINT32                  Value,
    UINT32                  Width)
{
    switch (Width) {
    case 8:
        outb(Value, Address);
        break;
    case 16:
        outw(Value, Address);
        break;
    case 32:
        outl(Value, Address);
        break;
    default:
        return AE_BAD_PARAMETER;
    }
    return AE_OK;
}


ACPI_STATUS
AcpiOsReadMemory (
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT64                  *Value,
    UINT32                  Width)
{
    switch (Width) {
    case 8:
        *Value = *mmu::phys_cast<u8>(Address);
        break;
    case 16:
        *Value = *mmu::phys_cast<u16>(Address);
        break;
    case 32:
        *Value = *mmu::phys_cast<u32>(Address);
        break;
    case 64:
        *Value = *mmu::phys_cast<u64>(Address);
        break;
    default:
        return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

ACPI_STATUS
AcpiOsWriteMemory (
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT64                  Value,
    UINT32                  Width)
{
    switch (Width) {
    case 8:
        *mmu::phys_cast<u8>(Address) = Value;
        break;
    case 16:
        *mmu::phys_cast<u16>(Address) = Value;
        break;
    case 32:
        *mmu::phys_cast<u32>(Address) = Value;
        break;
    case 64:
        *mmu::phys_cast<u64>(Address) = Value;
        break;
    default:
        return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

ACPI_STATUS
AcpiOsReadPciConfiguration(
    ACPI_PCI_ID             *PciId,
    UINT32                  Reg,
    UINT64                  *Value,
    UINT32                  Width)
{
    switch(Width) {
    case 64:
        // OSv pci config functions does not do 64 bits reads
        return AE_NOT_IMPLEMENTED;
        break;
    case 32:
        *Value = pci::read_pci_config(PciId->Bus,
                                      PciId->Device,
                                      PciId->Function,
                                      Reg);
        break;
    case 16:
        *Value = pci::read_pci_config_word(PciId->Bus,
                                           PciId->Device,
                                           PciId->Function,
                                           Reg);
        break;
    case 8:
        *Value = pci::read_pci_config_byte(PciId->Bus,
                                           PciId->Device,
                                           PciId->Function,
                                           Reg);
        break;
    default:
        return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

ACPI_STATUS
AcpiOsWritePciConfiguration (
    ACPI_PCI_ID             *PciId,
    UINT32                  Reg,
    UINT64                  Value,
    UINT32                  Width)
{
    switch(Width) {
    case 64:
        // OSv pci config functions does not do 64 bits writes
        return AE_NOT_IMPLEMENTED;
        break;
    case 32:
        pci::write_pci_config(PciId->Bus,
                              PciId->Device,
                              PciId->Function,
                              Reg,
                              Value);
        break;
    case 16:
        pci::write_pci_config_word(PciId->Bus,
                                   PciId->Device,
                                   PciId->Function,
                                   Reg,
                                   Value);
        break;
    case 8:
        pci::write_pci_config_byte(PciId->Bus,
                                   PciId->Device,
                                   PciId->Function,
                                   Reg,
                                   Value);
        break;
    default:
        return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

BOOLEAN
AcpiOsReadable(void *Pointer, ACPI_SIZE Length)
{
    return mmu::isreadable(Pointer, Length);
}

BOOLEAN
AcpiOsWritable(void *Pointer, ACPI_SIZE Length)
{
    return true;
}

UINT64 AcpiOsGetTimer()
{
    return clock::get()->time() / 100;
}

ACPI_STATUS AcpiOsSignal(UINT32 Function, void *Info)
{
    abort();
}

void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf(const char *Format, ...)
{
    va_list va;
    va_start(va, Format);
    AcpiOsVprintf(Format, va);
    va_end(va);
}

void AcpiOsVprintf(const char *Format, va_list Args)
{
    static char msg[1024];

    vsnprintf(msg, sizeof(msg), Format, Args);

    acpi_i(msg);
}

namespace acpi {

uint64_t pvh_rsdp_paddr = 0;

#define ACPI_MAX_INIT_TABLES 16

static ACPI_TABLE_DESC TableArray[ACPI_MAX_INIT_TABLES];

static bool enabled = false;

bool is_enabled() {
    return enabled;
}

#define ACPI_MADT_GEN_INT   11
#define ACPI_MADT_GEN_DIST  12
#define ACPI_MADT_GEN_RDIST 14
#define ACPI_MADT_GEN_TRANS 15

#define MADT_GENINT_ENABLED 0x1ul

struct acpi_gen_dist {  /* Generic Distributor */
    u8 type;
    u8 length;
    u16 res;
    u32 gic_id;
    u64 base_address;
    u32 global_irq_base;
    u8 version;
    u8 res2[3];
} __attribute__((packed));

struct acpi_gen_redist {    /* Generic Redistributor */
    u8 type;
    u8 length;
    u16 res;
    u64 base_address;
    u32 len;
} __attribute__((packed));

struct acpi_gen_trans { /* Generic Translator */
    u8 type;
    u8 length;
    u16 res;
    u32 translation_id;
    u64 base_address;
    u32 res2;
} __attribute__((packed));

struct acpi_gen_int
{
    u8 type;
    u8 length;
    u16 res;
    u32 cpu_iface_num;
    u32 acpi_proc_uid;
    u32 flags;
    u32 parking_proto_ver;
    u32 perf_int_gsiv;
    u64 parked_addr;
    u64 base_address;
    u64 gicv_base_addr;
    u64 gich_base_addr;
    u32 vgic_int;
    u64 gicr_base_addr;
    u64 mpidr;
    u8 efficiency_class;
    u8 res2;
    u16 spe_int;
}  __attribute__((packed));

#define MAX_CPU_COUNT 32
static int cpu_count = 0;
static u64 cpus_mpids[MAX_CPU_COUNT];
/*
static ACPI_STATUS acpi_device_handler(ACPI_HANDLE object, u32 nesting_level, void *context,
                                       void **return_value)
{
    ACPI_DEVICE_INFO *dev_info;
    ACPI_STATUS rv = AcpiGetObjectInfo(object, &dev_info);
    if (ACPI_SUCCESS(rv)) {
        if (dev_info->Flags & ACPI_PCI_ROOT_BRIDGE) {
            acpi_debug("retrieving PCI root bridge resources for %p", object);
            id_heap iomem = allocate_id_heap(acpi_heap, acpi_heap, PAGESIZE, true);
            assert(iomem != INVALID_ADDRESS);
            struct acpi_pci_res_ctx ctx = {
                    .bridge_window = irange(0, 0),
                    .iomem = iomem,
            };
            rv = AcpiWalkResources(object, METHOD_NAME__CRS, acpi_pci_res_handler, &ctx);
            if (ACPI_SUCCESS(rv))
                pci_bridge_set_iomem(ctx.bridge_window, iomem);
            else
                msg_err("ACPI: cannot retrieve PCI root bridge resources: %d", rv);
        }
        ACPI_FREE(dev_info);
    }

    return rv;
}*/

void early_init()
{
    debug_early_u64("In ACPI early_init, acpi::pvh_rsdp_paddr:", acpi::pvh_rsdp_paddr);
    if (!acpi::pvh_rsdp_paddr) {
        ACPI_SIZE rsdp;
        auto st = AcpiFindRootPointer(&rsdp);
        if (ACPI_FAILURE(st)) {
            acpi_w("Warning: Failed to find ACPI root pointer!\n");
            return;
        }
    }

    ACPI_STATUS status;

    status = AcpiInitializeTables(TableArray, ACPI_MAX_INIT_TABLES, TRUE);
    if (ACPI_FAILURE(status)) {
        acpi_e("AcpiInitializeTables failed: %s\n", AcpiFormatException(status));
        return;
    }

    // Initialize ACPICA subsystem
    status = AcpiInitializeSubsystem();
    if (ACPI_FAILURE(status)) {
        acpi_e("AcpiInitializeSubsystem failed: %s\n", AcpiFormatException(status));
        return;
    }

    // Copy the root table list to dynamic memory
    status = AcpiReallocateRootTable();
    if (ACPI_FAILURE(status)) {
        acpi_e("AcpiReallocateRootTable failed: %s\n", AcpiFormatException(status));
        return;
    }

    // Create the ACPI namespace from ACPI tables
    status = AcpiLoadTables();
    if (ACPI_FAILURE(status)) {
        acpi_e("AcpiLoadTables failed: %s\n", AcpiFormatException(status));
        return;
    }

    debug_early("ACPI early_init complete\n");
    enabled = true;

    debug_early_u64("From ACPI - timer irq: ", get_timer_irq());

    u8 console_type = 0;
    u64 console_addr = get_spcr_addr(console_type);
    debug_early_u64("From ACPI - console type: ", console_type);
    debug_early_u64("From ACPI - console addr: ", console_addr);

    debug_early_u64("From ACPI - PCI ecam addr: ", get_pci_ecam());

    //Parse GIC settings
    parse_madt([](u8 type, void *p) {
        if (type == ACPI_MADT_GEN_DIST)
	    debug_early_u64("From ACPI - GIC dist base: ", ((acpi_gen_dist *)p)->base_address);
	else if (type == ACPI_MADT_GEN_INT)
	    debug_early_u64("From ACPI - GIC cpuinf base: ", ((acpi_gen_int *)p)->base_address);
	else if (type == ACPI_MADT_GEN_RDIST)
	    debug_early_u64("From ACPI - GIC rdist base: ", ((acpi_gen_redist *)p)->base_address);
	else if (type == ACPI_MADT_GEN_TRANS)
	    debug_early_u64("From ACPI - GIC trans base: ", ((acpi_gen_trans *)p)->base_address);
    });

    //Parse CPUs
    parse_madt([](u8 type, void *p) {
        if (type == ACPI_MADT_GEN_INT) {
	    acpi_gen_int *agi = (acpi_gen_int*)p;
	    if (agi->flags & MADT_GENINT_ENABLED) {
	        u64 mpidr = agi->mpidr;
	        debug_early_u64("From ACPI - CPU mpidr: ", mpidr);
		cpus_mpids[cpu_count++] = mpidr;
	    }
	}
    });

    //AcpiGetDevices(0, acpi_device_handler, 0, 0);

    //TODO: Based on https://www.kernel.org/doc/html/v5.6/PCI/acpi-info.html the PCI interrupt
    //number mappings can be somehow retrieved from _PRT (Pci Routing Table)
    //Hopefully we can figure out on how QEMU exposes this in ACPI
    //See acpi_dsdt_add_pci_route_table() in hw/pci-host/gpex-acpi.c
    //called from acpi_dsdt_add_pci() in hw/arm/virt-acpi-build.c
    //
    //For now however we can assume we only need to support legacy (SPI-based) PCI
    //interrupts on QEMU with artificially disabled DTB and somehow ITS/LPIs not available
    //Otherwise we would be running on ACPI platforms where we would use ITS/LPIs anyway
    //
    //There are only 4 PCI interrupts - A, B, C, D and on arm64 qemu virt
    //these map to 32 (SPI base) + 3, 4, 5, 6 = 35, 36, 37, 38
    //The device id (aka slot) determines which one to use -
    //Maybe (slot % 4) could be an index to a 4 elements table [35, 36, 37, 38]
}

UINT32 acpi_poweroff(void *unused)
{
    osv::shutdown();
    return 1;
}

// must be called after the scheduler, apic and smp where started to run
// The following function comes from the documentation example page 262
void init()
{
    if (!enabled) {
        return;
    }

    ACPI_STATUS status;


    // TODO: Installation of Local handlers

    // Initialize the ACPI hardware
    status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(status)) {
        acpi_e("AcpiEnableSubsystem failed: %s\n", AcpiFormatException(status));
        return;
    }

    // Complete the ACPI namespace object initialization
    status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(status)) {
        acpi_e("AcpiInitializeObjects failed: %s\n", AcpiFormatException(status));
    }

    AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON, acpi_poweroff, nullptr);
    AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON, 0);
}

u32 get_timer_irq()
{
    ACPI_TABLE_HEADER *t;
    ACPI_STATUS rv = AcpiGetTable(ACPI_SIG_GTDT, 1, &t);
    if (ACPI_FAILURE(rv))
        return 0;
    ACPI_TABLE_GTDT *gtdt = (ACPI_TABLE_GTDT *)t;
    u32 irq = gtdt->VirtualTimerInterrupt;
    AcpiPutTable(t);
    return irq;
}

u64 get_spcr_addr(u8 &type)
{
    ACPI_TABLE_HEADER *t;
    ACPI_STATUS rv = AcpiGetTable(ACPI_SIG_SPCR, 1, &t);
    if (ACPI_FAILURE(rv))
        return 0;
    ACPI_TABLE_SPCR *spcr = (ACPI_TABLE_SPCR *)t;
    u64 addr = spcr->SerialPort.Address;
    type = spcr->InterfaceType;
    AcpiPutTable(t);
    return addr;
}

bool is_serial_16550(u8 spcr_type)
{
    return 
	spcr_type == SERIAL_16550_COMPATIBLE ||
	spcr_type == SERIAL_16550_SUBSET ||
	spcr_type == SERIAL_16550_WITH_GAS;
}

void find_mcfg(std::function<bool(u64 addr, u16 segment, u8 bus_start, u8 bus_end)> mcfg_fun)
{
    ACPI_TABLE_HEADER *mcfg;
    ACPI_STATUS rv = AcpiGetTable(ACPI_SIG_MCFG, 1, &mcfg);
    if (ACPI_FAILURE(rv))
        return;
    ACPI_MCFG_ALLOCATION *a = (ACPI_MCFG_ALLOCATION *)(((ACPI_TABLE_MCFG *)mcfg) + 1);
    int n = (mcfg->Length - sizeof(ACPI_TABLE_MCFG)) / sizeof(ACPI_MCFG_ALLOCATION);
    for (int i = 0; i < n; i++) {
        if (mcfg_fun(a->Address, a->PciSegment, a->StartBusNumber, a->EndBusNumber))
            break;
    }
    AcpiPutTable(mcfg);
}

void parse_madt(std::function<void(u8 type, void *p)> consume_fun)
{
    ACPI_TABLE_HEADER *madt;
    ACPI_STATUS rv = AcpiGetTable(ACPI_SIG_MADT, 1, &madt);
    if (ACPI_FAILURE(rv))
        return;
    u8 *p = (u8 *)madt + sizeof(ACPI_TABLE_MADT);
    u8 *pe = (u8 *)madt + madt->Length;
    for (; p < pe; p += p[1])
        consume_fun(p[0], p);
    AcpiPutTable(madt);
}

#define GICC_MEM_SZ	0x2000

#define ACPI_MADT_GICD_VERSION_2   0x2
#define ACPI_MADT_GICD_VERSION_3   0x3

#define GICD_V2_MEM_SZ             0x01000
#define GICD_V3_MEM_SZ             0x10000

static void parse_gic_dist(void *p, u64 *dist, size_t *dist_len)
{
    *dist = 0;
    *dist_len = 0;

    acpi_gen_dist *entry = (acpi_gen_dist *)p;
    if (entry->base_address) {
         *dist = entry->base_address;
         debug_early_u64("From ACPI - GIC dist base: ", *dist);
         if (entry->version == ACPI_MADT_GICD_VERSION_2)
             *dist_len = GICD_V2_MEM_SZ;
         else if (entry->version == ACPI_MADT_GICD_VERSION_3)
             *dist_len = GICD_V3_MEM_SZ;
    }
}

bool get_gic_v2(u64 *dist, size_t *dist_len, u64 *cpu, size_t *cpu_len)
{
    *cpu = 0;

    parse_madt([dist, dist_len, cpu, cpu_len](u8 type, void *p) {
        if (type == ACPI_MADT_GEN_INT && ((acpi_gen_int *)p)->base_address) {//GICC
	    *cpu = ((acpi_gen_int *)p)->base_address;
	    *cpu_len = GICC_MEM_SZ; //Which doc is it specified?
	    debug_early_u64("From ACPI - GIC cpuif base: ", *cpu);
	} else if (type == ACPI_MADT_GEN_DIST) {
	    parse_gic_dist(p, dist, dist_len);
	}
    });

    return *dist && *cpu && *dist_len;
}

bool get_gic_v3(u64 *dist, size_t *dist_len, u64 *redist, size_t *redist_len)
{
    *redist = 0;
    *redist_len = 0;

    parse_madt([dist, dist_len, redist, redist_len](u8 type, void *p) {
	if (type == ACPI_MADT_GEN_RDIST && ((acpi_gen_redist *)p)->base_address) {
	    acpi_gen_redist *entry = (acpi_gen_redist *)p;
	    debug_early_u64("From ACPI - GIC rdist base: ", entry->base_address);
	    *redist = entry->base_address;
	    *redist_len = entry->len;
	} else if (type == ACPI_MADT_GEN_DIST) {
	    parse_gic_dist(p, dist, dist_len);
	}
    });

    return *dist && *redist && *dist_len  && *redist_len;
}

int get_cpus_count()
{
    return cpu_count;
}

void get_cpus_mpids(u64 *mpids, int n) {
    for (auto i = 0; i < n; i++) {
        mpids[i] = cpus_mpids[i];
    }
}

u64 get_pci_ecam()
{
    u64 pci_ecam_addr = 0;
    find_mcfg([& pci_ecam_addr](u64 addr, u16 segment, u8 bus_start, u8 bus_end) {
        if (segment == 0 && bus_start == 0) {
	    pci_ecam_addr = addr;
	    return true;
	} else {
	    return false;
	}
    });
    debug_early_u64("From ACPI - PCI ecam addr: ", pci_ecam_addr);
    return pci_ecam_addr;
}
}

//void __attribute__((constructor(init_prio::acpi))) acpi_init_early()
//{
/*#if CONF_drivers_xen
    XENPV_ALTERNATIVE({ acpi::early_init(); }, {}); //xen_start_info not available in aarch64
#else*/
//    acpi::early_init();
//#endif
//}
