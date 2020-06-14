#include "private.h"

#include <Elementary.h>
#include <assert.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_mouse.h"
#include "main.h"

typedef struct _Mouse_Ctx {
     Evas_Object *term;
     Config *config;
} Mouse_Ctx;

OPTIONS_CB(Mouse_Ctx, active_links_email, 0);
OPTIONS_CB(Mouse_Ctx, active_links_file, 0);
OPTIONS_CB(Mouse_Ctx, active_links_url, 0);
OPTIONS_CB(Mouse_Ctx, active_links_escape, 0);
OPTIONS_CB(Mouse_Ctx, active_links_color, 0);
OPTIONS_CB(Mouse_Ctx, drag_links, 0);
OPTIONS_CB(Mouse_Ctx, gravatar,  0);

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

#define _CB(_CFG, _NAME)                                       \
static void                                                    \
_cb_op_##_NAME(void *data,                                     \
               Evas_Object *obj,                               \
               void *_event EINA_UNUSED)                       \
{                                                              \
   Mouse_Ctx *ctx = data;                                      \
   Config *config = ctx->config;                               \
   Evas_Object *term = ctx->term;                              \
   char *txt;                                                  \
                                                               \
   if (config->_CFG)                                           \
     {                                                         \
        eina_stringshare_del(config->_CFG);                    \
        config->_CFG = NULL;                                   \
     }                                                         \
   txt = elm_entry_markup_to_utf8(elm_object_text_get(obj));   \
   if (txt)                                                    \
     {                                                         \
        config->_CFG = eina_stringshare_add(txt);              \
        free(txt);                                             \
     }                                                         \
   termio_config_update(term);                                 \
   windows_update();                                           \
   config_save(config);                                        \
}

_CB(helper.email, helper_email);
_CB(helper.url.image, helper_url_image);
_CB(helper.url.video, helper_url_video);
_CB(helper.url.general, helper_url_general);
_CB(helper.local.image, helper_local_image);
_CB(helper.local.video, helper_local_video);
_CB(helper.local.general, helper_local_general);
#undef _CB


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

   OPTIONS_SEPARATOR;
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

#define HELPERS_LINE(_TXT, _CFG, _NAME)                        \
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
   elm_object_text_set(o, _TXT);                               \
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
   txt = elm_entry_utf8_to_markup(config->_CFG);               \
   if (txt)                                                    \
     {                                                         \
        elm_object_text_set(o, txt);                           \
        free(txt);                                             \
     }                                                         \
   elm_box_pack_end(hbx, o);                                   \
   evas_object_show(o);                                        \
   evas_object_smart_callback_add(o, "changed",                \
                                  _cb_op_##_NAME, ctx);        \
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
