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
 * Amazon Elastic Network Adapter (ENA), the NIC of EC2's Nitro instances.
 *
 * Register and descriptor layouts are from Amazon's ena_com headers
 * (amzn-drivers). Host queue placement only: no LLQ.
 *
 * There is no public spec for this driver. The details were learned from the
 * Linux and FreeBSD drivers.
 */

#include "bindings.h"
#include "version.h"

#define ENA_MAX_DEVICES      2
#define ENA_PAGE_SIZE        4096
#define ENA_ADMIN_DEPTH      8
#define ENA_AENQ_DEPTH       8
#define ENA_ADMIN_ENTRY_SIZE 64
#define ENA_TX_DEPTH         128
#define ENA_RX_DEPTH         256
#define ENA_BUF_SIZE         2048
#define ENA_MTU              1500
#define ENA_ADMIN_TIMEOUT_NS (5ULL * 1000000000ULL)
/* RX polling while idle in solo5_yield */
#define ENA_POLL_INTERVAL_NS 1000000ULL
#define ENA_MIN_FRAME_SIZE   60 /* Ethernet, without FCS */

/* Registers (ena_regs_defs.h) */
#define ENA_REG_VERSION            0x00
#define ENA_REG_CONTROLLER_VERSION 0x04
#define ENA_REG_CAPS               0x08
#define ENA_REG_AQ_BASE_LO         0x10
#define ENA_REG_AQ_CAPS            0x18
#define ENA_REG_ACQ_BASE_LO        0x20
#define ENA_REG_ACQ_CAPS           0x28
#define ENA_REG_AQ_DB              0x2c
#define ENA_REG_AENQ_CAPS          0x34
#define ENA_REG_AENQ_BASE_LO       0x38
#define ENA_REG_DEV_CTL            0x54
#define ENA_REG_DEV_STS            0x58

#define ENA_CAPS_RESET_TIMEOUT(caps)      (((caps) >> 1) & 0x1f)
#define ENA_RESET_TIMEOUT_UNIT_NS         100000000ULL
#define ENA_RESET_TIMEOUT_MIN_NS          1000000000ULL
#define ENA_QUEUE_CAPS(depth, entry_size) ((depth) | ((entry_size) << 16))
#define ENA_DEV_CTL_RESET                 (1u << 0)
#define ENA_DEV_STS_READY                 (1u << 0)
#define ENA_DEV_STS_RESET_IN_PROGRESS     (1u << 3)

/* Admin opcodes and features (ena_admin_defs.h) */
#define ENA_ADMIN_CREATE_SQ           1
#define ENA_ADMIN_CREATE_CQ           3
#define ENA_ADMIN_GET_FEATURE         8
#define ENA_ADMIN_SET_FEATURE         9
#define ENA_FEATURE_DEVICE_ATTRIBUTES 1
#define ENA_FEATURE_AENQ_CONFIG       26
#define ENA_FEATURE_HOST_ATTR_CONFIG  28

#define ENA_SQ_DIRECTION_TX          (1u << 5)
#define ENA_SQ_DIRECTION_RX          (2u << 5)
#define ENA_PLACEMENT_POLICY_HOST    1
#define ENA_SQ_PHYSICALLY_CONTIGUOUS 1
#define ENA_MSIX_VECTOR_NONE         0xffffffffu

#define ENA_ADMIN_ID_MASK     0xfff
#define ENA_ADMIN_PHASE       (1u << 0)
#define ENA_FEATURE_DATA_SIZE 44

/* Host info (ena_admin_host_info) */
/*
 * There is no OS type for unikernels. DPDK is closest: a single-process,
 * polled driver with no interrupts or kernel network stack.
 * XXX: ask Amazon (amzn-drivers) to assign an OS type for Solo5/MirageOS.
 */
#define ENA_OS_DPDK             3
#define ENA_HOST_DRIVER_VERSION 2
#define ENA_SPEC_VERSION_2_0    0x0200
#define ENA_OS_DIST             "Solo5"

