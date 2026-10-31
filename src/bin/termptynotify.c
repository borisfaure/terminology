#include "private.h"
#include <Elementary.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <assert.h>

#include "termio.h"
#include "termpty.h"
#include "termptynotify.h"
#include "utf8.h"
#include "utils.h"

#if defined(BINARY_TYTEST)
#include "tytest.h"
#endif

/* What one OSC 99 says about itself */
typedef enum _Notify_Payload
{
   NOTIFY_PAYLOAD_TITLE,
   NOTIFY_PAYLOAD_BODY,
   NOTIFY_PAYLOAD_QUERY,
   NOTIFY_PAYLOAD_CLOSE,
   NOTIFY_PAYLOAD_ALIVE,
   NOTIFY_PAYLOAD_UNKNOWN,
} Notify_Payload;

typedef enum _Notify_Occasion
{
   NOTIFY_OCCASION_UNSET = -1,
   NOTIFY_OCCASION_ALWAYS,
   NOTIFY_OCCASION_UNFOCUSED,
   NOTIFY_OCCASION_INVISIBLE,
} Notify_Occasion;

/* What one OSC 99 said, parsed */
typedef struct _Notify_Meta
{
   char id[NOTIFY_ID_MAX + 1];
   Notify_Payload payload;
   Notify_Occasion occasion;
   int timeout_ms;        /* -2 when unset, as kitty */
   int urgency;           /* -1 when unset */
   Eina_Bool has_id;
   Eina_Bool done;
   Eina_Bool base64;
} Notify_Meta;

/* A notification whose chunks are still arriving */
typedef struct _Notify_Pending
{
   char id[NOTIFY_ID_MAX + 1];
   Eina_Strbuf *title, *body;
   Ty_Base64_Stream b64;
   Notify_Payload b64_payload;
   Notify_Occasion occasion;
   int timeout_ms;
   int urgency;
   Eina_Bool has_id;
} Notify_Pending;

static Eina_Bool
_notify_id_char_valid(char c)
{
   return ((c >= 'a') && (c <= 'z')) ||
          ((c >= 'A') && (c <= 'Z')) ||
          ((c >= '0') && (c <= '9')) ||
          (c == '_') || (c == '+') || (c == '.') ||
          (c == '-') ;
}

static Eina_Bool
_notify_id_is_valid(const char *id, size_t len)
{
   size_t i;

   if ((len == 0) || (len > NOTIFY_ID_MAX))
     return EINA_FALSE;
   for (i = 0; i < len; i++)
     {
        if (!_notify_id_char_valid(id[i]))
          return EINA_FALSE;
     }
   return EINA_TRUE;
}

static void
_notify_meta_parse(const char *s, size_t len, Notify_Meta *meta)
{
   size_t pos = 0;

   while (pos < len)
     {
        const char *part = s + pos;
        const char *end = memchr(part, ':', len - pos);
        size_t part_len = end ? (size_t)(end - part) : (len - pos);
        const char *value;
        size_t value_len;

        pos += part_len + 1;

        /* a part is used only if it is at least two bytes long and its
         * second byte is '='; anything else is skipped, as in foot */
        if ((part_len < 2) || (part[1] != '='))
          continue;
        value = part + 2;
        value_len = part_len - 2;

        switch (part[0])
          {
           case 'i':
             meta->has_id = _notify_id_is_valid(value, value_len);
             if (meta->has_id)
               memcpy(meta->id, value, value_len), meta->id[value_len] = '\0';
             break;
           case 'p':
             if ((value_len == 5) && !strncmp(value, "title", 5))
               meta->payload = NOTIFY_PAYLOAD_TITLE;
             else if ((value_len == 4) && !strncmp(value, "body", 4))
               meta->payload = NOTIFY_PAYLOAD_BODY;
             else if ((value_len == 1) && (value[0] == '?'))
               meta->payload = NOTIFY_PAYLOAD_QUERY;
             else if ((value_len == 5) && !strncmp(value, "close", 5))
               meta->payload = NOTIFY_PAYLOAD_CLOSE;
             else if ((value_len == 5) && !strncmp(value, "alive", 5))
               meta->payload = NOTIFY_PAYLOAD_ALIVE;
             else
               meta->payload = NOTIFY_PAYLOAD_UNKNOWN;
             break;
           case 'd':
             meta->done = !(value_len == 1 && value[0] == '0');
             break;
           case 'e':
             meta->base64 = (value_len == 1 && value[0] == '1');
             break;
           case 'u':
             meta->urgency = ((value_len == 1) && (value[0] >= '0') &&
                              (value[0] <= '2')) ? (value[0] - '0') : -1;
             break;
           case 'o':
             if ((value_len == 6) && !strncmp(value, "always", 6))
               meta->occasion = NOTIFY_OCCASION_ALWAYS;
             else if ((value_len == 9) && !strncmp(value, "unfocused", 9))
               meta->occasion = NOTIFY_OCCASION_UNFOCUSED;
             else if ((value_len == 9) && !strncmp(value, "invisible", 9))
               meta->occasion = NOTIFY_OCCASION_INVISIBLE;
             else
               meta->occasion = NOTIFY_OCCASION_UNSET;
             break;
           case 'w':
             {
                char buf[32];
                char *endptr = NULL;
                long long v;

                if (value_len >= sizeof(buf))
                  break;
                memcpy(buf, value, value_len);
                buf[value_len] = '\0';
                errno = 0;
                v = strtoll(buf, &endptr, 10);
                if (errno || !endptr || *endptr || (v > INT_MAX))
                  break;
                if (v < -1)
                  v = -1;
                meta->timeout_ms = (int)v;
                break;
             }
           default:
             /* a, c, f, g, n, s, t and unknown keys are ignored */
             break;
          }
     }
}

