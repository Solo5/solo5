/*
 * Copyright (c) 2015-2020 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a sandboxed execution environment.
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

/*
 * NVMe block devices. On EC2 each EBS volume is its own NVMe controller.
 * Polled: one I/O queue pair per controller, one command in flight.
 */

#include "bindings.h"

#define NVME_MAX_CONTROLLERS 8
#define NVME_PAGE_SIZE       4096
#define NVME_PAGES(n)        (((n) + NVME_PAGE_SIZE - 1) / NVME_PAGE_SIZE)
#define NVME_ADMIN_DEPTH     8
#define NVME_SQE_SIZE        64
#define NVME_CQE_SIZE        16
#define NVME_MAX_TRANSFER    (1024 * 1024)
#define NVME_IO_TIMEOUT_NS   (30ULL * 1000000000ULL)
#define NVME_BOUNCE_SIZE     (64 * 1024)
#define NVME_PRP_ALIGN       4
#define NVME_IO_QID          1
#define NVME_NSID            1 /* the only namespace used */
#define NVME_LBA_SHIFT_MIN   9
#define NVME_LBA_SHIFT_MAX   12

/* Controller registers */
#define NVME_REG_CAP       0x00
#define NVME_REG_CC        0x14
#define NVME_REG_CSTS      0x1c
#define NVME_REG_AQA       0x24
#define NVME_REG_ASQ       0x28
#define NVME_REG_ACQ       0x30
#define NVME_REG_DOORBELLS 0x1000

#define NVME_CAP_MQES(cap)    (((cap) & 0xffff) + 1)
#define NVME_CAP_TO(cap)      (((cap) >> 24) & 0xff)
#define NVME_CAP_DSTRD(cap)   (((cap) >> 32) & 0xf)
#define NVME_CAP_CSS_NVM(cap) (((cap) >> 37) & 1)
#define NVME_CAP_MPSMIN(cap)  (((cap) >> 48) & 0xf)
#define NVME_CAP_TO_UNIT_NS   500000000ULL
#define NVME_DB_STRIDE(dstrd) (4ULL << (dstrd))
#define NVME_AQA(depth)       ((((depth) - 1) << 16) | ((depth) - 1))

#define NVME_CC_EN     (1u << 0)
#define NVME_CC_IOSQES (6u << 16) /* log2(NVME_SQE_SIZE) */
#define NVME_CC_IOCQES (4u << 20) /* log2(NVME_CQE_SIZE) */
#define NVME_CSTS_RDY  (1u << 0)
#define NVME_CSTS_CFS  (1u << 1)

/* Completion queue entry status */
#define NVME_CQE_PHASE(st)  ((st) & 1)
#define NVME_CQE_STATUS(st) ((st) >> 1)

/* Create I/O CQ/SQ */
#define NVME_CDW10_QUEUE(qid, depth) ((((depth) - 1) << 16) | (qid))
#define NVME_QUEUE_PC                (1u << 0) /* physically contiguous */
#define NVME_CQ_IEN                  (1u << 1) /* interrupts enabled */
#define NVME_SQ_CQID(id)             ((uint32_t)(id) << 16)

/* Opcodes */
#define NVME_ADMIN_CREATE_SQ 0x01
#define NVME_ADMIN_CREATE_CQ 0x05
#define NVME_ADMIN_IDENTIFY  0x06
#define NVME_CMD_WRITE       0x01
#define NVME_CMD_READ        0x02

/* Identify */
#define NVME_CNS_NAMESPACE       0
#define NVME_CNS_CONTROLLER      1
#define NVME_ID_SN               4
#define NVME_ID_SN_SIZE          20
#define NVME_ID_MDTS             77
#define NVME_ID_NN               516
#define NVME_ID_EBS_DEVNAME      3072 /* EBS block device mapping name */
#define NVME_ID_EBS_DEVNAME_SIZE 32
#define NVME_ID_NS_NSZE          0
#define NVME_ID_NS_FLBAS         26
#define NVME_ID_NS_LBAF          128
#define NVME_FLBAS_INDEX(flbas)  ((flbas) & 0xf)
#define NVME_LBAF_MS(lbaf)       ((lbaf) & 0xffff)
#define NVME_LBAF_LBADS(lbaf)    (((lbaf) >> 16) & 0xff)

