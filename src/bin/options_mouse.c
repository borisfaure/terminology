#include "private.h"

#include <Elementary.h>
#include <assert.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_mouse.h"
#include "main.h"

typedef struct tag_Mouse_Ctx {
     Config *config;
     Evas_Object *term;
     Evas_Object *sld_hide_cursor;
} Mouse_Ctx;

OPTIONS_CB(Mouse_Ctx, active_links_email, 0);
OPTIONS_CB(Mouse_Ctx, active_links_file, 0);
OPTIONS_CB(Mouse_Ctx, active_links_url, 0);
OPTIONS_CB(Mouse_Ctx, active_links_escape, 0);
OPTIONS_CB(Mouse_Ctx, active_links_color, 0);
OPTIONS_CB(Mouse_Ctx, drag_links, 0);
OPTIONS_CB(Mouse_Ctx, gravatar,  0);
OPTIONS_CB(Mouse_Ctx, mouse_over_focus, 0);
OPTIONS_CB(Mouse_Ctx, disable_focus_visuals, 1);

static void
_cb_op_helper_inline_chg(void *data,
                         Evas_Object *obj,
                         void *_event EINA_UNUSED)
{
   Mouse_Ctx *ctx = data;
   Config *config = ctx->config;
   Evas_Object *term = ctx->term;

   config->helper.inline_please = elm_check_state_get(obj);
   termio_config_update(term);
   windows_update();
   config_save(config);
}

#define CB_(CFG_, NAME_)                                       \
static void                                                    \
_cb_op_##NAME_(void *data,                                     \
               Evas_Object *obj,                               \
               void *_event EINA_UNUSED)                       \
{                                                              \
   Mouse_Ctx *ctx = data;                                      \
   Config *config = ctx->config;                               \
   Evas_Object *term = ctx->term;                              \
   char *txt;                                                  \
                                                               \
   if (config->CFG_)                                           \
     {                                                         \
        eina_stringshare_del(config->CFG_);                    \
        config->CFG_ = NULL;                                   \
     }                                                         \
   txt = elm_entry_markup_to_utf8(elm_object_text_get(obj));   \
   if (txt)                                                    \
     {                                                         \
        config->CFG_ = eina_stringshare_add(txt);              \
        free(txt);                                             \
     }                                                         \
   termio_config_update(term);                                 \
   windows_update();                                           \
   config_save(config);                                        \
}

CB_(helper.email, helper_email);
CB_(helper.url.image, helper_url_image);
CB_(helper.url.video, helper_url_video);
CB_(helper.url.general, helper_url_general);
CB_(helper.local.image, helper_local_image);
CB_(helper.local.video, helper_local_video);
CB_(helper.local.general, helper_local_general);
#undef CB_

static void
_cb_op_hide_cursor_changed(void *data,
                           Evas_Object *obj,
                           void *_event EINA_UNUSED)
{
   Mouse_Ctx *ctx = data;
   Config *config = ctx->config;

   if (elm_check_state_get(obj))
     {
        config->hide_cursor = elm_slider_value_get(ctx->sld_hide_cursor);
        elm_object_disabled_set(ctx->sld_hide_cursor, EINA_FALSE);
     }
   else
     {
        config->hide_cursor = CONFIG_CURSOR_IDLE_TIMEOUT_MAX + 1.0;
        elm_object_disabled_set(ctx->sld_hide_cursor, EINA_TRUE);
     }
   windows_update();
   config_save(config);
}

static void
_cb_hide_cursor_slider_chg(void *data,
                 Evas_Object *obj,
                 void *_event EINA_UNUSED)
{
   Mouse_Ctx *ctx = data;
   Config *config = ctx->config;
   double value = elm_slider_value_get(obj);

   if (config->hide_cursor == value)
       return;

   config->hide_cursor = value;
   windows_update();
   config_save(config);
}



static void
_parent_del_cb(void *data,
               Evas *_e EINA_UNUSED,
               Evas_Object *_obj EINA_UNUSED,
               void *_event_info EINA_UNUSED)
{
   Mouse_Ctx *ctx = data;

   free(ctx);
}

