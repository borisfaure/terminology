#ifndef TERMINOLOGY_URI_DECODE_H_
#define TERMINOLOGY_URI_DECODE_H_

/* Percent-decode a URI string in-place. Decodes %XX sequences to
 * their byte value. Malformed sequences (incomplete or non-hex)
 * pass through unchanged.
 *
 * Per RFC 3986 §2.1. Used to convert file:// URIs to plain paths.
 *
 * Safe to call on NULL (no-op).
 */
void uri_percent_decode_inplace(char *s);

#endif