#define DEV_PREFIX "/dev/"

struct nvme_sqe {
    uint8_t opcode;
    uint8_t flags;
    uint16_t cid;
    uint32_t nsid;
    uint64_t rsvd;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10, cdw11, cdw12, cdw13, cdw14, cdw15;
};

struct nvme_cqe {
    uint32_t dw0, dw1;
    uint16_t sq_head, sq_id;
    uint16_t cid, status;
};

_Static_assert(sizeof(struct nvme_sqe) == NVME_SQE_SIZE, "nvme_sqe");
_Static_assert(sizeof(struct nvme_cqe) == NVME_CQE_SIZE, "nvme_cqe");

struct nvme_queue {
    struct nvme_sqe *sq;
    volatile struct nvme_cqe *cq;
    unsigned depth;
    unsigned sq_tail, cq_head, phase;
    uint16_t cid;
    uint64_t sq_db, cq_db;
};

struct nvme_controller {
    struct pci_dev pci;
    uint64_t regs;
    unsigned dstrd;
    uint64_t ready_timeout_ns;
    struct nvme_queue admin, io;
    uint64_t *prp_list;
    uint32_t nsid;
    uint64_t blocks;
    unsigned lba_shift;
    size_t max_transfer;
    bool failed;
    char devname[NVME_ID_EBS_DEVNAME_SIZE + 1];
};

static struct {
    struct nvme_sqe asq[NVME_ADMIN_DEPTH]
        __attribute__((aligned(NVME_PAGE_SIZE)));
    struct nvme_cqe acq[NVME_ADMIN_DEPTH]
        __attribute__((aligned(NVME_PAGE_SIZE)));
    uint64_t prp_list[NVME_PAGE_SIZE / sizeof(uint64_t)]
        __attribute__((aligned(NVME_PAGE_SIZE)));
    uint8_t identify[NVME_PAGE_SIZE];
} nvme_mem[NVME_MAX_CONTROLLERS] __attribute__((aligned(NVME_PAGE_SIZE)));

/* Staging for reads and writes whose buffer is not NVME_PRP_ALIGN aligned */
static uint8_t nvme_bounce[NVME_BOUNCE_SIZE]
    __attribute__((aligned(NVME_PAGE_SIZE)));

static struct nvme_controller controllers[NVME_MAX_CONTROLLERS];
static unsigned controllers_num;

static void nvme_queue_init(struct nvme_queue *q,
                            const struct nvme_controller *c, unsigned qid,
                            struct nvme_sqe *sq, struct nvme_cqe *cq,
                            unsigned depth)
{
    memset(sq, 0, depth * sizeof *sq);
    memset(cq, 0, depth * sizeof *cq);
    q->sq = sq;
    q->cq = cq;
    q->depth = depth;
    q->sq_tail = 0;
    q->cq_head = 0;
    q->phase = 1;
    q->cid = 0;
    uint64_t stride = NVME_DB_STRIDE(c->dstrd);
    q->sq_db = c->regs + NVME_REG_DOORBELLS + (2 * qid) * stride;
    q->cq_db = c->regs + NVME_REG_DOORBELLS + (2 * qid + 1) * stride;
}

