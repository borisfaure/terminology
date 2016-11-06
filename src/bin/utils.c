#include "private.h"
#include "utils.h"
#include <unistd.h>
#include <pwd.h>

#include <Edje.h>
#include <Efreet.h>
#include <Elementary.h>

const char *
theme_path_get(const char *name)
{
   static char path1[PATH_MAX] = "";
   static char path2[PATH_MAX] = "";

   /* use the newer file */
   struct stat s2;

   snprintf(path2, sizeof(path2) - 1, "%s/terminology/themes/%s",
            efreet_config_home_get(), name);

   if (stat(path2, &s2) == 0) return path2;
   snprintf(path1, sizeof(path1) - 1, "%s/themes/%s",
            elm_app_data_dir_get(), name);
   return path1;
}

Eina_Bool
theme_apply(Evas_Object *edje, const Config *config, const char *group)
{
   const char *errmsg;

   EINA_SAFETY_ON_NULL_RETURN_VAL(edje, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(config, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(group, EINA_FALSE);

   if (edje_object_file_set(edje, config_theme_path_get(config), group))
     return EINA_TRUE;

   errmsg = edje_load_error_str(edje_object_load_error_get(edje));
   INF("Cannot find theme: file=%s group=%s error='%s', trying default...",
       config_theme_path_get(config), group, errmsg);

   if (edje_object_file_set(edje, config_theme_path_default_get(config), group))
     return EINA_TRUE;

   errmsg = edje_load_error_str(edje_object_load_error_get(edje));
   ERR(_("Could not load any theme for group=%s: %s"), group, errmsg);
   return EINA_FALSE;
}

Eina_Bool
theme_apply_default(Evas_Object *edje, const Config *config, const char *group)
{
   const char *errmsg;

   EINA_SAFETY_ON_NULL_RETURN_VAL(edje, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(config, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(group, EINA_FALSE);

   if (edje_object_file_set(edje, config_theme_path_default_get(config), group))
     return EINA_TRUE;

   errmsg = edje_load_error_str(edje_object_load_error_get(edje));
   ERR(_("Could not load default theme for group=%s: %s"), group, errmsg);
   return EINA_FALSE;
}

void
theme_reload(Evas_Object *edje)
{
   const char *file;
   const char *group;

   edje_object_file_get(edje, &file, &group);
   INF("file=%s, group=%s", file, group);
   if (!edje_object_file_set(edje, file, group)) return;
}

static void
theme_reload_cb(void *_data EINA_UNUSED,
                Evas_Object *obj,
                const char *_emission EINA_UNUSED,
                const char *_source EINA_UNUSED)
{
   void (*func) (void *d);
   void *func_data;
   
   theme_reload(obj);
   func = evas_object_data_get(obj, "theme_reload_func");
   func_data = evas_object_data_get(obj, "theme_reload_func_data");
   printf("%p %p\n", func, func_data);
   if (func) func(func_data);
}

void
theme_auto_reload_enable(Evas_Object *edje)
{
   edje_object_signal_callback_add
     (edje, "edje,change,file", "edje", theme_reload_cb, NULL);
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

Eina_Bool
link_is_protocol(const char *str)
{
   const char *p = str;
   int c = *p;

   if (!isalpha(c))
     return EINA_FALSE;

   /* Try to follow RFC3986 a bit
    * URI         = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
    * hier-part   = "//" authority path-abempty
    *             [...] other stuff not taken into account
    * scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    */

   do
     {
        p++;
        c = *p;
     }
   while (isalpha(c) || (c == '.') || (c == '-') || (c == '+'));

   return (p[0] == ':') && (p[1] == '/') && (p[2] == '/');
}

Eina_Bool
link_is_url(const char *str)
{
   if (link_is_protocol(str) ||
       casestartswith(str, "www.") ||
       casestartswith(str, "ftp."))
     return EINA_TRUE;
   return EINA_FALSE;
}

Eina_Bool
link_is_email(const char *str)
{
   const char *at = strchr(str, '@');
   if (at && strchr(at + 1, '.'))
     return EINA_TRUE;
   if (casestartswith(str, "mailto:"))
     return EINA_TRUE;
   return EINA_FALSE;
}
