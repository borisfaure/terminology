#include "private.h"

#include <Elementary.h>
#include "options.h"
#include "options_font.h"
#include "options_helpers.h"
#include "options_behavior.h"
#include "options_video.h"
#include "options_theme.h"
#include "options_wallpaper.h"
#include "options_colors.h"
#include "config.h"
#include "termio.h"

static Evas_Object *op_frame, *op_box = NULL, *op_toolbar = NULL,
                   *op_opbox = NULL, *op_tbox = NULL, *op_temp = NULL,
                   *op_over = NULL;
static Eina_Bool op_out = EINA_FALSE;
static Ecore_Timer *op_del_timer = NULL;
static Evas_Object *saved_win = NULL;
static Evas_Object *saved_bg = NULL;
static int mode = -1;

static void
_cb_op_font(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   if (mode == 1) return;
   mode = 1;
   edje_object_signal_emit(saved_bg, "optdetails,hide", "terminology");
}

static void
_cb_op_theme(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   if (mode == 2) return;
   mode = 2;
   edje_object_signal_emit(saved_bg, "optdetails,hide", "terminology");
}

static void
_cb_op_wallpaper(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   if (mode == 3) return;
   mode = 3;
   edje_object_signal_emit(saved_bg, "optdetails,hide", "terminology");
}

static void
_cb_op_colors(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   if (mode == 4) return;
   mode = 4;
   edje_object_signal_emit(saved_bg, "optdetails,hide", "terminology");
}

static void
_cb_op_video(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   if (mode == 5) return;
   mode = 5;
   edje_object_signal_emit(saved_bg, "optdetails,hide", "terminology");
}

static void
_cb_op_behavior(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   if (mode == 6) return;
   mode = 6;
   edje_object_signal_emit(saved_bg, "optdetails,hide", "terminology");
}

static void
_cb_op_helpers(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   if (mode == 7) return;
   mode = 7;
   edje_object_signal_emit(saved_bg, "optdetails,hide", "terminology");
}

static void
_cb_op_tmp_chg(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Config *config = data;
   config->temporary = elm_check_state_get(obj);
}

static Eina_Bool
_cb_op_del_delay(void *data __UNUSED__)
{
   evas_object_del(op_opbox);
   evas_object_del(op_frame);
   options_font_clear();
   op_opbox = NULL;
   op_frame = NULL;
   op_del_timer = NULL;
   elm_cache_all_flush();
   return EINA_FALSE;
}

static void
_cb_mouse_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *ev __UNUSED__)
{
   options_toggle(saved_win, saved_bg, data);
}

static void
_cb_opdt_hide_done(void *data, Evas_Object *obj __UNUSED__, const char *sig, const char *src)
{
   elm_box_clear(op_opbox);
   switch (mode)
     {
      case 1: options_font(op_opbox, data); break;
      case 2: options_theme(op_opbox, data); break;
      case 3: options_wallpaper(op_opbox, data); break;
      case 4: options_colors(op_opbox, data); break;
      case 5: options_video(op_opbox, data); break;
      case 6: options_behavior(op_opbox, data); break;
      case 7: options_helpers(op_opbox, data); break;
      default: break;
     }
   edje_object_signal_emit(saved_bg, "optdetails,show", "terminology");
}

