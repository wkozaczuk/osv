/*
 * This file is produced automatically.
 * Do not modify anything in here by hand.
 *
 * Created from source file
 *   vmbus_if.m
 * with
 *   makeobjops.awk
 *
 * See the source file for legal information
 */


#ifndef _vmbus_if_h_
#define _vmbus_if_h_


struct hyperv_guid;
struct taskqueue;

/** @brief Unique descriptor for the VMBUS_GET_VERSION() method */
extern struct kobjop_desc vmbus_get_version_desc;
/** @brief A function implementing the VMBUS_GET_VERSION() method */
typedef uint32_t vmbus_get_version_t(device_t bus, device_t dev);

static __inline uint32_t VMBUS_GET_VERSION(device_t bus, device_t dev)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)bus)->ops,vmbus_get_version);
	return ((vmbus_get_version_t *) _m)(bus, dev);
}

/** @brief Unique descriptor for the VMBUS_PROBE_GUID() method */
extern struct kobjop_desc vmbus_probe_guid_desc;
/** @brief A function implementing the VMBUS_PROBE_GUID() method */
typedef int vmbus_probe_guid_t(device_t bus, device_t dev,
                               const struct hyperv_guid *guid);

static __inline int VMBUS_PROBE_GUID(device_t bus, device_t dev,
                                     const struct hyperv_guid *guid)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)bus)->ops,vmbus_probe_guid);
	return ((vmbus_probe_guid_t *) _m)(bus, dev, guid);
}

/** @brief Unique descriptor for the VMBUS_GET_VCPU_ID() method */
extern struct kobjop_desc vmbus_get_vcpu_id_desc;
/** @brief A function implementing the VMBUS_GET_VCPU_ID() method */
typedef uint32_t vmbus_get_vcpu_id_t(device_t bus, device_t dev, int cpu);

static __inline uint32_t VMBUS_GET_VCPU_ID(device_t bus, device_t dev, int cpu)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)bus)->ops,vmbus_get_vcpu_id);
	return ((vmbus_get_vcpu_id_t *) _m)(bus, dev, cpu);
}

/** @brief Unique descriptor for the VMBUS_GET_EVENT_TASKQ() method */
extern struct kobjop_desc vmbus_get_event_taskq_desc;
/** @brief A function implementing the VMBUS_GET_EVENT_TASKQ() method */
typedef struct taskqueue * vmbus_get_event_taskq_t(device_t bus, device_t dev,
                                                   int cpu);

static __inline struct taskqueue * VMBUS_GET_EVENT_TASKQ(device_t bus,
                                                         device_t dev, int cpu)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)bus)->ops,vmbus_get_event_taskq);
	return ((vmbus_get_event_taskq_t *) _m)(bus, dev, cpu);
}

#endif /* _vmbus_if_h_ */