/* Returns the completion status, or -1 on timeout. */
static int nvme_submit_sync(struct nvme_queue *q, struct nvme_sqe *sqe,
                            uint64_t timeout_ns)
{
    sqe->cid = q->cid++;
    q->sq[q->sq_tail] = *sqe;
    q->sq_tail = (q->sq_tail + 1) % q->depth;
    ec2_mb();
    mmio_write32(q->sq_db, q->sq_tail);

    volatile struct nvme_cqe *cqe = &q->cq[q->cq_head];
    solo5_time_t deadline = solo5_clock_monotonic() + timeout_ns;
    while (NVME_CQE_PHASE(cqe->status) != q->phase) {
        if (solo5_clock_monotonic() > deadline)
            return -1;
        cpu_relax();
    }
    cc_barrier();
    uint16_t cid = cqe->cid;
    uint16_t status = cqe->status;

    q->cq_head = (q->cq_head + 1) % q->depth;
    if (q->cq_head == 0)
        q->phase ^= 1;
    mmio_write32(q->cq_db, q->cq_head);

    if (cid != sqe->cid) {
        log(WARN, "Solo5: NVMe: completion for command %u, expected %u\n", cid,
            sqe->cid);
        return -1;
    }
    return NVME_CQE_STATUS(status);
}

static int nvme_wait_ready(const struct nvme_controller *c, uint32_t ready)
{
    solo5_time_t deadline = solo5_clock_monotonic() + c->ready_timeout_ns;
    for (;;) {
        uint32_t csts = mmio_read32(c->regs + NVME_REG_CSTS);
        if (csts & NVME_CSTS_CFS)
            return -1;
        if ((csts & NVME_CSTS_RDY) == ready)
            return 0;
        if (solo5_clock_monotonic() > deadline)
            return -1;
        cpu_relax();
    }
}

static int nvme_disable(const struct nvme_controller *c)
{
    uint32_t cc = mmio_read32(c->regs + NVME_REG_CC);
    if (cc & NVME_CC_EN)
        mmio_write32(c->regs + NVME_REG_CC, cc & ~NVME_CC_EN);
    return nvme_wait_ready(c, 0);
}

static int nvme_identify(struct nvme_controller *c, uint32_t nsid, uint32_t cns,
                         void *buf)
{
    struct nvme_sqe sqe = {0};
    sqe.opcode = NVME_ADMIN_IDENTIFY;
    sqe.nsid = nsid;
    sqe.prp1 = (uint64_t)buf;
    sqe.cdw10 = cns;
    return nvme_submit_sync(&c->admin, &sqe, c->ready_timeout_ns);
}

/* EBS device name, else the serial number (as set by QEMU). */
static void nvme_set_devname(struct nvme_controller *c, const uint8_t *id)
{
    const uint8_t *src = id + NVME_ID_EBS_DEVNAME;
    size_t len = NVME_ID_EBS_DEVNAME_SIZE;
    if (src[0] == '\0' || src[0] == ' ') {
        src = id + NVME_ID_SN;
        len = NVME_ID_SN_SIZE;
    }
    if (len >= sizeof(DEV_PREFIX) - 1 &&
        memcmp(src, DEV_PREFIX, sizeof(DEV_PREFIX) - 1) == 0) {
        src += sizeof(DEV_PREFIX) - 1;
        len -= sizeof(DEV_PREFIX) - 1;
    }
    size_t n = 0;
    while (n < len && src[n] > ' ' && src[n] <= '~') {
        c->devname[n] = src[n];
        n++;
    }
    c->devname[n] = '\0';
}

/*
 * As on virtio, the queue is as deep as the device offers, though only one
 * entry is ever in use: Solo5's block API is synchronous.
 */
