/*
 * Host stand-in for GCC's <arm_acle.h> (AArch64 ACLE intrinsics).
 * Only the CRC32C intrinsics are needed by libnx's switch/crypto/crc.h.
 * Implemented in software so the host build behaves like the target.
 */
#pragma once

#include <stdint.h>

static inline uint32_t __ldn_host_crc32c(uint32_t crc, uint64_t value, unsigned bits) {
    crc = ~crc;
    for (unsigned i = 0; i < bits; ++i) {
        crc ^= (uint32_t)((value >> i) & 1u);
        crc = (crc >> 1) ^ (0x82F63B78u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static inline uint32_t __crc32b(uint32_t crc, uint8_t value)  { return __ldn_host_crc32c(crc, value, 8); }
static inline uint32_t __crc32h(uint32_t crc, uint16_t value) { return __ldn_host_crc32c(crc, value, 16); }
static inline uint32_t __crc32w(uint32_t crc, uint32_t value) { return __ldn_host_crc32c(crc, value, 32); }
static inline uint32_t __crc32d(uint32_t crc, uint64_t value) { return __ldn_host_crc32c(crc, value, 64); }

/* CRC32C (Castagnoli) variants. */
static inline uint32_t __crc32cb(uint32_t crc, uint8_t value)  { return __ldn_host_crc32c(crc, value, 8); }
static inline uint32_t __crc32ch(uint32_t crc, uint16_t value) { return __ldn_host_crc32c(crc, value, 16); }
static inline uint32_t __crc32cw(uint32_t crc, uint32_t value) { return __ldn_host_crc32c(crc, value, 32); }
static inline uint32_t __crc32cd(uint32_t crc, uint64_t value) { return __ldn_host_crc32c(crc, value, 64); }
