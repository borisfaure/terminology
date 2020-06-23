#include "private.h"
#include "theme.h"
#include "config.h"
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

const char *
config_theme_path_get(const Config *config)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(config, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(config->theme, NULL);

   if (strchr(config->theme, '/'))
     return config->theme;

   return theme_path_get(config->theme);
}

const char *
config_theme_path_default_get(const Config *config)
{
   static char path[PATH_MAX] = "";

   EINA_SAFETY_ON_NULL_RETURN_VAL(config, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(config->theme, NULL);

   if (path[0]) return path;

   snprintf(path, sizeof(path), "%s/themes/default.edj",
            elm_app_data_dir_get());
   return path;
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
theme_apply_elm(Evas_Object *layout, const Config *config, const char *group)
{
   const char *errmsg;
   Evas_Object *edje;

   EINA_SAFETY_ON_NULL_RETURN_VAL(layout, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(config, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(group, EINA_FALSE);

   if (elm_layout_file_set(layout, config_theme_path_get(config), group))
     return EINA_TRUE;

   edje = elm_layout_edje_get(layout);
   errmsg = edje_load_error_str(edje_object_load_error_get(edje));
   INF("Cannot find theme: file=%s group=%s error='%s', trying default...",
       config_theme_path_get(config), group, errmsg);

   if (elm_layout_file_set(layout, config_theme_path_default_get(config), group))
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
utils_need_scale_wizard(void)
{
   static char path[PATH_MAX] = "";
   struct stat st;
   int res;
   char *config_xdg = getenv("ELM_CONFIG_DIR_XDG");


   snprintf(path, sizeof(path) -1, "%s/terminology/config/",
            efreet_config_home_get());
   res = stat(path, &st);
   if (res == 0)
     return EINA_FALSE;

   if (config_xdg)
     {
        snprintf(path, sizeof(path) - 1,
                 "%s/elementary/config/standard/base.cfg", config_xdg);
     }
   else
     {
        const char *suffix = "/.elementary/config/standard/base.cfg";
        char home[PATH_MAX - strlen(suffix)];

        if (!homedir_get(home, sizeof(home)))
            return EINA_TRUE;
        snprintf(path, sizeof(path) - 1,
                 "%s%s", home, suffix);
     }
   res = stat(path, &st);
   if (res == 0)
     return EINA_FALSE;

   return EINA_TRUE;
}
