#include "private.h"

#include <Elementary.h>
#include <Efreet.h>
#include "config.h"
#include "main.h"
#include "col.h"
#include "utils.h"

#define CONF_VER 14

#define LIM(v, min, max) {if (v >= max) v = max; else if (v <= min) v = min;}

static Eet_Data_Descriptor *edd_base = NULL;
static Eet_Data_Descriptor *edd_color = NULL;
static Eet_Data_Descriptor *edd_keys = NULL;

static const char *
_config_home_get(void)
{
   return efreet_config_home_get();
}

void
config_init(void)
{
   Eet_Data_Descriptor_Class eddc;
   Eet_Data_Descriptor_Class eddkc;
   char path[PATH_MAX] = {};

   elm_need_efreet();
   efreet_init();

   snprintf(path, sizeof(path) -1, "%s/terminology/themes",
            _config_home_get());
   ecore_file_mkpath(path);

   eet_eina_stream_data_descriptor_class_set
     (&eddc, sizeof(eddc), "Config", sizeof(Config));
   edd_base = eet_data_descriptor_stream_new(&eddc);

   eet_eina_stream_data_descriptor_class_set
     (&eddc, sizeof(eddc), "Config_Color", sizeof(Config_Color));
   edd_color = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_color, Config_Color, "r", r, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_color, Config_Color, "g", g, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_color, Config_Color, "b", b, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_color, Config_Color, "a", a, EET_T_UCHAR);


   eet_eina_stream_data_descriptor_class_set
     (&eddkc, sizeof(eddkc), "Config_Keys", sizeof(Config_Keys));
   edd_keys = eet_data_descriptor_stream_new(&eddkc);

   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_keys, Config_Keys, "keyname", keyname, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_keys, Config_Keys, "ctrl", ctrl, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_keys, Config_Keys, "alt", alt, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_keys, Config_Keys, "shift", shift, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_keys, Config_Keys, "win", win, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_keys, Config_Keys, "meta", meta, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_keys, Config_Keys, "hyper", hyper, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_keys, Config_Keys, "cb", cb, EET_T_STRING);

   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "version", version, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "font.name", font.name, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "font.size", font.size, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "font.bitmap", font.bitmap, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "font.bolditalic", font.bolditalic, EET_T_UCHAR);
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
     (edd_base, Config, "scrollback", scrollback, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "tab_zoom", tab_zoom, EET_T_DOUBLE);
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
     (edd_base, Config, "active_links", active_links, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "translucent", translucent, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "opacity", opacity, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "mute", mute, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "visualize", visualize, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "urg_bell", urg_bell, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "multi_instance", multi_instance, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "xterm_256color", xterm_256color, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "erase_is_del", erase_is_del, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "custom_geometry", custom_geometry, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "cg_width", cg_width, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "cg_height", cg_height, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "drag_links", drag_links, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "login_shell", login_shell, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "mouse_over_focus",
      mouse_over_focus, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "disable_focus_visuals",
      disable_focus_visuals, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "colors_use", colors_use, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_ARRAY
     (edd_base, Config, "colors", colors, edd_color);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "bell_rings", bell_rings, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_LIST
     (edd_base, Config, "keys", keys, edd_keys);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "gravatar", gravatar, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "notabs", notabs, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "mv_always_show", mv_always_show, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "ty_escapes", ty_escapes, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_base, Config, "changedir_to_current", changedir_to_current, EET_T_UCHAR);
}

