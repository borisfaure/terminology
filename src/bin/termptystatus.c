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

void
termpty_program_status_summary_get(const Termpty *ty,
                                   Termpty_Program_State *state,
                                   int *progress)
{
   const Termpty_Program_Record *best = NULL;
   const Termpty_Program_Record *rec;

   /* from the tail: the first record of each urgency wins, so ties go
    * to the most recently updated one */
   EINA_INLIST_REVERSE_FOREACH(ty->program_records, rec)
     {
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
