#include "private.h"

#include <Elementary.h>
#include "controls.h"
#include "options.h"
#include "about.h"
#include "termio.h"
#include "main.h"

static Evas_Object *ct_frame = NULL, *ct_boxh = NULL, *ct_boxv = NULL;
static Evas_Object *ct_box = NULL, *ct_box2 = NULL, *ct_box3 = NULL, *ct_over = NULL;
static Eina_Bool ct_out = EINA_FALSE;
static Ecore_Timer *ct_del_timer = NULL;
static Evas_Object *ct_win = NULL, *ct_bg = NULL, *ct_term = NULL;
static void (*ct_donecb) (void *data) = NULL;
static void *ct_donedata = NULL;

static void
_cb_sel_on(void *data EINA_UNUSED, Evas_Object *term EINA_UNUSED, void *ev EINA_UNUSED)
{
   Evas_Object *bt_copy = evas_object_data_get(ct_frame, "bt_copy");
   if (bt_copy)
     elm_object_disabled_set(bt_copy, EINA_FALSE);
}

static void
_cb_sel_off(void *data EINA_UNUSED, Evas_Object *term EINA_UNUSED, void *ev EINA_UNUSED)
{
   Evas_Object *bt_copy = evas_object_data_get(ct_frame, "bt_copy");
   if (bt_copy)
     elm_object_disabled_set(bt_copy, EINA_TRUE);
}

static Eina_Bool
_cb_ct_del_delay(void *data EINA_UNUSED)
{
   if (ct_over)
     {
        evas_object_del(ct_over);
        ct_over = NULL;
     }
   if (ct_frame)
     {
        evas_object_del(ct_frame);
        ct_frame = NULL;
     }
   ct_del_timer = NULL;
   elm_cache_all_flush();
   return EINA_FALSE;
}

static void
_cb_ct_copy(void *data EINA_UNUSED,
            Evas_Object *obj EINA_UNUSED,
            void *event EINA_UNUSED)
{
   termio_take_selection(ct_term, ELM_SEL_TYPE_CLIPBOARD);
   controls_toggle(ct_win, ct_bg, ct_term, ct_donecb, ct_donedata);
}

static void
_cb_ct_paste(void *data EINA_UNUSED,
             Evas_Object *obj EINA_UNUSED,
             void *event EINA_UNUSED)
{
   termio_paste_selection(ct_term, ELM_SEL_TYPE_CLIPBOARD);
   controls_toggle(ct_win, ct_bg, ct_term, ct_donecb, ct_donedata);
}

static void
_cb_ct_new(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   main_new(ct_win, ct_term);
}

static void
_cb_ct_split_v(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   split_vertically(ct_win, ct_term, NULL);
}

static void
_cb_ct_split_h(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   split_horizontally(ct_win, ct_term, NULL);
}

static void
_cb_ct_miniview(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   term_miniview_toggle(termio_term_get(ct_term));
}

static void
_cb_ct_close(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   main_close(ct_win, ct_term);
}

static void
_cb_ct_options(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   controls_toggle(ct_win, ct_bg, ct_term, ct_donecb, ct_donedata);
   options_toggle(ct_win, ct_bg, ct_term, ct_donecb, ct_donedata);
}

static void
_cb_ct_about(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   controls_toggle(ct_win, ct_bg, ct_term, ct_donecb, ct_donedata);
   about_toggle(ct_win, ct_bg, ct_term, ct_donecb, ct_donedata);
}

static void
_cb_mouse_down(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *ev EINA_UNUSED)
{
   controls_toggle(ct_win, ct_bg, ct_term, ct_donecb, ct_donedata);
}

static void
_cb_frame_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *ev EINA_UNUSED)
{
   if (ct_win)
     {
        evas_object_smart_callback_del(ct_win, "selection,on", _cb_sel_on);
        evas_object_smart_callback_del(ct_win, "selection,off", _cb_sel_off);
     }
   ct_frame = NULL;
}

static void
_cb_over_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *ev EINA_UNUSED)
{
   ct_over = NULL;
}

