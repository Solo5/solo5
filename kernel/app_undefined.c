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































void _ctype(void) { PANIC("_ctype\n"); }
void monotonic_clock(void) { PANIC("monotonic_clock\n"); }
void not_running_time(void) { PANIC("not_running_time\n"); }
//void print(void) { PANIC("print\n"); }
void sscanf(void) { PANIC("sscanf\n"); }
void strcat(void) { PANIC("strcat\n"); }
void stub_evtchn_alloc_unbound(void) { PANIC("stub_evtchn_alloc_unbound\n"); }
void stub_evtchn_bind_interdomain(void) { PANIC("stub_evtchn_bind_interdomain\n"); }
void stub_evtchn_bind_virq(void) { PANIC("stub_evtchn_bind_virq\n"); }
void stub_evtchn_close(void) { PANIC("stub_evtchn_close\n"); }
void stub_evtchn_test_and_clear(void) { PANIC("stub_evtchn_test_and_clear\n"); }
void stub_evtchn_unbind(void) { PANIC("stub_evtchn_unbind\n"); }
void stub_evtchn_virq_dom_exc(void) { PANIC("stub_evtchn_virq_dom_exc\n"); }
void stub_gntshr_close(void) { PANIC("stub_gntshr_close\n"); }
void stub_gntshr_end_access(void) { PANIC("stub_gntshr_end_access\n"); }
void stub_gntshr_grant_access(void) { PANIC("stub_gntshr_grant_access\n"); }
void stub_gntshr_munmap_batched(void) { PANIC("stub_gntshr_munmap_batched\n"); }
void stub_gntshr_open(void) { PANIC("stub_gntshr_open\n"); }
void stub_gntshr_share_pages_batched(void) { PANIC("stub_gntshr_share_pages_batched\n"); }
void stub_gnttab_fini(void) { PANIC("stub_gnttab_fini\n"); }
void stub_gnttab_interface_close(void) { PANIC("stub_gnttab_interface_close\n"); }
void stub_gnttab_interface_open(void) { PANIC("stub_gnttab_interface_open\n"); }
void stub_gnttab_map_fresh(void) { PANIC("stub_gnttab_map_fresh\n"); }
void stub_gnttab_map_onto(void) { PANIC("stub_gnttab_map_onto\n"); }
void stub_gnttab_mapv_batched(void) { PANIC("stub_gnttab_mapv_batched\n"); }
void stub_gnttab_unmap(void) { PANIC("stub_gnttab_unmap\n"); }
void stub_hypervisor_suspend(void) { PANIC("stub_hypervisor_suspend\n"); }
void stub_sched_shutdown(void) { PANIC("stub_sched_shutdown\n"); }
void unix_gmtime(void) { PANIC("unix_gmtime\n"); }
