#ifndef _SB_H__
#define _SB_H__

#include <stddef.h>

struct ty_sb {
   char *buf;
   size_t len;
   size_t alloc;
};

int
ty_sb_add(struct ty_sb *sb, const char *s, size_t len);
void
ty_sb_spaces_rtrim(struct ty_sb *sb);

#endif
