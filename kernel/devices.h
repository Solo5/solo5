#ifndef DEVICES_H
#define DEVICES_H

#define SOLO5_NUM_DEVICES	2

typedef enum _solo5_device_type {
    SOLO5_BLK = 0,
    SOLO5_NET
} solo5_device_type;

typedef struct {
    void *_req; // to keep track of async reads and writes
} solo5_request;

typedef struct solo5_device {
    int poll_event_idx; // unique index for poll
    solo5_device_type type;
    int irq_num;
    void (*irq_handler)(void);
    void *info;
} solo5_device;

extern solo5_device solo5_devices[SOLO5_NUM_DEVICES];

static inline solo5_device *solo5_get_first_netiface(void)
{
    int i;
    int n = sizeof(solo5_devices) / sizeof(solo5_device);
    for (i = 0; i < n; i++) {
        if (solo5_devices[i].type == SOLO5_NET)
            return &solo5_devices[i];
    }
    return NULL;
}

static inline solo5_device *solo5_get_first_disk(void)
{
    int i;
    int n = sizeof(solo5_devices) / sizeof(solo5_device);
    for (i = 0; i < n; i++) {
        if (solo5_devices[i].type == SOLO5_BLK)
            return &solo5_devices[i];
    }
    return NULL;
}

#endif