/* Sanitize UTF-8 into dst, capped at NOTIFY_TEXT_MAX; \n kept if multiline */
static void
_notify_text_append(Eina_Strbuf *dst, const char *src, size_t len,
                    Eina_Bool multiline)
{
   int i = 0;

   while (i < (int)len)
     {
        Eina_Unicode g;
        char buf[8];
        int char_len;

        if (src[i])
          g = eina_unicode_utf8_next_get(src, &i);
        else
          {
             /* step over an embedded NUL by hand */
             g = 0;
             i++;
          }
        /* a dropped \t or \n joins words; termio renders a body \n <br/> */
        if ((g == '\t') || ((g == '\n') && !multiline))
          g = ' ';
        else if (((g < 0x20) && (g != '\n')) || (g == 0x7f) ||
                 ((g >= 0x80) && (g <= 0x9f)) ||
                 ((g >= 0xd800) && (g <= 0xdfff)) ||
                 (g > 0x10ffff))
          continue;
        char_len = codepoint_to_utf8(g, buf);
        if (eina_strbuf_length_get(dst) + char_len > NOTIFY_TEXT_MAX)
          break;
        eina_strbuf_append_length(dst, buf, char_len);
     }
}

static Eina_Bool
_notify_pending_matches(const Notify_Pending *pending, const Notify_Meta *meta)
{
   if (pending->has_id != meta->has_id)
     return EINA_FALSE;
   if (!meta->has_id)
     return EINA_TRUE;
   return !strcmp(pending->id, meta->id);
}

static Notify_Pending *
_notify_pending_find(Termpty *ty, const Notify_Meta *meta)
{
   const Eina_List *l;
   Notify_Pending *pending;

   EINA_LIST_FOREACH(ty->notify_pending, l, pending)
     {
        if (_notify_pending_matches(pending, meta))
          return pending;
     }
   return NULL;
}

static Notify_Pending *
_notify_pending_new(Termpty *ty, const Notify_Meta *meta)
{
   Notify_Pending *pending;

   pending = calloc(1, sizeof(Notify_Pending));
   if (!pending)
     return NULL;
   pending->title = eina_strbuf_new();
   pending->body = eina_strbuf_new();
   if (!pending->title || !pending->body)
     {
        eina_strbuf_free(pending->title);
        eina_strbuf_free(pending->body);
        free(pending);
        return NULL;
     }
   pending->has_id = meta->has_id;
   if (meta->has_id)
     strcpy(pending->id, meta->id);
   pending->b64_payload = NOTIFY_PAYLOAD_UNKNOWN;
   pending->occasion = NOTIFY_OCCASION_UNSET;
   pending->timeout_ms = -2;
   pending->urgency = -1;

   if (eina_list_count(ty->notify_pending) >= NOTIFY_PENDING_MAX)
     {
        Notify_Pending *oldest = eina_list_data_get(ty->notify_pending);

        ty->notify_pending = eina_list_remove(ty->notify_pending, oldest);
        eina_strbuf_free(oldest->title);
        eina_strbuf_free(oldest->body);
        free(oldest);
     }
   ty->notify_pending = eina_list_append(ty->notify_pending, pending);
   return pending;
}

