#include "private.h"

#include <Elementary.h>
#include "config.h"

#define LIM(v, min, max) {if (v >= max) v = max; else if (v <= min) v = min;}

static Eet_Data_Descriptor *edd_base = NULL;

static const char *
_config_home_get(void)
{
#ifdef ELM_EFREET
   return efreet_config_home_get();
#else
   static char path[PATH_MAX] = "";
   const char *v = getenv("XDG_CONFIG_HOME");
   if (v) eina_strlcpy(path, v, sizeof(path));
   else
     {
        v = getenv("HOME");
        if (v) snprintf(path, sizeof(path), "%s/.config", v);
        else
          {
             if (!v) v = getenv("TMP");
             if (!v) v = "/tmp";
             eina_strlcpy(path, v, sizeof(path));
          }
     }
   return path;
#endif
}

void
config_init(void)
{
   Eet_Data_Descriptor_Class eddc;

   elm_need_efreet();
   
   eet_eina_stream_data_descriptor_class_set
     (&eddc, sizeof(eddc), "Config", sizeof(Config));
   edd_base = eet_data_descriptor_stream_new(&eddc);
   
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "font.name", font.name, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "font.size", font.size, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "font.bitmap", font.bitmap, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "theme", theme, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "background", background, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "wordsep", wordsep, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "scrollback", scrollback, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "vidmod", vidmod, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "jump_on_change", jump_on_change, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "flicker_on_key", flicker_on_key, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "translucent", translucent, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "mute", mute, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "urg_bell", urg_bell, EET_T_UCHAR);
}

void
config_shutdown(void)
{
   if (edd_base)
     {
        eet_data_descriptor_free(edd_base);
        edd_base = NULL;
     }
}

void
config_save(const Config *config, const char *key)
{
   Eet_File *ef;
   char buf[PATH_MAX], buf2[PATH_MAX];
   const char *cfgdir;
   int ok;

   EINA_SAFETY_ON_NULL_RETURN(config);

   if (config->temporary) return;
   if (!key) key = config->config_key;

   cfgdir = _config_home_get();
   snprintf(buf, sizeof(buf), "%s/terminology/config/standard", cfgdir);
   ecore_file_mkpath(buf);
   snprintf(buf, sizeof(buf), "%s/terminology/config/standard/base.cfg.tmp", cfgdir);
   snprintf(buf2, sizeof(buf2), "%s/terminology/config/standard/base.cfg", cfgdir);
   ef = eet_open(buf, EET_FILE_MODE_WRITE);
   if (ef)
     {
        ok = eet_data_write(ef, edd_base, key, config, 1);
        eet_close(ef);
        if (ok) ecore_file_mv(buf, buf2);
     }
}

Config *
config_load(const char *key)
{
   Eet_File *ef;
   char buf[PATH_MAX];
   const char *cfgdir;
   Config *config = NULL;

   EINA_SAFETY_ON_NULL_RETURN_VAL(key, NULL);

   cfgdir = _config_home_get();
   snprintf(buf, sizeof(buf), "%s/terminology/config/standard/base.cfg", cfgdir);
   ef = eet_open(buf, EET_FILE_MODE_READ);
   if (ef)
     {
        config = eet_data_read(ef, edd_base, key);
        eet_close(ef);
        if (config)
          {
             LIM(config->font.size, 3, 400);
          }
     }
   if (!config)
     {
        config = calloc(1, sizeof(Config));
        config->font.bitmap = EINA_TRUE;
        config->font.name = eina_stringshare_add("nexus.pcf");
        config->font.size = 10;
        config->scrollback = 2000;
        config->theme = eina_stringshare_add("default.edj");
        config->background = NULL;
        config->translucent = EINA_FALSE;
        config->jump_on_change = EINA_FALSE;
        config->flicker_on_key = EINA_TRUE;
        config->wordsep = eina_stringshare_add(" '\"()[]{}=*!#$^\\:;,?`");
        config->vidmod = 0;
        config->mute = EINA_FALSE;
        config->urg_bell = EINA_TRUE;
     }

   config->config_key = eina_stringshare_add(key); /* not in eet */

   return config;
}

void
config_del(Config *config)
{
   if (!config) return;

   eina_stringshare_del(config->font.name);
   eina_stringshare_del(config->theme);
   eina_stringshare_del(config->background);
   eina_stringshare_del(config->wordsep);

   eina_stringshare_del(config->config_key); /* not in eet */
   free(config);
}

const char *
config_theme_path_get(const Config *config)
{
   static char path[PATH_MAX];

   EINA_SAFETY_ON_NULL_RETURN_VAL(config, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(config->theme, NULL);

   if (strchr(config->theme, '/'))
     return config->theme;

   snprintf(path, sizeof(path), "%s/themes/%s",
            elm_app_data_dir_get(), config->theme);
   return path;
}

const char *
config_theme_path_default_get(const Config *config __UNUSED__)
{
   static char path[PATH_MAX] = "";

   EINA_SAFETY_ON_NULL_RETURN_VAL(config, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(config->theme, NULL);

   if (path[0]) return path;

   snprintf(path, sizeof(path), "%s/themes/default.edj",
            elm_app_data_dir_get());
   return path;
}
