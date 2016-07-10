#ifndef SOLO5_H_INCLUDED
#define SOLO5_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#include "devices.h"

/* Solo5 public APIs */

/* Application entry point */
int solo5_app_main(char *cmdline);

/* Network */
struct solo5_device_t *solo5_get_first_netiface(void);

int solo5_net_write_sync(uint64_t unused, uint8_t *data, int n);
int solo5_net_read_sync(uint64_t unused, uint8_t *data, int *n);
char *solo5_net_mac_str(void);

/* Block */
struct solo5_device_t *solo5_get_first_disk(void);

int solo5_blk_write_sync(uint64_t sec, uint8_t *data, int n);
int solo5_blk_read_sync(uint64_t sec, uint8_t *data, int *n);
solo5_request solo5_blk_write_async(uint64_t sec, uint8_t *data, int n);
solo5_request solo5_blk_read_async_submit(uint64_t sec, int *n);
int solo5_blk_read_async_complete(solo5_request req, uint8_t *data, int *n);
int solo5_blk_write_async_complete(solo5_request req, int *n);
int solo5_blk_sector_size(void);
uint64_t solo5_blk_sectors(void);
int solo5_blk_rw(void);

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
#define SOLO5_POLLIN	1
int solo5_poll(uint64_t until_nsecs, short *events, short *revents);

#endif
