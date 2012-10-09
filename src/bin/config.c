#include "private.h"

#include <Elementary.h>
#include <Efreet.h>
#include "config.h"

#define CONF_VER 1

#define LIM(v, min, max) {if (v >= max) v = max; else if (v <= min) v = min;}

static Eet_Data_Descriptor *edd_base = NULL;

static const char *
_config_home_get(void)
{
   return efreet_config_home_get();
}

void
config_init(void)
{
   Eet_Data_Descriptor_Class eddc;

   elm_need_efreet();
   efreet_init();
   
   eet_eina_stream_data_descriptor_class_set
     (&eddc, sizeof(eddc), "Config", sizeof(Config));
   edd_base = eet_data_descriptor_stream_new(&eddc);
   
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "version", version, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "font.name", font.name, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "font.size", font.size, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "font.bitmap", font.bitmap, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "helper.email", helper.email, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "helper.url.general", helper.url.general, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "helper.url.video", helper.url.video, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "helper.url.image", helper.url.image, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "helper.local.general", helper.local.general, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "helper.local.video", helper.local.video, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "helper.local.image", helper.local.image, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "helper.inline_please", helper.inline_please, EET_T_UCHAR);
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
     (edd_base, Config, "jump_on_keypress", jump_on_keypress, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "flicker_on_key", flicker_on_key, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "disable_cursor_blink", disable_cursor_blink, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "disable_visual_bell", disable_visual_bell, EET_T_UCHAR);
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

   efreet_shutdown();
}

