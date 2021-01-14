#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <linux/vfio.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "../common/pci_attach.h"
#include "hvt.h"
#include "hvt_kvm.h"
#include "hvt_cpu_x86_64.h"
#include "solo5.h"
#include "hvt_abi.h"

static bool module_in_use;
static unsigned device_index = 0;
static int max_slots;
static int current_slot = 1; // slot 0 contains the unikernel's main memory
static struct mft *host_mft;

static int handle_cmdarg(char *cmdarg, struct mft *mft)
{
    char name[MFT_NAME_SIZE];
    unsigned dom, bus, dev, func;
    char addr[13]; /* PCIe addresses look like '00:00.0'. */
    int rc = sscanf(cmdarg, "--pci:%" XSTR(MFT_NAME_MAX)
            "[A-Za-z0-9]=%4x:%2x:%2x.%1x", name, &dom, &bus, &dev, &func);
    if (rc != 5)
        return -1;

    sprintf(addr, "%04x:%02x:%02x.%01x", dom, bus, dev, func);

    struct mft_entry *e = mft_get_by_name(mft, name, MFT_DEV_PCI_BASIC, NULL);
    if (e == NULL) {
        warnx("Resource not declared in manifest: '%s'", name);
        return -1;
    }

    printf("attaching device %s\n", name);

    int fd = pci_attach(addr);
    if (fd < 0) {
        errx(1, "Could not attach PCIe device: %s", name);
    }

    // TODO check vendor, device_id, ...

    e->u.pci_basic.device_index = device_index++;

    if (e->u.pci_basic.bus_master_enable) {
        pci_enable_dma(fd);
    }

    e->b.hostfd = fd;
    e->attached = true;

    module_in_use = true;

    return 0;
}

#define HUGE_PAGE_BITS 21
#define HUGE_PAGE_SIZE (1 << HUGE_PAGE_BITS)

static void hvt_x86_setup_dma_pagetable(struct hvt *hvt, struct mft *mft)
{
    size_t size = mft->dma_size;
    assert((size & (HUGE_PAGE_SIZE - 1)) == 0);
    assert(size >= HUGE_PAGE_SIZE);
    assert(size < (512 * HUGE_PAGE_SIZE));

    uint64_t *pml4 = (uint64_t *) (hvt->mem + X86_PML4_BASE);
    uint64_t *pdpte = (uint64_t *) (hvt->mem + X86_PDPTE_DMA_BASE);
    uint64_t *pde = (uint64_t *) (hvt->mem + X86_PDE_DMA_BASE);

    memset(pdpte, 0, X86_PDPTE_DMA_SIZE);
    memset(pde, 0, X86_PDE_DMA_SIZE);

    pml4[2] = X86_PDPTE_DMA_BASE | (X86_PDPT_P | X86_PDPT_RW);
    pdpte[0] = X86_PDE_DMA_BASE | (X86_PDPT_P | X86_PDPT_RW);

    uint64_t paddr = HVT_DMA_BASE;
    for (; paddr < HVT_DMA_BASE + size; paddr += HUGE_PAGE_SIZE, pde++)
        *pde = paddr | (X86_PDPT_P | X86_PDPT_RW | X86_PDPT_PS);
}

/*
 * TODO this works but is super-wonky
 * We always map 2 MiB per region into the unikernel, even if the region is
 * smaller. We only support 1 pci device.
 */
static void hvt_x86_setup_pci_device_pagetable(struct hvt *hvt,
        struct mft_entry *e)
{
    assert(e->type == MFT_DEV_PCI_BASIC);
    struct mft_pci_basic pci = e->u.pci_basic;
    assert(pci.device_index == 0);

    uint64_t *pml4 = (uint64_t *) (hvt->mem + X86_PML4_BASE);
    uint64_t *pdpte = (uint64_t *) (hvt->mem + X86_PDPTE_PCI_BASE);
    uint64_t *pde = (uint64_t *) (hvt->mem + X86_PDE_PCI_BASE);

