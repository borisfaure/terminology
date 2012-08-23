#include "private.h"

#include <Ecore_Getopt.h>
#include <Elementary.h>
#include "main.h"
#include "win.h"
#include "termio.h"
#include "termcmd.h"
#include "config.h"
#include "controls.h"
#include "media.h"
#include "utils.h"

int _log_domain = -1;

static Evas_Object *win = NULL, *bg = NULL, *backbg = NULL, *term = NULL, *media = NULL;
static Evas_Object *cmdbox = NULL;
static Evas_Object *popmedia = NULL;
static Evas_Object *conform = NULL;
static Ecore_Timer *flush_timer = NULL;
static Eina_Bool focused = EINA_FALSE;
static Eina_Bool hold = EINA_FALSE;
static Eina_Bool cmdbox_up = EINA_FALSE;
static Ecore_Timer *_cmdbox_focus_timer = NULL;

static void
_cb_del(void *data __UNUSED__, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   win = NULL;
}

static void
_cb_focus_in(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   if (!focused) elm_win_urgent_set(win, EINA_FALSE);
   focused = EINA_TRUE;
   edje_object_signal_emit(bg, "focus,in", "terminology");
   if (cmdbox_up)
     elm_object_focus_set(cmdbox, EINA_TRUE);
   else
     elm_object_focus_set(data, EINA_TRUE);
}

static void
_cb_focus_out(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   focused = EINA_FALSE;
   edje_object_signal_emit(bg, "focus,out", "terminology");
   if (cmdbox_up)
     elm_object_focus_set(cmdbox, EINA_FALSE);
   else
     elm_object_focus_set(data, EINA_FALSE);
   elm_cache_all_flush();
}

static void
_cb_size_hint(void *data __UNUSED__, Evas *e __UNUSED__, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Coord mw, mh, rw, rh, w = 0, h = 0;

   evas_object_size_hint_min_get(obj, &mw, &mh);
   evas_object_size_hint_request_get(obj, &rw, &rh);

   edje_object_size_min_calc(bg, &w, &h);
   evas_object_size_hint_min_set(bg, w, h);
   elm_win_size_base_set(win, w - mw, h - mh);
   elm_win_size_step_set(win, mw, mh);
   if (!evas_object_data_get(obj, "sizedone"))
     {
        evas_object_resize(win, w - mw + rw, h - mh + rh);
        evas_object_data_set(obj, "sizedone", obj);
     }
}

static void
_cb_options(void *data __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   controls_toggle(win, bg, term);
}

static Eina_Bool
_cb_flush(void *data __UNUSED__)
{
   flush_timer = NULL;
   elm_cache_all_flush();
   return EINA_FALSE;
}

static void
_cb_change(void *data __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   if (!flush_timer) flush_timer = ecore_timer_add(0.25, _cb_flush, NULL);
   else ecore_timer_delay(flush_timer, 0.25);
}

static void
_cb_exited(void *data __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   if (!hold) elm_exit();
}

static void
_cb_bell(void *data __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Config *config = termio_config_get(term);

   if (!config->disable_visual_bell)
     edje_object_signal_emit(bg, "bell", "terminology");
   if (!config) return;
   if (config->urg_bell)
     {
        if (!focused) elm_win_urgent_set(win, EINA_TRUE);
     }
}

static void
_cb_popmedia_done(void *data __UNUSED__, Evas_Object *obj __UNUSED__, const char *sig __UNUSED__, const char *src __UNUSED__)
{
   if (popmedia)
     {
        evas_object_del(popmedia);
        popmedia = NULL;
        termio_mouseover_suspend_pushpop(term, -1);
     }
}

static void
_cb_popmedia_del(void *data __UNUSED__, Evas *e __UNUSED__, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   edje_object_signal_emit(bg, "popmedia,off", "terminology");
}

