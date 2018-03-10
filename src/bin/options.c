#include "private.h"

#include <Elementary.h>
#include <assert.h>
#include "options.h"
#include "options_font.h"
#include "options_theme.h"
#include "options_background.h"
#include "options_colors.h"
#include "options_video.h"
#include "options_behavior.h"
#include "options_keys.h"
#include "options_helpers.h"
#include "options_elm.h"
#include "config.h"
#include "termio.h"


enum option_mode {
     OPTION_NONE = 0,
     OPTION_BEHAVIOR,
     OPTION_FONT,
     OPTION_THEME,
     OPTION_BACKGROUND,
     OPTION_COLORS,
     OPTION_VIDEO,
     OPTION_KEYS,
     OPTION_HELPERS,
     OPTION_ELM,

     OPTIONS_MODE_NB
};

typedef struct _Options_Ctx {
     enum option_mode mode;
     Evas_Object *frame;
     Evas_Object *toolbar;
     Evas_Object *opbox;
     Evas_Object *over;
     Evas_Object *win;
     Evas_Object *base;
     Evas_Object *bg;
     Evas_Object *term;
     Config *config;
     void (*donecb) (void *data);
     void *donedata;
     struct _Options_Ctx *modes[OPTIONS_MODE_NB];
} Options_Ctx;

static void
_cb_op(void *data,
       Evas_Object *_obj EINA_UNUSED,
       void *_event EINA_UNUSED)
{
   Options_Ctx **pctx = data;
   Options_Ctx *ctx = *pctx;
   enum option_mode mode = pctx - ctx->modes;

   if (mode == ctx->mode)
     return;

   ctx->mode = mode;

   edje_object_signal_emit(ctx->base, "optdetails,hide", "terminology");
}

static void
_cb_op_tmp_chg(void *data, Evas_Object *obj, void *_event EINA_UNUSED)
{
   Options_Ctx *ctx = data;
   Config *config = ctx->config;

   config->temporary = elm_check_state_get(obj);
}

static Eina_Bool
_cb_op_del_delay(void *data)
{
   Options_Ctx *ctx = data;

   evas_object_del(ctx->opbox);
   evas_object_del(ctx->frame);

   free(ctx);
   elm_cache_all_flush();
   return EINA_FALSE;
}

static void
_cb_opdt_hide_done(void *data,
                   Evas_Object *_obj EINA_UNUSED,
                   const char *_sig EINA_UNUSED,
                   const char *_src EINA_UNUSED)
{
   Options_Ctx *ctx = data;

   elm_box_clear(ctx->opbox);
   switch (ctx->mode)
     {
      case OPTION_NONE:      break;
      case OPTION_BEHAVIOR:  options_behavior(ctx->opbox, ctx->term); break;
      case OPTION_FONT:      options_font(ctx->opbox, ctx->term); break;
      case OPTION_THEME:     options_theme(ctx->opbox, ctx->term); break;
      case OPTION_BACKGROUND: options_background(ctx->opbox, ctx->term); break;
      case OPTION_COLORS:    options_colors(ctx->opbox, ctx->term, ctx->bg); break;
      case OPTION_VIDEO:     options_video(ctx->opbox, ctx->term); break;
      case OPTION_KEYS:      options_keys(ctx->opbox, ctx->term); break;
      case OPTION_HELPERS:   options_helpers(ctx->opbox, ctx->term); break;
      case OPTION_ELM:       options_elm(ctx->opbox, ctx->term); break;
      case OPTIONS_MODE_NB:  assert(0 && "should not occur");
     }
   edje_object_signal_emit(ctx->base, "optdetails,show", "terminology");
}

static void
_cb_opdt_hide_done2(void *data,
                    Evas_Object *_obj EINA_UNUSED,
                    const char *_sig EINA_UNUSED,
                    const char *_src EINA_UNUSED)
{
   Options_Ctx *ctx = data;

   edje_object_signal_callback_del(ctx->base, "optdetails,hide,done",
                                   "terminology",
                                   _cb_opdt_hide_done2);
   ecore_timer_add(10.0, _cb_op_del_delay, ctx);
}

static void
options_hide(Options_Ctx *ctx)
{
   edje_object_part_swallow(ctx->base, "terminology.optdetails", ctx->opbox);
   edje_object_part_swallow(ctx->base, "terminology.options", ctx->frame);
   edje_object_signal_emit(ctx->base, "optdetails,show", "terminology");
   edje_object_signal_emit(ctx->base, "options,show", "terminology");

   edje_object_signal_callback_del(ctx->base, "optdetails,hide,done",
                                   "terminology",
                                   _cb_opdt_hide_done);
   edje_object_signal_callback_add(ctx->base, "optdetails,hide,done",
                                   "terminology",
                                   _cb_opdt_hide_done2, ctx);
   elm_object_focus_set(ctx->frame, EINA_FALSE);
   elm_object_focus_set(ctx->opbox, EINA_FALSE);
   elm_object_focus_set(ctx->toolbar, EINA_FALSE);

   evas_object_del(ctx->over);
   ctx->over = NULL;

   edje_object_signal_emit(ctx->base, "options,hide", "terminology");
   edje_object_signal_emit(ctx->base, "optdetails,hide", "terminology");

   if (ctx->donecb)
     ctx->donecb(ctx->donedata);
}

