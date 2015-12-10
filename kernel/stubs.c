/* Copyright (c) 2015, IBM 
 * Author(s): Dan Williams <djwillia@us.ibm.com> 
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

#include "kernel.h"

#define IGNORE(x...) do {                                       \
        printf("IGNORING: ");                                   \
        printf(x);                                              \
    } while (0)

int errno;

void init_events(void) { IGNORE("init_events\n"); }
void setup_xen_features(void) { IGNORE("setup_xen_features\n"); }
void init_mm(void) { IGNORE("init_mm\n"); }
void init_time(void) { IGNORE("init_time\n"); }
void init_console(void) { IGNORE("init_console\n"); }
void init_gnttab(void) { IGNORE("init_gnttab\n"); }

void console_print(void *dev __attribute__((__unused__)), 
                   char *data, 
                   int length __attribute__((__unused__))){
    printf(data);
}

void *_xmalloc(size_t size, size_t align) {
    return memalign(align, size);
}
void do_exit(void) {
    printf("Mirage on Solo5 exiting... Goodbye!\n");
    kernel_hang();
}
