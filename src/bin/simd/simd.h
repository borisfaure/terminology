#ifndef TERMINOLOGY_SIMD_H_
#define TERMINOLOGY_SIMD_H_ 1

#include <Eina.h>
#include <stddef.h>

/* Scanning kernels for the pty intake path. Each has a scalar form and, where
 * the architecture provides one, a vector form; both are exported so the
 * parity test can compare them directly. */

/* Advanced SIMD is mandatory in ARMv8-A, so no runtime probe is needed. */
#if defined(__aarch64__) && !defined(__ARM_BIG_ENDIAN)
# define TERMINOLOGY_HAVE_NEON 1
#endif

/* Index of the first byte that is not plain printable ASCII (0x20..0x7e),
 * or len. */
size_t simd_scan_plain_ascii(const unsigned char *buf, size_t len);
size_t simd_scan_plain_ascii_scalar(const unsigned char *buf, size_t len);
#if defined(TERMINOLOGY_HAVE_NEON)
size_t simd_scan_plain_ascii_neon(const unsigned char *buf, size_t len);
#endif

/* Same, over decoded codepoints. */
size_t simd_scan_plain_ascii_u32(const Eina_Unicode *buf, size_t len);
size_t simd_scan_plain_ascii_u32_scalar(const Eina_Unicode *buf, size_t len);
#if defined(TERMINOLOGY_HAVE_NEON)
size_t simd_scan_plain_ascii_u32_neon(const Eina_Unicode *buf, size_t len);
#endif

/* Index one past the last non-zero byte, or 0 if every byte is zero. */
size_t simd_rscan_nonzero(const unsigned char *buf, size_t len);
size_t simd_rscan_nonzero_scalar(const unsigned char *buf, size_t len);
#if defined(TERMINOLOGY_HAVE_NEON)
size_t simd_rscan_nonzero_neon(const unsigned char *buf, size_t len);
#endif

/* OR 'bit' into byte 'off' of each of 'n' records of 'rec' bytes. 'off' must
 * be less than 'rec'. */
void simd_records_or_byte(void *buf, size_t n, size_t rec, size_t off,
                          unsigned char bit);
void simd_records_or_byte_scalar(void *buf, size_t n, size_t rec, size_t off,
                                 unsigned char bit);
#if defined(TERMINOLOGY_HAVE_NEON)
void simd_records_or_byte_neon(void *buf, size_t n, size_t rec, size_t off,
                               unsigned char bit);
#endif

/* Widen bytes already known to be plain printable ASCII into codepoints. */
void simd_widen_ascii(const unsigned char *buf, size_t len, Eina_Unicode *out);
void simd_widen_ascii_scalar(const unsigned char *buf, size_t len,
                             Eina_Unicode *out);
#if defined(TERMINOLOGY_HAVE_NEON)
void simd_widen_ascii_neon(const unsigned char *buf, size_t len,
                           Eina_Unicode *out);
#endif

/* Read TERMINOLOGY_SIMD_DISABLE, which switches the vector kernels off without
 * a rebuild. Modelled on EFL's EVAS_NEON_DISABLE. */
void simd_init(void);

Eina_Bool simd_enabled(void);

#endif
