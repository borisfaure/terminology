#include "private.h"

#include <Elementary.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_behavior.h"
#include "main.h"

static Evas_Object *op_sbslider, *op_jumpcheck, *op_wordsep;

static void
_cb_op_behavior_jump_chg(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->jump_on_change = elm_check_state_get(obj);
   termio_config_update(term);
   config_save(config, NULL);
}

static void
_cb_op_behavior_cursor_blink_chg(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->disable_cursor_blink = elm_check_state_get(obj);
   termio_config_update(term);
   config_save(config, NULL);
}

static void
_cb_op_behavior_flicker_chg(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->flicker_on_key = elm_check_state_get(obj);
   termio_config_update(term);
   config_save(config, NULL);
}

static void
_cb_op_behavior_urg_bell_chg(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->urg_bell = elm_check_state_get(obj);
   config_save(config, NULL);
}

static void
_cb_op_behavior_wsep_chg(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   char *txt;

   if (config->wordsep)
     {
        eina_stringshare_del(config->wordsep);
        config->wordsep = NULL;
     }
   txt = elm_entry_markup_to_utf8(elm_object_text_get(obj));
   if (txt)
     {
        config->wordsep = eina_stringshare_add(txt);
        free(txt);
     }
   termio_config_update(term);
   config_save(config, NULL);
}

static void
_cb_op_behavior_sback_chg(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);

   config->scrollback = elm_slider_value_get(obj) + 0.5;
   termio_config_update(term);
   config_save(config, NULL);
}

void
options_behavior(Evas_Object *opbox, Evas_Object *term)
{
   Config *config = termio_config_get(term);
   Evas_Object *o;
   char *txt;

   op_jumpcheck = o = elm_check_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "Jump to bottom on change");
   elm_check_state_set(o, config->jump_on_change);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_jump_chg, term);

   o = elm_check_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "React to key press");
   elm_check_state_set(o, config->flicker_on_key);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_flicker_chg, term);

   o = elm_check_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "Disable cursor blinking");
   elm_check_state_set(o, config->disable_cursor_blink);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_cursor_blink_chg, term);
   
   o = elm_check_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "Urgent on bell");
   elm_check_state_set(o, config->urg_bell);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_urg_bell_chg, term);
   
   o = elm_separator_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_separator_horizontal_set(o, EINA_TRUE);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   
   o = elm_label_add(opbox);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "Word separators:");
   elm_box_pack_end(opbox, o);
   evas_object_show(o);

   op_wordsep = o = elm_entry_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_entry_scrollbar_policy_set(o, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
   txt = elm_entry_utf8_to_markup(config->wordsep);
   if (txt)
     {
        elm_object_text_set(o, txt);
        free(txt);
     }
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_wsep_chg, term);

   o = elm_separator_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_separator_horizontal_set(o, EINA_TRUE);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   
   o = elm_label_add(opbox);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "Scrollback lines:");
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   
   op_sbslider = o = elm_slider_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_slider_span_size_set(o, 160);
   elm_slider_unit_format_set(o, "%1.0f");
   elm_slider_indicator_format_set(o, "%1.0f");
   elm_slider_min_max_set(o, 0, 10000);
   elm_slider_value_set(o, config->scrollback);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "delay,changed",
                                  _cb_op_behavior_sback_chg, term);
   
   evas_object_size_hint_weight_set(opbox, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(opbox, EVAS_HINT_FILL, 0.0);
   evas_object_show(o);
}
