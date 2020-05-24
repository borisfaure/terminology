#include "private.h"

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "sb.h"

#if defined(BINARY_TYTEST)
#include "unit_tests.h"
#endif

int
ty_sb_add(struct ty_sb *sb, const char *s, size_t len)
{
   size_t new_len = sb->len + len;

   if ((new_len + sb->gap >= sb->alloc) || !sb->buf)
     {
        size_t new_alloc = ((new_len + sb->gap + 15) / 16) * 24;
        char *new_buf;
        char *buf = sb->buf;

        if (buf && sb->gap)
          buf -= sb->gap;
        new_buf = realloc(buf, new_alloc);
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
ty_sb_prepend(struct ty_sb *sb, const char *s, size_t len)
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
        free(sb->buf - sb->gap);
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
ty_sb_spaces_ltrim(struct ty_sb *sb)
{
   if (!sb->buf)
     return;

   while (sb->len > 0)
     {
        char c = sb->buf[0];
        if ((c != ' ') && (c != '\t') && (c != '\f'))
            break;
        sb->len--;
        sb->buf++;
        sb->gap++;
     }
   sb->buf[sb->len] = '\0';
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
   char *buf;

   if (!sb->len)
     return NULL;

   if (sb->gap != 0)
     {
        size_t i;

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
ty_sb_lskip(struct ty_sb *sb, size_t len)
{
   if (len >= sb->len)
     len = sb->len;
   sb->len -= len;
   if (sb->len)
     {
        sb->gap += len;
        sb->buf += len;
     }
   else
     {
        /* buffer is empty, get rid of gap */
        if (sb->buf)
          sb->buf -= sb->gap;
        sb->gap = 0;
     }
}

void
ty_sb_rskip(struct ty_sb *sb, size_t len)
{
   if (len >= sb->len)
     len = sb->len;
   sb->len -= len;
   if (sb->alloc)
     sb->buf[sb->len] = '\0';
}

void
ty_sb_free(struct ty_sb *sb)
{
   char *buf = sb->buf;
   if (buf && sb->gap)
     buf -= sb->gap;
   free(buf);
   sb->gap = sb->len = sb->alloc = 0;
   sb->buf = NULL;
}

#if defined(BINARY_TYTEST)
static int
tytest_sb_skip(void)
{
   struct ty_sb sb = {};
   const char *data = "foobar";

   /* lskip normal */
   assert(ty_sb_add(&sb, data, strlen(data)) == 0);
   ty_sb_lskip(&sb, 3);
   assert(sb.len == strlen(data) - 3);
   assert(sb.gap == 3);
   assert(strncmp(sb.buf, data+3, sb.len) == 0);
   /* lskip too large */
   ty_sb_lskip(&sb, 30);
   assert(sb.len == 0);
   assert(sb.gap == 0);
   ty_sb_free(&sb);

   /* lskip all */
   assert(ty_sb_add(&sb, data, strlen(data)) == 0);
   ty_sb_lskip(&sb, strlen(data));
   assert(sb.len == 0);
   assert(sb.gap == 0);
   ty_sb_free(&sb);
   /* lskip empty */
   ty_sb_lskip(&sb, 3);
   assert(sb.len == 0);
   assert(sb.gap == 0);

   /* rskip normal */
   assert(ty_sb_add(&sb, data, strlen(data)) == 0);
   ty_sb_rskip(&sb, 3);
   assert(sb.len == strlen(data) - 3);
   assert(sb.gap == 0);
   assert(strncmp(sb.buf, data, sb.len) == 0);
   /* rskip too large */
   ty_sb_rskip(&sb, 30);
   assert(sb.len == 0);
   assert(sb.gap == 0);
   ty_sb_free(&sb);

   /* rskip all */
   assert(ty_sb_add(&sb, data, strlen(data)) == 0);
   ty_sb_rskip(&sb, strlen(data));
   assert(sb.len == 0);
   assert(sb.gap == 0);
   ty_sb_free(&sb);
   /* rskip empty */
   ty_sb_rskip(&sb, 3);
   assert(sb.len == 0);
   assert(sb.gap == 0);

   return 0;
}

static int
tytest_sb_trim(void)
{
   struct ty_sb sb = {};

   assert(ty_sb_add(&sb,
                    " \f \t sb_trim_spaces \t ",
                    strlen(" \f \t sb_trim_spaces \t ")) == 0);

   ty_sb_spaces_ltrim(&sb);
   assert(sb.gap == 5);
   assert(strncmp(sb.buf, "sb", 2) == 0);
   ty_sb_spaces_rtrim(&sb);
   assert(sb.gap == 5);
   assert(sb.len == strlen("sb_trim_spaces"));
   assert(strncmp(sb.buf, "sb_trim_spaces", sb.len) == 0);

   ty_sb_free(&sb);
   /* on empty */
   ty_sb_spaces_rtrim(&sb);
   ty_sb_spaces_ltrim(&sb);
   return 0;
}

static int
tytest_sb_gap(void)
{
   struct ty_sb sb = {};
   const char *data = "alpha bravo charlie delta";

   assert(ty_sb_add(&sb, data, strlen(data)) == 0);
   /* prepend with no gap */
   assert(ty_sb_prepend(&sb, ">>>", strlen(">>>")) == 0);
   assert(sb.len == strlen(data) + 3);
   assert(strncmp(sb.buf,">>>alpha", 3+5) == 0);
   /* make gap */
   ty_sb_lskip(&sb, 3);
   /* prepend with enough gap */
   assert(ty_sb_prepend(&sb, "!!", strlen("!!")) == 0);
   assert(strncmp(sb.buf,"!!alpha", 2+5) == 0);
   /* make gap larger */
   ty_sb_lskip(&sb, 2);
   /* prepend with not enough gap */
   assert(ty_sb_prepend(&sb, ">>>>>>", strlen(">>>>>>")) == 0);
   assert(sb.len == strlen(data) + 6);
   assert(strncmp(sb.buf,">>>>>>alpha", 6+5) == 0);
   ty_sb_free(&sb);

   /** add that realloc, with gap */
   assert(ty_sb_add(&sb, "foobar", strlen("foobar")) == 0);
   ty_sb_lskip(&sb, 3);
   assert(ty_sb_add(&sb, data, strlen(data)) == 0);

   ty_sb_free(&sb);
   return 0;
}

static int
tytest_sb_steal(void)
{
   struct ty_sb sb = {};
   const char *data = "foobar foobar";
   char *buf;
   assert(ty_sb_steal_buf(&sb) == NULL);

   /* with gap */
   assert(ty_sb_add(&sb, data, strlen(data)) == 0);
   ty_sb_lskip(&sb, 7);
   buf = ty_sb_steal_buf(&sb);
   assert(strlen(buf) == strlen("foobar"));
   assert(sb.buf == NULL);
   assert(sb.len == 0);
   assert(sb.alloc == 0);
   assert(sb.gap == 0);
   free(buf);
   ty_sb_free(&sb);
   return 0;
}


int
tytest_sb(void)
{
   assert(tytest_sb_skip() == 0);
   assert(tytest_sb_trim() == 0);
   assert(tytest_sb_gap() == 0);
   assert(tytest_sb_steal() == 0);
   return 0;
}
#endif
