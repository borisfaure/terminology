#include "private.h"

#include <Elementary.h>
#include "controls.h"
#include "options.h"
#include "about.h"
#include "termio.h"

#include "background_generated.h"

static Evas_Object *ct_frame = NULL, *ct_box = NULL, *ct_over = NULL;
static Eina_Bool ct_out = EINA_FALSE;
static Ecore_Timer *ct_del_timer = NULL;
static Evas_Object *saved_win = NULL;
static Evas_Object *saved_bg = NULL;

static Evas_Object *ct_win, *ct_bg, *ct_term;

static Eina_Bool
_cb_ct_del_delay(void *data __UNUSED__)
{
   evas_object_del(ct_frame);
   ct_frame = NULL;
   ct_del_timer = NULL;
   elm_cache_all_flush();
   return EINA_FALSE;
}

static void
_cb_ct_copy(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   controls_toggle(ct_win, ct_bg, ct_term);
   termio_copy_clipboard(data);
}

static void
_cb_ct_paste(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   controls_toggle(ct_win, ct_bg, ct_term);
   termio_paste_clipboard(data);
}

static void
_cb_ct_options(void *data __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   controls_toggle(ct_win, ct_bg, ct_term);
   options_toggle(ct_win, ct_bg, ct_term);
}

static void
_cb_ct_about(void *data __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   controls_toggle(ct_win, ct_bg, ct_term);
   about_toggle(ct_win, ct_bg, ct_term);
}

static void
_cb_mouse_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *ev __UNUSED__)
{
   controls_toggle(saved_win, saved_bg, data);
}

void
controls_toggle(Evas_Object *win, Evas_Object *bg, Evas_Object *term)
{
   Evas_Object *o;

   saved_win = win;
   saved_bg = bg;
   if (!ct_out)
     {
        if (options_active_get())
          {
             options_toggle(win, bg, term);
             return;
          }
     }
   if (!ct_frame)
     {
        ct_frame = o = elm_frame_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, "Controls");

        ct_box = o = elm_box_add(win);
        elm_object_content_set(ct_frame, o);
        evas_object_show(o);
        
        o = elm_button_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, "Copy");
        elm_box_pack_end(ct_box, o);
        evas_object_show(o);
        evas_object_smart_callback_add(o, "clicked", _cb_ct_copy, term);
        
        o = elm_button_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, "Paste");
        elm_box_pack_end(ct_box, o);
        evas_object_show(o);
        evas_object_smart_callback_add(o, "clicked", _cb_ct_paste, term);
        
        o = elm_separator_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
        elm_separator_horizontal_set(o, EINA_TRUE);
        elm_box_pack_end(ct_box, o);
        evas_object_show(o);
        
        o = elm_button_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, "Options");
        elm_box_pack_end(ct_box, o);
        evas_object_show(o);
        evas_object_smart_callback_add(o, "clicked", _cb_ct_options, NULL);
        
        o = elm_separator_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
        elm_separator_horizontal_set(o, EINA_TRUE);
        elm_box_pack_end(ct_box, o);
        evas_object_show(o);
        
        o = elm_button_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, "About");
        elm_box_pack_end(ct_box, o);
        evas_object_show(o);
        evas_object_smart_callback_add(o, "clicked", _cb_ct_about, NULL);
        
        background_controls_set(bg, ct_frame);
        evas_object_show(ct_frame);
     }
   if (!ct_out)
     {
        ct_over = o = evas_object_rectangle_add(evas_object_evas_get(win));
        evas_object_color_set(o, 0, 0, 0, 0);
        background_dismiss_set(bg, o);
        evas_object_show(o);
        evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                       _cb_mouse_down, term);
        
        ct_win = win;
        ct_bg = bg;
        ct_term = term;
        background_controls_show_emit(bg);
        ct_out = EINA_TRUE;
        elm_object_focus_set(ct_frame, EINA_TRUE);
        if (ct_del_timer)
          {
             ecore_timer_del(ct_del_timer);
             ct_del_timer = NULL;
          }
     }
   else
     {
        evas_object_del(ct_over);
        ct_over = NULL;

        background_controls_hide_emit(bg);
        ct_out = EINA_FALSE;
        elm_object_focus_set(ct_frame, EINA_FALSE);
        elm_object_focus_set(term, EINA_TRUE);
        if (ct_del_timer) ecore_timer_del(ct_del_timer);
        ct_del_timer = ecore_timer_add(10.0, _cb_ct_del_delay, NULL);
     }
}