static void
_notify_pending_del(Termpty *ty, Notify_Pending *pending)
{
   ty->notify_pending = eina_list_remove(ty->notify_pending, pending);
   eina_strbuf_free(pending->title);
   eina_strbuf_free(pending->body);
   free(pending);
}

/* Append raw payload bytes, dropping anything past NOTIFY_TEXT_MAX */
static void
_notify_raw_append(Eina_Strbuf *dst, const char *src, size_t len)
{
   size_t used = (size_t)eina_strbuf_length_get(dst);

   if (used >= NOTIFY_TEXT_MAX)
     return;
   if (len > NOTIFY_TEXT_MAX - used)
     len = NOTIFY_TEXT_MAX - used;
   eina_strbuf_append_length(dst, src, len);
}

static void
_notify_payload_append(Notify_Pending *pending, const Notify_Meta *meta,
                       const char *payload, size_t len)
{
   Eina_Strbuf *dst = (meta->payload == NOTIFY_PAYLOAD_TITLE) ?
      pending->title : pending->body;

   if (meta->base64)
     {
        Eina_Binbuf *decoded = eina_binbuf_new();
        size_t len_decoded;
        const char *decoded_s;

        if (!decoded)
          return;
        if (pending->b64_payload != meta->payload)
          memset(&pending->b64, 0, sizeof(pending->b64));
        pending->b64_payload = meta->payload;
        if (!ty_base64_stream_decode(&pending->b64, payload, len, decoded))
          {
             WRN("OSC 99: invalid base64 in payload, dropping it");
             eina_binbuf_free(decoded);
             return;
          }
        /* kept raw: a UTF-8 character may be split across two chunks */
        decoded_s = (const char *)eina_binbuf_string_get(decoded);
        len_decoded = eina_binbuf_length_get(decoded);
        _notify_raw_append(dst, decoded_s, len_decoded);
        eina_binbuf_free(decoded);
        return;
     }
   _notify_raw_append(dst, payload, len);
}

static void
_notify_pending_show(Termpty *ty, const Notify_Pending *pending)
{
   Eina_Strbuf *title, *body;
   int urgency = pending->urgency;

   title = eina_strbuf_new();
   body = eina_strbuf_new();
   if (!title || !body)
     {
        eina_strbuf_free(title);
        eina_strbuf_free(body);
        return;
     }
   /* the raw bytes of the chunks are sanitized once, here */
   _notify_text_append(title, eina_strbuf_string_get(pending->title),
                       eina_strbuf_length_get(pending->title), EINA_FALSE);
   _notify_text_append(body, eina_strbuf_string_get(pending->body),
                       eina_strbuf_length_get(pending->body), EINA_TRUE);
   /* a notification with no title uses the body as title; one with no
    * title and no body is ignored (kitty desktop-notifications.rst) */
   if (eina_strbuf_length_get(title) == 0)
     {
        Eina_Strbuf *swap = title;

        title = body;
        body = swap;
     }
   if (eina_strbuf_length_get(title) == 0)
     {
        eina_strbuf_free(title);
        eina_strbuf_free(body);
        return;
     }
   if (urgency < 0)
     urgency = ELM_SYS_NOTIFY_URGENCY_NORMAL;
   termpty_notify_show(ty, eina_strbuf_string_get(title),
                       eina_strbuf_string_get(body), urgency,
                       pending->timeout_ms == -2 ? -1 : pending->timeout_ms);
   eina_strbuf_free(title);
   eina_strbuf_free(body);
}

void
termpty_notify_show(Termpty *ty, const char *title, const char *body,
                    int urgency, int timeout_ms)
{
   Eina_Strbuf *title_sb, *body_sb;

   if (!ty->config->notification_escapes)
     return;

   title_sb = eina_strbuf_new();
   body_sb = eina_strbuf_new();
   if (!title_sb || !body_sb)
     {
        eina_strbuf_free(title_sb);
        eina_strbuf_free(body_sb);
        return;
     }
   if (title)
     _notify_text_append(title_sb, title, strlen(title), EINA_FALSE);
   if (body)
     _notify_text_append(body_sb, body, strlen(body), EINA_TRUE);
   if (eina_strbuf_length_get(title_sb) == 0 &&
       eina_strbuf_length_get(body_sb) == 0)
     {
        eina_strbuf_free(title_sb);
        eina_strbuf_free(body_sb);
        return;
     }
   termio_notification_send(ty->obj,
                            eina_strbuf_string_get(title_sb),
                            eina_strbuf_string_get(body_sb),
                            (Elm_Sys_Notify_Urgency)urgency, timeout_ms);
   eina_strbuf_free(title_sb);
   eina_strbuf_free(body_sb);
}

