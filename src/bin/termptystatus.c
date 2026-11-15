#include "private.h"
#include <Elementary.h>
#include "termptystatus.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

/* Limits from the Program Status Protocol, revision 0.4:
 * https://superlogical.com/rex/docs/build/program-status
 */
#define PS_KEY_MAX            16
#define PS_ID_MAX             128
#define PS_ID_SEGMENT_MAX     32
#define PS_ID_LEVELS_MAX      8
#define PS_APP_MAX            32
#define PS_TITLE_ENCODED_MAX  256
#define PS_TITLE_DECODED_MAX  192
#define PS_MSG_ENCODED_MAX    2732
#define PS_MSG_DECODED_MAX    2048
#define PS_RECORDS_MAX        256

/* The most a 7501 payload can be: the OSC buffer holds 4096
 * codepoints including "7501;". */
#define PS_BODY_MAX           4096

#define _PS_REC(l) EINA_INLIST_CONTAINER_GET(l, Termpty_Program_Record)

/* The pairs of one report, validated but not yet applied. Strings
 * point into a scratch copy of the payload. */
struct ps_staging
{
   const char *state;
   const char *id;
   const char *kind;
   const char *progress;
   const char *app;
   const char *title;
   const char *msg;
};

static void
_ps_record_free(Termpty_Program_Record *rec)
{
   free(rec->title);
   free(rec->msg);
   free(rec);
}

static Eina_Bool
_ps_key_ok(const char *key, size_t len)
{
   size_t i;

   if ((len == 0) || (len > PS_KEY_MAX))
     return EINA_FALSE;
   for (i = 0; i < len; i++)
     {
        if ((key[i] < 'a') || (key[i] > 'z'))
          return EINA_FALSE;
     }
   return EINA_TRUE;
}

