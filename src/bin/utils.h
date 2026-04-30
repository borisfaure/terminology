#ifndef TERMINOLOGY_UTILS_H_
#define TERMINOLOGY_UTILS_H_

#include <Eina.h>
#include "config.h"

Eina_Bool homedir_get(char *buf, size_t size);
void open_url(const Config *config, const char *url);

char * ty_eina_unicode_base64_decode(Eina_Unicode *c);

/* POSIX single-quote shell escaping.  Wraps arg in single quotes and
 * escapes any embedded single-quote via the standard '"'"' dance.
 *
 * Returns a newly malloc'd string; caller must free().
 * Returns NULL on allocation failure or NULL input.
 *
 * Single-quoted strings in POSIX sh are immune to ALL metacharacters
 * (backtick, $, \, newline, …).  The only character that cannot appear
 * literally inside single-quotes is ' itself, handled via the '"'"' idiom.
 *
 * This is safer than ecore_file_escape_name(), which does NOT escape
 * backtick — a known shell command-substitution injection class.
 */
char *shell_quote(const char *arg);

#endif