static int nvme_create_io_queues(struct nvme_controller *c, unsigned depth)
{
    struct nvme_sqe sqe;
    struct nvme_sqe *sq = mem_ialloc_pages(NVME_PAGES(depth * sizeof *sq));
    struct nvme_cqe *cq = mem_ialloc_pages(NVME_PAGES(depth * sizeof *cq));

    nvme_queue_init(&c->io, c, NVME_IO_QID, sq, cq, depth);

    memset(&sqe, 0, sizeof sqe);
    sqe.opcode = NVME_ADMIN_CREATE_CQ;
    sqe.prp1 = (uint64_t)cq;
    sqe.cdw10 = NVME_CDW10_QUEUE(NVME_IO_QID, depth);
    sqe.cdw11 = NVME_QUEUE_PC; /* no NVME_CQ_IEN: polled */
    int st = nvme_submit_sync(&c->admin, &sqe, c->ready_timeout_ns);
    if (st != 0) {
        log(WARN, "Solo5: NVMe: Create I/O CQ failed (status 0x%x)\n", st);
        return -1;
    }

    memset(&sqe, 0, sizeof sqe);
    sqe.opcode = NVME_ADMIN_CREATE_SQ;
    sqe.prp1 = (uint64_t)sq;
    sqe.cdw10 = NVME_CDW10_QUEUE(NVME_IO_QID, depth);
    sqe.cdw11 = NVME_SQ_CQID(NVME_IO_QID) | NVME_QUEUE_PC;
    st = nvme_submit_sync(&c->admin, &sqe, c->ready_timeout_ns);
    if (st != 0) {
        log(WARN, "Solo5: NVMe: Create I/O SQ failed (status 0x%x)\n", st);
        return -1;
    }
    return 0;
}