void
config_save(Config *config, const char *key)
{
   Eet_File *ef;
   char buf[PATH_MAX], buf2[PATH_MAX];
   const char *cfgdir;
   int ok;

   EINA_SAFETY_ON_NULL_RETURN(config);

   if (config->temporary) return;
   if (!key) key = config->config_key;
   config->font.orig_size = config->font.size;
   eina_stringshare_del(config->font.orig_name);
   config->font.orig_name = NULL;
   if (config->font.name) config->font.orig_name = eina_stringshare_add(config->font.name);
   config->font.orig_bitmap = config->font.bitmap;
   
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
             config->font.orig_size = config->font.size;
             if (config->font.name) config->font.orig_name = eina_stringshare_add(config->font.name);
             config->font.orig_bitmap = config->font.bitmap;
             if (config->version < CONF_VER)
               {
                  // currently no upgrade path so reset config.
                  config_del(config);
                  config = NULL;
               }
             else
               {
                  LIM(config->font.size, 3, 400);
                  LIM(config->scrollback, 0, 200000);
                  LIM(config->vidmod, 0, 3)
               }
          }
     }
   if (!config)
     {
        // http://en.wikipedia.org/wiki/Asterisk
        // http://en.wikipedia.org/wiki/Comma
        // http://en.wikipedia.org/wiki/Interpunct
        // http://en.wikipedia.org/wiki/Bracket
        const Eina_Unicode sep[] =
          {
             // invisible spaces
             ' ',
             0x00a0,
             0x1680,
             0x180e,
             0x2000,
             0x2001,
             0x2002,
             0x2003,
             0x2004,
             0x2005,
             0x2006,
             0x2007,
             0x2008,
             0x2009,
             0x200a,
             0x200b,
             0x202f,
             0x205f,
             0x3000,
             0xfeff,
             // visible spaces
             0x2420,
             0x2422,
             0x2423,
             0x00b7,
             0x2022,
             0x2027,
             0x30fb,
             0xff65,
             0x0387,
             // other chars
             0x00ab,
             0x00bb,
             0x2039,
             0x203a,
             0x300c,
             0x300d,
             0x300c,
             0x300d,
             0x300e,
             0x300f,
             0xfe41,
             0xfe42,
             0xfe43,
             0xfe44,
             0xfe62,
             0xfe63,
             '\'',
             0x2018,
             0x2019,
             0x201a,
             0x201b,
             0xff07,
             '"',
             0x201c,
             0x201d,
             0x201e,
             0x201f,
             0x301d,
             0x301e,
             0x301f,
             0xff02,
             '(',
             ')',
             '[',
             ']',
             '{',
             '}',
             0x2308,
             0x2309,
             0xff62,
             0xff63,
             0x3008,
             0x3009,
             0x300a,
             0x300b,
             0x3010,
             0x3011,
             0xff08,
             0xff09,
             0xff3b,
             0xff3d,
             0xff1c,
             0xff1e,
             0xff5b,
             0xff5d,
             '=',
             '*',
             0xfe61,
             0xff0a,
             0x204e,
             0x2217,
             0x2731,
             0x2732,
             0x2733,
             0x273a,
             0x273b,
             0x273c,
             0x273d,
             0x2722,
             0x2723,
             0x2724,
             0x2725,
             0x2743,
             0x2749,
             0x274a,
             0x274b,
             0x066d,
             0x203b,
             0xe002a,
             '!',
             '#',
             '$',
             '^',
             '\\',
             ':',
             0x02d0,
             ';',
             ',',
             0xff1b,
             0x02bb,
             0x02bd,
             0x060c,
             0x1802,
             0x3001,
             0xfe10,
             0xfe50,
             0xfe51,
             0xff0c,
             0xff64,
             0x1363,
             0x0312,
             0x0313,
             0x0314,
             0x0315,
             0x0326,
             0x14fe,
             0x1808,
             0x07fb,
             0xa60d,
             0x055d,
             0xa6f5,
             '?',
             0x055e,
             0xff1f,
             0x0294,
             0x2e2e,
             0x225f,
             0x2a7b,
             0x2a7c,
             0x2047,
             0xfe56,
             0x2048,
             0x2049,
             0x203d,
             0x061f,
             0x2e2e,
             0x1367,
             0xa60f,
             0x2cfa,
             
             '`',
             0
          };
        char *s;
        int slen = 0;
        
        config = calloc(1, sizeof(Config));
        if (config)
          {
             config->version = CONF_VER;
             config->font.bitmap = EINA_TRUE;
             config->font.name = eina_stringshare_add("nexus.pcf");
             config->font.size = 10;
             config->helper.email = eina_stringshare_add("xdg-email");;
             config->helper.url.general = eina_stringshare_add("xdg-open");
             config->helper.url.video = eina_stringshare_add("xdg-open");
             config->helper.url.image = eina_stringshare_add("xdg-open");
             config->helper.local.general = eina_stringshare_add("xdg-open");
             config->helper.local.video = eina_stringshare_add("xdg-open");
             config->helper.local.image = eina_stringshare_add("xdg-open");
             config->helper.inline_please = EINA_TRUE;
             config->scrollback = 2000;
             config->theme = eina_stringshare_add("default.edj");
             config->background = NULL;
             config->translucent = EINA_FALSE;
             config->jump_on_change = EINA_TRUE;
	     config->jump_on_keypress = EINA_TRUE;
             config->flicker_on_key = EINA_TRUE;
             config->disable_cursor_blink = EINA_FALSE;
             config->disable_visual_bell = EINA_FALSE;
             s = eina_unicode_unicode_to_utf8(sep, &slen);
             if (s)
               {
                  config->wordsep = eina_stringshare_add(s);
                  free(s);
               }
             config->vidmod = 0;
             config->mute = EINA_FALSE;
             config->urg_bell = EINA_TRUE;
          }
     }

   config->config_key = eina_stringshare_add(key); /* not in eet */

   return config;
}

void
config_del(Config *config)
{
   if (!config) return;

   eina_stringshare_del(config->font.name);
   eina_stringshare_del(config->font.orig_name);
   eina_stringshare_del(config->theme);
   eina_stringshare_del(config->background);
   eina_stringshare_del(config->wordsep);
   eina_stringshare_del(config->helper.email);
   eina_stringshare_del(config->helper.url.general);
   eina_stringshare_del(config->helper.url.video);
   eina_stringshare_del(config->helper.url.image);
   eina_stringshare_del(config->helper.local.general);
   eina_stringshare_del(config->helper.local.video);
   eina_stringshare_del(config->helper.local.image);

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
