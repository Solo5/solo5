#ifndef COMMON_TAP_IO_H
#define COMMON_TAP_IO_H

#include "tap_frame.h"
#include "tap_offload.h"

/*
 * It fills the given (raw) (the offload header according to the system) with
 * metadata that we can retrieve from the given frame (buf).
 */
bool tx_fill_offload_hdr(struct tap *d, const struct frame_info fi,
                         const uint8_t *buf, size_t len, uint8_t *raw);

/*
 * Validates the given frame (buf) according to the given offload header (raw)
 * (according to the system) and its metadata. If it returns (false), the user
 * must drop the given frame (buf).
 */
bool rx_validate_hdr(struct tap *d, const uint8_t *raw, uint8_t *buf,
                     size_t len);

#endif /* COMMON_TAP_IO_H */
