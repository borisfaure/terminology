#include <Elementary.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_video.h"
#include "main.h"

static Evas_Object *op_trans, *op_mute, *op_vidmod;

static void
_cb_op_video_trans_chg(void *data, Evas_Object *obj, void *event)
{
   config->translucent = elm_check_state_get(obj);
   main_trans_update();
   config_save();
}

static void
_cb_op_video_mute_chg(void *data, Evas_Object *obj, void *event)
{
   config->mute = elm_check_state_get(obj);
   main_media_mute_update();
   config_save();
}

static void
_cb_op_video_vidmod_chg(void *data, Evas_Object *obj, void *event)
{
   int v = elm_radio_value_get(obj);
   if (v == config->vidmod) return;
   config->vidmod = v;
   main_media_update();
   config_save();
}

void
options_video(Evas_Object *opbox, Evas_Object *term)
{
   Evas_Object *o;
   char *txt;

   op_trans = o = elm_check_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "Translucent");
   elm_check_state_set(o, config->translucent);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_trans_chg, NULL);

   o = elm_separator_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_separator_horizontal_set(o, EINA_TRUE);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   
   op_mute = o = elm_check_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "Audio muted");
   elm_check_state_set(o, config->mute);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_mute_chg, NULL);
   o = elm_separator_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_separator_horizontal_set(o, EINA_TRUE);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   
   o = elm_label_add(opbox);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "Video Engine:");
   elm_box_pack_end(opbox, o);
   evas_object_show(o);

   op_vidmod = o = elm_radio_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "Automatic");
   elm_radio_state_value_set(o, 0);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_vidmod_chg, NULL);
   
   o = elm_radio_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "Gstreamer");
   elm_radio_state_value_set(o, 1);
   elm_radio_group_add(o, op_vidmod);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_vidmod_chg, NULL);
   
   o = elm_radio_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "Xine");
   elm_radio_state_value_set(o, 2);
   elm_radio_group_add(o, op_vidmod);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_vidmod_chg, NULL);
   
   o = elm_radio_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "Generic (VLC)");
   elm_radio_state_value_set(o, 3);
   elm_radio_group_add(o, op_vidmod);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_vidmod_chg, NULL);
   
   elm_radio_value_set(o, config->vidmod);

   evas_object_size_hint_weight_set(opbox, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(opbox, EVAS_HINT_FILL, 0.0);
   evas_object_show(o);
}
