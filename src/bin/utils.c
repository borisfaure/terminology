#include "private.h"
#include "utils.h"
#include "sb.h"

#include <Ecore.h>
#include <Ecore_File.h>
#include <Emile.h>

#include <assert.h>
#include <unistd.h>
#include <pwd.h>

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
   char buf[PATH_MAX], *s = NULL, *escaped = NULL;
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

   s = eina_str_escape(eina_strbuf_string_get(sb));
   if (!s)
     goto end;
   if (casestartswith(s, "http://") ||
        casestartswith(s, "https://") ||
        casestartswith(s, "ftp://") ||
        casestartswith(s, "mailto:"))
     prefix = "";

   escaped = ecore_file_escape_name(s);
   if (!escaped)
     goto end;

   snprintf(buf, sizeof(buf), "%s %s%s", cmd, prefix, escaped);

   WRN("trying to launch '%s'", buf);
   ecore_exe_run(buf, NULL);

end:
   eina_strbuf_free(sb);
   free(escaped);
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


#if defined(__has_feature)
#  if __has_feature(memory_sanitizer)
   __attribute__((no_sanitize("memory")))
// disable with msan due to false positives
#  endif
#endif
int tytest_base64(void)
{
   Eina_Unicode *src;
   char *res;
   const char *expected;


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
#endif