/* Descriptor bits (ena_eth_io_defs.h) */
#define ENA_TX_DESC_REQ_ID_HI_SHIFT 16
#define ENA_TX_DESC_PHASE           (1u << 24)
#define ENA_TX_DESC_FIRST           (1u << 26)
#define ENA_TX_DESC_LAST            (1u << 27)
#define ENA_TX_DESC_COMP_REQ        (1u << 28)
#define ENA_TX_DESC_REQ_ID_LO_SHIFT 22
#define ENA_TX_DESC_REQ_ID_LO_BITS  10
#define ENA_TX_DESC_REQ_ID_LO_MASK  ((1u << ENA_TX_DESC_REQ_ID_LO_BITS) - 1)
#define ENA_TX_DESC_ADDR_HI_MASK    0xffff

#define ENA_RX_DESC_PHASE    (1u << 0)
#define ENA_RX_DESC_FIRST    (1u << 2)
#define ENA_RX_DESC_LAST     (1u << 3)
#define ENA_RX_DESC_COMP_REQ (1u << 4)

#define ENA_RX_CDESC_PHASE (1u << 24)
#define ENA_RX_CDESC_FIRST (1u << 26)
#define ENA_RX_CDESC_LAST  (1u << 27)

#define ENA_TX_CDESC_PHASE (1u << 0)

struct ena_host_info {
    uint32_t os_type;
    char os_dist_str[128];
    uint32_t os_dist;
    char kernel_ver_str[32];
    uint32_t kernel_ver;
    uint32_t driver_version;
    uint32_t supported_network_features[2];
    uint16_t ena_spec_version;
    uint16_t bdf;
    uint16_t num_cpus;
    uint16_t reserved;
    uint32_t driver_supported_features;
} __attribute__((packed));

struct ena_tx_desc {
    uint32_t len_ctrl;
    uint32_t meta_ctrl;
    uint32_t buff_addr_lo;
    uint32_t buff_addr_hi_hdr_sz;
} __attribute__((packed));

struct ena_tx_cdesc {
    uint16_t req_id;
    uint8_t status;
    uint8_t flags;
    uint16_t sub_qid;
    uint16_t sq_head_idx;
} __attribute__((packed));

struct ena_rx_desc {
    uint16_t length;
    uint8_t reserved2;
    uint8_t ctrl;
    uint16_t req_id;
    uint16_t reserved6;
    uint32_t buff_addr_lo;
    uint16_t buff_addr_hi;
    uint16_t reserved16_w3;
} __attribute__((packed));

struct ena_rx_cdesc {
    uint32_t status;
    uint16_t length;
    uint16_t req_id;
    uint32_t hash;
    uint16_t sub_qid;
    uint8_t offset;
    uint8_t reserved;
} __attribute__((packed));

/* Admin queue layouts (ena_admin_defs.h) */
struct ena_mem_addr {
    uint32_t lo;
    uint16_t hi;
    uint16_t reserved;
} __attribute__((packed));

struct ena_admin_cmd {
    uint16_t id;
    uint8_t opcode;
    uint8_t flags;
    union {
        struct {
            uint8_t caps_1; /* interrupt mode */
            uint8_t entry_words;
            uint16_t depth;
            uint32_t msix_vector;
            struct ena_mem_addr ring;
        } __attribute__((packed)) create_cq;
        struct {
            uint8_t direction;
            uint8_t reserved;
            uint8_t policy;
            uint8_t caps_3;
            uint16_t cq_id;
            uint16_t depth;
            struct ena_mem_addr ring;
        } __attribute__((packed)) create_sq;
        struct {
            uint32_t buf_length;
            struct ena_mem_addr buf;
            uint8_t flags;
            uint8_t id;
            uint8_t version;
            uint8_t reserved;
            uint8_t data[ENA_FEATURE_DATA_SIZE];
        } __attribute__((packed)) feature;
    };
} __attribute__((packed));

struct ena_admin_rsp {
    uint16_t id;
    uint8_t status;
    uint8_t flags;
    uint16_t extended_status;
    uint16_t sq_head;
    union {
        struct {
            uint16_t cq_id;
            uint16_t depth;
        } __attribute__((packed)) create_cq;
        struct {
            uint16_t sq_id;
            uint16_t reserved;
            uint32_t doorbell;
        } __attribute__((packed)) create_sq;
        struct {
            uint32_t impl_id;
            uint32_t device_version;
            uint32_t features;
            uint32_t reserved;
            uint32_t phys_addr_width;
            uint32_t virt_addr_width;
            uint8_t mac[6];
            uint8_t reserved2[2];
            uint32_t max_mtu;
        } __attribute__((packed)) dev_attr;
        uint8_t data[ENA_ADMIN_ENTRY_SIZE - 8];
    };
} __attribute__((packed));

