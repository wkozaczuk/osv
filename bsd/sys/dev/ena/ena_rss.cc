/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015-2021 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <bsd/sys/sys/libkern.h>

#define RSS 1
#include "ena_rss.h"

/*
 * Maximum key size used throughout.  It's OK for hardware to use only the
 * first 16 bytes, which is all that's required for IPv4.
 */
#define RSS_KEYSIZE     40

/*
 * Supported RSS hash functions.
 */
#define RSS_HASH_NAIVE          0x00000001      /* Poor but fast hash. */
#define RSS_HASH_TOEPLITZ       0x00000002      /* Required by RSS. */

/*
 * Compile-time limits on the size of the indirection table.
 */
#define	RSS_MAXBITS	7
#define	RSS_TABLE_MAXLEN	(1 << RSS_MAXBITS)
#define	RSS_MAXCPUS	(1 << (RSS_MAXBITS - 1))

static uint8_t rss_key[RSS_KEYSIZE] = {
        0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
        0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
        0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
        0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
        0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
};

/*
 * Toeplitz is the only required hash function in the RSS spec, so use it by
 * default.
 */
static u_int    rss_hashalgo = RSS_HASH_TOEPLITZ;

static u_int    rss_mask = 0;
/*
 * Variable exists just for reporting rss_bits in a user-friendly way.
 */
static u_int    rss_buckets = 0;

static u_int	rss_bits = 0;

/*
 * RSS hash->CPU table, which maps hashed packet headers to particular CPUs.
 * Drivers may supplement this table with a separate CPU<->queue table when
 * programming devices.
 */
struct rss_table_entry {
	uint8_t		rte_cpu;	/* CPU affinity of bucket. */
};
static struct rss_table_entry	rss_table[RSS_TABLE_MAXLEN];

static u_int	rss_ncpus;
void rss_init()
{
	/*
	 * Count available CPUs.
	 */
	rss_ncpus = sched::cpus.size();
	if (rss_ncpus > RSS_MAXCPUS)
		rss_ncpus = RSS_MAXCPUS;

	/*
	 * Tune RSS table entries to be no less than 2x the number of CPUs
	 * -- unless we're running uniprocessor, in which case there's not
	 * much point in having buckets to rearrange for load-balancing!
	 */
	if (rss_ncpus > 1) {
		if (rss_bits == 0)
			rss_bits = fls(rss_ncpus - 1) + 1;

		/*
		 * Microsoft limits RSS table entries to 128, so apply that
		 * limit to both auto-detected CPU counts and user-configured
		 * ones.
		 */
		if (rss_bits == 0 || rss_bits > RSS_MAXBITS) {
			ena_log(nullptr, WARN, "RSS bits %u not valid, coercing to %u\n",
			    rss_bits, RSS_MAXBITS);
			rss_bits = RSS_MAXBITS;
		}

		/*
		 * Figure out how many buckets to use; warn if less than the
		 * number of configured CPUs, although this is not a fatal
		 * problem.
		 */
		rss_buckets = (1 << rss_bits);
		if (rss_buckets < rss_ncpus)
			ena_log(nullptr, WARN, "WARNING: rss_buckets (%u) less than "
			    "rss_ncpus (%u)\n", rss_buckets, rss_ncpus);
		rss_mask = rss_buckets - 1;
	} else {
		rss_bits = 0;
		rss_buckets = 1;
		rss_mask = 0;
	}
	ena_log(nullptr, INFO, "set rss_buckets to: (%u) with rss_cpus: (%u)\n", rss_buckets, rss_ncpus);

	/*
	 * Set up initial CPU assignments: round-robin by default.
	 */
	for (u_int i = 0; i < rss_buckets; i++) {
		rss_table[i].rte_cpu = i % sched::cpus.size();
	}
}

void
rss_getkey(uint8_t *key)
{
        bcopy(rss_key, key, sizeof(rss_key));
}

/*
 * Query the RSS hash algorithm.
 */
u_int
rss_gethashalgo(void)
{
        return (rss_hashalgo);
}

/*
 * Query the RSS layer bucket associated with the given
 * entry in the RSS hash space.
 *
 * The RSS indirection table is 0 .. rss_buckets-1,
 * covering the low 'rss_bits' of the total 128 slot
 * RSS indirection table.  So just mask off rss_bits and
 * return that.
 *
 * NIC drivers can then iterate over the 128 slot RSS
 * indirection table and fetch which RSS bucket to
 * map it to.  This will typically be a CPU queue
 */
