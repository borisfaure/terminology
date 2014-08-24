#ifndef _CONFIG_H__
#define _CONFIG_H__ 1

typedef struct _Config Config;
typedef struct _Config_Color Config_Color;
typedef struct _Config_Keys Config_Keys;

struct _Config_Keys
{
   const char *keyname;
   Eina_Bool ctrl;
   Eina_Bool alt;
   Eina_Bool shift;
   const char *cb;
};
/* TODO: separate config per terminal (tab, window) and global. */

struct _Config_Color
{
   unsigned char r, g, b, a;
};

struct _Config
{
   int               version;
   int               scrollback;
   struct {
      const char    *name;
      const char    *orig_name; /* not in EET */
      int            size;
      int            orig_size; /* not in EET */
      unsigned char  bitmap;
      unsigned char  orig_bitmap; /* not in EET */
   } font;
   struct {
      const char    *email;
      struct {
         const char    *general;
         const char    *video;
         const char    *image;
      } url, local;
      Eina_Bool      inline_please;
   } helper;
   const char       *theme;
   const char       *background;
   double            tab_zoom;
   int               vidmod;
   Eina_Bool         jump_on_keypress;
   Eina_Bool         jump_on_change;
   Eina_Bool         flicker_on_key;
   Eina_Bool         disable_cursor_blink;
   Eina_Bool         disable_visual_bell;
   Eina_Bool         bell_rings;
   Eina_Bool         active_links;
   Eina_Bool         translucent;
   int               opacity;
   Eina_Bool         mute;
   Eina_Bool         visualize;
   Eina_Bool         urg_bell;
   Eina_Bool         multi_instance;
   Eina_Bool         application_server;
   Eina_Bool         application_server_restore_views;
   Eina_Bool         xterm_256color;
   Eina_Bool         erase_is_del;
   Eina_Bool         custom_geometry;
   Eina_Bool         drag_links;
   Eina_Bool         login_shell;
   Eina_Bool         mouse_over_focus;
   int               cg_width;
   int               cg_height;
   Eina_Bool         colors_use;
   Config_Color      colors[(4 * 12)];
   Eina_Bool         miniview;
   Eina_List        *keys;

   Eina_Bool         temporary; /* not in EET */
   const char       *config_key; /* not in EET, the key that config was loaded */
};

void config_init(void);
void config_shutdown(void);
void config_sync(const Config *config_src, Config *config);
void config_save(Config *config, const char *key);
Config *config_load(const char *key);
Config *config_fork(Config *config);
void config_del(Config *config);

const char *config_theme_path_get(const Config *config);
const char *config_theme_path_default_get(const Config *config);

#endif