static void
_cb_saved_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *ev EINA_UNUSED)
{
   if ((obj == ct_win) || (obj == ct_term))
     {
        if (obj == ct_term)
          {
             if (ct_out) 
               controls_toggle(ct_win, ct_bg, ct_term, ct_donecb, ct_donedata);
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

static Evas_Object *
_button_add(Evas_Object *win, const char *label, const char *icon, Evas_Smart_Cb cb, void *cbdata)
{
   Evas_Object *o, *bt;
   
   bt = o = elm_button_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   if (label) elm_object_text_set(o, label);
   evas_object_smart_callback_add(o, "clicked", cb, cbdata);
        
   if (icon)
     {
        o = elm_icon_add(win);
        evas_object_size_hint_aspect_set(o, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
        elm_icon_standard_set(o, icon);
        elm_object_part_content_set(bt, "icon", o);
        evas_object_show(o);
     }
   
   evas_object_show(bt);
   return bt;
}

static Evas_Object *
_sep_add_v(Evas_Object *win)
{
   Evas_Object *o = elm_separator_add(win);
   
   evas_object_size_hint_weight_set(o, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, 0.5, EVAS_HINT_FILL);
   elm_separator_horizontal_set(o, EINA_FALSE);
   evas_object_show(o);
   return o;
}

static Evas_Object *
_sep_add_h(Evas_Object *win)
{
   Evas_Object *o = elm_separator_add(win);
   
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_separator_horizontal_set(o, EINA_TRUE);
   evas_object_show(o);
   return o;
}

void
controls_toggle(Evas_Object *win, Evas_Object *bg, Evas_Object *term,
                void (*donecb) (void *data), void *donedata)
{
   Evas_Object *o;

   if (!ct_out)
     {
        if (options_active_get())
          {
             options_toggle(win, bg, term, ct_donecb, ct_donedata);
             return;
          }
     }
   if ((win != ct_win) && (ct_frame))
     {
        evas_object_del(ct_frame);
        ct_frame = NULL;
        ct_win = NULL;
        ct_term = NULL;
     }
   if (!ct_frame)
     {
        ct_frame = o = elm_frame_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, _("Controls"));

        ct_boxv = o = elm_box_add(win);
        elm_box_horizontal_set(o, EINA_FALSE);
        elm_object_content_set(ct_frame, o);
        evas_object_show(o);

        ct_boxh = o = elm_box_add(win);
        elm_box_pack_end(ct_boxv, o);
        elm_box_horizontal_set(o, EINA_TRUE);
        evas_object_show(o);

        ct_box = o = elm_box_add(win);
        elm_box_pack_end(ct_boxh, o);
        evas_object_show(o);

        o = _button_add(win, _("New"), "new", _cb_ct_new, NULL);
        elm_box_pack_end(ct_box, o);

        o = _sep_add_h(win);
        elm_box_pack_end(ct_box, o);

        o = _button_add(win, _("Split V"), "split-h", _cb_ct_split_v, NULL);
        elm_box_pack_end(ct_box, o);
        o = _button_add(win, _("Split H"), "split-v", _cb_ct_split_h, NULL);
        elm_box_pack_end(ct_box, o);

        o = _sep_add_h(win);
        elm_box_pack_end(ct_box, o);

        o = _button_add(win, _("Miniview"), "mini-view", _cb_ct_miniview, NULL);
        elm_box_pack_end(ct_box, o);

        o = _sep_add_v(win);
        elm_box_pack_end(ct_boxh, o);

        ct_box2 = o = elm_box_add(win);
        elm_box_pack_end(ct_boxh, o);
        evas_object_show(o);

        o = _button_add(win, _("Copy"), "copy", _cb_ct_copy, NULL);
        evas_object_data_set(ct_frame, "bt_copy", o);
        if (!termio_selection_exists(term))
          elm_object_disabled_set(o, EINA_TRUE);
        elm_box_pack_end(ct_box2, o);

        o = _button_add(win, _("Paste"), "paste", _cb_ct_paste, NULL);
        elm_box_pack_end(ct_box2, o);

        o = _sep_add_h(win);
        elm_box_pack_end(ct_box2, o);

        o = _button_add(win, _("Settings"), "settings", _cb_ct_options, NULL);
        elm_box_pack_end(ct_box2, o);

        o = _sep_add_h(win);
        elm_box_pack_end(ct_box2, o);

        o = _button_add(win, _("About"), "about", _cb_ct_about, NULL);
        elm_box_pack_end(ct_box2, o);

        o = _sep_add_h(win);
        elm_box_pack_end(ct_boxv, o);

        ct_box3 = o = elm_box_add(win);
        elm_box_pack_end(ct_boxv, o);
        evas_object_show(o);

        o = _button_add(win, _("Close Terminal"), "close", _cb_ct_close, NULL);
        elm_box_pack_end(ct_box3, o);

        evas_object_event_callback_add(ct_frame, EVAS_CALLBACK_DEL,
                                       _cb_frame_del, NULL);

        evas_object_smart_callback_add(win, "selection,on", _cb_sel_on,
                                       NULL);
        evas_object_smart_callback_add(win, "selection,off", _cb_sel_off,
                                       NULL);
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
                                       _cb_mouse_down, NULL);
        evas_object_event_callback_add(ct_over, EVAS_CALLBACK_DEL,
                                       _cb_over_del, NULL);
        
        ct_win = win;
        ct_bg = bg;
        ct_term = term;
        ct_donecb = donecb;
        ct_donedata = donedata;
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
        if (ct_donecb) ct_donecb(ct_donedata);
//        elm_object_focus_set(ct_term, EINA_TRUE);
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
