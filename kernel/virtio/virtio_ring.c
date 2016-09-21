/* Copyright (c) 2015, IBM
 * Author(s): Dan Williams <djwillia@us.ibm.com>
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

/* WARNING: called in interrupt context */
void virtq_handle_interrupt(struct virtq *vq)
{
    struct virtq_used_elem *e;

    for (;;) {
        uint16_t desc_idx;
        struct io_buffer *head_buf;

        if ((vq->used->idx % vq->num) == vq->last_used)
            break;

        e = &(vq->used->ring[vq->last_used % vq->num]);
        desc_idx = e->id;

        head_buf = (struct io_buffer *) vq->desc[desc_idx].addr;

        /* Overwrite buf->len with the number of bytes written by the device.
         * This will be 0 for the tx/blk_write case. */
        head_buf->len = e->len;
        head_buf->completed = 1;
        assert(e->len <= MAX_BUFFER_LEN);

        vq->num_avail++;
        while (vq->desc[desc_idx].flags & VIRTQ_DESC_F_NEXT) {
            vq->num_avail++;
            desc_idx = vq->desc[desc_idx].next;
        }

        vq->last_used = (vq->last_used + 1) % vq->num;
    }
}

/*
 * Create a descriptor chain starting at index head, using vq->bufs also
 * starting at index head.
 * Make sure the vq-bufs are cleaned before using them again.
 */
int virtq_add_descriptor_chain(struct virtq *vq,
                               uint32_t head,
                               uint32_t num)
{
    struct virtq_desc *desc;
    uint32_t i;
    int dbg = 0;
    uint32_t used_descs = num;

    if (vq->num_avail < used_descs) {
        printf("buffer full! next_avail:%d last_used:%d\n",
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
        vq->bufs[i].completed = 0;
        desc->addr = (uint64_t) vq->bufs[i].data;
        desc->len = vq->bufs[i].len;
        desc->flags = VIRTQ_DESC_F_NEXT | vq->bufs[i].extra_flags;

        i = (i + 1) % vq->num;
        desc->next = i;
    }

    /* The last descriptor in the chain does not have a next */
    desc->next = 0;
    desc->flags &= ~VIRTQ_DESC_F_NEXT;

    if (dbg)
        atomic_printf("0x%p next_avail %d last_used %d\n",
                      desc->addr, vq->next_avail,
                      (vq->last_used * num) % vq->num);

    vq->num_avail -= num;
    /* Memory barriers should be unnecessary with one processor */
    vq->avail->ring[vq->avail->idx % vq->num] = head;
    /* avail->idx always increments and wraps naturally at 65536 */
    vq->avail->idx++;
    vq->next_avail = (vq->next_avail + num) % vq->num;

    return 0;
}

void virtq_init_rings(uint16_t pci_base, struct virtq *vq, int selector)
{
    uint8_t *data;

    outw(pci_base + VIRTIO_PCI_QUEUE_SEL, selector);
    vq->last_used = vq->next_avail = 0;
    vq->num = vq->num_avail = inw(pci_base + VIRTIO_PCI_QUEUE_SIZE);

    data = memalign(4096, VIRTQ_SIZE(vq->num));
    assert(data);
    memset(data, 0, VIRTQ_SIZE(vq->num));

    assert(vq->num <= VIRTQ_MAX_QUEUE_SIZE);

    vq->desc = (struct virtq_desc *)(data + VIRTQ_OFF_DESC(vq->num));
    vq->avail = (struct virtq_avail *)(data + VIRTQ_OFF_AVAIL(vq->num));
    vq->used = (struct virtq_used *)(data + VIRTQ_OFF_USED(vq->num));

    outw(pci_base + VIRTIO_PCI_QUEUE_SEL, selector);
    outl(pci_base + VIRTIO_PCI_QUEUE_PFN, (uint64_t) data
         >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
}
