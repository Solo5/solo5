#include "solo5.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define RUNS 1000

char* requests[RUNS];
uint8_t cleared[RUNS];
size_t lengths[RUNS];
size_t hash_value[RUNS];

size_t hash(char* str, int len)
{
  int i, tmp_hash = 0;
    for(i = 0; i < len; i++) {
      tmp_hash ^= str[i];
    }
    return tmp_hash;
 }  

int solo5_app_main(char *cmdline __attribute__((unused)))
{
    size_t i, len, tmp_hash, cleared_count;
    uint8_t delete_or_expand;
    char buffer[1024];
    char* expanded;

    printf("\n**** Solo5 standalone test_nolibc ****\n\n");

    /*
     * Allocate some strings
     */
    for (i = 0; i < RUNS; i++) {
        snprintf(buffer, 1024, "%d - %u", i, solo5_clock_monotonic());
        len = strlen(buffer);
        lengths[i] = len;
        requests[i] = strncpy(malloc(len + 1), buffer, len + 1);
        cleared[i] = 0;
        hash_value[i] = hash(buffer, len);
    }

    /*
     * Delete or expand, to put stress on the memory management
     */
    delete_or_expand = 0;
    cleared_count = 0;
    i = 0;
    while (cleared_count < RUNS) {
        /*
         * Pseudo-random advance
         */
        i = ( i + 17 ) % RUNS;
        while (cleared[i] == 1) {
            i = ( i + 1 ) % RUNS;
        }
        
        /*
         * check
         */
        if(strlen(requests[i]) != lengths[i]){
            printf("Length mismatch, position %d, expected %d, got %d\n", i, lengths[i], strlen(requests[i]));
            return 1;
        }
        tmp_hash = hash(requests[i], lengths[i]);
        if(tmp_hash != hash_value[i]) {
            printf("Hash mismatch, position %d, expected %d, got %d\n", i, hash_value[i], tmp_hash);
            return 2;
        }

        /*
         * Expand and delete accordingly
         */
        if (delete_or_expand == 1) {
            free(requests[i]);
            cleared[i] = 1;
            delete_or_expand = 0;
            ++cleared_count;
        } else {
            snprintf(buffer, 1024, "; %d - %u", i, solo5_clock_monotonic());
            len = strlen(buffer);
            expanded = malloc(len + lengths[i] + 1);
            strncpy(expanded, requests[i], len + lengths[i] + 1);
            free(requests[i]);
            strncpy(&expanded[lengths[i]], buffer, len + 1);
            requests[i] = expanded;
            lengths[i] += len;
#ifdef VERBOSE            
            printf("Requests[%d] = '%s', len=%d, lenghts=%d\n", i, requests[i], strlen(requests[i]), lengths[i]);
#endif            
            hash_value[i] = hash(requests[i], lengths[i]);
            delete_or_expand = 1;
        }            
    }
    printf("SUCCESS\n");

    return 0;
}
