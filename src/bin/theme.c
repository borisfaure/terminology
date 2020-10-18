#include "private.h"
#include "theme.h"
#include "config.h"
#include "colors.h"
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

   /* use the file from home if available */
   struct stat s2;

   snprintf(path2, sizeof(path2) - 1, "%s/terminology/themes/%s",
            efreet_config_home_get(), name);

   if (stat(path2, &s2) == 0)
     return path2;
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
theme_path_default_get(void)
{
   static char path[PATH_MAX] = "";

   if (path[0]) return path;

   snprintf(path, sizeof(path), "%s/themes/default.edj",
            elm_app_data_dir_get());
   return path;
}

Eina_Bool
theme_apply(Evas_Object *obj,
            const Config *config,
            const char *group,
            const char *theme_file,
            const Color_Scheme *cs,
            Eina_Bool is_elm_layout)
{
   const char *theme_path = theme_file;
   Evas_Object *edje = obj;
   const char *errmsg;

   if (!theme_file)
     theme_path = config_theme_path_get(config);

   if (is_elm_layout)
     {
        edje = elm_layout_edje_get(obj);

        if (elm_layout_file_set(obj, config_theme_path_get(config), group))
          goto done;
        if (elm_layout_file_set(obj, theme_path_default_get(), group))
          goto done;
     }
   else
     {
        if (edje_object_file_set(edje, config_theme_path_get(config), group))
          goto done;
        if (edje_object_file_set(edje, theme_path_default_get(), group))
          goto done;
     }

   errmsg = edje_load_error_str(edje_object_load_error_get(edje));
   ERR(_("Could not load any theme for group=%s: %s"), group, errmsg);
   return EINA_FALSE;

done:
   if (cs)
     color_scheme_apply(edje, cs);
   else
     color_scheme_apply_from_config(edje, config);
   return EINA_TRUE;
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