static void
_cb_popup(void *data __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Evas_Object *o;
   Config *config = termio_config_get(term);
   const char *src;
   int type = 0;

   if (!config) return;
   src = termio_link_get(term);
   if (!src) return;
   if (popmedia) evas_object_del(popmedia);
   if (!popmedia) termio_mouseover_suspend_pushpop(term, 1);
   popmedia = o = media_add(win, src, config, MEDIA_POP, &type);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _cb_popmedia_del, NULL);
   edje_object_part_swallow(bg, "terminology.popmedia", o);
   evas_object_show(o);
   if (type == TYPE_IMG)
     edje_object_signal_emit(bg, "popmedia,image", "terminology");
   else if (type == TYPE_SCALE)
     edje_object_signal_emit(bg, "popmedia,scale", "terminology");
   else if (type == TYPE_EDJE)
     edje_object_signal_emit(bg, "popmedia,edje", "terminology");
   else if (type == TYPE_MOV)
     edje_object_signal_emit(bg, "popmedia,movie", "terminology");
}

static Eina_Bool
_cb_cmd_focus(void *data)
{
   _cmdbox_focus_timer = NULL;
   elm_object_focus_set(data, EINA_TRUE);
   return EINA_FALSE;
}

static void
_cb_cmdbox(void *data __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   cmdbox_up = EINA_TRUE;
   elm_object_focus_set(term, EINA_FALSE);
   edje_object_signal_emit(bg, "cmdbox,show", "terminology");
   elm_entry_entry_set(cmdbox, "");
   evas_object_show(cmdbox);
   if (_cmdbox_focus_timer) ecore_timer_del(_cmdbox_focus_timer);
   _cmdbox_focus_timer = ecore_timer_add(0.1, _cb_cmd_focus, cmdbox);
}

static void
_cb_cmd_activated(void *data __UNUSED__, Evas_Object *obj, void *event __UNUSED__)
{
   char *cmd;
   
   elm_object_focus_set(obj, EINA_FALSE);
   edje_object_signal_emit(bg, "cmdbox,hide", "terminology");
   elm_object_focus_set(term, EINA_TRUE);
   cmd = (char *)elm_entry_entry_get(obj);
   if (cmd)
     {
        cmd = elm_entry_markup_to_utf8(cmd);
        if (cmd)
          {
             termcmd_do(term, win, bg, cmd);
             free(cmd);
          }
     }
   if (_cmdbox_focus_timer)
     {
        ecore_timer_del(_cmdbox_focus_timer);
        _cmdbox_focus_timer = NULL;
     }
   cmdbox_up = EINA_FALSE;
}

static void
_cb_cmd_aborted(void *data __UNUSED__, Evas_Object *obj, void *event __UNUSED__)
{
   elm_object_focus_set(obj, EINA_FALSE);
   edje_object_signal_emit(bg, "cmdbox,hide", "terminology");
   elm_object_focus_set(term, EINA_TRUE);
   if (_cmdbox_focus_timer)
     {
        ecore_timer_del(_cmdbox_focus_timer);
        _cmdbox_focus_timer = NULL;
     }
   cmdbox_up = EINA_FALSE;
}

static void
_cb_cmd_changed(void *data __UNUSED__, Evas_Object *obj, void *event __UNUSED__)
{
   char *cmd;
   
   cmd = (char *)elm_entry_entry_get(obj);
   if (cmd)
     {
        cmd = elm_entry_markup_to_utf8(cmd);
        if (cmd)
          {
             termcmd_watch(term, win, bg, cmd);
             free(cmd);
          }
     }
}

static void
_cb_cmd_hints_changed(void *data, Evas *e __UNUSED__, Evas_Object *obj, void *event_info __UNUSED__)
{
   evas_object_show(obj);
   edje_object_part_swallow(data, "terminology.cmdbox", obj);
}

void
main_trans_update(const Config *config)
{
   if (config->translucent)
     {
        edje_object_signal_emit(bg, "translucent,on", "terminology");
        elm_win_alpha_set(win, EINA_TRUE);
        evas_object_hide(backbg);
     }
   else
     {
        edje_object_signal_emit(bg, "translucent,off", "terminology");
        elm_win_alpha_set(win, EINA_FALSE);
        evas_object_show(backbg);
     }
}

static void
_cb_media_del(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   Config *config = data;
   media = NULL;
   edje_object_signal_emit(bg, "media,off", "terminology");
   if (config->temporary) eina_stringshare_replace(&(config->background), NULL);
}

