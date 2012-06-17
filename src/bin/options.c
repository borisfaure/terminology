#include <Elementary.h>
#include "options.h"
#include "options_font.h"
#include "options_behavior.h"
#include "options_video.h"

static Evas_Object *op_frame, *op_box = NULL, *op_toolbar = NULL, *op_opbox = NULL;
static Eina_Bool op_out = EINA_FALSE;

static void
_cb_op_font(void *data, Evas_Object *obj, void *event)
{
   elm_box_clear(op_opbox);
   options_font(op_opbox, data);
}

static void
_cb_op_theme(void *data, Evas_Object *obj, void *event)
{
   elm_box_clear(op_opbox);
   // XXX: not done yet
}

static void
_cb_op_wallpaper(void *data, Evas_Object *obj, void *event)
{
   elm_box_clear(op_opbox);
   // XXX: not done yet
}

static void
_cb_op_video(void *data, Evas_Object *obj, void *event)
{
   elm_box_clear(op_opbox);
   options_video(op_opbox, data);
}

static void
_cb_op_behavior(void *data, Evas_Object *obj, void *event)
{
   elm_box_clear(op_opbox);
   options_behavior(op_opbox, data);
}

void
options_toggle(Evas_Object *win, Evas_Object *bg, Evas_Object *term)
{
   Evas_Object *o;

   if (!op_frame)
     {
        Elm_Object_Item *it_fn, *it_th, *it_wp, *it_bh;

        op_frame = o = elm_frame_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, "Options");

        op_box = o = elm_box_add(win);
        elm_box_horizontal_set(o, EINA_TRUE);
        elm_object_content_set(op_frame, o);
        evas_object_show(o);

        op_opbox = o = elm_box_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_box_pack_end(op_box, o);
        evas_object_show(o);

        op_toolbar = o = elm_toolbar_add(win);
        elm_toolbar_horizontal_set(o, EINA_FALSE);
        elm_object_style_set(o, "item_horizontal");
        evas_object_size_hint_weight_set(o, 0.0, 0.0);
        evas_object_size_hint_align_set(o, 0.5, 0.0);
        elm_toolbar_icon_size_set(o, 16);
        elm_toolbar_shrink_mode_set(o, ELM_TOOLBAR_SHRINK_NONE);
        elm_toolbar_select_mode_set(o, ELM_OBJECT_SELECT_MODE_DEFAULT);
        elm_toolbar_menu_parent_set(o, win);
        elm_toolbar_homogeneous_set(o, EINA_FALSE);

        it_fn = elm_toolbar_item_append(o, "preferences-desktop-font",
                                        "Font", _cb_op_font, term);
        it_th = elm_toolbar_item_append(o, "preferences-desktop-theme",
                                        "Theme", _cb_op_theme, NULL);
        it_wp = elm_toolbar_item_append(o, "preferences-desktop-wallpaper",
                                        "Wallpaper", _cb_op_wallpaper, NULL);
        it_wp = elm_toolbar_item_append(o, "preferences-desktop-multimedia",
                                        "Video", _cb_op_video, NULL);
        it_bh = elm_toolbar_item_append(o, "system-run",
                                        "Behavior", _cb_op_behavior, term);

        elm_box_pack_end(op_box, o);
        evas_object_show(o);

        elm_toolbar_item_selected_set(it_fn, EINA_TRUE);

        evas_smart_objects_calculate(evas_object_evas_get(win));
        edje_object_part_swallow(bg, "terminology.options", op_frame);
        evas_object_show(o);
     }
   if (!op_out)
     {
        edje_object_signal_emit(bg, "options,show", "terminology");
        op_out = EINA_TRUE;
     }
   else
     {
        edje_object_signal_emit(bg, "options,hide", "terminology");
        op_out = EINA_FALSE;
        elm_object_focus_set(term, EINA_TRUE);
     }
}
