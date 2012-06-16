#include <Elementary.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_font.h"
#include "main.h"

static Evas_Object *op_sbslider, *op_jumpcheck, *op_trans, *op_wordsep;

static void
_cb_op_behavior_jump_chg(void *data, Evas_Object *obj, void *event)
{
   config->jump_on_change = elm_check_state_get(obj);
   termio_config_update(data);
   config_save();
}

static void
_cb_op_behavior_trans_chg(void *data, Evas_Object *obj, void *event)
{
   config->translucent = elm_check_state_get(obj);
   main_trans_update();
   config_save();
}

static void
_cb_op_behavior_wsep_chg(void *data, Evas_Object *obj, void *event)
{
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
   termio_config_update(data);
   config_save();
}

static void
_cb_op_behavior_sback_chg(void *data, Evas_Object *obj, void *event)
{
   config->scrollback = elm_slider_value_get(obj) + 0.5;
   termio_config_update(data);
   config_save();
}

void
options_behavior(Evas_Object *opbox, Evas_Object *term)
{
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
   
   op_jumpcheck = o = elm_check_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "Translucent");
   elm_check_state_set(o, config->translucent);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_trans_chg, NULL);

   o = elm_label_add(opbox);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "Word separators:");
   elm_box_pack_end(opbox, o);
   evas_object_show(o);

   op_wordsep = o = elm_entry_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_entry_single_line_set(o, EINA_TRUE);
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
   
   elm_box_pack_end(opbox, o);
   evas_object_size_hint_weight_set(opbox, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(opbox, EVAS_HINT_FILL, 0.0);
   evas_object_show(o);
}
