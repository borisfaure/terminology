#ifndef TERMINOLOGY_SB_H_
#define TERMINOLOGY_SB_H_

#include <stddef.h>

struct ty_sb {
   char *buf;
   size_t gap;
   size_t len;
   size_t alloc;
};

int ty_sb_add(struct ty_sb *sb, const char *s, size_t len);
#define TY_SB_ADD(SB_, S_) ty_sb_add(SB_, S_, strlen(S_))
void ty_sb_spaces_rtrim(struct ty_sb *sb);
void ty_sb_spaces_ltrim(struct ty_sb *sb);
int ty_sb_prepend(struct ty_sb *sb, const char *s, size_t len);
#define TY_SB_PREPEND(SB_, S_) ty_sb_prepend(SB_, S_, strlen(S_))
char *ty_sb_steal_buf(struct ty_sb *sb);
void ty_sb_lskip(struct ty_sb *sb, size_t len);
void ty_sb_rskip(struct ty_sb *sb, size_t len);
void ty_sb_free(struct ty_sb *sb);

#define sbstartswith(SB, ConstRef) \
     (((SB)->len >= sizeof(ConstRef) -1) \
        && (!strncmp((SB)->buf, ConstRef, sizeof(ConstRef) - 1)))
#endif