/* The capability query, answered with what is implemented and nothing else */
static void
_notify_query_reply(Termpty *ty, const Notify_Meta *meta, Eina_Bool bel)
{
   char buf[NOTIFY_ID_MAX + 128];
   int l;

   l = snprintf(buf, sizeof(buf),
                "\033]99;i=%s:p=?;p=title,body,?:o=always:u=0,1,2:w=1%s",
                meta->has_id ? meta->id : "0",
                bel ? "\007" : "\033\\");
   if ((l > 0) && (l < (int)sizeof(buf)))
     termpty_write(ty, buf, l);
}

void
termpty_notify_osc99(Termpty *ty, const char *s, size_t len,
                     Eina_Bool bel)
{
   const char *sep;
   Notify_Meta meta;
   Notify_Pending *pending;

   if (!ty->config->notification_escapes)
     {
        termpty_notify_free(ty);
        return;
     }
   sep = memchr(s, ';', len);
   if (!sep)
     {
        WRN("OSC 99: no payload separator");
        ty->decoding_error = EINA_TRUE;
        return;
     }
   memset(&meta, 0, sizeof(meta));
   meta.payload = NOTIFY_PAYLOAD_TITLE;
   meta.timeout_ms = -2;
   meta.urgency = -1;
   meta.done = EINA_TRUE;
   meta.occasion = NOTIFY_OCCASION_UNSET;
   _notify_meta_parse(s, sep - s, &meta);
   sep++;
   len -= (sep - s);

   switch (meta.payload)
     {
      case NOTIFY_PAYLOAD_QUERY:
         _notify_query_reply(ty, &meta, bel);
         return;
      case NOTIFY_PAYLOAD_CLOSE:
      case NOTIFY_PAYLOAD_ALIVE:
         return;
      default:
         break;
     }

   pending = _notify_pending_find(ty, &meta);
   if (!pending)
     pending = _notify_pending_new(ty, &meta);
   if (!pending)
     return;
   /* set keys override, unset keys keep the previous value */
   if (meta.urgency >= 0)
     pending->urgency = meta.urgency;
   if (meta.occasion != NOTIFY_OCCASION_UNSET)
     pending->occasion = meta.occasion;
   if (meta.timeout_ms != -2)
     pending->timeout_ms = meta.timeout_ms;
   if ((meta.payload == NOTIFY_PAYLOAD_TITLE) ||
       (meta.payload == NOTIFY_PAYLOAD_BODY))
     _notify_payload_append(pending, &meta, sep, len);
   if (meta.done)
     {
        _notify_pending_show(ty, pending);
        _notify_pending_del(ty, pending);
     }
}

void
termpty_notify_free(Termpty *ty)
{
   Notify_Pending *pending;

   EINA_LIST_FREE(ty->notify_pending, pending)
     {
        eina_strbuf_free(pending->title);
        eina_strbuf_free(pending->body);
        free(pending);
     }
}

