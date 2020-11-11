#include "private.h"
#include "utils.h"
#include "sb.h"
#include <Ecore.h>
#include <Ecore_File.h>
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