void
options_mouse(Evas_Object *opbox, Evas_Object *term)
{
   Config *config = termio_config_get(term);
   Evas_Object *o, *sc, *frame, *bx, *hbx, *lbl;
   char *txt;
   Mouse_Ctx *ctx;

   ctx = calloc(1, sizeof(*ctx));
   assert(ctx);

   ctx->config = config;
   ctx->term = term;

   frame = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Mouse"));
   elm_box_pack_end(opbox, o);
   evas_object_show(o);

   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL,
                                  _parent_del_cb, ctx);

   sc = o = elm_scroller_add(opbox);
   elm_scroller_content_min_limit(sc, EINA_TRUE, EINA_FALSE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(frame, o);
   evas_object_show(o);

   bx = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_object_content_set(sc, o);
   evas_object_show(o);

   OPTIONS_CX(_("Focus split under the Mouse"), mouse_over_focus, 0);
   OPTIONS_CX(_("Focus-related visuals"), disable_focus_visuals, 1);
   OPTIONS_SEPARATOR;

   o = elm_check_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Auto hide the mouse cursor when idle:"));
   elm_check_state_set(o, config->hide_cursor < CONFIG_CURSOR_IDLE_TIMEOUT_MAX);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_hide_cursor_changed, ctx);

   o = elm_slider_add(bx);
   ctx->sld_hide_cursor = o;
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_slider_span_size_set(o, 40);
   elm_slider_unit_format_set(o, _("%1.1f s"));
   elm_slider_indicator_format_set(o, _("%1.1f s"));
   elm_slider_min_max_set(o, 0.0, CONFIG_CURSOR_IDLE_TIMEOUT_MAX);
   elm_object_disabled_set(o, config->hide_cursor >= CONFIG_CURSOR_IDLE_TIMEOUT_MAX);
   elm_slider_value_set(o, config->hide_cursor);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "delay,changed",
                                  _cb_hide_cursor_slider_chg, ctx);

   evas_object_size_hint_weight_set(opbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(opbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(o);
   OPTIONS_SEPARATOR;

   lbl = elm_label_add(bx);
   evas_object_size_hint_weight_set(lbl, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lbl, 0.0, 0.0);
   elm_layout_text_set(lbl, NULL, _("Active Links:"));
   elm_box_pack_end(bx, lbl);
   evas_object_show(lbl);

   OPTIONS_CX(_("On emails"), active_links_email, 0);
   OPTIONS_CX(_("On file paths"), active_links_file, 0);
   OPTIONS_CX(_("On URLs"), active_links_url, 0);
   OPTIONS_CX(_("On colors"), active_links_color, 0);
   OPTIONS_CX(_("Based on escape codes"), active_links_escape, 0);
   OPTIONS_CX(_("Gravatar integration"), gravatar, 0);
   OPTIONS_SEPARATOR;
   OPTIONS_CX(_("Drag & drop links"), drag_links, 0);
   OPTIONS_SEPARATOR;

   o = elm_check_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Inline if possible"));
   elm_check_state_set(o, config->helper.inline_please);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_helper_inline_chg, ctx);

   OPTIONS_SEPARATOR;

#define HELPERS_LINE(TXT_, CFG_, NAME_)                        \
   do {                                                        \
   hbx = o = elm_box_add(opbox);                               \
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0); \
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);    \
   elm_box_horizontal_set(o, EINA_TRUE);                       \
   elm_box_pack_end(bx, o);                                    \
   evas_object_show(o);                                        \
                                                               \
   o = elm_label_add(hbx);                                     \
   evas_object_size_hint_weight_set(o, 0.0, 0.0);              \
   evas_object_size_hint_align_set(o, 0.0, 0.5);               \
   elm_object_text_set(o, TXT_);                               \
   elm_box_pack_end(hbx, o);                                   \
   evas_object_show(o);                                        \
                                                               \
   o = elm_entry_add(hbx);                                     \
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0); \
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);    \
   elm_entry_single_line_set(o, EINA_TRUE);                    \
   elm_entry_scrollable_set(o, EINA_TRUE);                     \
   elm_scroller_policy_set(o, ELM_SCROLLER_POLICY_OFF,         \
                           ELM_SCROLLER_POLICY_OFF);           \
   txt = elm_entry_utf8_to_markup(config->CFG_);               \
   if (txt)                                                    \
     {                                                         \
        elm_object_text_set(o, txt);                           \
        free(txt);                                             \
     }                                                         \
   elm_box_pack_end(hbx, o);                                   \
   evas_object_show(o);                                        \
   evas_object_smart_callback_add(o, "changed",                \
                                  _cb_op_##NAME_, ctx);        \
   } while(0)

   HELPERS_LINE(_("E-mail:"), helper.email, helper_email);

   OPTIONS_SEPARATOR;

   HELPERS_LINE(_("URL (Images):"), helper.url.image,   helper_url_image);
   HELPERS_LINE(_("URL (Video):"),  helper.url.video,   helper_url_video);
   HELPERS_LINE(_("URL (All):"),    helper.url.general, helper_url_general);

   OPTIONS_SEPARATOR;
   HELPERS_LINE(_("Local (Images):"), helper.local.image,   helper_local_image);
   HELPERS_LINE(_("Local (Video):"),  helper.local.video,   helper_local_video);
   HELPERS_LINE(_("Local (All):"),    helper.local.general, helper_local_general);

#undef HELPERS_LINE
}