    pml4[1] = X86_PDPTE_PCI_BASE | (X86_PDPT_P | X86_PDPT_RW);
    pdpte[0] = X86_PDE_PCI_BASE | (X86_PDPT_P | X86_PDPT_RW);

#define setup_region_pagetable(n) \
    do { \
        if (pci.map_bar ## n) { \
            assert(pci.bar ## n ## _size <= HUGE_PAGE_SIZE); \
            pde[n] = HVT_REGION(pci.device_index, (n)) | \
                    (X86_PDPT_P | X86_PDPT_RW | X86_PDPT_PS); \
        } \
    } while (0)

    setup_region_pagetable(0);
    setup_region_pagetable(1);
    setup_region_pagetable(2);
    setup_region_pagetable(3);
    setup_region_pagetable(4);
    setup_region_pagetable(5);
}

static int get_slot()
{
    if (current_slot >= max_slots) {
        errx(1, "max_slots (%d) exceeded", max_slots);
    }
    return current_slot++;
}

static int setup(struct hvt *hvt, struct mft *mft)
{
    if (!module_in_use && mft->dma_size != 0)
        warn("DMA memory requested but no PCI devices mapped, "
                "ignoring DMA request");

    if (!module_in_use)
        return 0;

    host_mft = mft;

    max_slots = ioctl(hvt->b->kvmfd, KVM_CHECK_EXTENSION, KVM_CAP_NR_MEMSLOTS);
    if (max_slots <= 0) {
        err(1, "could not get KVM_CAP_NR_MEMSLOTS");
    }

    if (mft->dma_size > 0) {
        if (mft->dma_size & (HUGE_PAGE_SIZE - 1)) {
            mft->dma_size =
                    ((mft->dma_size >> HUGE_PAGE_BITS) + 1) << HUGE_PAGE_BITS;
        }
        void *buffer = pci_allocate_dma(mft->dma_size);
        if (buffer == MAP_FAILED) {
            err(1, "could not allocate dma memory");
        }

        struct vfio_iommu_type1_dma_map dma_map = {
            .argsz = sizeof(dma_map),
            .flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE,
            .vaddr = (uint64_t) buffer,
            .iova = HVT_DMA_BASE,
            .size = mft->dma_size
        };
        if (ioctl(get_vfio_container(), VFIO_IOMMU_MAP_DMA, &dma_map) == -1) {
            err(1, "could not map dma vfio");
        }

        // make sure the DMA-memory is page-aligned
        // see KVM_SET_USER_MEMORY_REGION documentation
        assert((((uint64_t) buffer) & (HUGE_PAGE_SIZE - 1)) == 0);

        struct kvm_userspace_memory_region region = {
            .slot = get_slot(),
            .guest_phys_addr = HVT_DMA_BASE,
            .memory_size = mft->dma_size,
            .userspace_addr = (uint64_t) buffer
        };
        if (ioctl(hvt->b->vmfd, KVM_SET_USER_MEMORY_REGION, &region) == -1)
            err(1, "KVM: ioctl (SET_USER_MEMORY_REGION) for DMA memory failed");

        hvt_x86_setup_dma_pagetable(hvt, mft);
    }

    for (int i = 0; i < mft->entries; i++) {
        struct mft_entry *e = &mft->e[i];

#define map_region(n) \
    do { \
        if (e->u.pci_basic.map_bar ## n) { \
            size_t size; \
            void *buffer = pci_map_region(e->b.hostfd, \
                    VFIO_PCI_BAR ## n ## _REGION_INDEX, &size); \
            if (buffer == MAP_FAILED) { \
                errx(1, "could not map BAR" #n); \
            } \
            e->u.pci_basic.bar ## n ## _size = size; \
            uint64_t phys_addr = HVT_REGION(e->u.pci_basic.device_index, n); \
            printf("mapping BAR" #n " to %#lx\n", phys_addr); \
            struct kvm_userspace_memory_region region = { \
                .slot = get_slot(), \
                .guest_phys_addr = phys_addr, \
                .memory_size = size, \
                .userspace_addr = (uint64_t) buffer \
            }; \
            if (ioctl(hvt->b->vmfd, KVM_SET_USER_MEMORY_REGION, \
                    &region) == -1) \
                err(1, "KVM: ioctl (SET_USER_MEMORY_REGION) for " \
                        "BAR" #n " failed"); \
            hvt_x86_setup_pci_device_pagetable(hvt, e); \
        } \
    } while (0)

        if (e->type == MFT_DEV_PCI_BASIC) {
            map_region(0);
            map_region(1);
            map_region(2);
            map_region(3);
            map_region(4);
            map_region(5);
        }
    }

    return 0;
}

static char *usage(void)
{
    return "--pci:NAME=ADDR (attach PCIe device at address ADDR as NAME)";
}

DECLARE_MODULE(pci,
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
)
