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

static void
_ps_record_free(Termpty_Program_Record *rec)
{
   free(rec->title);
   free(rec->msg);
   free(rec);
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
