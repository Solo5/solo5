/*
 * Copyright (c) 2015-2019 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a sandboxed execution environment.
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

/*
 * elf.h: ELF loader APIs.
 */

#ifndef COMMON_ELF_H
#define COMMON_ELF_H

/*
 * Load an ELF binary from (file) into (mem_size) bytes of memory at (*mem).
 * (p_min_loadaddr) is the lowest allowed load address within (*mem). 
 *
 * If successful, returns the entry point (*p_entry) and last address used by
 * the binary (*p_end).
 *
 * If the executable is invalid, or on any other error, reports to stderr and
 * terminates the program.
 */
void elf_load(const char *file, uint8_t *mem, size_t mem_size,
        uint64_t p_min_loadaddr, uint64_t *p_entry, uint64_t *p_end);

/*
 * Load the Solo5-owned NOTE of (note_type) from the ELF binary (file).
 * Internal alignment of the note descriptor (content) will be adjusted to
 * (note_align), and a descriptor with a descsz larger than (max_note_size)
 * will cause the executable to be rejected.
 *
 * Returns / error handling:
 *
 * On success: Returns 0 and memory for the note descriptor (content) is
 * allocated with malloc(), returned in (*note_data), with the note size,
 * **minus any alignment**, returned in (*note_size).
 *
 * If a Solo5-owned NOTE of (note_type) was NOT found, but the executable is
 * otherwise valid: Returns -1.
 *
 * In all other cases, reports any errors to stderr and terminates the program.
 */
int elf_load_note(const char *file, uint32_t note_type, size_t note_align,
        size_t max_note_size, void **note_data, size_t *note_size);

#endif /* COMMON_ELF_H */