/* Feature data */
struct ena_aenq_config {
    uint32_t supported_groups;
    uint32_t enabled_groups;
} __attribute__((packed));

struct ena_host_attr {
    struct ena_mem_addr host_info;
    struct ena_mem_addr debug_area;
    uint32_t debug_area_size;
} __attribute__((packed));

_Static_assert(sizeof(struct ena_admin_cmd) == ENA_ADMIN_ENTRY_SIZE, "cmd");
_Static_assert(sizeof(struct ena_admin_rsp) == ENA_ADMIN_ENTRY_SIZE, "rsp");
_Static_assert(sizeof(struct ena_host_attr) == 20, "ena_host_attr");
_Static_assert(sizeof(struct ena_tx_desc) == 16, "ena_tx_desc");
_Static_assert(sizeof(struct ena_tx_cdesc) == 8, "ena_tx_cdesc");
_Static_assert(sizeof(struct ena_rx_desc) == 16, "ena_rx_desc");
_Static_assert(sizeof(struct ena_rx_cdesc) == 16, "ena_rx_cdesc");

struct ena_io_queue {
    uint16_t sq_id, cq_id;
    uint32_t sq_doorbell;
    unsigned depth;
    unsigned cq_depth;
    unsigned fill; /* descriptors in use, min(depth, cq_depth) */
    uint32_t sq_prod, cq_cons;
};

struct ena_device {
    struct pci_dev pci;
    uint64_t regs;
    uint32_t admin_prod, admin_cons;
    uint8_t mac[6];
    uint32_t max_mtu;
    struct ena_io_queue tx, rx;
    uint16_t tx_free[ENA_TX_DEPTH];
    unsigned tx_free_num;
    solo5_handle_t handle;
};

static struct {
    struct ena_admin_cmd aq[ENA_ADMIN_DEPTH]
        __attribute__((aligned(ENA_PAGE_SIZE)));
    struct ena_admin_rsp acq[ENA_ADMIN_DEPTH]
        __attribute__((aligned(ENA_PAGE_SIZE)));
    uint8_t aenq[ENA_PAGE_SIZE] __attribute__((aligned(ENA_PAGE_SIZE)));
    uint8_t host_info[ENA_PAGE_SIZE];
    struct ena_tx_desc tx_sq[ENA_TX_DEPTH]
        __attribute__((aligned(ENA_PAGE_SIZE)));
    struct ena_tx_cdesc tx_cq[ENA_TX_DEPTH]
        __attribute__((aligned(ENA_PAGE_SIZE)));
    struct ena_rx_desc rx_sq[ENA_RX_DEPTH]
        __attribute__((aligned(ENA_PAGE_SIZE)));
    struct ena_rx_cdesc rx_cq[ENA_RX_DEPTH]
        __attribute__((aligned(ENA_PAGE_SIZE)));
    uint8_t tx_buf[ENA_TX_DEPTH][ENA_BUF_SIZE];
    uint8_t rx_buf[ENA_RX_DEPTH][ENA_BUF_SIZE];
} ena_mem[ENA_MAX_DEVICES] __attribute__((aligned(ENA_PAGE_SIZE)));

static struct ena_device devices[ENA_MAX_DEVICES];
static unsigned devices_num;

static struct ena_mem_addr ena_addr(const void *p)
{
    uint64_t a = (uint64_t)p;
    struct ena_mem_addr m = {.lo = (uint32_t)a, .hi = (uint16_t)(a >> 32)};
    return m;
}

static int ena_wait_sts(const struct ena_device *d, uint32_t mask,
                        uint32_t want, uint64_t timeout_ns)
{
    solo5_time_t deadline = solo5_clock_monotonic() + timeout_ns;
    while ((mmio_read32(d->regs + ENA_REG_DEV_STS) & mask) != want) {
        if (solo5_clock_monotonic() > deadline)
            return -1;
        cpu_relax();
    }
    return 0;
}