int nvme_attach(const struct pci_dev *d)
{
    if (controllers_num == NVME_MAX_CONTROLLERS) {
        log(WARN, "Solo5: NVMe: PCI:%02x:%02x.%x: too many controllers\n",
            d->bus, d->dev, d->fn);
        return -1;
    }
    struct nvme_controller *c = &controllers[controllers_num];
    memset(c, 0, sizeof *c);
    c->pci = *d;
    c->regs = pci_map_bar(d, 0);
    if (c->regs == 0)
        return -1;

    uint64_t cap = mmio_read64(c->regs + NVME_REG_CAP);
    unsigned mqes = NVME_CAP_MQES(cap);
    unsigned to = NVME_CAP_TO(cap);
    c->dstrd = NVME_CAP_DSTRD(cap);
    c->ready_timeout_ns = (to ? to : 1) * NVME_CAP_TO_UNIT_NS;
    if (NVME_CAP_MPSMIN(cap) != 0 || !NVME_CAP_CSS_NVM(cap)) {
        log(WARN, "Solo5: NVMe: unsupported CAP 0x%llx\n",
            (unsigned long long)cap);
        return -1;
    }

    if (nvme_disable(c) != 0) {
        log(WARN, "Solo5: NVMe: controller did not disable\n");
        return -1;
    }
    nvme_queue_init(&c->admin, c, 0, nvme_mem[controllers_num].asq,
                    nvme_mem[controllers_num].acq, NVME_ADMIN_DEPTH);
    mmio_write32(c->regs + NVME_REG_AQA, NVME_AQA(NVME_ADMIN_DEPTH));
    mmio_write64(c->regs + NVME_REG_ASQ, (uint64_t)c->admin.sq);
    mmio_write64(c->regs + NVME_REG_ACQ, (uint64_t)c->admin.cq);
    mmio_write32(c->regs + NVME_REG_CC,
                 NVME_CC_IOCQES | NVME_CC_IOSQES | NVME_CC_EN);
    if (nvme_wait_ready(c, 1) != 0) {
        log(WARN, "Solo5: NVMe: controller did not become ready\n");
        return -1;
    }

    uint8_t *id = nvme_mem[controllers_num].identify;
    if (nvme_identify(c, 0, NVME_CNS_CONTROLLER, id) != 0) {
        log(WARN, "Solo5: NVMe: Identify Controller failed\n");
        return -1;
    }
    nvme_set_devname(c, id);
    unsigned mdts = id[NVME_ID_MDTS];
    uint32_t nn = *(const uint32_t *)(id + NVME_ID_NN);
    c->max_transfer = mdts ? (size_t)NVME_PAGE_SIZE << mdts : NVME_MAX_TRANSFER;
    if (c->max_transfer > NVME_MAX_TRANSFER)
        c->max_transfer = NVME_MAX_TRANSFER;

    const char *name = ec2_block_manifest_name(c->devname);
    unsigned mft_index;
    struct mft_entry *e =
        mft_get_by_name(ec2_manifest, name, MFT_DEV_BLOCK_BASIC, &mft_index);
    if (e == NULL || e->attached) {
        log(INFO, "Solo5: NVMe: PCI:%02x:%02x.%x: '%s' %s, not used\n", d->bus,
            d->dev, d->fn, c->devname,
            e ? "is a duplicate" : "is not in the manifest");
        (void)nvme_disable(c);
        return 0;
    }

    if (nn < NVME_NSID ||
        nvme_identify(c, NVME_NSID, NVME_CNS_NAMESPACE, id) != 0) {
        log(WARN, "Solo5: NVMe: '%s': no namespace 1\n", c->devname);
        return -1;
    }
    uint64_t nsze = *(const uint64_t *)(id + NVME_ID_NS_NSZE);
    int format = NVME_FLBAS_INDEX(id[NVME_ID_NS_FLBAS]);
    uint32_t lbaf = *(const uint32_t *)(id + NVME_ID_NS_LBAF + 4 * format);
    c->nsid = NVME_NSID;
    c->lba_shift = NVME_LBAF_LBADS(lbaf);
    c->blocks = nsze;
    if (NVME_LBAF_MS(lbaf) != 0 || c->lba_shift < NVME_LBA_SHIFT_MIN ||
        c->lba_shift > NVME_LBA_SHIFT_MAX) {
        log(WARN, "Solo5: NVMe: '%s': unsupported LBA format 0x%x\n",
            c->devname, lbaf);
        return -1;
    }

    if (nvme_create_io_queues(c, mqes) != 0)
        return -1;
    c->prp_list = nvme_mem[controllers_num].prp_list;

    e->b.hostfd = controllers_num;
    e->u.block_basic.capacity = c->blocks;
    e->u.block_basic.block_size = 1u << c->lba_shift;
    e->attached = true;
    controllers_num++;

    log(INFO,
        "Solo5: NVMe: PCI:%02x:%02x.%x: '%s' as '%s': %llu MiB, %u-byte "
        "blocks, queue depth %u, max transfer %u KiB\n",
        d->bus, d->dev, d->fn, c->devname, name,
        (unsigned long long)((c->blocks << c->lba_shift) >> 20),
        1u << c->lba_shift, mqes, (unsigned)(c->max_transfer >> 10));
    return 0;
}

static struct nvme_controller *nvme_of_handle(solo5_handle_t h)
{
    struct mft_entry *e =
        mft_get_by_index(ec2_manifest, h, MFT_DEV_BLOCK_BASIC);
    if (e == NULL)
        return NULL;
    assert(e->attached);
    assert(e->b.hostfd >= 0 && (unsigned)e->b.hostfd < controllers_num);
    return &controllers[e->b.hostfd];
}

