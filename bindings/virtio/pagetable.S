/*
 * Static page tables for the virtio target.
 *
 * We identity-map the first 4GB with 2MB pages (a 4-level hierarchy). Unlike
 * hvt, virtio needs access to low memory for platform setup, so only the very
 * first page (NULL) is left unmapped. The heap/stack stay within the first 1GB
 * (see platform.c); the upper 3GB are mapped only so platform_exit() can walk
 * the ACPI tables, which the firmware (like SeaBIOS) places at the top of low
 * RAM, up to just below the 4GB PCI hole.
 *
 * Each paging structure is architecturally a single page of 512 8-byte entries,
 * so we build them with .rept rather than spelling out every entry:
 *  - cpu_pt0 : 512 x 4KB pages for the first 2MB (entry 0 unmapped = NULL guard)
 *  - cpu_pd  : entry 0 -> cpu_pt0, entries 1..511 = 2MB pages, covers 0-1GB
 *  - cpu_pd1/2/3 : 512 x 2MB pages each, covering 1-2, 2-3 and 3-4GB
 *  - cpu_pdpt : 4 entries, one per page directory (each covers 1GB)
 *  - cpu_pml4 : 1 entry -> cpu_pdpt
 *
 * Entry flags: 0x3 = present|write; the extra 0x80 sets PS (a 2MB huge page).
 */

.align 0x1000
cpu_pt0:
	.quad 0x0			/* page 0 (0x0-0xfff): unmapped, NULL guard */
	.set _addr, 0x1000
	.rept 511
		.quad _addr + 0x3
		.set _addr, _addr + 0x1000
	.endr

.align 0x1000
cpu_pd:
	.quad cpu_pt0 + 0x3		/* 0-2MB: 4KB pages via cpu_pt0 */
	.set _addr, 0x200000
	.rept 511
		.quad _addr + 0x3 + 0x80
		.set _addr, _addr + 0x200000
	.endr

.align 0x1000
cpu_pd1:
	.set _addr, 0x40000000
	.rept 512
		.quad _addr + 0x3 + 0x80
		.set _addr, _addr + 0x200000
	.endr

.align 0x1000
cpu_pd2:
	.set _addr, 0x80000000
	.rept 512
		.quad _addr + 0x3 + 0x80
		.set _addr, _addr + 0x200000
	.endr

.align 0x1000
cpu_pd3:
	.set _addr, 0xc0000000
	.rept 512
		.quad _addr + 0x3 + 0x80
		.set _addr, _addr + 0x200000
	.endr

.align 0x1000
cpu_pdpt:
	.quad cpu_pd + 0x3
	.quad cpu_pd1 + 0x3
	.quad cpu_pd2 + 0x3
	.quad cpu_pd3 + 0x3
	.fill 0x1fc, 0x8, 0x0

.align 0x1000
cpu_pml4:
	.quad cpu_pdpt + 0x3
	.fill 0x1ff, 0x8, 0x0
