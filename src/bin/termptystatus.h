#ifndef TERMINOLOGY_TERMPTYSTATUS_H_
#define TERMINOLOGY_TERMPTYSTATUS_H_

#include "termpty.h"

/* OSC 7501 program status, following the Program Status Protocol
 * revision 0.4: https://superlogical.com/rex/docs/build/program-status
 */

/* In urgency order: blocked and error need the user to act, done to be
 * seen, working is ambient. The summary picks the record with the
 * highest value, ties going to the most recently updated. */
typedef enum _Termpty_Program_State
{
   TERMPTY_PROGRAM_STATE_NONE = 0,
   TERMPTY_PROGRAM_STATE_IDLE,
   TERMPTY_PROGRAM_STATE_WORKING,
   TERMPTY_PROGRAM_STATE_DONE,
   TERMPTY_PROGRAM_STATE_ERROR,
   TERMPTY_PROGRAM_STATE_BLOCKED,
} Termpty_Program_State;

typedef enum _Termpty_Program_Kind
{
   TERMPTY_PROGRAM_KIND_NONE = 0,
   TERMPTY_PROGRAM_KIND_PERMISSION,
   TERMPTY_PROGRAM_KIND_QUESTION,
   TERMPTY_PROGRAM_KIND_AUTH,
} Termpty_Program_Kind;

typedef struct _Termpty_Program_Record
{
   EINA_INLIST;
   Termpty_Program_State state;
   Termpty_Program_Kind kind;
   signed char progress; /* -1: the record has none */
   char id[129];   /* "" is the root record */
   char app[33];   /* "" when the report had none */
   char *title;    /* NULL when the report had none */
   char *msg;      /* NULL when the report had none */
} Termpty_Program_Record;

/* The state of the most urgent record, and its progress (-1 if it has
 * none). No records: TERMPTY_PROGRAM_STATE_NONE. */
void termpty_program_status_summary_get(const Termpty *ty,
                                        Termpty_Program_State *state,
                                        int *progress);

void termpty_program_status_clear_all(Termpty *ty);

#endif