void
main_media_update(const Config *config)
{
   Evas_Object *o;
   int type = 0;

   if ((config->background) && (config->background[0]))
     {
        if (media)
          {
             evas_object_event_callback_del(media, EVAS_CALLBACK_DEL,
                                            _cb_media_del);
             evas_object_del(media);
          }
        o = media = media_add(win, config->background, config, MEDIA_BG, &type);
        evas_object_event_callback_add(media, EVAS_CALLBACK_DEL,
                                       _cb_media_del, config);
        edje_object_part_swallow(bg, "terminology.background", o);
        evas_object_show(o);
        if (type == TYPE_IMG)
          edje_object_signal_emit(bg, "media,image", "terminology");
        else if (type == TYPE_SCALE)
          edje_object_signal_emit(bg, "media,scale", "terminology");
        else if (type == TYPE_EDJE)
          edje_object_signal_emit(bg, "media,edje", "terminology");
        else if (type == TYPE_MOV)
          edje_object_signal_emit(bg, "media,movie", "terminology");
     }
   else
     {
        if (media)
          {
             edje_object_signal_emit(bg, "media,off", "terminology");
             evas_object_del(media);
             media = NULL;
          }
     }
}

void
main_media_mute_update(const Config *config)
{
   if (media) media_mute_set(media, config->mute);
}

static const char *emotion_choices[] = {
  "auto", "gstreamer", "xine", "generic",
  NULL
};

static const Ecore_Getopt options = {
   PACKAGE_NAME,
   "%prog [options]",
   PACKAGE_VERSION,
   "(C) 2012 Carsten Haitzler and others",
   "BSD 2-Clause",
   "Terminal emulator written with Enlightenment Foundation Libraries.",
   EINA_TRUE,
   {
      ECORE_GETOPT_STORE_STR ('e', "exec",
                              "command to execute. "
                              "Defaults to $SHELL (or passwd shel or /bin/sh)"),
      ECORE_GETOPT_STORE_STR ('d', "current-directory",
                              "Change to directory for execution of terminal command."),
      ECORE_GETOPT_STORE_STR ('t', "theme",
                              "Use the named edje theme or path to theme file."),
      ECORE_GETOPT_STORE_STR ('b', "background",
                              "Use the named file as a background wallpaper."),
      ECORE_GETOPT_STORE_STR ('g', "geometry",
                              "Terminal geometry to use (eg 80x24 or 80x24+50+20 etc.)."),
      ECORE_GETOPT_STORE_STR ('n', "name",
                              "Set window name."),
      ECORE_GETOPT_STORE_STR ('r', "role",
                              "Set window role."),
      ECORE_GETOPT_STORE_STR ('T', "title",
                              "Set window title."),
      ECORE_GETOPT_STORE_STR ('i', "icon-name",
                              "Set icon name."),
      ECORE_GETOPT_STORE_STR ('f', "font",
                              "Set font (NAME/SIZE for scalable, NAME for bitmap."),
      ECORE_GETOPT_CHOICE    ('v', "video-module",
                              "Set emotion module to use.", emotion_choices),
        
      ECORE_GETOPT_STORE_BOOL('m', "video-mute",
                              "Set mute mode for video playback."),
      ECORE_GETOPT_STORE_BOOL('c', "cursor-blink",
                              "Set cursor blink mode."),
      ECORE_GETOPT_STORE_BOOL('G', "visual-bell",
                              "Set visual bell mode."),
      ECORE_GETOPT_STORE_TRUE('F', "fullscreen",
                              "Go into the fullscreen mode from start."),
      ECORE_GETOPT_STORE_TRUE('I', "iconic",
                              "Go into an iconic state from the start."),
      ECORE_GETOPT_STORE_TRUE('B', "borderless",
                              "Become a borderless managed window."),
      ECORE_GETOPT_STORE_TRUE('O', "override",
                              "Become an override-redirect window."),
      ECORE_GETOPT_STORE_TRUE('M', "maximized",
                              "Become maximized from the start."),
      ECORE_GETOPT_STORE_TRUE('W', "nowm",
                              "Terminology is run without a wm."),
      ECORE_GETOPT_STORE_TRUE('H', "hold",
                              "Don't exit when the command process exits."),
        
      ECORE_GETOPT_VERSION   ('V', "version"),
      ECORE_GETOPT_COPYRIGHT ('C', "copyright"),
      ECORE_GETOPT_LICENSE   ('L', "license"),
      ECORE_GETOPT_HELP      ('h', "help"),
      ECORE_GETOPT_SENTINEL
   }
};

