#include "private.h"

#include <Elementary.h>
#include <assert.h>
#include "controls.h"
#include "options.h"
#include "about.h"
#include "termio.h"
#include "main.h"

static Eina_Hash *controls = NULL;

typedef struct tag_Controls_Ctx {
     Evas_Object *frame;
     Evas_Object *group_input_cx;
     Evas_Object *over;
     Evas_Object *win;
     Win *wn;
     Evas_Object *base;
     Evas_Object *bg;
     Evas_Object *term;
     Evas_Object *new_btn;
     void (*donecb) (void *data);
     void *donedata;
} Controls_Ctx;


/* /!\ can free ctx */
static void
controls_hide(Controls_Ctx *ctx, Eina_Bool call_cb);

static void
_cb_sel_on(void *data,
           Evas_Object *_term EINA_UNUSED,
           void *_ev EINA_UNUSED)
{
   Controls_Ctx *ctx = data;
   Evas_Object *bt_copy = evas_object_data_get(ctx->frame, "bt_copy");
   if (bt_copy)
     elm_object_disabled_set(bt_copy, EINA_FALSE);
}

static void
_cb_sel_off(void *data,
            Evas_Object *_term EINA_UNUSED,
            void *_ev EINA_UNUSED)
{
   Controls_Ctx *ctx = data;
   Evas_Object *bt_copy = evas_object_data_get(ctx->frame, "bt_copy");
   if (bt_copy)
     elm_object_disabled_set(bt_copy, EINA_TRUE);
}

static void
_cb_hidden(void *data,
           Evas *_e EINA_UNUSED,
           Evas_Object *_obj EINA_UNUSED,
           void *_ev EINA_UNUSED)
{
   Controls_Ctx *ctx = data;
   Evas_Object *frame = ctx->frame;
   Evas_Object *o;

   evas_object_event_callback_del_full(ctx->frame, EVAS_CALLBACK_HIDE,
                                       _cb_hidden, ctx);
   evas_object_del(frame);

   o = elm_layout_content_get(ctx->base, "terminology.controls");
   if (o)
     {
        elm_layout_content_unset(ctx->base, "terminology.controls");
     }

   elm_cache_all_flush();
   free(ctx);
}

static void
_cb_ct_copy(void *data,
            Evas_Object *_obj EINA_UNUSED,
            void *_event EINA_UNUSED)
{
   Controls_Ctx *ctx = data;
   Evas_Object *term = ctx->term;

   controls_hide(ctx, EINA_TRUE);

   termio_take_selection(term, ELM_SEL_TYPE_CLIPBOARD);
}

static void
_cb_ct_paste(void *data,
             Evas_Object *_obj EINA_UNUSED,
             void *_event EINA_UNUSED)
{
   Controls_Ctx *ctx = data;
   Evas_Object *term = ctx->term;

   controls_hide(ctx, EINA_TRUE);

   termio_paste_selection(term, ELM_SEL_TYPE_CLIPBOARD);
}

static void
_cb_ct_new(void *data,
           Evas_Object *_obj EINA_UNUSED,
           void *_event EINA_UNUSED)
{
   Controls_Ctx *ctx = data;
   Evas_Object *term = ctx->term;

   controls_hide(ctx, EINA_TRUE);

   main_new(term);
}

static void
_cb_ct_split_v(void *data,
               Evas_Object *_obj EINA_UNUSED,
               void *_event EINA_UNUSED)
{
   Controls_Ctx *ctx = data;
   Evas_Object *term = ctx->term;


   controls_hide(ctx, EINA_TRUE);

   split_vertically(term, NULL);
}

static void
_cb_ct_split_h(void *data,
               Evas_Object *_obj EINA_UNUSED,
               void *_event EINA_UNUSED)
{
   Controls_Ctx *ctx = data;
   Evas_Object *term = ctx->term;

   controls_hide(ctx, EINA_TRUE);

   split_horizontally(term, NULL);
}

static void
_cb_ct_miniview(void *data,
                Evas_Object *_obj EINA_UNUSED,
                void *_event EINA_UNUSED)
{
   Controls_Ctx *ctx = data;
   term_miniview_toggle(termio_term_get(ctx->term));
}

static void
_cb_ct_set_title(void *data,
                 Evas_Object *_obj EINA_UNUSED,
                 void *_event EINA_UNUSED)
{
   Controls_Ctx *ctx = data;
   Evas_Object *term = ctx->term;

   controls_hide(ctx, EINA_TRUE);

   term_set_title(termio_term_get(term));
}

