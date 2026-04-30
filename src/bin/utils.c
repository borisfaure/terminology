#include "private.h"
#include "utils.h"
#include "sb.h"

#include <Ecore.h>
#include <Ecore_File.h>
#include <Emile.h>

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>

char *
shell_quote(const char *arg)
{
   const char *p;
   char       *quoted, *q;
   size_t      quoted_len;

   if (!arg) return NULL;

   /* Size the output:
    *   2 chars for outer single-quotes
    *   each ' in arg expands to 5 chars: '"'"'
    *   all other chars are 1 char each
    *   +1 for trailing NUL
    */
   quoted_len = 2;
   for (p = arg; *p; p++)
     quoted_len += (*p == '\'') ? 5 : 1;

   quoted = malloc(quoted_len + 1);
   if (!quoted) return NULL;

   q = quoted;
   *q++ = '\'';
   for (p = arg; *p; p++)
     {
        if (*p == '\'')
          {
             *q++ = '\'';
             *q++ = '"';
             *q++ = '\'';
             *q++ = '"';
             *q++ = '\'';
          }
        else
          *q++ = *p;
     }
   *q++ = '\'';
   *q   = '\0';
   return quoted;
}

Eina_Bool
homedir_get(char *buf, size_t size)
{
   const char *home = getenv("HOME");
   if (!home)
     {
        uid_t uid = getuid();
        struct passwd *pw = getpwuid(uid);
        if (pw) home = pw->pw_dir;
     }
   if (!home)
     {
        ERR("Could not get $HOME");
        return EINA_FALSE;
     }
   return eina_strlcpy(buf, home, size) < size;
}


void
open_url(const Config *config, const char *url)
{
   char buf[PATH_MAX], *s = NULL, *quoted = NULL;
   const char *cmd;
   const char *prefix = "http://";
   Eina_Strbuf *sb = NULL;

   EINA_SAFETY_ON_NULL_RETURN(config);

   if (!(config->helper.url.general) ||
       !(config->helper.url.general[0]))
     return;
   if (!url || url[0] == '\0')
     return;

   cmd = config->helper.url.general;

   sb = eina_strbuf_new();
   if (!sb)
     return;
   eina_strbuf_append(sb, url);
   eina_strbuf_trim(sb);

   s = eina_strbuf_string_steal(sb);
   eina_strbuf_free(sb);
   sb = NULL;
   if (!s)
     goto end;
   if (casestartswith(s, "http://") ||
        casestartswith(s, "https://") ||
        casestartswith(s, "ftp://") ||
        casestartswith(s, "mailto:"))
     prefix = "";

   quoted = shell_quote(s);
   if (!quoted)
     goto end;

   snprintf(buf, sizeof(buf), "%s %s%s", cmd, prefix, quoted);

   WRN("trying to launch '%s'", buf);
   ecore_exe_run(buf, NULL);

end:
   free(quoted);
   free(s);
}

char *
ty_eina_unicode_base64_decode(Eina_Unicode *unicode)
{
   int utf8_len = 0;
   Eina_Binbuf *bb;
   char *src;
   char *res;
   Eina_Strbuf *sb;

   src = eina_unicode_unicode_to_utf8(unicode, &utf8_len);
   if (!src)
     return NULL;
   sb = eina_strbuf_manage_new_length(src, utf8_len);
   if (!sb)
     {
        free(src);
        return NULL;
     }

   bb = emile_base64_decode(sb);
   eina_strbuf_free(sb);
   if (!bb)
     return NULL;

   res = (char*) eina_binbuf_string_steal(bb);
   eina_binbuf_free(bb);
   return res;
}

#if defined(BINARY_TYTEST)


int tytest_base64(void)
{
   Eina_Unicode *src;
   char *res;
   const char *expected;

#if defined(__has_feature)
#  if __has_feature(memory_sanitizer)
// disable with msan due to false positives
   return 0;
#  endif
#endif

   const char *terminology_rox = "VGVybWlub2xvZ3kgcm94IQ==";
   src = eina_unicode_utf8_to_unicode(terminology_rox, NULL);
   assert(src);
   res = ty_eina_unicode_base64_decode(src);
   assert(res);
   expected = "Terminology rox!";
   assert(memcmp(res, expected, strlen(expected)) == 0);
   free(src);
   free(res);


   const char *hearts = "4pml4pmh8J+RjfCfmrLinL8g4p2AIOKdgfCfmYw=";
   src = eina_unicode_utf8_to_unicode(hearts, NULL);
   assert(src);
   res = ty_eina_unicode_base64_decode(src);
   assert(res);
   expected = "♥♡👍🚲✿ ❀ ❁🙌";
   assert(memcmp(res, expected, strlen(expected)) == 0);
   free(src);
   free(res);

   return 0;
}

int tytest_shell_quote(void)
{
   char *q;

   /* 1. Empty string */
   q = shell_quote("");
   assert(q);
   assert(strcmp(q, "''") == 0);
   free(q);

   /* 2. Plain text (no metachars) */
   q = shell_quote("hello");
   assert(q);
   assert(strcmp(q, "'hello'") == 0);
   free(q);

   /* 3. Spaces */
   q = shell_quote("hello world");
   assert(q);
   assert(strcmp(q, "'hello world'") == 0);
   free(q);

   /* 4. Backtick — the bug we are fixing.
    *    Inside single quotes, backtick is literal. */
   q = shell_quote("`id`");
   assert(q);
   assert(strcmp(q, "'`id`'") == 0);
   free(q);

   /* 5. Dollar + parens (command substitution attempt) */
   q = shell_quote("$(whoami)");
   assert(q);
   assert(strcmp(q, "'$(whoami)'") == 0);
   free(q);

   /* 6. Backslash (literal inside single quotes) */
   q = shell_quote("a\\b");
   assert(q);
   assert(strcmp(q, "'a\\b'") == 0);
   free(q);

   /* 7. Newline (literal inside single quotes) */
   q = shell_quote("a\nb");
   assert(q);
   assert(strcmp(q, "'a\nb'") == 0);
   free(q);

   /* 8. Single quote alone — the only character that needs escaping.
    *    ' becomes '"'"' (close-SQ, open-DQ, lit-SQ, close-DQ, open-SQ).
    *    Wrapped in outer quotes: ''"'"''  (7 bytes) */
   q = shell_quote("'");
   assert(q);
   assert(strcmp(q, "''\"'\"''") == 0);
   free(q);

   /* 9. One single quote in middle */
   q = shell_quote("o'reilly");
   assert(q);
   assert(strcmp(q, "'o'\"'\"'reilly'") == 0);
   free(q);

   /* 10. Multiple single quotes */
   q = shell_quote("'a'");
   assert(q);
   assert(strcmp(q, "''\"'\"'a'\"'\"''") == 0);
   free(q);

   /* 11. NULL input returns NULL gracefully */
   q = shell_quote(NULL);
   assert(q == NULL);

   /* 12. Mix of all metacharacters in one string.
    *     Inside single quotes, every byte except ' is literal. */
   q = shell_quote("`$&|;<>(){}[]!#*?~\"");
   assert(q);
   assert(strcmp(q, "'`$&|;<>(){}[]!#*?~\"'") == 0);
   free(q);

   return 0;
}
#endif