#if defined(BINARY_TYTEST)
int
tytest_notify_meta_parse(void)
{
   Notify_Meta meta;

#define META_PARSE(s) memset(&meta, 0, sizeof(meta)); \
     meta.payload = NOTIFY_PAYLOAD_TITLE; \
     meta.timeout_ms = -2; \
     meta.urgency = -1; \
     meta.done = EINA_TRUE; \
     meta.occasion = NOTIFY_OCCASION_UNSET; \
     _notify_meta_parse(s, strlen(s), &meta)

   /* defaults for an empty metadata */
   META_PARSE("");
   assert(meta.payload == NOTIFY_PAYLOAD_TITLE);
   assert(meta.done);
   assert(!meta.base64);
   assert(meta.urgency == -1);
   assert(meta.occasion == NOTIFY_OCCASION_UNSET);
   assert(meta.timeout_ms == -2);
   assert(!meta.has_id);

   META_PARSE("i=abc:p=body");
   assert(meta.has_id && !strcmp(meta.id, "abc"));
   assert(meta.payload == NOTIFY_PAYLOAD_BODY);

   META_PARSE("d=0");
   assert(!meta.done);
   META_PARSE("d=2");
   assert(meta.done);
   META_PARSE("e=1");
   assert(meta.base64);
   META_PARSE("e=2");
   assert(!meta.base64);

   META_PARSE("u=0");
   assert(meta.urgency == 0);
   META_PARSE("u=2");
   assert(meta.urgency == 2);
   META_PARSE("u=3");
   assert(meta.urgency == -1);

   META_PARSE("o=unfocused");
   assert(meta.occasion == NOTIFY_OCCASION_UNFOCUSED);
   META_PARSE("o=sometimes");
   assert(meta.occasion == NOTIFY_OCCASION_UNSET);

   META_PARSE("w=-5");
   assert(meta.timeout_ms == -1);
   META_PARSE("w=12x");
   assert(meta.timeout_ms == -2);
   META_PARSE("w=99999999999");
   assert(meta.timeout_ms == -2);
   META_PARSE("w=5000");
   assert(meta.timeout_ms == 5000);

   /* ids */
   META_PARSE("i=");
   assert(!meta.has_id);
   META_PARSE("i=a/b");
   assert(!meta.has_id);
   {
      char *long_id = malloc(NOTIFY_ID_MAX + 2);

      memset(long_id, 'a', NOTIFY_ID_MAX + 1);
      long_id[NOTIFY_ID_MAX + 1] = '\0';
      META_PARSE("");
      /* build "i=<long_id>" by hand */
      {
         char buf[NOTIFY_ID_MAX + 8];

         snprintf(buf, sizeof(buf), "i=%s", long_id);
         memset(&meta, 0, sizeof(meta));
         _notify_meta_parse(buf, strlen(buf), &meta);
         assert(!meta.has_id);
         long_id[NOTIFY_ID_MAX] = '\0';
         snprintf(buf, sizeof(buf), "i=%s", long_id);
         memset(&meta, 0, sizeof(meta));
         _notify_meta_parse(buf, strlen(buf), &meta);
         assert(meta.has_id);
      }
      free(long_id);
   }

   META_PARSE("p=?");
   assert(meta.payload == NOTIFY_PAYLOAD_QUERY);
   META_PARSE("p=query");
   assert(meta.payload == NOTIFY_PAYLOAD_UNKNOWN);
   META_PARSE("p=close");
   assert(meta.payload == NOTIFY_PAYLOAD_CLOSE);
   META_PARSE("p=alive");
   assert(meta.payload == NOTIFY_PAYLOAD_ALIVE);

   /* parts that are not key=value are skipped */
   META_PARSE("::x:=v:ab=c:i=ok");
   assert(meta.has_id && !strcmp(meta.id, "ok"));

   /* the last key wins */
   META_PARSE("u=0:u=2");
   assert(meta.urgency == 2);

#undef META_PARSE
   return 0;
}

int
tytest_notify_text_append(void)
{
   Eina_Strbuf *sb = eina_strbuf_new();
   size_t i;

   /* C0, DEL and C1 are dropped */
   eina_strbuf_reset(sb);
   _notify_text_append(sb, "a\x01\x07\x7f\x1b\233b", 7, EINA_FALSE);
   assert(!strcmp(eina_strbuf_string_get(sb), "ab"));

   /* surrogates, which is how eina reports invalid bytes, are dropped */
   eina_strbuf_reset(sb);
   _notify_text_append(sb, "\x80\xff", 2, EINA_FALSE);
   assert(eina_strbuf_length_get(sb) == 0);

   /* a tab becomes a space: dropping it would join the words */
   eina_strbuf_reset(sb);
   _notify_text_append(sb, "tab\there", 8, EINA_TRUE);
   assert(!strcmp(eina_strbuf_string_get(sb), "tab here"));

   /* a newline is kept in the body: termio renders it as a line break */
   eina_strbuf_reset(sb);
   _notify_text_append(sb, "line1\nline2", 11, EINA_TRUE);
   assert(!strcmp(eina_strbuf_string_get(sb), "line1\nline2"));

   /* the title is a single line: a newline becomes a space, as a tab */
   eina_strbuf_reset(sb);
   _notify_text_append(sb, "line1\nline2", 11, EINA_FALSE);
   assert(!strcmp(eina_strbuf_string_get(sb), "line1 line2"));

   /* a 4-byte character that would cross NOTIFY_TEXT_MAX is not cut
    * in half */
   eina_strbuf_reset(sb);
   for (i = 0; i < NOTIFY_TEXT_MAX / 3; i++)
     _notify_text_append(sb, "\xe2\x9c\x93", 3, EINA_TRUE);  /* U+2713 */
   assert((size_t)eina_strbuf_length_get(sb) <= NOTIFY_TEXT_MAX);
   assert((size_t)eina_strbuf_length_get(sb) % 3 == 0);

   eina_strbuf_free(sb);
   return 0;
}
#endif