static void
_cb_ct_close(void *data,
             Evas_Object *_obj EINA_UNUSED,
             void *_event EINA_UNUSED)
{
   Controls_Ctx *ctx = data;
   Evas_Object *term = ctx->term;
   Evas_Object *win = ctx->win;

   controls_hide(ctx, EINA_TRUE);

   term_close(win, term, EINA_FALSE);
}

static void
_on_sub_done(void *data)
{
   Controls_Ctx *ctx = data;

   if (ctx->donecb)
     {
        ctx->donecb(ctx->donedata);
        ctx->donecb = NULL;

        eina_hash_del(controls, &ctx->win, ctx);

        if (evas_object_visible_get(ctx->frame))
          evas_object_event_callback_add(ctx->frame, EVAS_CALLBACK_HIDE,
                                         _cb_hidden, ctx);
        else
          _cb_hidden(ctx, NULL, NULL, NULL);
     }
}

static void
_cb_ct_options(void *data,
               Evas_Object *_obj EINA_UNUSED,
               void *_event EINA_UNUSED)
{
   Controls_Ctx *ctx = data;

   options_show(ctx->win, ctx->base, ctx->bg, ctx->term, _on_sub_done, ctx);
   controls_hide(ctx, EINA_FALSE);
}


static void
_cb_ct_about(void *data,
             Evas_Object *_obj EINA_UNUSED,
             void *_event EINA_UNUSED)
{
   Controls_Ctx *ctx = data;

   about_show(ctx->win, ctx->base, ctx->term, _on_sub_done, ctx);
   controls_hide(ctx, EINA_FALSE);
}

static void
_cb_group_input_changed(void *data, Evas_Object *obj EINA_UNUSED,
                        void *_event EINA_UNUSED)
{
   Controls_Ctx *ctx = data;
   Win *wn = ctx->wn;
   Config *config = termio_config_get(ctx->term);

   controls_hide(ctx, EINA_TRUE);
   if (config && config->group_all)
     win_toggle_all_group(wn);
   else
     win_toggle_visible_group(wn);
}

static void
_cb_mouse_down(void *data,
               Evas *_e EINA_UNUSED,
               Evas_Object *_obj EINA_UNUSED,
               void *_ev EINA_UNUSED)
{
   Controls_Ctx *ctx = data;

   controls_hide(ctx, EINA_TRUE);
}

