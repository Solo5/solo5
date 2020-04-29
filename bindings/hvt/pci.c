#include "bindings.h"

static const struct mft *mft;

#define setup_bar(index, n) \
    do { \
        if (e->u.pci_basic.map_bar ## n) { \
            info->bar ## n = (uint8_t *) HVT_REGION(index, n); \
            info->bar ## n ## _size = e->u.pci_basic.bar ## n ## _size; \
        } else { \
            info->bar ## n = NULL; \
            info->bar ## n ## _size = 0; \
        } \
    } while (0)

solo5_result_t solo5_pci_acquire(const char *name, struct solo5_pci_info *info)
{
    unsigned mft_index;
    const struct mft_entry *e =
        mft_get_by_name(mft, name, MFT_DEV_PCI_BASIC, &mft_index);
    if (e == NULL)
        return SOLO5_R_EINVAL;
    assert(e->attached);

    info->bus_master_enable = e->u.pci_basic.bus_master_enable;
    info->class_code = e->u.pci_basic.class_code;
    info->subclass_code = e->u.pci_basic.subclass_code;
    info->progif = e->u.pci_basic.progif;
    info->vendor_id = e->u.pci_basic.vendor;
    info->device_id = e->u.pci_basic.device_id;

    uint8_t index = e->u.pci_basic.device_index;
    setup_bar(index, 0);
    setup_bar(index, 1);
    setup_bar(index, 2);
    setup_bar(index, 3);
    setup_bar(index, 4);
    setup_bar(index, 5);

    assert(info->bar0_size != 0);

    return SOLO5_R_OK;
}

solo5_result_t solo5_dma_acquire(uint8_t **buffer, size_t *size)
{
    if (mft->dma_size > 0) {
        *buffer = (uint8_t *) HVT_DMA_BASE;
        *size = mft->dma_size;
        return SOLO5_R_OK;
    } else {
        *buffer = NULL;
        *size = 0;
        return SOLO5_R_EINVAL;
    }
}

void pci_init(const struct hvt_boot_info *bi)
{
    mft = bi->mft;
}