void
config_shutdown(void)
{
   if (edd_base)
     {
        eet_data_descriptor_free(edd_base);
        edd_base = NULL;
     }
   if (edd_color)
     {
        eet_data_descriptor_free(edd_color);
        edd_color = NULL;
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

   if (config->temporary)
     {
        main_config_sync(config);
        return;
     }
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
   main_config_sync(config);
}

void
config_sync(const Config *config_src, Config *config)
{
   // SOME fields have to be consistent between configs
   config->font.size = config_src->font.size;
   eina_stringshare_replace(&(config->font.name), config_src->font.name);
   config->font.bitmap = config_src->font.bitmap;
   config->font.bolditalic = config_src->font.bolditalic;
   config->helper.inline_please = config_src->helper.inline_please;
   eina_stringshare_replace(&(config->helper.email), config_src->helper.email);
   eina_stringshare_replace(&(config->helper.url.general), config_src->helper.url.general);
   eina_stringshare_replace(&(config->helper.url.video), config_src->helper.url.video);
   eina_stringshare_replace(&(config->helper.url.image), config_src->helper.url.image);
   eina_stringshare_replace(&(config->helper.local.general), config_src->helper.local.general);
   eina_stringshare_replace(&(config->helper.local.video), config_src->helper.local.video);
   eina_stringshare_replace(&(config->helper.local.image), config_src->helper.local.image);
   eina_stringshare_replace(&(config->theme), config_src->theme);
   config->scrollback = config_src->scrollback;
   config->tab_zoom = config_src->tab_zoom;
   config->vidmod = config_src->vidmod;
   config->jump_on_keypress = config_src->jump_on_keypress;
   config->jump_on_change = config_src->jump_on_change;
   config->flicker_on_key = config_src->flicker_on_key;
   config->disable_cursor_blink = config_src->disable_cursor_blink;
   config->disable_visual_bell = config_src->disable_visual_bell;
   config->bell_rings = config_src->bell_rings;
   config->active_links = config_src->active_links;
   config->mute = config_src->mute;
   config->visualize = config_src->visualize;
   config->urg_bell = config_src->urg_bell;
   config->multi_instance = config_src->multi_instance;
   config->xterm_256color = config_src->xterm_256color;
   config->erase_is_del = config_src->erase_is_del;
   config->temporary = config_src->temporary;
   config->custom_geometry = config_src->custom_geometry;
   config->login_shell = config_src->login_shell;
   config->cg_width = config_src->cg_width;
   config->cg_height = config_src->cg_height;
   config->colors_use = config_src->colors_use;
   memcpy(config->colors, config_src->colors, sizeof(config->colors));
   config->mouse_over_focus = config_src->mouse_over_focus;
   config->disable_focus_visuals = config_src->disable_focus_visuals;
   /* TODO: config->keys */
   config->gravatar = config_src->gravatar;
   config->notabs = config_src->notabs;
   config->mv_always_show = config_src->mv_always_show;
   config->ty_escapes = config_src->ty_escapes;
   config->changedir_to_current = config_src->changedir_to_current;
}

static void
_config_upgrade_to_v2(Config *config)
{
   int i, j;

   /* Colors */
   config->colors_use = EINA_FALSE;
   for (j = 0; j < 4; j++)
     {
        for (i = 0; i < 12; i++)
          {
             unsigned char rr = 0, gg = 0, bb = 0, aa = 0;

             colors_standard_get(j, i, &rr, &gg, &bb, &aa);
             config->colors[(j * 12) + i].r = rr;
             config->colors[(j * 12) + i].g = gg;
             config->colors[(j * 12) + i].b = bb;
             config->colors[(j * 12) + i].a = aa;
          }
     }
   config->version = 2;
}

#define ADD_KB(Name, Ctrl, Alt, Shift, Win, Cb)                   \
   kb = calloc(1, sizeof(Config_Keys));                           \
   if (!kb) return;                                               \
   kb->keyname = eina_stringshare_add_length(Name, strlen(Name)); \
   kb->ctrl = Ctrl;                                               \
   kb->alt = Alt;                                                 \
   kb->shift = Shift;                                             \
   kb->win = Win;                                                 \
   kb->cb = eina_stringshare_add_length(Cb, strlen(Cb));          \
   config->keys = eina_list_append(config->keys, kb)

static void
_add_default_keys(Config *config)
{
   Config_Keys *kb;

   ADD_KB("F11", 0, 0, 0, 0, "win_fullscreen");

   /* Ctrl- */
   ADD_KB("Prior", 1, 0, 0, 0, "term_prev");
   ADD_KB("Next", 1, 0, 0, 0, "term_next");
   ADD_KB("0", 1, 0, 0, 0, "tab_10");
   ADD_KB("1", 1, 0, 0, 0, "tab_1");
   ADD_KB("2", 1, 0, 0, 0, "tab_2");
   ADD_KB("3", 1, 0, 0, 0, "tab_3");
   ADD_KB("4", 1, 0, 0, 0, "tab_4");
   ADD_KB("5", 1, 0, 0, 0, "tab_5");
   ADD_KB("6", 1, 0, 0, 0, "tab_6");
   ADD_KB("7", 1, 0, 0, 0, "tab_7");
   ADD_KB("8", 1, 0, 0, 0, "tab_8");
   ADD_KB("9", 1, 0, 0, 0, "tab_9");

   /* Alt- */
   ADD_KB("Home", 0, 1, 0, 0, "cmd_box");
   ADD_KB("w", 0, 1, 0, 0, "copy_primary");
   ADD_KB("Return", 0, 1, 0, 0, "paste_primary");

   /* Ctrl-Shift- */
   ADD_KB("Prior", 1, 0, 1, 0, "split_h");
   ADD_KB("Next", 1, 0, 1, 0, "split_v");
   ADD_KB("t", 1, 0, 1, 0, "tab_new");
   ADD_KB("End", 1, 0, 1, 0, "exited");
   ADD_KB("Home", 1, 0, 1, 0, "tab_select");
   ADD_KB("c", 1, 0, 1, 0, "copy_clipboard");
   ADD_KB("v", 1, 0, 1, 0, "paste_clipboard");
   ADD_KB("h", 1, 0, 1, 0, "miniview");
   ADD_KB("Insert", 1, 0, 1, 0, "paste_clipboard");
   ADD_KB("n", 1, 0, 1, 0, "term_new");

   /* Ctrl-Alt- */
   ADD_KB("equal", 1, 1, 0, 0, "increase_font_size");
   ADD_KB("minus", 1, 1, 0, 0, "decrease_font_size");
   ADD_KB("0", 1, 1, 0, 0, "reset_font_size");
   ADD_KB("9", 1, 1, 0, 0, "big_font_size");
   ADD_KB("t", 1, 1, 0, 0, "tab_title");

   /* Shift- */
   ADD_KB("Prior", 0, 0, 1, 0, "one_page_up");
   ADD_KB("Next", 0, 0, 1, 0, "one_page_down");
   ADD_KB("Up", 0, 0, 1, 0, "one_line_up");
   ADD_KB("Down", 0, 0, 1, 0, "one_line_down");
   ADD_KB("Insert", 0, 0, 1, 0, "paste_primary");
   ADD_KB("KP_Add", 0, 0, 1, 0, "increase_font_size");
   ADD_KB("KP_Subtract", 0, 0, 1, 0, "decrease_font_size");
   ADD_KB("KP_Multiply", 0, 0, 1, 0, "reset_font_size");
   ADD_KB("KP_Divide", 0, 0, 1, 0, "copy_clipboard");
   ADD_KB("Left", 0, 0, 1, 0, "term_prev");
   ADD_KB("Right", 0, 0, 1, 0, "term_next");
   ADD_KB("Home", 0, 0, 1, 0, "top_backlog");
   ADD_KB("End", 0, 0, 1, 0, "reset_scroll");
}

void
config_reset_keys(Config *config)
{
   Config_Keys *key;

   EINA_LIST_FREE(config->keys, key)
     {
        eina_stringshare_del(key->keyname);
        eina_stringshare_del(key->cb);
        free(key);
     }
   _add_default_keys(config);
}


static void
_add_key(Config *config, const char *name, int ctrl, int alt, int shift,
         int win, const char *cb_name)
{
   Config_Keys *kb;

   ADD_KB(name, ctrl, alt, shift, win, cb_name);
}

#undef ADD_KB

void
config_default_font_set(Config *config, Evas *evas)
{
   Eina_List *fontlist, *l;
   const char *fname;
   Eina_Bool dejavu_found = EINA_FALSE,
             bitstream_found = EINA_FALSE,
             droid_found = EINA_FALSE,
             liberation_found = EINA_FALSE;

   if (config->font_set) return;

#define FONT_DEJAVU     "DejaVu Sans Mono:style=Book"
#define FONT_LIBERATION "Liberation Mono:style=Regular"
#define FONT_DROID      "Droid Sans Mono:style=Regular"
#define FONT_BITSTREAM  "Bitstream Vera Sans Mono:style=Roman"

   fontlist = evas_font_available_list(evas);
   if (!fontlist) return;

   EINA_LIST_FOREACH(fontlist, l, fname)
     {
        if (strncmp(fname, FONT_DEJAVU, strlen(FONT_DEJAVU)) == 0)
          dejavu_found = EINA_TRUE;
        else if (strncmp(fname, FONT_LIBERATION, strlen(FONT_LIBERATION)) == 0)
          liberation_found = EINA_TRUE;
        else if (strncmp(fname, FONT_DROID, strlen(FONT_DROID)) == 0)
          droid_found = EINA_TRUE;
        else if (strncmp(fname, FONT_BITSTREAM, strlen(FONT_BITSTREAM)) == 0)
          bitstream_found = EINA_TRUE;
     }

   evas_font_available_list_free(evas, fontlist);

   fname = config->font.name;
   config->font_set = EINA_TRUE;
   if (dejavu_found)
     {
        config->font.name = eina_stringshare_add(FONT_DEJAVU);
     }
   else if (liberation_found)
     {
        config->font.name = eina_stringshare_add(FONT_LIBERATION);
     }
   else if (droid_found)
     {
        config->font.name = eina_stringshare_add(FONT_DROID);
     }
   else if (bitstream_found)
     {
        config->font.name = eina_stringshare_add(FONT_BITSTREAM);
     }
   else
     {
        config->font_set = EINA_FALSE;
     }

   if (config->font_set)
     {
        config->font.bitmap = EINA_FALSE;
        config->font.size = 12;
        config->font.bolditalic = EINA_TRUE;
        eina_stringshare_del(fname);
     }
#undef FONT_DEJAVU
#undef FONT_LIBERATION
#undef FONT_DROID
#undef FONT_BITSTREAM
}

Config *
config_new(void)
{
   Config *config;
   config = calloc(1, sizeof(Config));
   if (config)
     {
        int i, j;

        config->version = CONF_VER;
        config->font.bitmap = EINA_TRUE;
        config->font.name = eina_stringshare_add("nexus.pcf");
        config->font.size = 10;
        config->font.bolditalic = EINA_TRUE;
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
        config->tab_zoom = 0.5;
        config->vidmod = 0;
        config->opacity = 50;
        config->cg_width = 80;
        config->cg_height = 24;
        config->jump_on_change = EINA_FALSE;
        config->jump_on_keypress = EINA_TRUE;
        config->flicker_on_key = EINA_FALSE;
        config->disable_cursor_blink = EINA_FALSE;
        config->disable_visual_bell = EINA_FALSE;
        config->bell_rings = EINA_TRUE;
        config->active_links = EINA_TRUE;
        config->translucent = EINA_FALSE;
        config->mute = EINA_FALSE;
        config->visualize = EINA_TRUE;
        config->urg_bell = EINA_TRUE;
        config->multi_instance = EINA_FALSE;
        config->xterm_256color = EINA_FALSE;
        config->erase_is_del = EINA_FALSE;
        config->custom_geometry = EINA_FALSE;
        config->drag_links = EINA_FALSE;
        config->login_shell = EINA_FALSE;
        config->mouse_over_focus = EINA_TRUE;
        config->disable_focus_visuals = EINA_FALSE;
        config->colors_use = EINA_FALSE;
        config->gravatar = EINA_TRUE;
        config->notabs = EINA_FALSE;
        config->mv_always_show = EINA_FALSE;
        config->ty_escapes = EINA_TRUE;
        config->changedir_to_current = EINA_TRUE;
        for (j = 0; j < 4; j++)
          {
             for (i = 0; i < 12; i++)
               {
                  unsigned char rr = 0, gg = 0, bb = 0, aa = 0;

                  colors_standard_get(j, i, &rr, &gg, &bb, &aa);
                  config->colors[(j * 12) + i].r = rr;
                  config->colors[(j * 12) + i].g = gg;
                  config->colors[(j * 12) + i].b = bb;
                  config->colors[(j * 12) + i].a = aa;
               }
          }
        _add_default_keys(config);
     }
   return config;
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
             switch (config->version)
               {
                case 0:
                case 1:
                   _config_upgrade_to_v2(config);
                  /*pass through*/
                case 2:
                  LIM(config->font.size, 3, 400);
                  LIM(config->scrollback, 0, 131072);
                  LIM(config->tab_zoom, 0.0, 1.0);
                  LIM(config->vidmod, 0, 4)

                  /* upgrade to v3 */
                  config->active_links = EINA_TRUE;
                  config->bell_rings = EINA_TRUE;
                  /*pass through*/
                case 3:
                  if (eina_list_count(config->keys) == 0)
                    {
                       _add_default_keys(config);
                    }
                  config->gravatar = EINA_TRUE;
                  /*pass through*/
                case 4:
                  /*pass through*/
                case 5:
                  config->ty_escapes = EINA_TRUE;
                  /*pass through*/
                case 6:
                  config->changedir_to_current = EINA_TRUE;
                  /*pass through*/
                case 7:
                  _add_key(config, "n", 1, 0, 1, 0, "term_new");
                  /*pass through*/
                case 8:
                  _add_key(config, "t", 1, 1, 0, 0, "tab_title");
                  /*pass through*/
                case 9:
                  /* actually do nothing */
                  /*pass through*/
                case 10:
                  config->font.bolditalic = EINA_TRUE;
                  /*pass through*/
                case 11:
                  _add_key(config, "Left", 0, 0, 1, 0, "term_prev");
                  _add_key(config, "Right", 0, 0, 1, 0, "term_next");
                  /*pass through*/
                case 12:
                  _add_key(config, "Home", 0, 0, 1, 0, "top_backlog");
                  /*pass through*/
                case 13:
                  _add_key(config, "End", 0, 0, 1, 0, "reset_scroll");
                  /*pass through*/
                case CONF_VER: /* 13 */
                  config->version = CONF_VER;
                  break;
                default:
                  if (config->version < CONF_VER)
                    {
                       // currently no upgrade path so reset config.
                       config_del(config);
                       config = NULL;
                    }
                  /* do no thing in case the config is from a newer
                   * terminology, we don't want to remove it. */
               }
          }
     }
   if (!config)
     {
        config = config_new();
     }
   else
     {
        config->font_set = 1;
     }

   if (config)
     config->config_key = eina_stringshare_add(key); /* not in eet */

   return config;
}

