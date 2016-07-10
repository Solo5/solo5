#ifndef DEVICES_H
#define DEVICES_H

typedef enum _solo5_device_type {
    SOLO5_BLK = 0,
    SOLO5_NET
} solo5_device_type;

typedef struct {
    void *_req; // to keep track of async reads and writes
} solo5_request;

struct solo5_device_t {
    int poll_event_idx; // unique index for poll
    solo5_device_type type;
    int (*sync_read)(uint64_t off, uint8_t *data, int *n);
    int (*sync_write)(uint64_t off, uint8_t *data, int n);
    solo5_request (*async_read)(uint64_t off, int *n);
    solo5_request (*async_write)(uint64_t off, uint8_t *data, int n);
    int (*async_read_result)(solo5_request req, uint8_t *data, int *n);
    int (*async_write_result)(solo5_request req, int *n);
    int (*get_device_info)(void *info); // TODO: to return a mac address or device size
    int irq_num;
    void (*irq_handler)(void);
    void *info;
};

// FIXME: clean this (and don't hardcode the "2")
extern struct solo5_device_t solo5_devices[2];
#define SOLO5_NUM_DEVICES	2

static inline struct solo5_device_t *solo5_get_first_netiface(void)
{
    int i;
    int n = sizeof(solo5_devices) / sizeof(struct solo5_device_t);
    for (i = 0; i < n; i++) {
        if (solo5_devices[i].type == SOLO5_NET)
            return &solo5_devices[i];
    }
    return NULL;
}

static inline struct solo5_device_t *solo5_get_first_disk(void)
{
    int i;
    int n = sizeof(solo5_devices) / sizeof(struct solo5_device_t);
    for (i = 0; i < n; i++) {
        if (solo5_devices[i].type == SOLO5_BLK)
            return &solo5_devices[i];
    }
    return NULL;
}



// the other option
#if 0
typedef enum _solo5_request_type {
    SOLO5_NET_READ = 0,
    SOLO5_NET_WRITE,
    SOLO5_BLK_READ,
    SOLO5_BLK_WRITE
} solo5_request_type;

struct solo5_device_t {
    int poll_event_idx; // unique index for poll
    solo5_device_type type;
    int (*poll)(void);
    void *(*create_request)(solo5_request_type type, ...);
    int (*sync_submit)(void *req);
    int (*async_submit)(void *req); // call solo5_poll() after this
    int (*async_complete)(void *req); // to be called after async_submit
    int irq_num; // if any
    void (*irq_handler)(void);
    void *info;
};

// FIXME: there should be one per type
struct solo5_net_request {
    solo5_request_type type;
    uint8_t *data; // IN/OUT
    int *n; // IN/OUT
    char **mac_str; // OUT (if used)
};

struct solo5_blk_request {
    solo5_request_type type;
    uint64_t sec; // IN
    uint8_t *data; // IN/OUT
    int *n; // IN/OUT
};
#endif

#endif
