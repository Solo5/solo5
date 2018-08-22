/* An interface for efficient virtio implementation.
 *
 * This header is BSD licensed so anyone can use the definitions
 * to implement compatible drivers/servers.
 *
 * Copyright 2007, 2009, IBM Corporation
 * Copyright 2011, Red Hat, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef VIRTQUEUE_H
#define VIRTQUEUE_H

typedef uint16_t le16;
typedef uint32_t le32;
typedef uint64_t le64;

/* This marks a buffer as continuing via the next field. */
#define VIRTQ_DESC_F_NEXT       1
/* This marks a buffer as write-only (otherwise read-only). */
#define VIRTQ_DESC_F_WRITE      2
/* This means the buffer contains a list of buffer descriptors. */
#define VIRTQ_DESC_F_INDIRECT   4

/* The device uses this in used->flags to advise the driver: don't kick me
 * when you add a buffer.  It's unreliable, so it's simply an
 * optimization. */
#define VIRTQ_USED_F_NO_NOTIFY  1
/* The driver uses this in avail->flags to advise the device: don't
 * interrupt me when you consume a buffer.  It's unreliable, so it's
 * simply an optimization.  */
#define VIRTQ_AVAIL_F_NO_INTERRUPT      1

/* Support for indirect descriptors */
#define VIRTIO_F_INDIRECT_DESC    28

/* Support for avail_event and used_event fields */
#define VIRTIO_F_EVENT_IDX        29

/* Arbitrary descriptor layouts. */
#define VIRTIO_F_ANY_LAYOUT       27

#define VIRTQ_OFF_DESC(q)  0
#define VIRTQ_OFF_AVAIL(q) ((q) * sizeof(struct virtq_desc))
#define VIRTQ_OFF_AVAIL_RING(q) (VIRTQ_OFF_AVAIL(q)             \
                                 + sizeof(struct virtq_avail))
#define VIRTQ_OFF_PADDING(q) (VIRTQ_OFF_AVAIL_RING(q)       \
                              + (sizeof(le16) * (q)))
#define VIRTQ_OFF_USED(q) ((VIRTQ_OFF_PADDING(q) + PAGE_SIZE - 1) & PAGE_MASK)
#define VIRTQ_OFF_USED_RING(q) (VIRTQ_OFF_USED(q) + sizeof(struct virtq_used))

#define VIRTQ_SIZE(q) (VIRTQ_OFF_USED_RING(q) \
                                   + (sizeof(struct virtq_used_elem) * (q)))

/* Virtqueue descriptors: 16 bytes.
 * These can chain together via "next". */
struct virtq_desc {
        /* Address (guest-physical). */
        le64 addr;
        /* Length. */
        le32 len;
        /* The flags as indicated above. */
        le16 flags;
        /* We chain unused descriptors via this, too */
        le16 next;
};

struct virtq_avail {
        le16 flags;
        le16 idx;
        le16 ring[];
        /* Only if VIRTIO_F_EVENT_IDX: le16 used_event; */
};

/* le32 is used here for ids for padding reasons. */
struct virtq_used_elem {
        /* Index of start of used descriptor chain. */
        le32 id;
        /* Total length of the descriptor chain which was written to. */
        le32 len;
};

struct virtq_used {
        le16 flags;
        volatile le16 idx;
        struct virtq_used_elem ring[];
        /* Only if VIRTIO_F_EVENT_IDX: le16 avail_event; */
};

/* This is the max buffer length per descriptor. */
#define MAX_BUFFER_LEN 1526

/*
 * Each one of these io_buffer's map to a descriptor. An array of io_buffer's
 * of size virtq.num (same as virtq.desc) is allocated during init.
 */
struct io_buffer {
    uint8_t data[MAX_BUFFER_LEN];

    /* Data length in Bytes. It is written by the driver on a tx/write, or
     * by the device on a rx/read on interrupt handling (do not remove the
     * volatile). */
    volatile uint32_t len;

    /* Extra flags to be added to the corresponding descriptor. */
    uint16_t extra_flags;
};

struct virtq {
        unsigned int num;

        struct virtq_desc *desc;
        struct virtq_avail *avail;
        struct virtq_used *used;
        struct io_buffer *bufs;

        /* Keep track of available (free) descriptors */
        uint16_t num_avail;

        /* Indexes in the descriptors array */
        uint16_t last_used;
        uint16_t next_avail;
};

static inline int virtq_need_event(uint16_t event_idx, uint16_t new_idx, uint16_t old_idx)
{
         return (uint16_t)(new_idx - event_idx - 1) < (uint16_t)(new_idx - old_idx);
}

/* Get location of event indices (only with VIRTIO_F_EVENT_IDX) */
static inline le16 *virtq_used_event(struct virtq *vq)
{
        /* For backwards compat, used event index is at *end* of avail ring. */
        return &vq->avail->ring[vq->num];
}

static inline le16 *virtq_avail_event(struct virtq *vq)
{
        /* For backwards compat, avail event index is at *end* of used ring. */
        return (le16 *)&vq->used->ring[vq->num];
}

/*
 * Create a descriptor chain starting at index head, using vq->bufs also
 * starting at index head. Num is the number of descriptors (and number of bufs).
 *
 * Returns 0 on success.
 */
int virtq_add_descriptor_chain(struct virtq *vq,
                               uint16_t head,
                               uint16_t num);

void virtq_init_rings(uint16_t pci_base, struct virtq *vq, int selector);

#endif /* VIRTQUEUE_H */