static Eina_Bool
_ps_value_byte_ok(char c)
{
   return ((c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '_' || c == '.' || c == ',' || c == '+' ||
           c == '/' || c == '=' || c == '-');
}

static Eina_Bool
_ps_value_ok(const char *value, size_t len)
{
   size_t i;

   for (i = 0; i < len; i++)
     {
        if (!_ps_value_byte_ok(value[i]))
          return EINA_FALSE;
     }
   return EINA_TRUE;
}

static void
_ps_stage_set(struct ps_staging *staging, const char *key, size_t key_len,
              char *value)
{
   /* last value wins: a repeated key overwrites the earlier one */
   switch (key_len)
     {
      case 2:
        if (memcmp(key, "id", 2) == 0)
          staging->id = value;
        break;
      case 3:
        if (memcmp(key, "app", 3) == 0)
          staging->app = value;
        else if (memcmp(key, "msg", 3) == 0)
          staging->msg = value;
        break;
      case 4:
        if (memcmp(key, "kind", 4) == 0)
          staging->kind = value;
        break;
      case 5:
        if (memcmp(key, "state", 5) == 0)
          staging->state = value;
        else if (memcmp(key, "title", 5) == 0)
          staging->title = value;
        break;
      case 8:
        if (memcmp(key, "progress", 8) == 0)
          staging->progress = value;
        break;
      default:
        break;
     }
}

/* Split the payload into pairs on ':', each pair on its first '='.
 * Whitespace around keys and values is removed. A malformed pair is
 * skipped; the rest of the report still applies. */
static void
_ps_pairs_parse(char *body, struct ps_staging *staging)
{
   char *p = body;

   while (p)
     {
        char *next = strchr(p, ':');
        char *pair_end, *eq, *key, *key_end, *value, *value_end;

        if (next)
          *next++ = '\0';
        pair_end = p + strlen(p);

        while ((p < pair_end) && ((p[0] == ' ') || (p[0] == '\t')))
          p++;
        while ((pair_end > p) &&
               ((pair_end[-1] == ' ') || (pair_end[-1] == '\t')))
          pair_end--;

        eq = memchr(p, '=', pair_end - p);
        if (!eq)
          goto next_pair;

        key = p;
        key_end = eq;
        while ((key_end > key) &&
               ((key_end[-1] == ' ') || (key_end[-1] == '\t')))
          key_end--;
        value = eq + 1;
        value_end = pair_end;
        while ((value < value_end) && ((value[0] == ' ') || (value[0] == '\t')))
          value++;
        while ((value_end > value) &&
               ((value_end[-1] == ' ') || (value_end[-1] == '\t')))
          value_end--;

        if (!_ps_key_ok(key, key_end - key))
          goto next_pair;
        if (!_ps_value_ok(value, value_end - value))
          goto next_pair;

        *key_end = '\0';
        *value_end = '\0';
        _ps_stage_set(staging, key, key_end - key, value);

next_pair:
        p = next;
     }
}

static Eina_Bool
_ps_state_parse(const char *s, Termpty_Program_State *state,
                Eina_Bool *is_clear)
{
   struct { const char *name; Termpty_Program_State value; } const states[] = {
       { "idle",    TERMPTY_PROGRAM_STATE_IDLE    },
       { "working", TERMPTY_PROGRAM_STATE_WORKING },
       { "done",    TERMPTY_PROGRAM_STATE_DONE    },
       { "error",   TERMPTY_PROGRAM_STATE_ERROR   },
       { "blocked", TERMPTY_PROGRAM_STATE_BLOCKED },
   };
   size_t i;

   for (i = 0; i < sizeof(states) / sizeof(states[0]); i++)
     {
        if (strcmp(s, states[i].name) == 0)
          {
             *state = states[i].value;
             *is_clear = EINA_FALSE;
             return EINA_TRUE;
          }
     }
   if (strcmp(s, "clear") == 0)
     {
        *is_clear = EINA_TRUE;
        return EINA_TRUE;
     }
   return EINA_FALSE;
}

/* "segment(/segment)*", each segment 1-32 bytes of [A-Za-z0-9_.+-],
 * at most 8 levels and 128 bytes in total */
static Eina_Bool
_ps_id_ok(const char *id)
{
   size_t len = strlen(id), segment = 0;
   int levels = 1;
   const char *p;

   if ((len == 0) || (len > PS_ID_MAX))
     return EINA_FALSE;
   for (p = id; ; p++)
     {
        if ((*p == '/') || (*p == '\0'))
          {
             if (segment == 0)
               return EINA_FALSE; /* empty segment, as in "a//b" */
             if (*p == '\0')
               break;
             levels++;
             segment = 0;
             if (levels > PS_ID_LEVELS_MAX)
               return EINA_FALSE;
          }
        else if (((*p >= 'A') && (*p <= 'Z')) ||
                 ((*p >= 'a') && (*p <= 'z')) ||
                 ((*p >= '0') && (*p <= '9')) ||
                 (*p == '_') || (*p == '.') || (*p == '+') || (*p == '-'))
          segment++;
        else
          return EINA_FALSE;
        if (segment > PS_ID_SEGMENT_MAX)
          return EINA_FALSE;
     }
   return EINA_TRUE;
}

static Eina_Bool
_ps_app_ok(const char *app)
{
   size_t len = strlen(app), i;

   if ((len == 0) || (len > PS_APP_MAX))
     return EINA_FALSE;
   for (i = 0; i < len; i++)
     {
        char c = app[i];

        if (((c < 'A') || (c > 'Z')) && ((c < 'a') || (c > 'z')) &&
            ((c < '0') || (c > '9')) &&
            (c != '_') && (c != '.') && (c != '+') && (c != '-'))
          return EINA_FALSE;
     }
   return EINA_TRUE;
}

/* ty_base64_stream_decode() is deliberately lenient (it must accept
 * chunks cut anywhere), so 7501 checks the shape itself: padding only
 * closes the last quantum, at most two '=', never leaving 4n+1
 * characters. Padding is optional. */
static Eina_Bool
_ps_b64_shape_ok(const char *value, size_t len)
{
   size_t tail = 0, i;

   while ((tail < 2) && (tail < len) && (value[len - 1 - tail] == '='))
     tail++;
   if (((len - tail) % 4) == 1)
     return EINA_FALSE;
   for (i = 0; i < len - tail; i++)
     {
        if (value[i] == '=')
          return EINA_FALSE;
     }
   return EINA_TRUE;
}

/* Decode one UTF-8 character, as strictly as eina does: no overlong
 * forms, no surrogates, nothing above U+10FFFF. */
static int
_ps_utf8_next(const unsigned char *s, size_t len, Eina_Unicode *cp)
{
   unsigned char b = s[0];
   int n, i;
   Eina_Unicode v;

   if (b < 0x80)
     {
        *cp = b;
        return 1;
     }
   if ((b & 0xe0) == 0xc0)
     {
        n = 1;
        v = b & 0x1f;
        if (v < 2)
          return 0; /* overlong */
     }
   else if ((b & 0xf0) == 0xe0)
     {
        n = 2;
        v = b & 0x0f;
     }
   else if ((b & 0xf8) == 0xf0)
     {
        n = 3;
        v = b & 0x07;
        if (v > 4)
          return 0; /* above U+10FFFF */
     }
   else
     return 0;
   if ((size_t)n + 1 > len)
     return 0;
   for (i = 1; i <= n; i++)
     {
        if ((s[i] & 0xc0) != 0x80)
          return 0;
        v = (v << 6) | (s[i] & 0x3f);
     }
   if ((n == 2) && (v < 0x800))
     return 0; /* overlong */
   if ((n == 3) && (v < 0x10000))
     return 0; /* overlong */
   if ((v >= 0xd800) && (v <= 0xdfff))
     return 0; /* surrogate */
   *cp = v;
   return n + 1;
}

static Eina_Bool
_ps_cp_stripped(Eina_Unicode cp)
{
   /* bidi marks, zero-width joiners and friends: invisible, but they
    * can reorder or hide the text around them */
   return ((cp >= 0x200b && cp <= 0x200f) ||
           (cp >= 0x202a && cp <= 0x202e) ||
           (cp >= 0x2060 && cp <= 0x2064) ||
           (cp >= 0x2066 && cp <= 0x2069) ||
           cp == 0xfeff);
}

/* Decode the base64 of msg or title, check its size, reject control
 * characters and strip the invisible ones. Returns a malloc'd string,
 * or NULL with *discard set when the whole report is invalid. */
static char *
_ps_text_decode(const char *value, size_t max_encoded, size_t max_decoded,
                Eina_Bool *discard)
{
   Eina_Binbuf *bb;
   Ty_Base64_Stream st = { 0, 0 };
   size_t encoded_len = strlen(value);
   const unsigned char *raw;
   size_t raw_len, i;
   char *res, *w;

   *discard = EINA_FALSE;
   if (encoded_len > max_encoded)
     goto discard;
   if (!_ps_b64_shape_ok(value, encoded_len))
     goto discard;

   bb = eina_binbuf_new();
   if (!bb)
     return NULL;
   if (!ty_base64_stream_decode(&st, value, encoded_len, bb))
     goto discard_free;
   raw_len = eina_binbuf_length_get(bb);
   if (raw_len > max_decoded)
     goto discard_free;
   raw = (const unsigned char *)eina_binbuf_string_get(bb);

   res = malloc(raw_len + 1);
   if (!res)
     {
        eina_binbuf_free(bb);
        return NULL;
     }
   w = res;
   i = 0;
   while (i < raw_len)
     {
        Eina_Unicode cp;
        int n = _ps_utf8_next(raw + i, raw_len - i, &cp);

        if (n == 0)
          goto discard_free_res; /* not UTF-8 */
        /* C0, DEL and the C1 codepoints, an embedded NUL included */
        if ((cp < 0x20) || (cp == 0x7f) || (cp >= 0x80 && cp <= 0x9f))
          goto discard_free_res;
        if (!_ps_cp_stripped(cp))
          {
             memcpy(w, raw + i, n);
             w += n;
          }
        i += n;
     }
   *w = '\0';
   eina_binbuf_free(bb);
   return res;

discard_free_res:
   free(res);
discard_free:
   eina_binbuf_free(bb);
discard:
   *discard = EINA_TRUE;
   return NULL;
}

/* no EINA_INLIST_FOREACH: it trips the undefined-behavior sanitizer */
static Termpty_Program_Record *
_ps_record_find(Termpty *ty, const char *id)
{
   Eina_Inlist *l;

   for (l = ty->program_records; l; l = l->next)
     {
        Termpty_Program_Record *rec = _PS_REC(l);

        if (strcmp(rec->id, id) == 0)
          return rec;
     }
   return NULL;
}

/* clear with an id takes the record and every record beneath it;
 * a sibling that only shares the prefix stays */
static void
_ps_apply_clear(Termpty *ty, const char *id)
{
   size_t id_len = strlen(id);
   Eina_Inlist *l = ty->program_records;

   while (l)
     {
        Eina_Inlist *next = l->next;
        Termpty_Program_Record *rec = _PS_REC(l);

        if ((strncmp(rec->id, id, id_len) == 0) &&
            ((rec->id[id_len] == '\0') || (rec->id[id_len] == '/')))
          {
             ty->program_records = eina_inlist_remove(ty->program_records, l);
             ty->program_records_count--;
             _ps_record_free(rec);
          }
        l = next;
     }
}

void
termpty_program_status_handle(Termpty *ty, const Eina_Unicode *body)
{
   char ascii[PS_BODY_MAX];
   size_t n = 0;
   const Eina_Unicode *p;
   struct ps_staging staging;
   Termpty_Program_State state = TERMPTY_PROGRAM_STATE_NONE;
   Termpty_Program_Kind kind = TERMPTY_PROGRAM_KIND_NONE;
   Eina_Bool is_clear = EINA_FALSE;
   Eina_Bool discard = EINA_FALSE;
   signed char progress = -1;
   char *title = NULL, *msg = NULL;
   Termpty_Program_Record *rec;

   if (!body || !*body)
     return;

   /* the payload grammar is bytes; a codepoint above 0x7f makes its
    * pair fail the value check, and so is skipped */
   for (p = body; *p; p++)
     {
        if (n >= sizeof(ascii) - 1)
          return;
        ascii[n++] = (*p < 0x80) ? (char)*p : '\x01';
     }
   ascii[n] = '\0';

   memset(&staging, 0, sizeof(staging));
   _ps_pairs_parse(ascii, &staging);

   /* no state, or a state outside the grammar: the report is ignored */
   if (!staging.state)
     return;
   if (!_ps_state_parse(staging.state, &state, &is_clear))
     return;
   if (staging.id && !_ps_id_ok(staging.id))
     return;

   /* msg and title make the whole report invalid when they are over
    * their limits, undecodable or hold control characters */
   if (staging.title)
     {
        title = _ps_text_decode(staging.title, PS_TITLE_ENCODED_MAX,
                                PS_TITLE_DECODED_MAX, &discard);
        if (discard)
          return;
     }
   if (staging.msg)
     {
        msg = _ps_text_decode(staging.msg, PS_MSG_ENCODED_MAX,
                              PS_MSG_DECODED_MAX, &discard);
        if (discard)
          {
             free(title);
             return;
          }
     }

   /* a value that is valid but out of place or out of range is absent;
    * the rest of the report applies */
   if (staging.kind)
     {
        if (state != TERMPTY_PROGRAM_STATE_BLOCKED)
          staging.kind = NULL;
        else if (strcmp(staging.kind, "permission") == 0)
          kind = TERMPTY_PROGRAM_KIND_PERMISSION;
        else if (strcmp(staging.kind, "question") == 0)
          kind = TERMPTY_PROGRAM_KIND_QUESTION;
        else if (strcmp(staging.kind, "auth") == 0)
          kind = TERMPTY_PROGRAM_KIND_AUTH;
     }
   if (staging.progress && (state != TERMPTY_PROGRAM_STATE_WORKING) &&
       (state != TERMPTY_PROGRAM_STATE_BLOCKED))
     staging.progress = NULL;
   if (staging.progress)
     {
        int v = 0;
        const char *s = staging.progress;

        for (; *s; s++)
          {
             if ((*s < '0') || (*s > '9'))
               break;
             v = v * 10 + (*s - '0');
             if (v > 100)
               break;
          }
        /* every character a digit, at least one, and 100 at most */
        if ((s != staging.progress) && (*s == '\0'))
          progress = (signed char)v;
     }
   if (staging.app && !_ps_app_ok(staging.app))
     staging.app = NULL;

   ty->program_status_seen = EINA_TRUE;

   if (is_clear)
     {
        /* decoded above, owned by nobody on this path */
        free(title);
        free(msg);
        if (staging.id)
          _ps_apply_clear(ty, staging.id);
        else
          {
             while (ty->program_records)
               {
                  rec = _PS_REC(ty->program_records);
                  ty->program_records = eina_inlist_remove(
                     ty->program_records, ty->program_records);
                  ty->program_records_count--;
                  _ps_record_free(rec);
               }
          }
        return;
     }

   if (!staging.id)
     staging.id = "";
   rec = _ps_record_find(ty, staging.id);
   if (rec)
     {
        ty->program_records = eina_inlist_remove(ty->program_records,
                                                 EINA_INLIST_GET(rec));
        free(rec->title);
        rec->title = NULL;
        free(rec->msg);
        rec->msg = NULL;
     }
   else
     {
        if (ty->program_records_count >= PS_RECORDS_MAX)
          {
             /* the least recently updated record is the head of the list */
             Termpty_Program_Record *old = _PS_REC(ty->program_records);

             ty->program_records = eina_inlist_remove(ty->program_records,
                                                      ty->program_records);
             ty->program_records_count--;
             _ps_record_free(old);
          }
        rec = calloc(1, sizeof(*rec));
        if (!rec)
          {
             free(title);
             free(msg);
             return;
          }
        ty->program_records_count++;
     }

   rec->state = state;
   rec->kind = kind;
   rec->progress = progress;
   eina_strlcpy(rec->id, staging.id, sizeof(rec->id));
   eina_strlcpy(rec->app, staging.app ? staging.app : "", sizeof(rec->app));
   rec->title = title;
   rec->msg = msg;
   ty->program_records = eina_inlist_append(ty->program_records,
                                            EINA_INLIST_GET(rec));
}

void
termpty_program_status_summary_get(const Termpty *ty,
                                   Termpty_Program_State *state,
                                   int *progress)
{
   const Termpty_Program_Record *best = NULL;
   const Eina_Inlist *l;

   /* from the tail: the first record of each urgency wins, so ties go
    * to the most recently updated one */
   for (l = ty->program_records ? ty->program_records->last : NULL;
        l;
        l = l->prev)
     {
        const Termpty_Program_Record *rec = _PS_REC(l);

        if (!best || (rec->state > best->state))
          best = rec;
     }
   if (best)
     {
        *state = best->state;
        *progress = best->progress;
     }
   else
     {
        *state = TERMPTY_PROGRAM_STATE_NONE;
        *progress = -1;
     }
}

void
termpty_program_status_clear_all(Termpty *ty)
{
   while (ty->program_records)
     {
        Termpty_Program_Record *rec = _PS_REC(ty->program_records);

        ty->program_records = eina_inlist_remove(ty->program_records,
                                                 ty->program_records);
        _ps_record_free(rec);
     }
   ty->program_records_count = 0;
   ty->program_status_seen = EINA_FALSE;
}
