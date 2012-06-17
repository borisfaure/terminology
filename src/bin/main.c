#include <Elementary.h>
#include "main.h"
#include "win.h"
#include "termio.h"
#include "config.h"
#include "options.h"
#include "media.h"

static const char *cmd = NULL;
static Evas_Object *win = NULL, *bg = NULL, *term = NULL, *media = NULL;
static Ecore_Timer *flush_timer = NULL;

static void
_cb_focus_in(void *data, Evas_Object *obj, void *event)
{
   edje_object_signal_emit(bg, "focus,in", "terminology");
   elm_object_focus_set(data, EINA_TRUE);
}

static void
_cb_focus_out(void *data, Evas_Object *obj, void *event)
{
   edje_object_signal_emit(bg, "focus,out", "terminology");
   elm_object_focus_set(data, EINA_FALSE);
   elm_cache_all_flush();
}

static void
_cb_size_hint(void *data, Evas *e, Evas_Object *obj, void *event)
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
_cb_options(void *data, Evas_Object *obj, void *event)
{
   options_toggle(win, bg, term);
}

static Eina_Bool
_cb_flush(void *data)
{
   flush_timer = NULL;
   elm_cache_all_flush();
   return EINA_FALSE;
}

static void
_cb_change(void *data, Evas_Object *obj, void *event)
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
   
   if (config->background)
     {
        if (media) evas_object_del(media);
        o = media = media_add(win, config->background, MEDIA_BG);
        edje_object_part_swallow(bg, "terminology.background", o);
        evas_object_show(o);
     }
   else
     {
        if (media)
          {
             evas_object_del(media);
             media = NULL;
          }
     }
}

EAPI_MAIN int
elm_main(int argc, char **argv)
{
   int i;
   Evas_Object *o;
   char buf[4096], *p;

   config_init();
   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
   elm_app_compile_bin_dir_set(PACKAGE_BIN_DIR);
   elm_app_compile_data_dir_set(PACKAGE_DATA_DIR);
   elm_app_info_set(elm_main, "terminology", "themes/default.edj");

   for (i = 1; i < argc; i++)
     {
        if ((!strcmp(argv[i], "-e")) && (i < (argc - 1)))
          {
             i++;
             cmd = argv[i];
          }
        else if ((!strcmp(argv[i], "-t")) && (i < (argc - 1)))
          {
             i++;
             if (config->theme) eina_stringshare_del(config->theme);
             p = strchr(argv[i], '.');
             if ((!p) || (strcasecmp(p, ".edj")))
               {
                  snprintf(buf, sizeof(buf), "%s.edj", argv[i]);
                  config->theme = eina_stringshare_add(buf);
               }
             else
               config->theme = eina_stringshare_add(argv[i]);
          }
        else if ((!strcmp(argv[i], "-b")) && (i < (argc - 1)))
          {
             i++;
             if (config->background) eina_stringshare_del(config->background);
             config->background = eina_stringshare_add(argv[i]);
          }
     }

   win = tg_win_add();

   bg = o = edje_object_add(evas_object_evas_get(win));
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   if (strchr(config->theme, '/'))
        edje_object_file_set(o, config->theme, "terminology/background");
   else
     {
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
   config_shutdown();
   elm_shutdown();
   return 0;
}
ELM_MAIN()
