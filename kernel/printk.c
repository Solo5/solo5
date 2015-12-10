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

static char hexdigit_to_char(uint8_t v) {
    if ( v <= 0x9 )
        return v + '0';

    if ( v >= 0xa && v <= 0xf )
        return v + 'a' - 0xa;

    return ' ';
}

static char digit_to_char(uint8_t v) {
    return v + '0';
}


/* This simple printk only has a few options: 
   %c   : character
   %s   : string
   %d   : int32_t in decimal
   %b   : uint8_t in hex
   %x   : uint16_t in hex
   %lx  : uint32_t in hex
   %llx : uint64_t in hex 
*/
void printk(char *fmt, ...) {
    va_list va;
    char ch;
    uint32_t in_l_mode = 0;
    uint32_t in_p_mode = 0;

    va_start(va, fmt);

    for ( ch = *fmt; ch != 0; ch = *(++fmt) ) {

        /* check for long mode identifiers */
        if ( in_l_mode && ch == 'l' ) {
            in_l_mode++;
            continue;
        } 

        if ( ch == '%' ) {
            in_p_mode++;
            continue;
        }

        if ( !in_p_mode ) {
            serial_putc(ch);
            continue;
        }
        
        switch (ch) {
        case 'c': {
            serial_putc((char)va_arg(va, int));
            in_p_mode = 0;
            in_l_mode = 0;
            break;
        }
        case 'b': {
            uint32_t data = va_arg(va, uint32_t);
            uint8_t *ptr = (uint8_t *)&data;
            serial_putc(hexdigit_to_char(ptr[0] >> 4));
            serial_putc(hexdigit_to_char(ptr[0] & 0xf));
            in_p_mode = 0;
            in_l_mode = 0;
            break;
        }
        case 'd': {
            int32_t data = va_arg(va, int32_t);
            char tmp_str[16]; /* big enough for 32 bit integer */
            int i = 0, j;

            if ( data == 0 ) {
                serial_putc('0');
            } else {
                if ( data < 0 ) {
                    data *= -1;
                    serial_putc('-');
                }

                while (data > 0) {
                    int32_t div = data / 10;
                    tmp_str[i++] = digit_to_char(data - ((div) * 10));
                    data = div;
                }

                for ( j = i - 1; j >= 0; j-- )
                    serial_putc(tmp_str[j]);
            }

            in_p_mode = 0;
            in_l_mode = 0;
            break;
        }
        case 's': {
            char *data = va_arg(va, char *);
            char c;
            for ( c = *data; c != 0; c = *(++data) )
                serial_putc(c);
            in_p_mode = 0;
            in_l_mode = 0;
            break;
        }
        case 'x': {
            uint64_t data = va_arg(va, uint64_t);
            uint8_t *ptr = (uint8_t *)&data;
            int i;
            for ( i = (2 << in_l_mode) - 1; i >= 0; i-- ) {
                serial_putc(hexdigit_to_char(ptr[i] >> 4));
                serial_putc(hexdigit_to_char(ptr[i] & 0xf));
            }
            in_p_mode = 0;
            in_l_mode = 0;
            break;
        }
        case 'l': {
            in_l_mode++;
            break;
        }
        default:
            break;
        }
    }

    va_end(va);
}