/* Returns the completion status, or -1 on timeout. */
static int ena_admin(struct ena_device *d, struct ena_admin_cmd *cmd,
                     struct ena_admin_rsp *rsp)
{
    unsigned n = d - devices;
    unsigned i = d->admin_prod % ENA_ADMIN_DEPTH;
    uint16_t id = d->admin_prod & ENA_ADMIN_ID_MASK;
    uint8_t phase = ((d->admin_prod / ENA_ADMIN_DEPTH) & 1) ^ 1;

    cmd->id = id;
    cmd->flags = phase;
    ena_mem[n].aq[i] = *cmd;
    d->admin_prod++;
    ec2_mb();
    mmio_write32(d->regs + ENA_REG_AQ_DB, d->admin_prod);

    unsigned j = d->admin_cons % ENA_ADMIN_DEPTH;
    volatile struct ena_admin_rsp *cqe = &ena_mem[n].acq[j];
    uint8_t cq_phase = ((d->admin_cons / ENA_ADMIN_DEPTH) & 1) ^ 1;
    solo5_time_t deadline = solo5_clock_monotonic() + ENA_ADMIN_TIMEOUT_NS;
    while ((cqe->flags & ENA_ADMIN_PHASE) != cq_phase) {
        if (solo5_clock_monotonic() > deadline) {
            log(WARN, "Solo5: ENA: admin command %u (opcode %u) timed out\n",
                id, cmd->opcode);
            return -1;
        }
        cpu_relax();
    }
    cc_barrier();
    *rsp = ena_mem[n].acq[j];
    d->admin_cons++;

    uint16_t rsp_id = rsp->id & ENA_ADMIN_ID_MASK;
    if (rsp_id != id) {
        log(WARN, "Solo5: ENA: admin completion for %u, expected %u\n", rsp_id,
            id);
        return -1;
    }
    if (rsp->status != 0)
        log(WARN, "Solo5: ENA: admin opcode %u failed, status %u\n",
            cmd->opcode, rsp->status);
    return rsp->status;
}

static int ena_set_feature(struct ena_device *d, uint8_t feature,
                           const void *data, size_t len,
                           struct ena_admin_rsp *rsp)
{
    struct ena_admin_cmd cmd = {0};
    cmd.opcode = ENA_ADMIN_SET_FEATURE;
    cmd.feature.id = feature;
    memcpy(cmd.feature.data, data, len);
    return ena_admin(d, &cmd, rsp);
}

static int ena_get_feature(struct ena_device *d, uint8_t feature,
                           struct ena_admin_rsp *rsp)
{
    struct ena_admin_cmd cmd = {0};
    cmd.opcode = ENA_ADMIN_GET_FEATURE;
    cmd.feature.id = feature;
    return ena_admin(d, &cmd, rsp);
}

static int ena_create_cq(struct ena_device *d, struct ena_io_queue *q,
                         void *ring, unsigned entry_words)
{
    struct ena_admin_cmd cmd = {0};
    struct ena_admin_rsp rsp;
    cmd.opcode = ENA_ADMIN_CREATE_CQ;
    cmd.create_cq.caps_1 = 0; /* interrupts disabled */
    cmd.create_cq.entry_words = entry_words;
    cmd.create_cq.depth = q->depth;
    cmd.create_cq.msix_vector = ENA_MSIX_VECTOR_NONE;
    cmd.create_cq.ring = ena_addr(ring);
    if (ena_admin(d, &cmd, &rsp) != 0)
        return -1;
    q->cq_id = rsp.create_cq.cq_id;
    q->cq_depth = rsp.create_cq.depth;
    return 0;
}

static int ena_create_sq(struct ena_device *d, struct ena_io_queue *q,
                         uint8_t direction, void *ring)
{
    struct ena_admin_cmd cmd = {0};
    struct ena_admin_rsp rsp;
    cmd.opcode = ENA_ADMIN_CREATE_SQ;
    cmd.create_sq.direction = direction;
    cmd.create_sq.policy = ENA_PLACEMENT_POLICY_HOST;
    cmd.create_sq.caps_3 = ENA_SQ_PHYSICALLY_CONTIGUOUS;
    cmd.create_sq.cq_id = q->cq_id;
    cmd.create_sq.depth = q->depth;
    cmd.create_sq.ring = ena_addr(ring);
    if (ena_admin(d, &cmd, &rsp) != 0)
        return -1;
    q->sq_id = rsp.create_sq.sq_id;
    q->sq_doorbell = rsp.create_sq.doorbell;
    return 0;
}

