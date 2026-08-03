#ifndef TERMINOLOGY_UTF8_H_
#define TERMINOLOGY_UTF8_H_ 1
#include <Eina.h>
int codepoint_to_utf8(Eina_Unicode g, char *txt);

/* Longest UTF-8 sequence this decoder will carry across a read() boundary.
 * The leftover tail itself is always shorter than this, since a sequence is
 * only retained while at least one of its bytes is still missing. */
#define UTF8_CARRY_MAX 4

int utf8_to_codepoints(const char *buf, int len, Eina_Unicode *codepoints,
                       int *consumed);

#endif
