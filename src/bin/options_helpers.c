#include "private.h"

#include <Elementary.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_helpers.h"
#include "main.h"

static void
_cb_op_helper_inline_chg(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->helper.inline_please = elm_check_state_get(obj);
   config_save(config, NULL);
}

static void
_cb_op_helper_email_chg(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   char *txt;

   if (config->helper.email)
     {
        eina_stringshare_del(config->helper.email);
        config->helper.email = NULL;
     }
   txt = elm_entry_markup_to_utf8(elm_object_text_get(obj));
   if (txt)
     {
        config->helper.email = eina_stringshare_add(txt);
        free(txt);
     }
   config_save(config, NULL);
}

static void
_cb_op_helper_url_image_chg(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   char *txt;

   if (config->helper.url.image)
     {
        eina_stringshare_del(config->helper.url.image);
        config->helper.url.image = NULL;
     }
   txt = elm_entry_markup_to_utf8(elm_object_text_get(obj));
   if (txt)
     {
        config->helper.url.image = eina_stringshare_add(txt);
        free(txt);
     }
   config_save(config, NULL);
}

static void
_cb_op_helper_url_video_chg(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   char *txt;

   if (config->helper.url.video)
     {
        eina_stringshare_del(config->helper.url.video);
        config->helper.url.video = NULL;
     }
   txt = elm_entry_markup_to_utf8(elm_object_text_get(obj));
   if (txt)
     {
        config->helper.url.video = eina_stringshare_add(txt);
        free(txt);
     }
   config_save(config, NULL);
}

static void
_cb_op_helper_url_general_chg(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   char *txt;

   if (config->helper.url.general)
     {
        eina_stringshare_del(config->helper.url.general);
        config->helper.url.general = NULL;
     }
   txt = elm_entry_markup_to_utf8(elm_object_text_get(obj));
   if (txt)
     {
        config->helper.url.general = eina_stringshare_add(txt);
        free(txt);
     }
   config_save(config, NULL);
}

static void
_cb_op_helper_local_image_chg(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   char *txt;

   if (config->helper.local.image)
     {
        eina_stringshare_del(config->helper.local.image);
        config->helper.local.image = NULL;
     }
   txt = elm_entry_markup_to_utf8(elm_object_text_get(obj));
   if (txt)
     {
        config->helper.local.image = eina_stringshare_add(txt);
        free(txt);
     }
   config_save(config, NULL);
}

static void
_cb_op_helper_local_video_chg(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   char *txt;

   if (config->helper.local.video)
     {
        eina_stringshare_del(config->helper.local.video);
        config->helper.local.video = NULL;
     }
   txt = elm_entry_markup_to_utf8(elm_object_text_get(obj));
   if (txt)
     {
        config->helper.local.video = eina_stringshare_add(txt);
        free(txt);
     }
   config_save(config, NULL);
}

static void
_cb_op_helper_local_general_chg(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   char *txt;

   if (config->helper.local.general)
     {
        eina_stringshare_del(config->helper.local.general);
        config->helper.local.general = NULL;
     }
   txt = elm_entry_markup_to_utf8(elm_object_text_get(obj));
   if (txt)
     {
        config->helper.local.general = eina_stringshare_add(txt);
        free(txt);
     }
   config_save(config, NULL);
}

void
options_helpers(Evas_Object *opbox, Evas_Object *term)
{
   Config *config = termio_config_get(term);
   Evas_Object *o, *bx, *sc, *fr, *bx0;
   char *txt;

   fr = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, "Helpers");
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   
   bx0 = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(fr, o);
   evas_object_show(o);
   
   o = elm_check_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "Inline if possible");
   elm_check_state_set(o, config->helper.inline_please);
   elm_box_pack_end(bx0, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_helper_inline_chg, term);

   o = elm_separator_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_separator_horizontal_set(o, EINA_TRUE);
   elm_box_pack_end(bx0, o);
   evas_object_show(o);
   
   sc = o = elm_scroller_add(opbox);
   elm_scroller_content_min_limit(sc, EINA_TRUE, EINA_FALSE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(bx0, o);
   evas_object_show(o);

   bx = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_object_content_set(sc, o);
   evas_object_show(o);
   
   o = elm_label_add(bx);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "E-mail:");
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_entry_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_scroller_policy_set(o, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
   txt = elm_entry_utf8_to_markup(config->helper.email);
   if (txt)
     {
        elm_object_text_set(o, txt);
        free(txt);
     }
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_helper_email_chg, term);

   o = elm_label_add(bx);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "URL (Images):");
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_entry_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_scroller_policy_set(o, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
   txt = elm_entry_utf8_to_markup(config->helper.url.image);
   if (txt)
     {
        elm_object_text_set(o, txt);
        free(txt);
     }
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_helper_url_image_chg, term);
   
   o = elm_label_add(bx);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "URL (Video):");
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_entry_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_scroller_policy_set(o, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
   txt = elm_entry_utf8_to_markup(config->helper.url.video);
   if (txt)
     {
        elm_object_text_set(o, txt);
        free(txt);
     }
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_helper_url_video_chg, term);
   
   o = elm_label_add(bx);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "URL (All):");
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_entry_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_scroller_policy_set(o, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
   txt = elm_entry_utf8_to_markup(config->helper.url.general);
   if (txt)
     {
        elm_object_text_set(o, txt);
        free(txt);
     }
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_helper_url_general_chg, term);

   o = elm_label_add(bx);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "Local (Images):");
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_entry_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_scroller_policy_set(o, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
   txt = elm_entry_utf8_to_markup(config->helper.local.image);
   if (txt)
     {
        elm_object_text_set(o, txt);
        free(txt);
     }
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_helper_local_image_chg, term);
   
   o = elm_label_add(bx);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "Local (Video):");
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_entry_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_scroller_policy_set(o, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
   txt = elm_entry_utf8_to_markup(config->helper.local.video);
   if (txt)
     {
        elm_object_text_set(o, txt);
        free(txt);
     }
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_helper_local_video_chg, term);
   
   o = elm_label_add(bx);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "Local (All):");
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_entry_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_scroller_policy_set(o, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
   txt = elm_entry_utf8_to_markup(config->helper.local.general);
   if (txt)
     {
        elm_object_text_set(o, txt);
        free(txt);
     }
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_helper_local_general_chg, term);
   
   evas_object_size_hint_weight_set(opbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(opbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(o);
}
