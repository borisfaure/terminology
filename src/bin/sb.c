#include "private.h"
#include "sb.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

int
ty_sb_add(struct ty_sb *sb, const char *s, size_t len)
{
   size_t new_len = sb->len + len;

   if ((new_len >= sb->alloc) || !sb->buf)
     {
        size_t new_alloc = ((new_len + 15) / 16) * 24;
        char *new_buf;

        new_buf = realloc(sb->buf, new_alloc);
        if (new_buf == NULL)
          return -1;
        sb->buf = new_buf;
        sb->alloc = new_alloc;
     }
   memcpy(sb->buf + sb->len, s, len);
   sb->len += len;
   sb->buf[sb->len] = '\0';
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
