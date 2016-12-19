/* Copyright (c) 2015, IBM
 * Author(s): Dan Williams <djwillia@us.ibm.com>
 *            Ricardo Koller <kollerr@us.ibm.com>
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __UKVM_H__
#define __UKVM_H__

struct ukvm_boot_info {
    uint64_t mem_size;		/* Memory size in bytes */
    uint64_t kernel_end;	/* Address of end of kernel */
    uint64_t cmdline;		/* Address of command line (C string) */
};

/*
 * We can only send 32 bits via ports, so sending pointers will only
 * work for 32-bit addresses.  If we have unikernels with more than
 * 4GB of memory, we could be in trouble.
 */
static inline uint32_t ukvm_ptr(volatile void *p)
{
	assert((((uint64_t)p) & 0xffffffff00000000) == 0);
	return (uint32_t)((uint64_t)p & 0xffffffff);
}

#define UKVM_PORT_PUTS      0x499
/* was UKVM_PORT_NANOSLEEP 0x500 */

#define UKVM_PORT_BLKINFO   0x502
#define UKVM_PORT_BLKWRITE  0x503
#define UKVM_PORT_BLKREAD   0x504

#define UKVM_PORT_NETINFO   0x505
#define UKVM_PORT_NETWRITE  0x506
#define UKVM_PORT_NETREAD   0x507

/* was UKVM_PORT_DBG_STACK 0x508 */

#define UKVM_PORT_POLL      0x509
#define UKVM_PORT_TIME_INIT 0x50a

/*
 * Guest-provided pointers in UKVM I/O operations MUST be declared with
 * UKVM_GUEST_PTR(type), where type is the desired guest-side pointer type.
 *
 * This ensures that these pointers are not directly dereferencable on the host
 * (ukvm) side.
 */
#ifdef __UKVM_HOST__
#define UKVM_GUEST_PTR(T) uint64_t
#else
#define UKVM_GUEST_PTR(T) T
#endif

/* UKVM_PORT_PUTS */
struct ukvm_puts {
	/* IN */
	UKVM_GUEST_PTR(const char *) data;
	size_t len;
};

/* UKVM_PORT_BLKINFO */
struct ukvm_blkinfo {
	/* OUT */
	size_t sector_size;
	size_t num_sectors;
	int rw;
};

/* UKVM_PORT_BLKWRITE */
struct ukvm_blkwrite {
	/* IN */
	size_t sector;
	UKVM_GUEST_PTR(const void *) data;
	size_t len;

	/* OUT */
	int ret;
};

/* UKVM_PORT_BLKREAD */
struct ukvm_blkread {
	/* IN */
	size_t sector;
	UKVM_GUEST_PTR(void *) data;

	/* IN/OUT */
	size_t len;

	/* OUT */
	int ret;
};

/* UKVM_PORT_NETINFO */
struct ukvm_netinfo {
	/* OUT */
	char mac_str[18];
};

/* UKVM_PORT_NETWRITE */
struct ukvm_netwrite {
	/* IN */
	UKVM_GUEST_PTR(const void *) data;
	size_t len;

	/* OUT */
	int ret;
};

/* UKVM_PORT_NETREAD */
struct ukvm_netread {
	/* IN */
	UKVM_GUEST_PTR(void *) data;

	/* IN/OUT */
	size_t len;

	/* OUT */
	int ret;
};

/*
 * UKVM_PORT_POLL: Block until timeout_nsecs have passed or I/O is possible,
 * whichever is sooner. Returns 1 if I/O is possible, otherwise 0.
 *
 * TODO: Extend this interface to select which I/O events are of interest.
 */
struct ukvm_poll {
    /* IN */
    uint64_t timeout_nsecs;

    /* OUT */
    int ret;
};


/* UKVM_PORT_TIME_INIT */
struct ukvm_time_init {
	/* OUT */
	uint64_t freq;
};

#if 0
/* UKVM_PORT_NANOSLEEP */
struct ukvm_nanosleep {
	/* IN */
	uint64_t sec_in;
	uint64_t nsec_in;

	/* OUT */
	uint64_t sec_out;
	uint64_t nsec_out;
	int ret;
};

/* UKVM_PORT_CLKSPEED */
struct ukvm_clkspeed {
	/* OUT */
	uint64_t clkspeed;
#endif


#endif