EAPI_MAIN int
elm_main(int argc, char **argv)
{
   char *cmd = NULL;
   char *cd = NULL;
   char *theme = NULL;
   char *background = NULL;
   char *geometry = NULL;
   char *name = NULL;
   char *role = NULL;
   char *title = NULL;
   char *icon_name = NULL;
   char *font = NULL;
   char *video_module = NULL;
   Eina_Bool video_mute = 0xff; /* unset */
   Eina_Bool cursor_blink = 0xff; /* unset */
   Eina_Bool visual_bell = 0xff; /* unset */
   Eina_Bool fullscreen = EINA_FALSE;
   Eina_Bool iconic = EINA_FALSE;
   Eina_Bool borderless = EINA_FALSE;
   Eina_Bool override = EINA_FALSE;
   Eina_Bool maximized = EINA_FALSE;
   Eina_Bool nowm = EINA_FALSE;
   Eina_Bool quit_option = EINA_FALSE;
   Ecore_Getopt_Value values[] = {
     ECORE_GETOPT_VALUE_STR(cmd),
     ECORE_GETOPT_VALUE_STR(cd),
     ECORE_GETOPT_VALUE_STR(theme),
     ECORE_GETOPT_VALUE_STR(background),
     ECORE_GETOPT_VALUE_STR(geometry),
     ECORE_GETOPT_VALUE_STR(name),
     ECORE_GETOPT_VALUE_STR(role),
     ECORE_GETOPT_VALUE_STR(title),
     ECORE_GETOPT_VALUE_STR(icon_name),
     ECORE_GETOPT_VALUE_STR(font),
     ECORE_GETOPT_VALUE_STR(video_module),
      
     ECORE_GETOPT_VALUE_BOOL(video_mute),
     ECORE_GETOPT_VALUE_BOOL(cursor_blink),
     ECORE_GETOPT_VALUE_BOOL(visual_bell),
     ECORE_GETOPT_VALUE_BOOL(fullscreen),
     ECORE_GETOPT_VALUE_BOOL(iconic),
     ECORE_GETOPT_VALUE_BOOL(borderless),
     ECORE_GETOPT_VALUE_BOOL(override),
     ECORE_GETOPT_VALUE_BOOL(maximized),
     ECORE_GETOPT_VALUE_BOOL(nowm),
     ECORE_GETOPT_VALUE_BOOL(hold),
      
     ECORE_GETOPT_VALUE_BOOL(quit_option),
     ECORE_GETOPT_VALUE_BOOL(quit_option),
     ECORE_GETOPT_VALUE_BOOL(quit_option),
     ECORE_GETOPT_VALUE_BOOL(quit_option),
      
     ECORE_GETOPT_VALUE_NONE
   };
   int args, retval = EXIT_SUCCESS;
   Config *config;
   Evas_Object *o;
   int pos_set = 0, size_set = 0;
   int pos_x = 0, pos_y = 0;
   int size_w = 1, size_h = 1;

   _log_domain = eina_log_domain_register("terminology", NULL);
   if (_log_domain < 0)
     {
        EINA_LOG_CRIT("could not create log domain 'terminology'.");
        elm_shutdown();
        return EXIT_FAILURE;
     }

   config_init();

   config = config_load("config");

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
   elm_app_compile_bin_dir_set(PACKAGE_BIN_DIR);
   elm_app_compile_data_dir_set(PACKAGE_DATA_DIR);
   elm_app_info_set(elm_main, "terminology", "themes/default.edj");

   args = ecore_getopt_parse(&options, values, argc, argv);
   if (args < 0)
     {
        ERR("Could not parse command line options.");
        retval = EXIT_FAILURE;
        goto end;
     }

   if (quit_option) goto end;

   if (theme)
     {
        char path[PATH_MAX];
        char nom[PATH_MAX];

        if (eina_str_has_suffix(theme, ".edj"))
          eina_strlcpy(nom, theme, sizeof(nom));
        else
          snprintf(nom, sizeof(nom), "%s.edj", theme);

        if (strchr(nom, '/'))
          eina_strlcpy(path, nom, sizeof(path));
        else
          snprintf(path, sizeof(path), "%s/themes/%s",
                   elm_app_data_dir_get(), nom);

        eina_stringshare_replace(&(config->theme), path);
        config->temporary = EINA_TRUE;
     }

   if (background)
     {
        eina_stringshare_replace(&(config->background), background);
        config->temporary = EINA_TRUE;
     }

   if (font)
     {
        if (strchr(font, '/'))
          {
             char *fname = alloca(strlen(font) + 1);
             char *p;
             
             strcpy(fname, font);
             p = strrchr(fname, '/');
             if (p)
               {
                  int sz;
                  
                  *p = 0;
                  p++;
                  sz = atoi(p);
                  if (sz > 0) config->font.size = sz;
                  eina_stringshare_replace(&(config->font.name), fname);
               }
             config->font.bitmap = 0;
          }
        else
          {
             char buf[4096], *file;
             Eina_List *files;
             int n = strlen(font);
             
             snprintf(buf, sizeof(buf), "%s/fonts", elm_app_data_dir_get());
             files = ecore_file_ls(buf);
             EINA_LIST_FREE(files, file)
               {
                  if (n > 0)
                    {
                       if (!strncasecmp(file, font, n))
                         {
                            n = -1;
                            eina_stringshare_replace(&(config->font.name), file);
                            config->font.bitmap = 1;
                         }
                    }
                  free(file);
               }
          }
        config->temporary = EINA_TRUE;
     }

   if (video_module)
     {
        int i;
        for (i = 0; i < (int)EINA_C_ARRAY_LENGTH(emotion_choices); i++)
          {
             if (video_module == emotion_choices[i])
               break;
          }

        if (i == EINA_C_ARRAY_LENGTH(emotion_choices))
          i = 0; /* ecore getopt shouldn't let this happen, but... */
        config->vidmod = i;
        config->temporary = EINA_TRUE;
     }

   if (video_mute != 0xff)
     {
        config->mute = video_mute;
        config->temporary = EINA_TRUE;
     }
   if (cursor_blink != 0xff)
     {
        config->disable_cursor_blink = !cursor_blink;
        config->temporary = EINA_TRUE;
     }
   if (visual_bell != 0xff)
     {
        config->disable_visual_bell = !visual_bell;
        config->temporary = EINA_TRUE;
     }
   
   if (geometry)
     {
        if (sscanf(geometry,"%ix%i+%i+%i", &size_w, &size_h, &pos_x, &pos_y) == 4)
          {
             pos_set = 1;
             size_set = 1;
          }
        else if (sscanf(geometry,"%ix%i-%i+%i", &size_w, &size_h, &pos_x, &pos_y) == 4)
          {
             pos_x = -pos_x;
             pos_set = 1;
             size_set = 1;
          }
        else if (sscanf(geometry,"%ix%i-%i-%i", &size_w, &size_h, &pos_x, &pos_y) == 4)
          {
             pos_x = -pos_x;
             pos_y = -pos_y;
             pos_set = 1;
             size_set = 1;
          }
        else if (sscanf(geometry,"%ix%i+%i-%i", &size_w, &size_h, &pos_x, &pos_y) == 4)
          {
             pos_y = -pos_y;
             pos_set = 1;
             size_set = 1;
          }
        else if (sscanf(geometry,"%ix%i", &size_w, &size_h) == 2)
          {
             size_set = 1;
          }
        else if (sscanf(geometry,"+%i+%i", &pos_x, &pos_y) == 2)
          {
             pos_set = 1;
          }
        else if (sscanf(geometry,"-%i+%i", &pos_x, &pos_y) == 2)
          {
             pos_x = -pos_x;
             pos_set = 1;
          }
        else if (sscanf(geometry,"+%i-%i", &pos_x, &pos_y) == 2)
          {
             pos_y = -pos_y;
             pos_set = 1;
          }
        else if (sscanf(geometry,"-%i-%i", &pos_x, &pos_y) == 2)
          {
             pos_x = -pos_x;
             pos_y = -pos_y;
             pos_set = 1;
          }
     }
   if (!size_set)
     {
        size_w = 80;
        size_h = 24;
     }

   // set an env so terminal apps can detect they are in terminology :)
   putenv("TERMINOLOGY=1");
   unsetenv("DESKTOP_STARTUP_ID");

   win = tg_win_add(name, role, title, icon_name);

   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _cb_del, NULL);
   elm_win_conformant_set(win, EINA_TRUE);

   if (fullscreen) elm_win_fullscreen_set(win, EINA_TRUE);
   if (iconic) elm_win_iconified_set(win, EINA_TRUE);
   if (borderless) elm_win_borderless_set(win, EINA_TRUE);
   if (override) elm_win_override_set(win, EINA_TRUE);
   if (maximized) elm_win_maximized_set(win, EINA_TRUE);

   backbg = o = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_color_set(o, 0, 0, 0, 255);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_win_resize_object_add(win, o);
   evas_object_show(o);

   conform = o = elm_conformant_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_win_resize_object_add(win, o);
   evas_object_show(o);

   bg = o = edje_object_add(evas_object_evas_get(win));
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   if (!theme_apply(o, config, "terminology/background"))
     {
        CRITICAL("Couldn't find terminology theme! Forgot 'make install'?");
        retval = EXIT_FAILURE;
        goto end;
     }
   theme_auto_reload_enable(o);
   elm_object_content_set(conform, o);
   evas_object_show(o);

   edje_object_signal_callback_add(o, "popmedia,done", "terminology",
                                   _cb_popmedia_done, NULL);

   if (pos_set)
     {
        int screen_w, screen_h;
        elm_win_screen_size_get(win, NULL, NULL, &screen_w, &screen_h);
        if (pos_x < 0) pos_x = screen_w + pos_x;
        if (pos_y < 0) pos_y = screen_h + pos_y;
        evas_object_move(win, pos_x, pos_y);
     }
   
   cmdbox = o = elm_entry_add(win);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_FALSE);
   elm_entry_scrollbar_policy_set(o, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
   elm_entry_input_panel_layout_set(o, ELM_INPUT_PANEL_LAYOUT_TERMINAL);
   elm_entry_autocapital_type_set(o, ELM_AUTOCAPITAL_TYPE_NONE);
   elm_entry_input_panel_enabled_set(o, EINA_TRUE);
   elm_entry_input_panel_language_set(o, ELM_INPUT_PANEL_LANG_ALPHABET);
   elm_entry_input_panel_return_key_type_set(o, ELM_INPUT_PANEL_RETURN_KEY_TYPE_GO);
   elm_entry_prediction_allow_set(o, EINA_FALSE);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "activated", _cb_cmd_activated, bg);
   evas_object_smart_callback_add(o, "aborted", _cb_cmd_aborted, bg);
   evas_object_smart_callback_add(o, "changed,user", _cb_cmd_changed, bg);
   evas_object_event_callback_add(o, EVAS_CALLBACK_CHANGED_SIZE_HINTS, _cb_cmd_hints_changed, bg);
   edje_object_part_swallow(bg, "terminology.cmdbox", o);
   
   term = o = termio_add(win, config, cmd, cd, size_w, size_h);
   termio_win_set(o, win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_event_callback_add(o, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                  _cb_size_hint, win);
   edje_object_part_swallow(bg, "terminology.content", o);
   evas_object_smart_callback_add(o, "options", _cb_options, NULL);
   evas_object_smart_callback_add(o, "change", _cb_change, NULL);
   evas_object_smart_callback_add(o, "exited", _cb_exited, NULL);
   evas_object_smart_callback_add(o, "bell", _cb_bell, NULL);
   evas_object_smart_callback_add(o, "popup", _cb_popup, NULL);
   evas_object_smart_callback_add(o, "cmdbox", _cb_cmdbox, NULL);
   evas_object_show(o);

   main_trans_update(config);
   main_media_update(config);

   evas_object_smart_callback_add(win, "focus,in", _cb_focus_in, term);
   evas_object_smart_callback_add(win, "focus,out", _cb_focus_out, term);
   _cb_size_hint(win, evas_object_evas_get(win), term, NULL);

   evas_object_show(win);
   if (nowm)
     ecore_evas_focus_set(
        ecore_evas_ecore_evas_get(evas_object_evas_get(win)), 1);

   elm_run();
 end:

   config_del(config);
   config_shutdown();

   if (win) evas_object_del(win);

   eina_log_domain_unregister(_log_domain);
   _log_domain = -1;

   elm_shutdown();
   return retval;
}
ELM_MAIN()
