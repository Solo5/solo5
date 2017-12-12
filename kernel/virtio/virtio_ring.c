/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a unikernel base layer.
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

#include "kernel.h"
#include "virtio_ring.h"
#include "virtio_pci.h"

/*
 * There is no official max queue size. But we've seen 4096, so let's use the
 * double of that.
 */
#define VIRTQ_MAX_QUEUE_SIZE 8192


/*
 * Create a descriptor chain starting at index head, using vq->bufs also
 * starting at index head.
 * Make sure the vq-bufs are cleaned before using them again.
 */
int virtq_add_descriptor_chain(struct virtq *vq,
                               uint16_t head,
                               uint16_t num)
{
    uint16_t mask = vq->num - 1;
    struct virtq_desc *desc;
    uint16_t i;
    uint16_t used_descs = num;

    if (vq->num_avail < used_descs) {
        log(WARN, "Solo5: virtq full! next_avail:%d last_used:%d\n",
            vq->next_avail, vq->last_used);
        return -1;
    }

    assert(used_descs > 0);

    for (i = head; used_descs > 0; used_descs--) {
        desc = &(vq->desc[i]);

        assert(vq->bufs[i].len <= MAX_BUFFER_LEN);

        /*
         * The first field of a "struct io_buffer" is the "data" field, so in
         * the interrupt handler we can just cast this pointer back into a
         * 'struct io_buffer'.
         */
        assert(vq->bufs[i].data == (uint8_t *) &vq->bufs[i]);
        desc->addr = (uint64_t) vq->bufs[i].data;
        desc->len = vq->bufs[i].len;
        desc->flags = VIRTQ_DESC_F_NEXT | vq->bufs[i].extra_flags;

        i = (i + 1) & mask;
        desc->next = i;
    }

    /* The last descriptor in the chain does not have a next */
    desc->next = 0;
    desc->flags &= ~VIRTQ_DESC_F_NEXT;

    vq->num_avail -= num;
    /* Memory barriers should be unnecessary with one processor */
    vq->avail->ring[vq->avail->idx & mask] = head;
    /* avail->idx always increments and wraps naturally at 65536 */
    vq->avail->idx++;
    vq->next_avail += num;

    return 0;
}

void virtq_init_rings(uint16_t pci_base, struct virtq *vq, int selector)
{
    uint8_t *data;
    size_t pgs;
    
    outw(pci_base + VIRTIO_PCI_QUEUE_SEL, selector);
    vq->last_used = vq->next_avail = 0;
    vq->num = vq->num_avail = inw(pci_base + VIRTIO_PCI_QUEUE_SIZE);

    pgs = ((VIRTQ_SIZE(vq->num) - 1) >> PAGE_SHIFT) + 1;
    data = mem_ialloc_pages(pgs);
    assert(data);
    memset(data, 0, pgs << PAGE_SHIFT);

    assert(vq->num <= VIRTQ_MAX_QUEUE_SIZE);

    vq->desc = (struct virtq_desc *)(data + VIRTQ_OFF_DESC(vq->num));
    vq->avail = (struct virtq_avail *)(data + VIRTQ_OFF_AVAIL(vq->num));
    vq->used = (struct virtq_used *)(data + VIRTQ_OFF_USED(vq->num));

    outw(pci_base + VIRTIO_PCI_QUEUE_SEL, selector);
    outl(pci_base + VIRTIO_PCI_QUEUE_PFN, (uint64_t) data
         >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
}
