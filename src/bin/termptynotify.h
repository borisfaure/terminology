#ifndef TERMINOLOGY_TERMPTY_NOTIFY_H_
#define TERMINOLOGY_TERMPTY_NOTIFY_H_ 1

/* Identifier and text limits; NOTIFY_PENDING_MAX is how many
 * incomplete notifications a terminal buffers. */
#define NOTIFY_ID_MAX       128
#define NOTIFY_TEXT_MAX     1024
#define NOTIFY_PENDING_MAX  8

#include "termpty.h"

/* Handle OSC 99;metadata;payload; bel is EINA_TRUE for a BEL terminator */
void termpty_notify_osc99(Termpty *ty, const char *s, size_t len,
                          Eina_Bool bel);

/* Sanitize (NOTIFY_TEXT_MAX) and show unless gated; -1 args mean unset */
void termpty_notify_show(Termpty *ty, const char *title, const char *body,
                         int urgency, int timeout_ms);

/* Drop the notifications still waiting for their last chunk */
void termpty_notify_free(Termpty *ty);

#endif