u_int
rss_get_indirection_to_bucket(u_int index)
{
        return (index & rss_mask);
}

/*
 * Query the RSS bucket associated with an RSS hash.
 */
u_int
rss_getbucket(u_int hash)
{
        return (hash & rss_mask);
}

/*
 * Query the RSS bucket associated with the given hash value and
 * type.
 */
int
rss_hash2bucket(uint32_t hash_val, uint32_t hash_type, uint32_t *bucket_id)
{
        switch (hash_type) {
        case M_HASHTYPE_RSS_IPV4:
        case M_HASHTYPE_RSS_TCP_IPV4:
        case M_HASHTYPE_RSS_UDP_IPV4:
        case M_HASHTYPE_RSS_IPV6:
        case M_HASHTYPE_RSS_TCP_IPV6:
        case M_HASHTYPE_RSS_UDP_IPV6:
                *bucket_id = rss_getbucket(hash_val);
                return (0);
        default:
                return (-1);
        }
}

/*
 * Query the number of buckets; this may be used by both network device
 * drivers, which will need to populate hardware shadows of the software
 * indirection table, and the network stack itself (such as when deciding how
 * many connection groups to allocate).
 */
u_int
rss_getnumbuckets(void)
{
        return (rss_buckets);
}

/*
 * Query the RSS CPU associated with an RSS bucket.
 */
u_int
rss_getcpu(u_int bucket)
{
	return (rss_table[bucket].rte_cpu);
}

/*
 * This function should generate unique key for the whole driver.
 * If the key was already genereated in the previous call (for example
 * for another adapter), then it should be returned instead.
 */
void
ena_rss_key_fill(void *key, size_t size)
{
	static bool key_generated;
	static uint8_t default_key[ENA_HASH_KEY_SIZE];

	KASSERT(size <= ENA_HASH_KEY_SIZE,
	    ("Requested more bytes than ENA RSS key can hold"));

	if (!key_generated) {
		arc4rand(default_key, ENA_HASH_KEY_SIZE, 0);
		key_generated = true;
	}

	memcpy(key, default_key, size);
}

/*
 * ENA HW expects the key to be in reverse-byte order.
 */
static void
ena_rss_reorder_hash_key(u8 *reordered_key, const u8 *key, size_t key_size)
{
	int i;

	key = key + key_size - 1;

	for (i = 0; i < key_size; ++i)
		*reordered_key++ = *key--;
}

int
ena_rss_set_hash(struct ena_com_dev *ena_dev, const u8 *key)
{
	enum ena_admin_hash_functions ena_func = ENA_ADMIN_TOEPLITZ;
	u8 hw_key[ENA_HASH_KEY_SIZE];

	ena_rss_reorder_hash_key(hw_key, key, ENA_HASH_KEY_SIZE);

	return (ena_com_fill_hash_function(ena_dev, ena_func, hw_key,
	    ENA_HASH_KEY_SIZE, 0x0));
}

int
ena_rss_get_hash_key(struct ena_com_dev *ena_dev, u8 *key)
{
	u8 hw_key[ENA_HASH_KEY_SIZE];
	int rc;

	rc = ena_com_get_hash_key(ena_dev, hw_key);
	if (rc != 0)
		return rc;

	ena_rss_reorder_hash_key(key, hw_key, ENA_HASH_KEY_SIZE);

	return (0);
}