static void
_cb_mouse_down(void *data,
               Evas *_e EINA_UNUSED,
               Evas_Object *_obj EINA_UNUSED,
               void *_ev EINA_UNUSED)
{
   Options_Ctx *ctx = data;

   options_hide(ctx);
}


void
options_show(Evas_Object *win, Evas_Object *base, Evas_Object *bg, Evas_Object *term,
               void (*donecb) (void *data), void *donedata)
{
   Evas_Object *o, *op_box, *op_tbox;
   Options_Ctx *ctx;
   int i = 0;

   Elm_Object_Item *it_fn;
   Config *config = termio_config_get(term);

   if (!config) return;

   ctx = malloc(sizeof(*ctx));
   assert(ctx);
   ctx->mode = OPTION_NONE;
   ctx->win = win;
   ctx->base = base;
   ctx->bg = bg;
   ctx->term = term;
   ctx->donecb = donecb;
   ctx->donedata = donedata;
   ctx->config = config;

   for (i = 0; i < OPTIONS_MODE_NB; i++)
     ctx->modes[i] = ctx;

   ctx->opbox = o = elm_box_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   edje_object_part_swallow(ctx->base, "terminology.optdetails", o);
   evas_object_show(o);

   ctx->frame = o = elm_frame_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Options"));

   op_box = o = elm_box_add(win);
   elm_box_horizontal_set(o, EINA_TRUE);
   elm_object_content_set(ctx->frame, o);
   evas_object_show(o);

   op_tbox = o = elm_box_add(win);
   evas_object_size_hint_weight_set(o, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, 1.0, EVAS_HINT_FILL);
   elm_box_pack_end(op_box, o);
   evas_object_show(o);

   ctx->toolbar = o = elm_toolbar_add(win);
   evas_object_size_hint_weight_set(o, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, 0.5, EVAS_HINT_FILL);
   elm_toolbar_horizontal_set(o, EINA_FALSE);
   elm_object_style_set(o, "item_horizontal");
   elm_toolbar_icon_size_set(o, 16 * elm_config_scale_get());
   elm_toolbar_shrink_mode_set(o, ELM_TOOLBAR_SHRINK_SCROLL);
   elm_toolbar_select_mode_set(o, ELM_OBJECT_SELECT_MODE_ALWAYS);
   elm_toolbar_menu_parent_set(o, win);
   elm_toolbar_homogeneous_set(o, EINA_FALSE);

#define ITEM_APPEND(_icon_name, _name, _option_mode) \
   elm_toolbar_item_append(o, _icon_name, _name, _cb_op, \
                           (void*) &ctx->modes[OPTION_##_option_mode])

   it_fn = ITEM_APPEND("preferences-system", _("Behavior"), BEHAVIOR);
   ITEM_APPEND("preferences-desktop-font", _("Font"), FONT);
   ITEM_APPEND("preferences-desktop-theme", _("Theme"), THEME);
   ITEM_APPEND("preferences-desktop-background", _("Background"), BACKGROUND);
   ITEM_APPEND("preferences-desktop-theme", _("Colors"), COLORS);
   ITEM_APPEND("video-display", _("Video"), VIDEO);
   ITEM_APPEND("preferences-desktop-keyboard-shortcuts", _("Keys"), KEYS);
   ITEM_APPEND("system-run", _("Helpers"), HELPERS);
   ITEM_APPEND("preferences-color", _("Toolkit"), ELM);
#undef ITEM_APPEND

   elm_box_pack_end(op_tbox, o);
   evas_object_show(o);

   elm_toolbar_item_selected_set(it_fn, EINA_TRUE);

   o = elm_check_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 1.0);
   elm_object_text_set(o, _("Temporary"));
   elm_check_state_set(o, config->temporary);
   elm_box_pack_end(op_tbox, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed", _cb_op_tmp_chg, ctx);

   edje_object_part_swallow(base, "terminology.options", ctx->frame);
   evas_object_show(ctx->frame);

   edje_object_signal_callback_add(ctx->base, "optdetails,hide,done",
                                   "terminology",
                                   _cb_opdt_hide_done, ctx);
   ctx->over = o = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_color_set(o, 0, 0, 0, 0);
   edje_object_part_swallow(ctx->base, "terminology.dismiss", o);
   evas_object_show(o);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _cb_mouse_down, ctx);

   edje_object_signal_emit(ctx->base, "options,show", "terminology");
   elm_object_focus_set(ctx->toolbar, EINA_TRUE);
}
