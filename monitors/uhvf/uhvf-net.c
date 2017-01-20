#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include <sys/select.h>

/* for net */
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <err.h>

#include <dispatch/dispatch.h>
#include <vmnet/vmnet.h>


#include "../ukvm-private.h"
#include "../ukvm-modules.h"
#include "../ukvm.h"

static char *netiface;
static int netfd;
static struct ukvm_netinfo netinfo;

struct vmnet_state {
	interface_ref iface;
    const char *mac;
	unsigned int mtu;
	unsigned int max_packet_size;

    dispatch_queue_t if_q;
    int write_fd;
};

static struct vmnet_state vms;

static void vmn_enable_notifications(void)
{
    /* Whenever there are packets available we write to a pipe so that
     * the generic poll on the pipe's fd can pick it up.  This is not
     * ideal. */
    vmnet_interface_set_event_callback(vms.iface,
                                       VMNET_INTERFACE_PACKETS_AVAILABLE,
                                       vms.if_q,
    ^(interface_event_t event_id, xpc_object_t event)
    {
        size_t num_written;
        num_written = write(vms.write_fd, "x", 1);
        assert(num_written == 1);
        
        /* Disable the notifications until we're ready to hear about more.*/
        vmnet_interface_set_event_callback(vms.iface,
                                           VMNET_INTERFACE_PACKETS_AVAILABLE,
                                           NULL,
                                           NULL);
	});
}

static int vmn_create(void)
{
    int pipefds[2];
    xpc_object_t iface_desc;
	uuid_t uuid;
    
	__block interface_ref iface = NULL;
	__block vmnet_return_t iface_status = 0;
        
	iface_desc = xpc_dictionary_create(NULL, NULL, 0);
	xpc_dictionary_set_uint64(iface_desc, vmnet_operation_mode_key,
                              VMNET_SHARED_MODE);

#ifdef USE_TEST_UUID
    /* This will result in a test MAC address of 64:65:3a:31:64:3a */
    uint8_t test_uuid[] = {0x40,0xab,0xea,0x25,
                           0x95,0x2f,0x44,0xe8,
                           0x85,0x79,0xb7,0x73,
                           0x67,0x3c,0x2e,0xb8};
 
    memcpy(&uuid, test_uuid, sizeof(uuid));
#else
    uuid_generate_random(uuid);
#endif
	xpc_dictionary_set_uuid(iface_desc, vmnet_interface_id_key, uuid);

    pipe(pipefds);
    vms.write_fd = pipefds[1];

    /* do vmnet_start_interface synchronously */
    {
        dispatch_queue_t if_create_q;
        dispatch_semaphore_t if_create_sema;

        if_create_q = dispatch_queue_create("uhvf.vmnet.create",
                                            DISPATCH_QUEUE_SERIAL);
        if_create_sema = dispatch_semaphore_create(0);
        
        iface = vmnet_start_interface(iface_desc, if_create_q,
                ^(vmnet_return_t status,
                  xpc_object_t x)
                {
                    iface_status = status;

                    if (iface_status == VMNET_SUCCESS) {
                        vms.mtu = xpc_dictionary_get_uint64(x,
                                     vmnet_mtu_key);
                        vms.max_packet_size = xpc_dictionary_get_uint64(x,
                                                 vmnet_max_packet_size_key);
                        vms.mac = strdup(xpc_dictionary_get_string(x,
                                            vmnet_mac_address_key));
                    }
                    dispatch_semaphore_signal(if_create_sema);
                });
        dispatch_semaphore_wait(if_create_sema, DISPATCH_TIME_FOREVER);
        dispatch_release(if_create_q);
        dispatch_release(if_create_sema);
    }

    if (!iface || iface_status != VMNET_SUCCESS) {
		printf("vmnet: vmnet_start_interface failed\n");
        goto out;
    }

	vms.iface = iface;
	vms.if_q = dispatch_queue_create("uhvf.vmnet.iface_q", 0);

    vmn_enable_notifications();
    
	return pipefds[0];

 out:
    close(pipefds[0]);
    close(pipefds[1]);
    return -1;
}