Config *
config_fork(Config *config)
{
   Config_Keys *key;
   Eina_List *l;
   Config *config2;

   config2 = calloc(1, sizeof(Config));
   if (!config2) return NULL;
#define CPY(fld) config2->fld = config->fld;
#define SCPY(fld) if (config->fld) config2->fld = eina_stringshare_add(config->fld)
#define SLSTCPY(fld) \
   do { Eina_List *__l; const char *__s; \
     EINA_LIST_FOREACH(config->fld, __l, __s) \
       config2->fld = eina_list_append \
         (config2->fld, eina_stringshare_add(__s)); } while (0)

   CPY(version);
   SCPY(font.name);
   CPY(font.size);
   CPY(font.bitmap);
   SCPY(font.orig_name);
   CPY(font.orig_size);
   CPY(font.orig_bitmap);
   CPY(font.bolditalic);
   SCPY(helper.email);
   SCPY(helper.url.general);
   SCPY(helper.url.video);
   SCPY(helper.url.image);
   SCPY(helper.local.general);
   SCPY(helper.local.video);
   SCPY(helper.local.image);
   CPY(helper.inline_please);
   SCPY(theme);
   SCPY(background);
   CPY(scrollback);
   CPY(tab_zoom);
   CPY(vidmod);
   CPY(jump_on_change);
   CPY(jump_on_keypress);
   CPY(flicker_on_key);
   CPY(disable_cursor_blink);
   CPY(disable_visual_bell);
   CPY(bell_rings);
   CPY(active_links);
   CPY(translucent);
   CPY(opacity);
   CPY(mute);
   CPY(visualize);
   CPY(urg_bell);
   CPY(multi_instance);
   CPY(xterm_256color);
   CPY(erase_is_del);
   CPY(custom_geometry);
   CPY(login_shell);
   CPY(cg_width);
   CPY(cg_height);
   CPY(colors_use);
   memcpy(config2->colors, config->colors, sizeof(config->colors));
   CPY(mouse_over_focus);
   CPY(disable_focus_visuals);
   CPY(temporary);
   SCPY(config_key);
   CPY(font_set);
   CPY(gravatar);
   CPY(notabs);
   CPY(mv_always_show);
   CPY(ty_escapes);
   CPY(changedir_to_current);

   EINA_LIST_FOREACH(config->keys, l, key)
     {
        Config_Keys *key2 = calloc(1, sizeof(Config_Keys));
        if (!key2) break;
        key2->keyname = key->keyname;
        eina_stringshare_ref(key->keyname);
        key2->ctrl = key->ctrl;
        key2->alt = key->alt;
        key2->shift = key->shift;
        key2->win = key->win;
        key2->cb = key->cb;
        eina_stringshare_ref(key->cb);
        config2->keys = eina_list_append(config2->keys, key2);
     }

   return config2;
}

void
config_del(Config *config)
{
   Config_Keys *key;

   if (!config) return;

   eina_stringshare_del(config->font.name);
   eina_stringshare_del(config->font.orig_name);
   eina_stringshare_del(config->theme);
   eina_stringshare_del(config->background);
   eina_stringshare_del(config->helper.email);
   eina_stringshare_del(config->helper.url.general);
   eina_stringshare_del(config->helper.url.video);
   eina_stringshare_del(config->helper.url.image);
   eina_stringshare_del(config->helper.local.general);
   eina_stringshare_del(config->helper.local.video);
   eina_stringshare_del(config->helper.local.image);

   eina_stringshare_del(config->config_key); /* not in eet */

   EINA_LIST_FREE(config->keys, key)
     {
        eina_stringshare_del(key->keyname);
        eina_stringshare_del(key->cb);
        free(key);
     }
   free(config);
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
