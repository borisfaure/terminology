#ifndef _CONFIG_H__
#define _CONFIG_H__ 1

typedef struct _Config Config;

/* TODO: separate config per terminal (tab, window) and global. */

struct _Config
{
   struct {
      const char    *name;
      int            size;
      unsigned char  bitmap;
   } font;
   const char       *theme;
   const char       *background;
   const char       *wordsep;
   int               scrollback;
   int               vidmod;
   Eina_Bool         jump_on_change;
   Eina_Bool         flicker_on_key;
   Eina_Bool         disable_cursor_blink;
   Eina_Bool         translucent;
   Eina_Bool         mute;
   Eina_Bool         urg_bell;
   
   Eina_Bool         temporary; /* not in EET */
   const char       *config_key; /* not in EET, the key that config was loaded */
};

void config_init(void);
void config_shutdown(void);
void config_save(const Config *config, const char *key);
Config *config_load(const char *key);
void config_del(Config *config);

const char *config_theme_path_get(const Config *config);
const char *config_theme_path_default_get(const Config *config);

#endif
