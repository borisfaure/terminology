#ifndef TERMINOLOGY_COLOR_NAMES_H_
#define TERMINOLOGY_COLOR_NAMES_H_ 1

#include <stddef.h>

/* X11 colour names, as resolved by XParseColor. The table is generated
 * by gperf from color_names.gperf at build time. */

struct color_name {
     const char *name;
     unsigned char r;
     unsigned char g;
     unsigned char b;
};

/* Case-insensitive. str needs not be NUL-terminated; len is its length.
 * Returns NULL when str is not a known colour name. */
const struct color_name *color_name_lookup(const char *str, size_t len);

#endif
