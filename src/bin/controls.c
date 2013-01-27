#include "private.h"

#include <Elementary.h>
#include "controls.h"
#include "options.h"
#include "about.h"
#include "termio.h"
#include "main.h"

static Evas_Object *ct_frame = NULL, *ct_boxh = NULL, *ct_box = NULL;
static Evas_Object *ct_box2 = NULL, *ct_over = NULL;
static Eina_Bool ct_out = EINA_FALSE;
static Ecore_Timer *ct_del_timer = NULL;
static Evas_Object *ct_win = NULL, *ct_bg = NULL, *ct_term = NULL;

static Eina_Bool
_cb_ct_del_delay(void *data __UNUSED__)
{
   if (ct_over)
     {
        evas_object_del(ct_over);
        ct_over = NULL;
     }
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
_cb_ct_split_v(void *data __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   main_split_v(ct_win, ct_term);
}

static void
_cb_ct_split_h(void *data __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   main_split_h(ct_win, ct_term);
}

static void
_cb_ct_close(void *data __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   main_close(ct_win, ct_term);
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
   controls_toggle(ct_win, ct_bg, ct_term);
}

static void
_cb_saved_del(void *data __UNUSED__, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *ev __UNUSED__)
{
   if ((obj == ct_win) || (obj == ct_term))
     {
        if (obj == ct_term)
          {
             if (ct_out) controls_toggle(ct_win, ct_bg, ct_term);
             ct_term = NULL;
          }
        else
          {
             if (ct_frame)
               {
                  evas_object_del(ct_frame);
                  ct_frame = NULL;
               }
             if (ct_del_timer)
               {
                  ecore_timer_del(ct_del_timer);
                  ct_del_timer = NULL;
               }
             if (ct_over)
               {
                  evas_object_del(ct_over);
                  ct_over = NULL;
               }
             evas_object_event_callback_del(ct_win, EVAS_CALLBACK_DEL, _cb_saved_del);
             ct_win = NULL;
          }
        evas_object_event_callback_del(ct_term, EVAS_CALLBACK_DEL, _cb_saved_del);
        ct_bg = NULL;
     }
}

void
controls_toggle(Evas_Object *win, Evas_Object *bg, Evas_Object *term)
{
   Evas_Object *o;

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

        ct_boxh = o = elm_box_add(win);
        elm_box_horizontal_set(o, EINA_TRUE);
        elm_object_content_set(ct_frame, o);
        evas_object_show(o);

        ct_box2 = o = elm_box_add(win);
        elm_box_pack_end(ct_boxh, o);
        evas_object_show(o);
        
        o = elm_button_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, "|");
        elm_box_pack_end(ct_box2, o);
        evas_object_show(o);
        evas_object_smart_callback_add(o, "clicked", _cb_ct_split_v, NULL);
        
        o = elm_button_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, "-");
        elm_box_pack_end(ct_box2, o);
        evas_object_show(o);
        evas_object_smart_callback_add(o, "clicked", _cb_ct_split_h, NULL);
        
        o = elm_button_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, "X");
        elm_box_pack_end(ct_box2, o);
        evas_object_show(o);
        evas_object_smart_callback_add(o, "clicked", _cb_ct_close, NULL);
        
        o = elm_separator_add(win);
        evas_object_size_hint_weight_set(o, 0.0, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, 0.5, EVAS_HINT_FILL);
        elm_separator_horizontal_set(o, EINA_FALSE);
        elm_box_pack_end(ct_boxh, o);
        evas_object_show(o);
        
        ct_box = o = elm_box_add(win);
        elm_box_pack_end(ct_boxh, o);
        evas_object_show(o);
        
        o = elm_button_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, "Copy");
        elm_box_pack_end(ct_box, o);
        evas_object_show(o);
        evas_object_smart_callback_add(o, "clicked", _cb_ct_copy, term);
        
        o = elm_button_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
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
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
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
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, "About");
        elm_box_pack_end(ct_box, o);
        evas_object_show(o);
        evas_object_smart_callback_add(o, "clicked", _cb_ct_about, NULL);
        
     }
   if (!ct_out)
     {
        edje_object_part_swallow(bg, "terminology.controls", ct_frame);
        evas_object_show(ct_frame);
        ct_over = o = evas_object_rectangle_add(evas_object_evas_get(win));
        evas_object_color_set(o, 0, 0, 0, 0);
        edje_object_part_swallow(bg, "terminology.dismiss", o);
        evas_object_show(o);
        evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                       _cb_mouse_down, term);
        
        ct_win = win;
        ct_bg = bg;
        ct_term = term;
        edje_object_signal_emit(bg, "controls,show", "terminology");
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
        if (ct_over)
          {
             evas_object_del(ct_over);
             ct_over = NULL;
          }
        edje_object_signal_emit(ct_bg, "controls,hide", "terminology");
        ct_out = EINA_FALSE;
        elm_object_focus_set(ct_frame, EINA_FALSE);
        elm_object_focus_set(ct_term, EINA_TRUE);
        if (ct_del_timer) ecore_timer_del(ct_del_timer);
        ct_del_timer = ecore_timer_add(10.0, _cb_ct_del_delay, NULL);
     }
   if (ct_win)
     {
        evas_object_event_callback_del(ct_win, EVAS_CALLBACK_DEL, _cb_saved_del);
        evas_object_event_callback_del(ct_term, EVAS_CALLBACK_DEL, _cb_saved_del);
     }
   if (ct_out)
     {
        evas_object_event_callback_add(ct_win, EVAS_CALLBACK_DEL, _cb_saved_del, NULL);
        evas_object_event_callback_add(ct_term, EVAS_CALLBACK_DEL, _cb_saved_del, NULL);
     }
}