static void
_cb_saved_del(void *data,
              Evas *_e EINA_UNUSED,
              Evas_Object *obj,
              void *_ev EINA_UNUSED)
{
   Controls_Ctx *ctx = data;

   if (obj == ctx->win)
     ctx->win = NULL;
   else if (obj == ctx->term)
     ctx->term = NULL;

   controls_hide(ctx, EINA_FALSE);
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

static void
controls_hide(Controls_Ctx *ctx, Eina_Bool call_cb)
{
   elm_object_focus_set(ctx->frame, EINA_FALSE);
   if (ctx->win)
     {
        evas_object_event_callback_del(ctx->win, EVAS_CALLBACK_DEL, _cb_saved_del);
        evas_object_smart_callback_del(ctx->win, "selection,on", _cb_sel_on);
        evas_object_smart_callback_del(ctx->win, "selection,off", _cb_sel_off);
     }
   if (ctx->term)
     {
        evas_object_event_callback_del(ctx->term, EVAS_CALLBACK_DEL, _cb_saved_del);
        elm_layout_signal_emit(ctx->base, "controls,hide", "terminology");
     }

   if (ctx->over)
     {
        evas_object_del(ctx->over);
     }

   if (call_cb)
     {
        _on_sub_done(ctx);
     }
}


void
controls_show(Evas_Object *win, Evas_Object *base, Evas_Object *bg,
              Evas_Object *term, void (*donecb) (void *data), void *donedata)
{
   Evas_Object *o;
   Evas_Object *ct_boxh, *ct_boxv, *ct_box, *ct_box2;
   Controls_Ctx *ctx;

   ctx = eina_hash_find(controls, &win);
   if (ctx)
     {
        donecb(donedata);
     }
   else
     {
        ctx = malloc(sizeof(*ctx));
        assert(ctx);
        ctx->win = win;
        ctx->wn = win_evas_object_to_win(win);
        ctx->base = base;
        ctx->bg = bg;
        ctx->term = term;
        ctx->donecb = donecb;
        ctx->donedata = donedata;

        eina_hash_add(controls, &win, ctx);

        ctx->frame = o = elm_frame_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, _("Controls"));

        ct_boxv = o = elm_box_add(win);
        elm_box_horizontal_set(o, EINA_FALSE);
        elm_object_content_set(ctx->frame, o);
        evas_object_show(o);

        ct_boxh = o = elm_box_add(win);
        elm_box_pack_end(ct_boxv, o);
        elm_box_horizontal_set(o, EINA_TRUE);
        evas_object_show(o);

        ct_box = o = elm_box_add(win);
        elm_box_pack_end(ct_boxh, o);
        evas_object_show(o);

        o = _button_add(win, _("New"), "window-new",
                        _cb_ct_new, ctx);
        elm_box_pack_end(ct_box, o);
        ctx->new_btn=o;

        o = _sep_add_h(win);
        elm_box_pack_end(ct_box, o);

        o = _button_add(win, _("Split V"), "split-v",
                        _cb_ct_split_v, ctx);
        elm_box_pack_end(ct_box, o);

        o = _button_add(win, _("Split H"), "split-h",
                        _cb_ct_split_h, ctx);
        elm_box_pack_end(ct_box, o);

        o = _sep_add_h(win);
        elm_box_pack_end(ct_box, o);

        o = _button_add(win, _("Miniview"), "view-restore",
                        _cb_ct_miniview, ctx);
        elm_box_pack_end(ct_box, o);

        o = _sep_add_h(win);
        elm_box_pack_end(ct_box, o);

        o = _button_add(win, _("Set title"), "format-text-underline",
                        _cb_ct_set_title, ctx);
        elm_box_pack_end(ct_box, o);

        o = _sep_add_v(win);
        elm_box_pack_end(ct_boxh, o);

        ct_box2 = o = elm_box_add(win);
        elm_box_pack_end(ct_boxh, o);
        evas_object_show(o);

        o = _button_add(win, _("Copy"), "edit-copy", _cb_ct_copy, ctx);
        evas_object_data_set(ctx->frame, "bt_copy", o);
        if (!termio_selection_exists(term))
          elm_object_disabled_set(o, EINA_TRUE);
        elm_box_pack_end(ct_box2, o);

        o = _button_add(win, _("Paste"), "edit-paste", _cb_ct_paste, ctx);
        elm_box_pack_end(ct_box2, o);

        o = _sep_add_h(win);
        elm_box_pack_end(ct_box2, o);

        o = _button_add(win, _("Settings"), "preferences-desktop", _cb_ct_options, ctx);
        elm_box_pack_end(ct_box2, o);

        o = _sep_add_h(win);
        elm_box_pack_end(ct_box2, o);

        o = _button_add(win, _("About"), "help-about", _cb_ct_about, ctx);
        elm_box_pack_end(ct_box2, o);

        o = _sep_add_h(win);
        elm_box_pack_end(ct_boxv, o);

        o = elm_check_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
        elm_object_text_set(o, _("Grouped input"));
        elm_object_disabled_set(o, win_has_single_child(ctx->wn));
        elm_check_state_set(o, win_is_group_input(ctx->wn));
        elm_box_pack_end(ct_boxv, o);
        evas_object_show(o);
        ctx->group_input_cx = o;
        evas_object_smart_callback_add(o, "changed",
                                       _cb_group_input_changed, ctx);

        o = _sep_add_h(win);
        elm_box_pack_end(ct_boxv, o);

        o = _button_add(win, _("Close Terminal"), "window-close", _cb_ct_close, ctx);
        elm_box_pack_end(ct_boxv, o);

        evas_object_smart_callback_add(win, "selection,on", _cb_sel_on,
                                       ctx);
        evas_object_smart_callback_add(win, "selection,off", _cb_sel_off,
                                       ctx);

        elm_layout_content_set(base, "terminology.controls", ctx->frame);
        evas_object_show(ctx->frame);
        ctx->over = o = evas_object_rectangle_add(evas_object_evas_get(win));
        evas_object_color_set(o, 0, 0, 0, 0);
        elm_layout_content_set(base, "terminology.dismiss", o);
        evas_object_show(o);
        evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                       _cb_mouse_down, ctx);

        elm_layout_signal_emit(base, "controls,show", "terminology");
        evas_object_event_callback_add(ctx->win, EVAS_CALLBACK_DEL, _cb_saved_del, ctx);
        evas_object_event_callback_add(ctx->term, EVAS_CALLBACK_DEL, _cb_saved_del, ctx);
     }
   elm_object_focus_set(ctx->new_btn, EINA_TRUE);
}

void
controls_init(void)
{
   controls = eina_hash_pointer_new(NULL);
}

void
controls_shutdown(void)
{
   eina_hash_free(controls);
   controls = NULL;
}
