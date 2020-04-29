/*
 * Adapted from libixy-vfio.
 * https://github.com/emmericp/ixy
 */

#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <linux/limits.h>
#include <linux/vfio.h>
#include <linux/mman.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>

#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include "pci_attach.h"

volatile int VFIO_CONTAINER_FILE_DESCRIPTOR = -1;

int get_vfio_container()
{
    return VFIO_CONTAINER_FILE_DESCRIPTOR;
}

void set_vfio_container(int fd)
{
    VFIO_CONTAINER_FILE_DESCRIPTOR = fd;
}

size_t MIN_DMA_MEMORY = 4096; // we can not allocate less than page_size memory

void pci_enable_dma(int vfio_fd) {
    // write to the command register (offset 4) in the PCIe config space
    int command_register_offset = 4;
    // bit 2 is "bus master enable", see PCIe 3.0 specification section 7.5.1.1
    int bus_master_enable_bit = 2;
    // Get region info for config region
    struct vfio_region_info conf_reg = {.argsz = sizeof(conf_reg)};
    conf_reg.index = VFIO_PCI_CONFIG_REGION_INDEX;
    if (ioctl(vfio_fd, VFIO_DEVICE_GET_REGION_INFO, &conf_reg) == -1) {
        err(1, "Failed to get vfio config region info.");
    }
    uint16_t dma = 0;
    assert(pread(vfio_fd, &dma, 2, conf_reg.offset + command_register_offset) == 2);
    dma |= 1 << bus_master_enable_bit;
    assert(pwrite(vfio_fd, &dma, 2, conf_reg.offset + command_register_offset) == 2);
}

// returns the devices file descriptor or -1 on error
int pci_attach(const char *pci_addr) {
    // find iommu group for the device
    // `readlink /sys/bus/pci/device/<segn:busn:devn.funcn>/iommu_group`
    char path[PATH_MAX], iommu_group_path[PATH_MAX];
    struct stat st;
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/", pci_addr);
    if (stat(path, &st) == -1) {
        errx(1, "no such device: %s", pci_addr);
    }
    strncat(path, "iommu_group", sizeof(path) - strlen(path) - 1);

    int len = readlink(path, iommu_group_path, sizeof(iommu_group_path));
    if (len == -1) {
        err(1, "Failed to find the iommu_group for the device %s", pci_addr);
    }

    iommu_group_path[len] = '\0'; // append 0x00 to the string to end it
    char* group_name = basename(iommu_group_path);
    int groupid;
    if (sscanf(group_name, "%d", &groupid) == -1) {
        errx(1, "Failed to convert group id to int.");
    }

    int firstsetup = 0; // Need to set up the container exactly once
    int cfd = get_vfio_container();
    if (cfd == -1) {
        firstsetup = 1;
        // open vfio file to create new vfio container
        cfd = open("/dev/vfio/vfio", O_RDWR);
        if (cfd == -1) {
            err(1, "Failed to open '/dev/vfio/vfio'");
        }
        set_vfio_container(cfd);

        // check if the container's API version is the same as the VFIO API's
        if (ioctl(cfd, VFIO_GET_API_VERSION) != VFIO_API_VERSION) {
            err(1, "Failed to get a valid API version from the container.");
        }

        // check if type1 is supported
        if (ioctl(cfd, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU) != 1) {
            err(1, "Failed to get Type1 IOMMU support from the IOMMU container.");
        }
    }

    // open VFIO group containing the device
    snprintf(path, sizeof(path), "/dev/vfio/%d", groupid);
    int vfio_gfd = open(path, O_RDWR);
    if (vfio_gfd == -1) {
        err(1, "Failed to open vfio group.");
    }

    // check if group is viable
    struct vfio_group_status group_status = {.argsz = sizeof(group_status)};
    if (ioctl(vfio_gfd, VFIO_GROUP_GET_STATUS, &group_status) == -1) {
        err(1, "Failed to get VFIO group status.");
    }
    if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
        errx(1, "Failed to get viable VFIO group - "
                "are all devices in the group bound to the VFIO driver?");
    }

    // Add group to container
    if (ioctl(vfio_gfd, VFIO_GROUP_SET_CONTAINER, &cfd) == -1) {
        err(1, "Failed to set container.");
    }

    if (firstsetup != 0) {
        // Set vfio type (type1 is for IOMMU like VT-d or AMD-Vi) for the
        // container.
        // This can only be done after at least one group is in the container.
        if (ioctl(cfd, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU) == -1) {
            err(1, "Failed to set IOMMU type.");
        }
    }

    // get device file descriptor
    int vfio_fd = ioctl(vfio_gfd, VFIO_GROUP_GET_DEVICE_FD, pci_addr);
    if (vfio_fd == -1) {
        err(1, "Failed to get device fd.");
    }

    return vfio_fd;
}

void *pci_map_region(int vfio_fd, int region_index, size_t *size) {
    struct vfio_region_info region_info = {.argsz = sizeof(region_info)};
    region_info.index = region_index;
    if (ioctl(vfio_fd, VFIO_DEVICE_GET_REGION_INFO, &region_info) == -1) {
        err(1, "Failed to set IOMMU type.");
    }

    void *buffer = mmap(NULL, region_info.size, PROT_READ | PROT_WRITE,
            MAP_SHARED, vfio_fd, region_info.offset);

    if (buffer == MAP_FAILED) {
        err(1, "Failed to mmap vfio resource.");
    }

    *size = region_info.size;

    return buffer;
}

void *pci_allocate_dma(size_t size) {
    void *buffer = mmap(NULL, size, PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_2MB, -1, 0);
    if (buffer == MAP_FAILED) {
        err(1, "Failed to mmap hugepage.");
    }
    return buffer;
}
