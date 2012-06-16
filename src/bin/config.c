#include <Elementary.h>
#include "config.h"

#define LIM(v, min, max) {if (v >= max) v = max; else if (v <= min) v = min;}

static Eet_Data_Descriptor *edd_base = NULL;
Config *config = NULL;

static const char *
_homedir(void)
{
   const char *home;
   
   home = getenv("HOME");
   if (!home) home = getenv("TMP");
   if (!home) home = "/tmp";
   return home;
}

void
config_init(void)
{
   Eet_Data_Descriptor_Class eddc;
   Eet_File *ef;
   char buf[4096], *home;
   
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
     (edd_base, Config, "scrollback", scrollback, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "theme", theme, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "background", background, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "jump_on_change", jump_on_change, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "translucent", translucent, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "wordsep", wordsep, EET_T_STRING);

   home = (char *)_homedir();
   snprintf(buf, sizeof(buf), "%s/.terminology/config/standard/base.cfg", home);
   ef = eet_open(buf, EET_FILE_MODE_READ);
   if (ef)
     {
        config = eet_data_read(ef, edd_base, "config");
        eet_close(ef);
        if (config)
          {
             LIM(config->font.size, 3, 400);
          }
     }
   if (!config)
     {
        config = calloc(1, sizeof(Config));
//        config->font.bitmap = 0;
//        config->font.name = eina_stringshare_add("Monospace");
        config->font.bitmap = 1;
        config->font.name = eina_stringshare_add("nex6x10.pcf");
        config->font.size = 10;
        config->scrollback = 4096;
        config->theme = eina_stringshare_add("default.edj");
        config->background = NULL;
        config->translucent = 0;
        config->jump_on_change = 1;
        config->wordsep = eina_stringshare_add("'\"()[]{}=*!#$^\\:;,?` ");
     }
}

void
config_shutdown(void)
{
   if (config)
     {
        if (config->font.name) eina_stringshare_del(config->font.name);
        if (config->theme) eina_stringshare_del(config->theme);
        if (config->background) eina_stringshare_del(config->background);
        if (config->wordsep) eina_stringshare_del(config->wordsep);
        free(config);
        config = NULL;
     }
   if (edd_base)
     {
        eet_data_descriptor_free(edd_base);
        edd_base = NULL;
     }
}

void
config_save(void)
{
   Eet_File *ef;
   char buf[4096], buf2[4096], *home;
   int ok;
   
   home = (char *)_homedir();
   snprintf(buf, sizeof(buf), "%s/.terminology/config/standard", home);
   ecore_file_mkpath(buf);
   snprintf(buf, sizeof(buf), "%s/.terminology/config/standard/base.cfg.tmp", home);
   snprintf(buf2, sizeof(buf2), "%s/.terminology/config/standard/base.cfg", home);
   ef = eet_open(buf, EET_FILE_MODE_WRITE);
   if (ef)
     {
        ok = eet_data_write(ef, edd_base, "config", config, 1);
        eet_close(ef);
        if (ok) ecore_file_mv(buf, buf2);
     }
}