static ssize_t vmn_read(uint8_t *data, int len) {
    struct iovec iov;
	vmnet_return_t r;
	struct vmpktdesc v;
	int pktcnt;

	v.vm_pkt_size = len;

	assert(v.vm_pkt_size >= vms.max_packet_size);

    iov.iov_base = data;
    iov.iov_len = len;
	v.vm_pkt_iov = &iov;
	v.vm_pkt_iovcnt = 1;
	v.vm_flags = 0;
	pktcnt = 1;

	r = vmnet_read(vms.iface, &v, &pktcnt);
    {
        char throwaway;
        size_t num_read;
        num_read = read(netfd, &throwaway, 1);
        assert(num_read == 1);

        /* We're ready now for another notification. */
        vmn_enable_notifications();
    }
    
	assert(r == VMNET_SUCCESS);
    
	if (pktcnt < 1) {
		return 0;
	}

	return ((ssize_t) v.vm_pkt_size);
}

static size_t vmn_write(uint8_t *data, int len) {
    struct iovec iov;
    vmnet_return_t r;
    struct vmpktdesc v;
    int pktcnt;
    
    v.vm_pkt_size = len;
    assert(len <= vms.max_packet_size);

    iov.iov_base = data;
    iov.iov_len = len;
    v.vm_pkt_iov = &iov;
    v.vm_pkt_iovcnt = 1;
    v.vm_flags = 0;
    pktcnt = 1;
    
    r = vmnet_write(vms.iface, &v, &pktcnt);
	assert(r == VMNET_SUCCESS);

    return iov.iov_len;
}

static void ukvm_port_netinfo(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_netinfo));
    struct ukvm_netinfo *info = (struct ukvm_netinfo *)(mem + paddr);
    printf("netinfo!\n");
    memcpy(info->mac_str, netinfo.mac_str, sizeof(netinfo.mac_str));
}

static void ukvm_port_netwrite(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_netwrite));
    struct ukvm_netwrite *wr = (struct ukvm_netwrite *)(mem + paddr);
    int ret;
    
    GUEST_CHECK_PADDR(wr->data, GUEST_SIZE, wr->len);
    ret = vmn_write(mem + wr->data, wr->len);
    if (wr->len != ret)
        printf("wr->len=%zu ret=%d\n", wr->len, ret);
    assert(wr->len == ret);
    wr->ret = 0;
}

static void ukvm_port_netread(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_netread));
    struct ukvm_netread *rd = (struct ukvm_netread *)(mem + paddr);
    int ret;

    GUEST_CHECK_PADDR(rd->data, GUEST_SIZE, rd->len);
    ret = vmn_read(mem + rd->data, rd->len);
    if (ret == 0) {
        rd->ret = -1;
        return;
    }
    assert(ret > 0);
    rd->len = ret;
    rd->ret = 0;
}

static int handle_exit(struct platform *p)
{
    if (platform_get_exit_reason(p) != EXIT_IO)
        return -1;
    
    int port = platform_get_io_port(p);
    uint64_t data = platform_get_io_data(p);

    switch (port) {
    case UKVM_PORT_NETINFO:
        ukvm_port_netinfo(p->mem, data);
        break;
    case UKVM_PORT_NETWRITE:
        ukvm_port_netwrite(p->mem, data);
        break;
    case UKVM_PORT_NETREAD:
        ukvm_port_netread(p->mem, data);
        break;
    default:
        return -1;
    }

    platform_advance_rip(p);
    return 0;
}

static int handle_cmdarg(char *cmdarg)
{
    if (strncmp("--net=", cmdarg, 6))
        return -1;
    netiface = cmdarg + 6;
    return 0;
}

static int setup(struct platform *p)
{
    
    /* set up virtual network */
    netfd = vmn_create();
    if (netfd <= 0) {
        perror("Allocating interface");
        exit(1);
    }
    snprintf(netinfo.mac_str, sizeof(netinfo.mac_str),
            "%02x:%02x:%02x:%02x:%02x:%02x",
            vms.mac[0], vms.mac[1], vms.mac[2],
            vms.mac[3], vms.mac[4], vms.mac[5]);

    printf("Providing network: guest address %s\n", 
            netinfo.mac_str);

    return 0;
}

static int get_fd(void)
{
    return netfd;
}

static char *usage(void)
{
    return "--net=TAP (host tap device for guest network interface)";
}

struct ukvm_module ukvm_net = {
    .get_fd = get_fd,
    .handle_exit = handle_exit,
    .handle_cmdarg = handle_cmdarg,
    .setup = setup,
    .usage = usage
};
