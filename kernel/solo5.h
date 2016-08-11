#ifndef SOLO5_H_INCLUDED
#define SOLO5_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#include "devices.h"

/* Solo5 public APIs */

/* Application entry point */
int solo5_app_main(char *cmdline);

/* Network */
solo5_device *solo5_get_first_netiface(void);
int solo5_net_write_sync(solo5_device *, uint8_t *, int);
int solo5_net_read_sync(solo5_device *, uint8_t *, int *);
char *solo5_net_mac_str(solo5_device *);

/* Block */
solo5_device *solo5_get_first_disk(void);
int solo5_blk_write_sync(solo5_device *, uint64_t, uint8_t *, int);
int solo5_blk_read_sync(solo5_device *, uint64_t, uint8_t *, int *);
int solo5_blk_sector_size(solo5_device *);
uint64_t solo5_blk_sectors(solo5_device *);
int solo5_blk_rw(solo5_device *);

/* Console */
int solo5_console_write(const char *buf, size_t n);

/* Exit */
void solo5_exit(void) __attribute__((noreturn));

/* Memory allocation */
void *solo5_malloc(size_t);
void solo5_free(void *);
void *solo5_calloc(size_t, size_t);
void *solo5_realloc(void *, size_t);

/* Time */
uint64_t solo5_clock_monotonic(void);
uint64_t solo5_clock_wall(void);

/* Sched related functions */
/* solo5_poll(): Block until monotonic time reaches until_nsecs or I/O is
 * possible, whichever is sooner. Returns 1 if I/O is possible, otherwise 0.
 */
#define SOLO5_POLLIN		1
#define SOLO5_POLL_IO_READY	1
int solo5_poll(uint64_t, short *, short *);

#endif
