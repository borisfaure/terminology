#ifndef _SB_H__
#define _SB_H__

#include <stddef.h>

struct ty_sb {
   char *buf;
   size_t gap;
   size_t len;
   size_t alloc;
};

int ty_sb_add(struct ty_sb *sb, const char *s, size_t len);
#define TY_SB_ADD(_SB, _S) ty_sb_add(_SB, _S, strlen(_S))
void ty_sb_spaces_rtrim(struct ty_sb *sb);
void ty_sb_spaces_ltrim(struct ty_sb *sb);
int ty_sb_prepend(struct ty_sb *sb, const char *s, size_t len);
#define TY_SB_PREPEND(_SB, _S) ty_sb_prepend(_SB, _S, strlen(_S))
char *ty_sb_steal_buf(struct ty_sb *sb);
void ty_sb_lskip(struct ty_sb *sb, size_t len);
void ty_sb_rskip(struct ty_sb *sb, size_t len);
void ty_sb_free(struct ty_sb *sb);

#define sbstartswith(SB, ConstRef) \
     (((SB)->len >= sizeof(ConstRef) -1) \
        && (!strncmp((SB)->buf, ConstRef, sizeof(ConstRef) - 1)))
#endif
