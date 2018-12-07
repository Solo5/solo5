#include "../bindings.h"

void solo5_console_write(const char *buf, size_t size) { }
void solo5_exit(int status) { for(;;); }
void solo5_abort(void) { for(;;); }

solo5_time_t solo5_clock_monotonic(void) { return ~0; }
solo5_time_t solo5_clock_wall(void) { return ~0; }
bool solo5_yield(solo5_time_t deadline) { return false; }

void solo5_net_info(struct solo5_net_info *info) { }
solo5_result_t solo5_net_write(const uint8_t *buf, size_t size) { return SOLO5_R_EUNSPEC; }
solo5_result_t solo5_net_read(uint8_t *buf, size_t size, size_t *read_size) { return SOLO5_R_EUNSPEC; }

void solo5_block_info(struct solo5_block_info *info) { }
solo5_result_t solo5_block_write(solo5_off_t offset, const uint8_t *buf, size_t size) { return SOLO5_R_EUNSPEC; }
solo5_result_t solo5_block_read(solo5_off_t offset, uint8_t *buf, size_t size) { return SOLO5_R_EUNSPEC; }

uintptr_t __stack_chk_guard;
void __stack_chk_fail (void) { }