static int ena_create_queue_pair(struct ena_device *d, struct ena_io_queue *q,
                                 uint8_t direction, unsigned depth,
                                 void *sq_ring, void *cq_ring,
                                 unsigned cq_entry_words)
{
    q->depth = depth;
    if (ena_create_cq(d, q, cq_ring, cq_entry_words) != 0 ||
        ena_create_sq(d, q, direction, sq_ring) != 0)
        return -1;
    if (q->cq_depth == 0 || (q->cq_depth & (q->cq_depth - 1)) != 0 ||
        q->cq_depth > depth) {
        log(WARN, "Solo5: ENA: unusable CQ depth %u\n", q->cq_depth);
        return -1;
    }
    q->fill = q->cq_depth < q->depth ? q->cq_depth : q->depth;
    q->sq_prod = 0;
    q->cq_cons = 0;
    return 0;
}

static void ena_rx_post(struct ena_device *d, uint16_t id)
{
    unsigned i = d->rx.sq_prod % d->rx.depth;
    struct ena_rx_desc *desc = &ena_mem[d - devices].rx_sq[i];
    uint64_t a = (uint64_t)ena_mem[d - devices].rx_buf[id];
    uint8_t phase = ((d->rx.sq_prod / d->rx.depth) & 1) ^ 1;

    desc->length = ENA_BUF_SIZE;
    desc->req_id = id;
    desc->buff_addr_lo = (uint32_t)a;
    desc->buff_addr_hi = (uint16_t)(a >> 32);
    cc_barrier();
    desc->ctrl = (phase ? ENA_RX_DESC_PHASE : 0) | ENA_RX_DESC_FIRST |
                 ENA_RX_DESC_LAST | ENA_RX_DESC_COMP_REQ;
    d->rx.sq_prod++;
}

static void ena_rx_doorbell(struct ena_device *d)
{
    ec2_mb();
    mmio_write32(d->regs + d->rx.sq_doorbell, d->rx.sq_prod);
}