static int nvme_rw_chunk(struct nvme_controller *c, uint8_t opcode,
                         uint64_t lba, uint64_t data, size_t size)
{
    struct nvme_sqe sqe = {0};
    uint64_t prp2 = 0;
    size_t first = NVME_PAGE_SIZE - (data & (NVME_PAGE_SIZE - 1));

    if (size > first) {
        uint64_t next = data + first;
        if (size - first <= NVME_PAGE_SIZE) {
            prp2 = next;
        } else {
            size_t pages = (size - first + NVME_PAGE_SIZE - 1) / NVME_PAGE_SIZE;
            for (size_t i = 0; i < pages; i++)
                c->prp_list[i] = next + i * NVME_PAGE_SIZE;
            prp2 = (uint64_t)c->prp_list;
        }
    }
    sqe.opcode = opcode;
    sqe.nsid = c->nsid;
    sqe.prp1 = data;
    sqe.prp2 = prp2;
    sqe.cdw10 = (uint32_t)lba;
    sqe.cdw11 = (uint32_t)(lba >> 32);
    sqe.cdw12 = (uint32_t)((size >> c->lba_shift) - 1);

    int st = nvme_submit_sync(&c->io, &sqe, NVME_IO_TIMEOUT_NS);
    if (st < 0) {
        log(ERROR, "Solo5: NVMe: '%s': I/O timed out, device disabled\n",
            c->devname);
        c->failed = true;
    } else if (st != 0) {
        log(WARN, "Solo5: NVMe: '%s': %s at LBA %llu failed (status 0x%x)\n",
            c->devname, opcode == NVME_CMD_READ ? "read" : "write",
            (unsigned long long)lba, st);
    }
    return st;
}

static solo5_result_t nvme_rw(solo5_handle_t h, uint8_t opcode,
                              solo5_off_t offset, uint8_t *buf, size_t size)
{
    struct nvme_controller *c = nvme_of_handle(h);
    if (c == NULL)
        return SOLO5_R_EINVAL;
    size_t block_size = (size_t)1 << c->lba_shift;
    if (offset % block_size != 0 || size % block_size != 0)
        return SOLO5_R_EINVAL;
    uint64_t lba = offset >> c->lba_shift;
    uint64_t count = size >> c->lba_shift;
    if (lba > c->blocks || count > c->blocks - lba)
        return SOLO5_R_EINVAL;
    if (c->failed)
        return SOLO5_R_EUNSPEC;

    bool use_bounce = ((uint64_t)buf & (NVME_PRP_ALIGN - 1)) != 0;
    while (size > 0) {
        size_t chunk = size < c->max_transfer ? size : c->max_transfer;
        uint64_t data = (uint64_t)buf;
        if (use_bounce) {
            if (chunk > NVME_BOUNCE_SIZE)
                chunk = NVME_BOUNCE_SIZE;
            if (opcode == NVME_CMD_WRITE)
                memcpy(nvme_bounce, buf, chunk);
            data = (uint64_t)nvme_bounce;
        }
        if (nvme_rw_chunk(c, opcode, lba, data, chunk) != 0)
            return SOLO5_R_EUNSPEC;
        if (use_bounce && opcode == NVME_CMD_READ)
            memcpy(buf, nvme_bounce, chunk);
        buf += chunk;
        size -= chunk;
        lba += chunk >> c->lba_shift;
    }
    return SOLO5_R_OK;
}

solo5_result_t solo5_block_acquire(const char *name, solo5_handle_t *h,
                                   struct solo5_block_info *info)
{
    unsigned mft_index;
    struct mft_entry *e =
        mft_get_by_name(ec2_manifest, name, MFT_DEV_BLOCK_BASIC, &mft_index);
    if (e == NULL)
        return SOLO5_R_EINVAL;
    struct nvme_controller *c = nvme_of_handle(mft_index);

    info->block_size = (size_t)1 << c->lba_shift;
    info->capacity = c->blocks << c->lba_shift;
    *h = mft_index;
    log(INFO, "Solo5: Application acquired '%s' as block device\n", name);
    return SOLO5_R_OK;
}

solo5_result_t solo5_block_write(solo5_handle_t h, solo5_off_t offset,
                                 const uint8_t *buf, size_t size)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    return nvme_rw(h, NVME_CMD_WRITE, offset, (uint8_t *)buf, size);
#pragma GCC diagnostic pop
}

solo5_result_t solo5_block_read(solo5_handle_t h, solo5_off_t offset,
                                uint8_t *buf, size_t size)
{
    return nvme_rw(h, NVME_CMD_READ, offset, buf, size);
}
