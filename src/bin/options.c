#include "private.h"

#include <Elementary.h>
#include "options.h"
#include "options_font.h"
#include "options_theme.h"
#include "options_wallpaper.h"
#include "options_colors.h"
#include "options_video.h"
#include "options_behavior.h"
#include "options_keys.h"
#include "options_helpers.h"
#include "config.h"
#include "termio.h"

static Evas_Object *op_frame, *op_box = NULL, *op_toolbar = NULL,
                   *op_opbox = NULL, *op_tbox = NULL, *op_temp = NULL,
                   *op_over = NULL;
static Eina_Bool op_out = EINA_FALSE;
static Ecore_Timer *op_del_timer = NULL;
static Evas_Object *saved_win = NULL;
static Evas_Object *saved_bg = NULL;
static void (*op_donecb) (void *data) = NULL;
static void *op_donedata = NULL;

static enum option_mode {
     OPTION_NONE = 0,
     OPTION_FONT,
     OPTION_THEME,
     OPTION_WALLPAPER,
     OPTION_COLORS,
     OPTION_VIDEO,
     OPTION_BEHAVIOR,
     OPTION_KEYS,
     OPTION_HELPERS
} _mode = 0;

static void
_cb_op(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   enum option_mode mode = (intptr_t) data;

   if (_mode == mode) return;
   _mode = mode;
   edje_object_signal_emit(saved_bg, "optdetails,hide", "terminology");
}

static void
_cb_op_tmp_chg(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Config *config = data;
   config->temporary = elm_check_state_get(obj);
}

static Eina_Bool
_cb_op_del_delay(void *data EINA_UNUSED)
{
   evas_object_del(op_opbox);
   evas_object_del(op_frame);
   options_font_clear();
   options_wallpaper_clear();
   options_theme_clear();
   op_opbox = NULL;
   op_frame = NULL;
   op_del_timer = NULL;
   elm_cache_all_flush();
   return EINA_FALSE;
}

static void
_cb_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *ev EINA_UNUSED)
{
   options_toggle(saved_win, saved_bg, data, op_donecb, op_donedata);
}

static void
_cb_opdt_hide_done(void *data, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   elm_box_clear(op_opbox);
   switch (_mode)
     {
      case OPTION_NONE:      break;
      case OPTION_FONT:      options_font(op_opbox, data); break;
      case OPTION_THEME:     options_theme(op_opbox, data); break;
      case OPTION_WALLPAPER: options_wallpaper(op_opbox, data); break;
      case OPTION_COLORS:    options_colors(op_opbox, data); break;
      case OPTION_VIDEO:     options_video(op_opbox, data); break;
      case OPTION_BEHAVIOR:  options_behavior(op_opbox, data); break;
      case OPTION_KEYS:      options_keys(op_opbox, data); break;
      case OPTION_HELPERS:   options_helpers(op_opbox, data); break;
     }
   edje_object_signal_emit(saved_bg, "optdetails,show", "terminology");
}

static void
_cb_opdt_hide_done2(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   if (op_del_timer)
     {
        ecore_timer_del(op_del_timer);
        op_del_timer = NULL;
     }
   _cb_op_del_delay(NULL);
   edje_object_signal_callback_del(saved_bg, "optdetails,hide,done",
                                   "terminology",
                                   _cb_opdt_hide_done2);
}

void
options_toggle(Evas_Object *win, Evas_Object *bg, Evas_Object *term,
               void (*donecb) (void *data), void *donedata)
{
   Evas_Object *o;

   _mode = OPTION_NONE;
   if (!op_frame)
     {
        Elm_Object_Item *it_fn;
        Config *config = termio_config_get(term);
        
        if (!config) return;
        saved_win = win;
        saved_bg = bg;
        
        op_opbox = o = elm_box_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        edje_object_part_swallow(bg, "terminology.optdetails", o);
        evas_object_show(o);

        op_frame = o = elm_frame_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, _("Options"));

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

#define ITEM_APPEND(_icon_name, _name, _option_mode) \
        elm_toolbar_item_append(o, _icon_name, _name, _cb_op, \
                                (void*) OPTION_##_option_mode)

        it_fn =
        ITEM_APPEND("preferences-desktop-font", _("Font"), FONT);
        ITEM_APPEND("preferences-desktop-theme", _("Theme"), THEME);
        ITEM_APPEND("preferences-desktop-wallpaper", _("Wallpaper"), WALLPAPER);
        ITEM_APPEND("video-display", _("Video"), VIDEO);
        ITEM_APPEND("preferences-desktop-theme", _("Colors"), COLORS);
        ITEM_APPEND("preferences-system", _("Behavior"), BEHAVIOR);
        ITEM_APPEND("preferences-desktop-keyboard-shortcuts", _("Keys"), KEYS);
        ITEM_APPEND("system-run", _("Helpers"), HELPERS);
#undef ITEM_APPEND

        elm_box_pack_end(op_tbox, o);
        evas_object_show(o);

        elm_toolbar_item_selected_set(it_fn, EINA_TRUE);
        
        op_temp = o = elm_check_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 1.0);
        elm_object_text_set(o, _("Temporary"));
        elm_check_state_set(o, config->temporary);
        elm_box_pack_end(op_tbox, o);
        evas_object_show(o);
        evas_object_smart_callback_add(o, "changed", _cb_op_tmp_chg, config);

        edje_object_part_swallow(bg, "terminology.options", op_frame);
        evas_object_show(op_frame);
     }
   else if ((op_opbox) && (!op_out))
     {
        edje_object_part_swallow(bg, "terminology.optdetails", op_opbox);
        edje_object_part_swallow(bg, "terminology.options", op_frame);
        edje_object_signal_emit(bg, "optdetails,show", "terminology");
        edje_object_signal_emit(bg, "options,show", "terminology");
     }
     
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
        op_donecb = donecb;
        op_donedata = donedata;
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
        edje_object_signal_callback_add(bg, "optdetails,hide,done",
                                        "terminology",
                                        _cb_opdt_hide_done2, term);
        elm_object_focus_set(op_frame, EINA_FALSE);
        elm_object_focus_set(op_opbox, EINA_FALSE);
        elm_object_focus_set(op_toolbar, EINA_FALSE);
        if (op_donecb) op_donecb(op_donedata);
        evas_object_del(op_over);
        op_over = NULL;
        edje_object_signal_emit(bg, "options,hide", "terminology");
        edje_object_signal_emit(bg, "optdetails,hide", "terminology");
        op_out = EINA_FALSE;
        if (op_del_timer) ecore_timer_del(op_del_timer);
        op_del_timer = ecore_timer_add(10.0, _cb_op_del_delay, NULL);
     }
}

Eina_Bool
options_active_get(void)
{
   return op_out;
}