static int ena_bring_up(struct ena_device *d)
{
    struct ena_admin_rsp rsp;
    unsigned n = d - devices;
    uint32_t version = mmio_read32(d->regs + ENA_REG_VERSION);
    uint32_t ctrl_version = mmio_read32(d->regs + ENA_REG_CONTROLLER_VERSION);
    uint32_t caps = mmio_read32(d->regs + ENA_REG_CAPS);
    uint64_t reset_ns =
        ENA_CAPS_RESET_TIMEOUT(caps) * ENA_RESET_TIMEOUT_UNIT_NS;
    if (reset_ns < ENA_RESET_TIMEOUT_MIN_NS)
        reset_ns = ENA_RESET_TIMEOUT_MIN_NS;

    log(INFO,
        "Solo5: ENA: PCI:%02x:%02x.%x: version 0x%x, controller 0x%x, "
        "caps 0x%x, status 0x%x\n",
        d->pci.bus, d->pci.dev, d->pci.fn, version, ctrl_version, caps,
        mmio_read32(d->regs + ENA_REG_DEV_STS));

    mmio_write32(d->regs + ENA_REG_DEV_CTL, ENA_DEV_CTL_RESET);
    if (ena_wait_sts(d, ENA_DEV_STS_RESET_IN_PROGRESS,
                     ENA_DEV_STS_RESET_IN_PROGRESS, reset_ns) != 0) {
        log(WARN, "Solo5: ENA: reset not acknowledged\n");
        return -1;
    }
    mmio_write32(d->regs + ENA_REG_DEV_CTL, 0);
    if (ena_wait_sts(d, ENA_DEV_STS_RESET_IN_PROGRESS, 0, reset_ns) != 0 ||
        ena_wait_sts(d, ENA_DEV_STS_READY, ENA_DEV_STS_READY, reset_ns) != 0) {
        log(WARN, "Solo5: ENA: device not ready after reset\n");
        return -1;
    }

    memset(ena_mem[n].aq, 0, sizeof ena_mem[n].aq);
    memset(ena_mem[n].acq, 0, sizeof ena_mem[n].acq);
    memset(ena_mem[n].aenq, 0, sizeof ena_mem[n].aenq);
    mmio_write64(d->regs + ENA_REG_ACQ_BASE_LO, (uint64_t)ena_mem[n].acq);
    mmio_write32(d->regs + ENA_REG_ACQ_CAPS,
                 ENA_QUEUE_CAPS(ENA_ADMIN_DEPTH, ENA_ADMIN_ENTRY_SIZE));
    mmio_write64(d->regs + ENA_REG_AQ_BASE_LO, (uint64_t)ena_mem[n].aq);
    mmio_write32(d->regs + ENA_REG_AQ_CAPS,
                 ENA_QUEUE_CAPS(ENA_ADMIN_DEPTH, ENA_ADMIN_ENTRY_SIZE));
    mmio_write64(d->regs + ENA_REG_AENQ_BASE_LO, (uint64_t)ena_mem[n].aenq);
    mmio_write32(d->regs + ENA_REG_AENQ_CAPS,
                 ENA_QUEUE_CAPS(ENA_AENQ_DEPTH, ENA_ADMIN_ENTRY_SIZE));
    d->admin_prod = 0;
    d->admin_cons = 0;

    /* The device needs an AENQ, but no events are enabled */
    struct ena_aenq_config aenq = {0};
    if (ena_set_feature(d, ENA_FEATURE_AENQ_CONFIG, &aenq, sizeof aenq, &rsp) !=
        0)
        return -1;

    struct ena_host_info *info = (struct ena_host_info *)ena_mem[n].host_info;
    memset(info, 0, ENA_PAGE_SIZE);
    info->os_type = ENA_OS_DPDK;
    memcpy(info->os_dist_str, ENA_OS_DIST, sizeof ENA_OS_DIST);
    memcpy(info->kernel_ver_str, SOLO5_VERSION,
           strlen(SOLO5_VERSION) < sizeof info->kernel_ver_str
               ? strlen(SOLO5_VERSION)
               : sizeof info->kernel_ver_str - 1);
    info->driver_version = ENA_HOST_DRIVER_VERSION;
    info->ena_spec_version = ENA_SPEC_VERSION_2_0;
    info->bdf = (d->pci.bus << 8) | (d->pci.dev << 3) | d->pci.fn;
    info->num_cpus = 1;
    struct ena_host_attr host_attr = {.host_info = ena_addr(info)};
    if (ena_set_feature(d, ENA_FEATURE_HOST_ATTR_CONFIG, &host_attr,
                        sizeof host_attr, &rsp) != 0)
        return -1;

    if (ena_get_feature(d, ENA_FEATURE_DEVICE_ATTRIBUTES, &rsp) != 0)
        return -1;
    memcpy(d->mac, rsp.dev_attr.mac, sizeof d->mac);
    d->max_mtu = rsp.dev_attr.max_mtu;
    log(INFO,
        "Solo5: ENA: MAC %02x:%02x:%02x:%02x:%02x:%02x, max MTU %u, "
        "features 0x%x\n",
        d->mac[0], d->mac[1], d->mac[2], d->mac[3], d->mac[4], d->mac[5],
        d->max_mtu, rsp.dev_attr.features);
    if (d->max_mtu < ENA_MTU) {
        log(WARN, "Solo5: ENA: max MTU %u is below %u\n", d->max_mtu, ENA_MTU);
        return -1;
    }

    if (ena_create_queue_pair(d, &d->tx, ENA_SQ_DIRECTION_TX, ENA_TX_DEPTH,
                              ena_mem[n].tx_sq, ena_mem[n].tx_cq,
                              sizeof(struct ena_tx_cdesc) / 4) != 0 ||
        ena_create_queue_pair(d, &d->rx, ENA_SQ_DIRECTION_RX, ENA_RX_DEPTH,
                              ena_mem[n].rx_sq, ena_mem[n].rx_cq,
                              sizeof(struct ena_rx_cdesc) / 4) != 0) {
        log(WARN, "Solo5: ENA: could not create I/O queues\n");
        return -1;
    }
    for (unsigned i = 0; i < d->tx.fill; i++)
        d->tx_free[i] = i;
    d->tx_free_num = d->tx.fill;
    for (unsigned i = 0; i < d->rx.fill; i++)
        ena_rx_post(d, i);
    ena_rx_doorbell(d);

    log(INFO,
        "Solo5: ENA: TX SQ %u/CQ %u (%u descriptors), RX SQ %u/CQ %u (%u)\n",
        d->tx.sq_id, d->tx.cq_id, d->tx.fill, d->rx.sq_id, d->rx.cq_id,
        d->rx.fill);
    return 0;
}