static int
ena_rss_init_default(struct ena_adapter *adapter)
{
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	int qid, rc, i;
	uint8_t rss_algo;

	rc = ena_com_rss_init(ena_dev, ENA_RX_RSS_TABLE_LOG_SIZE);
	if (unlikely(rc != 0)) {
		ena_log(dev, ERR, "Cannot init indirect table\n");
		return (rc);
	}

	for (i = 0; i < ENA_RX_RSS_TABLE_SIZE; i++) {
#ifdef RSS
		qid = rss_get_indirection_to_bucket(i) % adapter->num_io_queues;
#else
		qid = i % adapter->num_io_queues;
#endif
		rc = ena_com_indirect_table_fill_entry(ena_dev, i,
		    ENA_IO_RXQ_IDX(qid));
		if (unlikely((rc != 0) && (rc != EOPNOTSUPP))) {
			ena_log(dev, ERR, "Cannot fill indirect table\n");
			goto err_rss_destroy;
		}
	}


#ifdef RSS
	rss_algo = rss_gethashalgo();
	if (rss_algo == RSS_HASH_TOEPLITZ) {
		uint8_t hash_key[RSS_KEYSIZE];

		rss_getkey(hash_key);
		rc = ena_rss_set_hash(ena_dev, hash_key);
	} else
#endif
		rc = ena_com_fill_hash_function(ena_dev, ENA_ADMIN_TOEPLITZ,
		    NULL, ENA_HASH_KEY_SIZE, 0x0);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP))) {
		ena_log(dev, ERR, "Cannot fill hash function\n");
		goto err_rss_destroy;
	}

	rc = ena_com_set_default_hash_ctrl(ena_dev);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP))) {
		ena_log(dev, ERR, "Cannot fill hash control\n");
		goto err_rss_destroy;
	}

	rc = ena_rss_indir_init(adapter);

	return (rc == EOPNOTSUPP ? 0 : rc);

err_rss_destroy:
	ena_com_rss_destroy(ena_dev);
	return (rc);
}

/* Configure the Rx forwarding */
int
ena_rss_configure(struct ena_adapter *adapter)
{
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	int rc;

	/* In case the RSS table was destroyed */
	if (!ena_dev->rss.tbl_log_size) {
		rc = ena_rss_init_default(adapter);
		if (unlikely((rc != 0) && (rc != EOPNOTSUPP))) {
			ena_log(adapter->pdev, ERR,
			    "WARNING: RSS was not properly re-initialized,"
			    " it will affect bandwidth\n");
			ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_RSS_ACTIVE, adapter);
			return (rc);
		}
	}

	/* Set indirect table */
	rc = ena_com_indirect_table_set(ena_dev);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP)))
		return (rc);

	/* Configure hash function (if supported) */
	rc = ena_com_set_hash_function(ena_dev);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP)))
		return (rc);

	/* Configure hash inputs (if supported) */
	rc = ena_com_set_hash_ctrl(ena_dev);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP)))
		return (rc);

	return (0);
}

int
ena_rss_indir_get(struct ena_adapter *adapter, uint32_t *table)
{
	int rc, i;

	rc = ena_com_indirect_table_get(adapter->ena_dev, table);
	if (rc != 0) {
		if (rc == EOPNOTSUPP)
			device_printf(adapter->pdev,
			    "Reading from indirection table not supported\n");
		else
			device_printf(adapter->pdev,
			    "Unable to get indirection table\n");
		return (rc);
	}

	for (i = 0; i < ENA_RX_RSS_TABLE_SIZE; ++i)
		table[i] = ENA_IO_RXQ_IDX_TO_COMBINED_IDX(table[i]);

	return (0);
}

int
ena_rss_indir_set(struct ena_adapter *adapter, uint32_t *table)
{
	int rc, i;

	for (i = 0; i < ENA_RX_RSS_TABLE_SIZE; ++i) {
		rc = ena_com_indirect_table_fill_entry(adapter->ena_dev, i,
		    ENA_IO_RXQ_IDX(table[i]));
		if (rc != 0) {
			device_printf(adapter->pdev,
			    "Cannot fill indirection table entry %d\n", i);
			return (rc);
		}
	}

	rc = ena_com_indirect_table_set(adapter->ena_dev);
	if (rc == EOPNOTSUPP)
		device_printf(adapter->pdev,
		    "Writing to indirection table not supported\n");
	else if (rc != 0)
		device_printf(adapter->pdev, "Cannot set indirection table\n");

	return (rc);
}

int
ena_rss_indir_init(struct ena_adapter *adapter)
{
	struct ena_indir *indir = adapter->rss_indir;
	int rc;

	if (indir == NULL) {
		adapter->rss_indir = indir = (ena_indir*)malloc(sizeof(struct ena_indir),
		    M_DEVBUF, M_WAITOK | M_ZERO);
		if (indir == NULL)
			return (ENOMEM);
	}

	rc = ena_rss_indir_get(adapter, indir->table);
	if (rc != 0) {
		free(adapter->rss_indir, M_DEVBUF);
		adapter->rss_indir = NULL;

		return (rc);
	}

	ena_rss_copy_indir_buf(indir->sysctl_buf, indir->table);

	return (0);
}
