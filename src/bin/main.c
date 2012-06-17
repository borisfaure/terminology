#include "private.h"

#include <Ecore_Getopt.h>
#include <Elementary.h>
#include "main.h"
#include "win.h"
#include "termio.h"
#include "config.h"
#include "options.h"
#include "media.h"

int _log_domain = -1;

static Evas_Object *win = NULL, *bg = NULL, *term = NULL, *media = NULL;
static Ecore_Timer *flush_timer = NULL;

static void
_cb_focus_in(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   edje_object_signal_emit(bg, "focus,in", "terminology");
   elm_object_focus_set(data, EINA_TRUE);
}

static void
_cb_focus_out(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   edje_object_signal_emit(bg, "focus,out", "terminology");
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
   options_toggle(win, bg, term);
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

void
main_trans_update(void)
{
   if (config->translucent)
     {
        edje_object_signal_emit(bg, "translucent,on", "terminology");
        elm_win_alpha_set(win, EINA_TRUE);
     }
   else
     {
        edje_object_signal_emit(bg, "translucent,off", "terminology");
        elm_win_alpha_set(win, EINA_FALSE);
     }
}

void
main_media_update(void)
{
   Evas_Object *o;
   int type = 0;
   
   if ((config->background) && (config->background[0]))
     {
        if (media) evas_object_del(media);
        o = media = media_add(win, config->background, MEDIA_BG, &type);
        edje_object_part_swallow(bg, "terminology.background", o);
        if (type == TYPE_IMG)
          edje_object_signal_emit(bg, "media,image", "terminology");
        else if (type == TYPE_SCALE)
          edje_object_signal_emit(bg, "media,scale", "terminology");
        else if (type == TYPE_EDJE)
          edje_object_signal_emit(bg, "media,edje", "terminology");
        else if (type == TYPE_MOV)
          edje_object_signal_emit(bg, "media,movie", "terminology");
        evas_object_show(o);
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
main_media_mute_update(void)
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
   "(C) 2012 Carsten Haitzler",
   "GPL-2",
   "Terminal emulator written with Enlightenment Foundation Libraries.",
   EINA_TRUE,
   {
      ECORE_GETOPT_STORE_STR('e', "exec",
                             "command to execute. "
                             "Defaults to $SHELL (or passwd shel or /bin/sh)"),
      ECORE_GETOPT_STORE_STR('t', "theme",
                             "Use the named edje theme or path to theme file."),
      ECORE_GETOPT_STORE_STR('b', "background",
                             "Use the named file as a background wallpaper."),
      ECORE_GETOPT_CHOICE(0, "video-module",
                          "Set emotion module to use.",
                          emotion_choices),
      ECORE_GETOPT_STORE_BOOL(0, "video-mute",
                              "Set mute mode for video playback."),
      ECORE_GETOPT_VERSION('V', "version"),
      ECORE_GETOPT_COPYRIGHT('C', "copyright"),
      ECORE_GETOPT_LICENSE('L', "license"),
      ECORE_GETOPT_HELP('h', "help"),
      ECORE_GETOPT_SENTINEL
   }
};

EAPI_MAIN int
elm_main(int argc, char **argv)
{
   char *cmd = NULL;
   char *theme = NULL;
   char *background = NULL;
   char *video_module = NULL;
   Eina_Bool video_mute = 0xff; /* unset */
   Eina_Bool quit_option = EINA_FALSE;
   Ecore_Getopt_Value values[] = {
     ECORE_GETOPT_VALUE_STR(cmd),
     ECORE_GETOPT_VALUE_STR(theme),
     ECORE_GETOPT_VALUE_STR(background),
     ECORE_GETOPT_VALUE_STR(video_module),
     ECORE_GETOPT_VALUE_BOOL(video_mute),
     ECORE_GETOPT_VALUE_BOOL(quit_option),
     ECORE_GETOPT_VALUE_BOOL(quit_option),
     ECORE_GETOPT_VALUE_BOOL(quit_option),
     ECORE_GETOPT_VALUE_BOOL(quit_option),
     ECORE_GETOPT_VALUE_NONE
   };
   int args, retval = EXIT_SUCCESS;
   Evas_Object *o;


   _log_domain = eina_log_domain_register("terminology", NULL);
   if (_log_domain < 0)
     {
        EINA_LOG_CRIT("could not create log domain 'terminology'.");
        elm_shutdown();
        return EXIT_FAILURE;
     }

   config_init();
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
        if (eina_str_has_suffix(theme, ".edj"))
          eina_stringshare_replace(&(config->theme), theme);
        else
          {
             char buf[PATH_MAX];
             snprintf(buf, sizeof(buf), "%s.edj", theme);
             eina_stringshare_replace(&(config->theme), buf);
          }
        config_tmp = EINA_TRUE;
     }

   if (background)
     {
        eina_stringshare_replace(&(config->background), background);
        config_tmp = EINA_TRUE;
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
        config_tmp = EINA_TRUE;
     }

   if (video_mute != 0xff)
     {
        config->mute = video_mute;
        config_tmp = EINA_TRUE;
     }

   win = tg_win_add();

   bg = o = edje_object_add(evas_object_evas_get(win));
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   if (strchr(config->theme, '/'))
        edje_object_file_set(o, config->theme, "terminology/background");
   else
     {
        char buf[PATH_MAX];
        snprintf(buf, sizeof(buf), "%s/themes/%s",
                 elm_app_data_dir_get(), config->theme);
        edje_object_file_set(o, buf, "terminology/background");
     }
   elm_win_resize_object_add(win, o);
   evas_object_show(o);

   term = o = termio_add(win, cmd, 80, 24);
   termio_win_set(o, win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_event_callback_add(o, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                  _cb_size_hint, win);
   edje_object_part_swallow(bg, "terminology.content", o);
   evas_object_smart_callback_add(o, "options", _cb_options, NULL);
   evas_object_smart_callback_add(o, "change", _cb_change, NULL);
   evas_object_show(o);

   main_trans_update();
   main_media_update();
   
   evas_object_smart_callback_add(win, "focus,in", _cb_focus_in, term);
   evas_object_smart_callback_add(win, "focus,out", _cb_focus_out, term);
   _cb_size_hint(win, evas_object_evas_get(win), term, NULL);

   evas_object_show(win);

   elm_run();
 end:
   config_shutdown();

   eina_log_domain_unregister(_log_domain);
   _log_domain = -1;

   elm_shutdown();
   return retval;
}
ELM_MAIN()
