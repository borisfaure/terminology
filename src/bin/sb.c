#include "private.h"
#include "sb.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

int
ty_sb_add(struct ty_sb *sb, const char *s, size_t len)
{
   size_t new_len = sb->len + len;

   if ((new_len + sb->gap >= sb->alloc) || !sb->buf)
     {
        size_t new_alloc = ((new_len + sb->gap + 15) / 16) * 24;
        char *new_buf;

        new_buf = realloc(sb->buf - sb->gap, new_alloc);
        if (new_buf == NULL)
          return -1;
        sb->buf = new_buf + sb->gap;
        sb->alloc = new_alloc;
     }
   memcpy(sb->buf + sb->len, s, len);
   sb->len += len;
   sb->buf[sb->len] = '\0';
   return 0;
}

int
ty_sb_prepend(struct ty_sb *sb, const char *s, size_t  len)
{
   if (len >= sb->gap)
     {
        size_t aligned_gap = ((len + 15) / 16) * 24;
        size_t third_of_alloc = (((sb->alloc / 3) + 15) / 16) * 16;
        size_t new_gap = MAX(aligned_gap, third_of_alloc);
        size_t new_alloc = sb->alloc + new_gap;
        char *new_buf;

        new_buf = calloc(new_alloc, 1);
        if (new_buf == NULL)
          return -1;

        memcpy(new_buf + new_gap, sb->buf, sb->len);
        sb->buf = new_buf + new_gap;
        sb->gap = new_gap;
        sb->alloc = new_alloc;
     }

   sb->buf -= len;
   sb->gap -= len;
   sb->len += len;
   memcpy(sb->buf, s, len);
   return 0;
}


/* unlike eina_strbuf_rtrim, only trims \t, \f, ' ' */
void
ty_sb_spaces_rtrim(struct ty_sb *sb)
{
   if (!sb->buf)
     return;

   while (sb->len > 0)
     {
        char c = sb->buf[sb->len - 1];
        if ((c != ' ') && (c != '\t') && (c != '\f'))
            break;
        sb->len--;
     }
   sb->buf[sb->len] = '\0';
}

char *
ty_sb_steal_buf(struct ty_sb *sb)
{
   size_t i;
   char *buf;

   if (sb->gap != 0)
     {
        sb->buf -= sb->gap;
        for (i = 0; i <= sb->len; i++)
          {
             sb->buf[i] = sb->buf[i + sb->gap];
          }
        sb->gap = 0;
     }

   sb->alloc = 0;
   sb->gap = 0;
   sb->len = 0;

   buf = sb->buf;

   sb->buf = NULL;

   return buf;
}

void
ty_sb_lskip(struct ty_sb *sb, int len)
{
   sb->len -= len;
   sb->gap += len;
   sb->buf += len;
}

void
ty_sb_rskip(struct ty_sb *sb, int len)
{
   sb->len -= len;
   sb->buf[sb->len] = '\0';
}

void
ty_sb_free(struct ty_sb *sb)
{
   free(sb->buf - sb->gap);
   sb->gap = sb->len = sb->alloc = 0;
   sb->buf = NULL;
}