void
options_toggle(Evas_Object *win, Evas_Object *bg, Evas_Object *term)
{
   Evas_Object *o;

   saved_win = win;
   saved_bg = bg;
   mode = -1;
   if (!op_frame)
     {
        Elm_Object_Item *it_fn;
        Config *config = termio_config_get(term);

        op_opbox = o = elm_box_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        edje_object_part_swallow(bg, "terminology.optdetails", o);
        evas_object_show(o);

        op_frame = o = elm_frame_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, "Options");

        op_box = o = elm_box_add(win);
        elm_box_horizontal_set(o, EINA_TRUE);
        elm_object_content_set(op_frame, o);
        evas_object_show(o);

        op_tbox = o = elm_box_add(win);
        evas_object_size_hint_weight_set(o, 0.0, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, 1.0, EVAS_HINT_FILL);
        elm_box_pack_end(op_box, o);
        evas_object_show(o);
        
        op_toolbar = o = elm_toolbar_add(win);
        evas_object_size_hint_weight_set(o, 0.0, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, 0.5, EVAS_HINT_FILL);
        elm_toolbar_horizontal_set(o, EINA_FALSE);
        elm_object_style_set(o, "item_horizontal");
        elm_toolbar_icon_size_set(o, 16);
        elm_toolbar_shrink_mode_set(o, ELM_TOOLBAR_SHRINK_SCROLL);
        elm_toolbar_select_mode_set(o, ELM_OBJECT_SELECT_MODE_ALWAYS);
        elm_toolbar_menu_parent_set(o, win);
        elm_toolbar_homogeneous_set(o, EINA_FALSE);

        it_fn = 
        elm_toolbar_item_append(o, "preferences-desktop-font",       "Font",      _cb_op_font,      term);
        elm_toolbar_item_append(o, "preferences-desktop-theme",      "Theme",     _cb_op_theme,     term);
        elm_toolbar_item_append(o, "preferences-desktop-wallpaper",  "Wallpaper", _cb_op_wallpaper, term);
        elm_toolbar_item_append(o, "preferences-color",              "Colors",    _cb_op_colors,    term);
        elm_toolbar_item_append(o, "preferences-desktop-multimedia", "Video",     _cb_op_video,     term);
        elm_toolbar_item_append(o, "system-run",                     "Behavior",  _cb_op_behavior,  term);
        elm_toolbar_item_append(o, "document-open",                  "Helpers",   _cb_op_helpers,   term);

        elm_box_pack_end(op_tbox, o);
        evas_object_show(o);

        elm_toolbar_item_selected_set(it_fn, EINA_TRUE);
        
        op_temp = o = elm_check_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 1.0);
        elm_object_text_set(o, "Temporary");
        elm_check_state_set(o, config->temporary);
        elm_box_pack_end(op_tbox, o);
        evas_object_show(o);
        evas_object_smart_callback_add(o, "changed", _cb_op_tmp_chg, config);

        edje_object_part_swallow(bg, "terminology.options", op_frame);
        evas_object_show(op_frame);
     }
   else if ((op_opbox) && (!op_out))
     edje_object_signal_emit(bg, "optdetails,show", "terminology");
     
   if (!op_out)
     {
        edje_object_signal_callback_add(bg, "optdetails,hide,done",
                                        "terminology",
                                        _cb_opdt_hide_done, term);
        op_over = o = evas_object_rectangle_add(evas_object_evas_get(win));
        evas_object_color_set(o, 0, 0, 0, 0);
        edje_object_part_swallow(bg, "terminology.dismiss", o);
        evas_object_show(o);
        evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                       _cb_mouse_down, term);

        edje_object_signal_emit(bg, "options,show", "terminology");
        op_out = EINA_TRUE;
        elm_object_focus_set(op_toolbar, EINA_TRUE);
        if (op_del_timer)
          {
             ecore_timer_del(op_del_timer);
             op_del_timer = NULL;
          }
     }
   else
     {
        edje_object_signal_callback_del(bg, "optdetails,hide,done",
                                        "terminology",
                                        _cb_opdt_hide_done);
        evas_object_del(op_over);
        op_over = NULL;
        edje_object_signal_emit(bg, "options,hide", "terminology");
        edje_object_signal_emit(bg, "optdetails,hide", "terminology");
        op_out = EINA_FALSE;
        elm_object_focus_set(op_frame, EINA_FALSE);
        elm_object_focus_set(term, EINA_TRUE);
        if (op_del_timer) ecore_timer_del(op_del_timer);
        op_del_timer = ecore_timer_add(10.0, _cb_op_del_delay, NULL);
     }
}

Eina_Bool
options_active_get(void)
{
   return op_out;
}