int ena_attach(const struct pci_dev *pci)
{
    if (devices_num == ENA_MAX_DEVICES) {
        log(WARN, "Solo5: ENA: too many devices\n");
        return -1;
    }

    struct mft_entry *e = NULL;
    unsigned mft_index = 0;
    for (unsigned i = 1; i < ec2_manifest->entries; i++) {
        if (ec2_manifest->e[i].type == MFT_DEV_NET_BASIC &&
            !ec2_manifest->e[i].attached) {
            e = &ec2_manifest->e[i];
            mft_index = i;
            break;
        }
    }
    if (e == NULL) {
        log(INFO,
            "Solo5: ENA: PCI:%02x:%02x.%x: not in the manifest, not used\n",
            pci->bus, pci->dev, pci->fn);
        return 0;
    }

    struct ena_device *d = &devices[devices_num];
    memset(d, 0, sizeof *d);
    d->pci = *pci;
    d->regs = pci_map_bar(pci, 0);
    if (d->regs == 0)
        return -1;
    if (ena_bring_up(d) != 0)
        return -1;

    d->handle = mft_index;
    e->b.hostfd = devices_num;
    memcpy(e->u.net_basic.mac, d->mac, sizeof e->u.net_basic.mac);
    e->u.net_basic.mtu = ENA_MTU;
    e->attached = true;
    devices_num++;
    log(INFO, "Solo5: ENA: attached as '%s'\n", e->name);
    return 0;
}

static struct ena_device *ena_of_handle(solo5_handle_t h)
{
    struct mft_entry *e = mft_get_by_index(ec2_manifest, h, MFT_DEV_NET_BASIC);
    if (e == NULL)
        return NULL;
    assert(e->attached);
    assert(e->b.hostfd >= 0 && (unsigned)e->b.hostfd < devices_num);
    return &devices[e->b.hostfd];
}

static void ena_tx_reap(struct ena_device *d)
{
    struct ena_tx_cdesc *cq = ena_mem[d - devices].tx_cq;
    for (;;) {
        unsigned i = d->tx.cq_cons % d->tx.cq_depth;
        uint8_t phase = ((d->tx.cq_cons / d->tx.cq_depth) & 1) ^ 1;
        volatile struct ena_tx_cdesc *c = &cq[i];
        if ((c->flags & ENA_TX_CDESC_PHASE) != phase)
            break;
        cc_barrier();
        uint16_t id = c->req_id;
        d->tx.cq_cons++;
        if (id < d->tx.fill && d->tx_free_num < d->tx.fill)
            d->tx_free[d->tx_free_num++] = id;
        else
            log(WARN, "Solo5: ENA: bad TX completion id %u\n", id);
    }
}

static bool ena_rx_ready(const struct ena_device *d)
{
    unsigned i = d->rx.cq_cons % d->rx.cq_depth;
    uint32_t phase = ((d->rx.cq_cons / d->rx.cq_depth) & 1) ^ 1;
    volatile const struct ena_rx_cdesc *c = &ena_mem[d - devices].rx_cq[i];
    return ((c->status & ENA_RX_CDESC_PHASE) != 0) == (phase != 0);
}

solo5_result_t solo5_net_acquire(const char *name, solo5_handle_t *h,
                                 struct solo5_net_info *info)
{
    unsigned mft_index;
    struct mft_entry *e =
        mft_get_by_name(ec2_manifest, name, MFT_DEV_NET_BASIC, &mft_index);
    if (e == NULL)
        return SOLO5_R_EINVAL;
    assert(e->attached);

    memcpy(info->mac_address, e->u.net_basic.mac, sizeof info->mac_address);
    info->mtu = e->u.net_basic.mtu;
    *h = mft_index;
    log(INFO, "Solo5: Application acquired '%s' as network device\n", name);
    return SOLO5_R_OK;
}

