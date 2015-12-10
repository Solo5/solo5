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

#include "loader.h"

struct e_ident {
    uint8_t magic[4]; /* 0x7f 'E' 'L' 'F' */
    uint8_t class;
    uint8_t data;
    uint8_t version;
    uint8_t osabi;
    uint8_t abiversion;
    uint8_t pad[7];
};

struct elf_header {
    struct e_ident ident;
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct elf_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct elf_shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

static uint32_t elf_get_entry(uint32_t start) {
    struct elf_header *h = (struct elf_header *)start;
    return (uint32_t)h->e_entry;
}

static uint32_t elf_get_end(uint32_t start) {
    struct elf_header *h = (struct elf_header *)start;
    struct elf_phdr *p = (struct elf_phdr *)(start 
                                             + (uint32_t)h->e_phoff 
                                             + h->e_phentsize);
    return (uint32_t)(p->p_paddr + p->p_memsz);
}

void loader_main(struct multiboot_info *mb)
{
    multiboot_module_t *mod;
    multiboot_memory_map_t *mmap;
    uint32_t kernel_entry, kernel_end;
    uint64_t max_addr = 0;
    uint64_t max_avail = 0;
    int m = 0;

    serial_init();

    printk("Memory map from multiboot info:\n");

    for (mmap = (multiboot_memory_map_t *)mb->mmap_addr; 
         (uint32_t)mmap < mb->mmap_addr + mb->mmap_length; mmap++) {
        m += mmap->len;
        printk("%d 0x%llx - 0x%llx [%d] (%d)\n", mmap->size, 
               mmap->addr, 
               mmap->addr + mmap->len, m/1024, mmap->type);
        max_addr = mmap->addr + mmap->len;
        if ( mmap->type == MULTIBOOT_MEMORY_AVAILABLE )
            max_avail = max_addr;
    }

    mod = (multiboot_module_t *) mb->mods_addr;

    printk("Found: %s\n", (char *)mod->cmdline);

    /* update mod_end with room for bss... we really should find the
       bss and fill it with zeros so the kernel doesn't need to worry
       about doing the zeroing. */
    mod->mod_end = elf_get_end(mod->mod_start);
    kernel_entry = elf_get_entry(mod->mod_start);

    printk("mod_start: 0x%lx\n", mod->mod_start);
    printk("mod_end: 0x%lx\n", mod->mod_end);
    printk("max_addr: 0x%llx\n", max_addr);
    printk("max_avail: 0x%llx\n", max_avail);

    kernel_end = ((mod->mod_end & PAGE_MASK) + PAGE_SIZE);
    pagetable_init(max_addr, kernel_end);
    gdt_init();

    to64_jump(kernel_entry, (uint32_t)mb, max_avail - 8);
}
