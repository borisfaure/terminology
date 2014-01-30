#include "private.h"
#include "utils.h"
#include <unistd.h>
#include <pwd.h>

#include <Edje.h>

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
   ERR("Could not load any theme for group=%s: %s", group, errmsg);
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
theme_reload_cb(void *data EINA_UNUSED, Evas_Object *obj, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
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
   if (casestartswith(str, "http://") ||
       casestartswith(str, "https://") ||
       casestartswith(str, "ftp://") ||
       casestartswith(str, "file://") ||
       casestartswith(str, "mailto:"))
     return EINA_TRUE;
   return EINA_FALSE;
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
