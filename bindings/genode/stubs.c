#include "../bindings.h"

void solo5_console_write(const char *buf, size_t size) { }
void solo5_exit(int status) { for(;;); }
void solo5_abort(void) { for(;;); }

solo5_time_t solo5_clock_monotonic(void) { return ~0; }
solo5_time_t solo5_clock_wall(void) { return ~0; }
bool solo5_yield(solo5_time_t deadline, solo5_handle_set_t *ready_set) { return false; }

solo5_result_t solo5_net_acquire(const char *name, solo5_handle_t *handle, struct solo5_net_info *info) { return SOLO5_R_EUNSPEC; }
solo5_result_t solo5_net_write(solo5_handle_t handle, const uint8_t *buf, size_t size) { return SOLO5_R_EUNSPEC; }
solo5_result_t solo5_net_read(solo5_handle_t handle, uint8_t *buf, size_t size, size_t *read_size) { return SOLO5_R_EUNSPEC; }

solo5_result_t solo5_block_acquire(const char *name, solo5_handle_t *handle, struct solo5_block_info *info) { return SOLO5_R_EUNSPEC; }

solo5_result_t solo5_block_write(solo5_handle_t handle, solo5_off_t offset, const uint8_t *buf, size_t size) { return SOLO5_R_EUNSPEC; }
solo5_result_t solo5_block_read(solo5_handle_t handle, solo5_off_t offset, uint8_t *buf, size_t size) { return SOLO5_R_EUNSPEC; }

solo5_result_t solo5_set_tls_base(uintptr_t base) { return SOLO5_R_EUNSPEC; }

uintptr_t SSP_GUARD;
void SSP_FAIL (void) { }
