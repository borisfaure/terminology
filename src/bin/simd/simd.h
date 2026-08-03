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

/* Read TERMINOLOGY_SIMD_DISABLE, which switches the vector kernels off without
 * a rebuild. Modelled on EFL's EVAS_NEON_DISABLE. */
void simd_init(void);

Eina_Bool simd_enabled(void);

#endif