solo5_result_t solo5_net_write(solo5_handle_t h, const uint8_t *buf,
                               size_t size)
{
    struct ena_device *d = ena_of_handle(h);
    if (d == NULL)
        return SOLO5_R_EINVAL;
    if (size > ENA_MTU + SOLO5_NET_HLEN)
        return SOLO5_R_EINVAL;

    ena_tx_reap(d);
    if (d->tx_free_num == 0 || d->tx.sq_prod - d->tx.cq_cons >= d->tx.fill)
        return SOLO5_R_OK;
    uint16_t id = d->tx_free[--d->tx_free_num];
    uint8_t *data = ena_mem[d - devices].tx_buf[id];
    memcpy(data, buf, size);
    size_t len = size;
    if (len < ENA_MIN_FRAME_SIZE) {
        memset(data + len, 0, ENA_MIN_FRAME_SIZE - len);
        len = ENA_MIN_FRAME_SIZE;
    }

    unsigned i = d->tx.sq_prod % d->tx.depth;
    uint32_t phase = ((d->tx.sq_prod / d->tx.depth) & 1) ^ 1;
    struct ena_tx_desc *desc = &ena_mem[d - devices].tx_sq[i];
    uint64_t a = (uint64_t)data;
    uint32_t id_lo = id & ENA_TX_DESC_REQ_ID_LO_MASK;
    uint32_t id_hi = id >> ENA_TX_DESC_REQ_ID_LO_BITS;
    desc->meta_ctrl = id_lo << ENA_TX_DESC_REQ_ID_LO_SHIFT;
    desc->buff_addr_lo = (uint32_t)a;
    desc->buff_addr_hi_hdr_sz = (uint32_t)(a >> 32) & ENA_TX_DESC_ADDR_HI_MASK;
    cc_barrier();
    desc->len_ctrl = (uint32_t)len | (id_hi << ENA_TX_DESC_REQ_ID_HI_SHIFT) |
                     (phase ? ENA_TX_DESC_PHASE : 0) | ENA_TX_DESC_FIRST |
                     ENA_TX_DESC_LAST | ENA_TX_DESC_COMP_REQ;
    d->tx.sq_prod++;
    ec2_mb();
    mmio_write32(d->regs + d->tx.sq_doorbell, d->tx.sq_prod);
    return SOLO5_R_OK;
}

solo5_result_t solo5_net_read(solo5_handle_t h, uint8_t *buf, size_t size,
                              size_t *read_size)
{
    struct ena_device *d = ena_of_handle(h);
    if (d == NULL)
        return SOLO5_R_EINVAL;

    while (ena_rx_ready(d)) {
        unsigned i = d->rx.cq_cons % d->rx.cq_depth;
        struct ena_rx_cdesc *c = &ena_mem[d - devices].rx_cq[i];
        cc_barrier();
        uint32_t status = c->status;
        uint16_t id = c->req_id;
        size_t len = c->length;
        d->rx.cq_cons++;
        if (id >= d->rx.fill) {
            log(WARN, "Solo5: ENA: bad RX completion id %u\n", id);
            continue;
        }

        bool whole =
            (status & ENA_RX_CDESC_FIRST) && (status & ENA_RX_CDESC_LAST);
        bool fits = whole && len <= size && len <= ENA_BUF_SIZE;
        if (fits)
            memcpy(buf, ena_mem[d - devices].rx_buf[id], len);
        ena_rx_post(d, id);
        ena_rx_doorbell(d);
        if (fits) {
            *read_size = len;
            return SOLO5_R_OK;
        }
    }
    return SOLO5_R_AGAIN;
}

static solo5_handle_set_t ena_ready_set(void)
{
    solo5_handle_set_t set = 0;
    for (unsigned i = 0; i < devices_num; i++) {
        if (ena_rx_ready(&devices[i]))
            set |= 1ULL << devices[i].handle;
    }
    return set;
}

/*
 * As on virtio, the NIC is the only device that solo5_yield() waits on, so it
 * lives here. ENA interrupts are not used: we wake on the timer and poll.
 */
void solo5_yield(solo5_time_t deadline, solo5_handle_set_t *ready_set)
{
    solo5_handle_set_t set = ena_ready_set();

    cpu_intr_disable();
    while (set == 0) {
        solo5_time_t now = solo5_clock_monotonic();
        if (now >= deadline)
            break;
        solo5_time_t until = deadline;
        if (devices_num > 0 && now + ENA_POLL_INTERVAL_NS < until)
            until = now + ENA_POLL_INTERVAL_NS;
        cpu_block(until);
        set = ena_ready_set();
    }
    cpu_intr_enable();

    if (ready_set)
        *ready_set = set;
}
