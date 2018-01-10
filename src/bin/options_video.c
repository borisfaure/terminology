#include "private.h"

#include <Elementary.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_video.h"
#include "main.h"


static void
_cb_op_video_mute_chg(void *data,
                      Evas_Object *obj,
                      void *_event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->mute = elm_check_state_get(obj);
   main_media_mute_update(config);
   config_save(config, NULL);
}

static void
_cb_op_video_visualize_chg(void *data,
                           Evas_Object *obj,
                           void *_event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->visualize = elm_check_state_get(obj);
   main_media_visualize_update(config);
   config_save(config, NULL);
}

static void
_cb_op_video_vidmod_chg(void *data,
                        Evas_Object *obj,
                        void *_event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   int v = elm_radio_value_get(obj);
   if (v == config->vidmod) return;
   config->vidmod = v;
   main_media_update(config);
   config_save(config, NULL);
}

void
options_video(Evas_Object *opbox, Evas_Object *term)
{
   Evas_Object *o, *fr, *bx0, *op_vidmod;
   Config *config = termio_config_get(term);

   fr = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Video"));
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
   elm_object_text_set(o, _("Audio muted"));
   elm_check_state_set(o, config->mute);
   elm_box_pack_end(bx0, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_mute_chg, term);

   /*
    * TODO: visualizing type configuration
    */
   o = elm_check_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Audio visualized"));
   elm_check_state_set(o, config->visualize);
   elm_box_pack_end(bx0, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_visualize_chg, term);

   o = elm_separator_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_separator_horizontal_set(o, EINA_TRUE);
   elm_box_pack_end(bx0, o);
   evas_object_show(o);

   o = elm_label_add(opbox);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, _("Video Engine:"));
   elm_box_pack_end(bx0, o);
   evas_object_show(o);

   op_vidmod = o = elm_radio_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Automatic"));
   elm_radio_state_value_set(o, 0);
   elm_box_pack_end(bx0, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_vidmod_chg, term);

   o = elm_radio_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "Gstreamer");
   elm_radio_state_value_set(o, 1);
   elm_radio_group_add(o, op_vidmod);
   elm_box_pack_end(bx0, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_vidmod_chg, term);

   o = elm_radio_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "Xine");
   elm_radio_state_value_set(o, 2);
   elm_radio_group_add(o, op_vidmod);
   elm_box_pack_end(bx0, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_vidmod_chg, term);

   o = elm_radio_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "VLC");
   elm_radio_state_value_set(o, 3);
   elm_radio_group_add(o, op_vidmod);
   elm_box_pack_end(bx0, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_vidmod_chg, term);

   o = elm_radio_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, "Gstreamer 1.X");
   elm_radio_state_value_set(o, 4);
   elm_radio_group_add(o, op_vidmod);
   elm_box_pack_end(bx0, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_vidmod_chg, term);

   elm_radio_value_set(o, config->vidmod);

   evas_object_size_hint_weight_set(opbox, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(opbox, EVAS_HINT_FILL, 0.0);
   evas_object_show(o);
}
