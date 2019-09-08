#include "private.h"

#include <Elementary.h>
#include "options.h"
#include "options_elm.h"

static void
launch_elm_config(void *_data EINA_UNUSED,
                Evas_Object *_obj EINA_UNUSED,
                void *_event_info EINA_UNUSED)
{
   Ecore_Exe *exe;

   exe = ecore_exe_pipe_run("elementary_config", ECORE_EXE_NONE, NULL);
   ecore_exe_free(exe);
}

static void
_scale_round(void *data       EINA_UNUSED,
             Evas_Object     *obj,
             void *event_info EINA_UNUSED)
{
   double val = elm_slider_value_get(obj);
   double v;

   v = ((double)((int)(val * 10.0))) / 10.0;
   if (v != val) elm_slider_value_set(obj, v);
}

static void
_scale_change(void *data       EINA_UNUSED,
              Evas_Object     *obj,
              void *event_info EINA_UNUSED)
{
   double scale = elm_config_scale_get();
   double val = elm_slider_value_get(obj);

   if (scale == val)
     return;
   elm_config_scale_set(val);
   elm_config_all_flush();
}

void
options_elm(Evas_Object *opbox, Evas_Object *_term EINA_UNUSED)
{
   Evas_Object *o, *fr, *bx, *bt, *en, *lbl, *sl, *sp;
   const char *txt;

   fr = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Toolkit"));
   elm_box_pack_end(opbox, o);
   evas_object_show(o);

   bx = elm_box_add(opbox);
   elm_object_content_set(fr, bx);
   evas_object_show(bx);

   en = elm_entry_add(opbox);
   elm_entry_context_menu_disabled_set(en, EINA_TRUE);
   elm_entry_editable_set(en, EINA_FALSE);
   elm_entry_line_wrap_set(en, ELM_WRAP_MIXED);
   elm_object_text_set(en, _("<em>Terminology</em> uses the <hilight>elementary</hilight> toolkit.<br>"
                             "The toolkit configuration settings can be accessed by running <keyword>elementary_config</keyword>."));
   evas_object_size_hint_weight_set(en, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(en, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(bx, en);
   evas_object_show(en);

   bt = elm_button_add(opbox);
   evas_object_smart_callback_add(bt, "clicked", launch_elm_config, NULL);
   evas_object_propagate_events_set(bt, EINA_FALSE);
   elm_layout_text_set(bt, NULL, _("Launch elementary_config"));
   elm_box_pack_end(bx, bt);
   evas_object_show(bt);

   sp = elm_separator_add(opbox);
   evas_object_size_hint_weight_set(sp, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sp, EVAS_HINT_FILL, 0.5);
   elm_separator_horizontal_set(sp, EINA_TRUE);
   elm_box_pack_end(bx, sp);
   evas_object_show(sp);

   lbl = elm_label_add(opbox);
   evas_object_size_hint_weight_set(lbl, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lbl, EVAS_HINT_FILL, 0.5);
   txt = eina_stringshare_printf("<hilight>%s</>",_("Scale"));
   elm_object_text_set(lbl, txt);
   eina_stringshare_del(txt);
   elm_box_pack_end(bx, lbl);
   evas_object_show(lbl);

   sl = elm_slider_add(opbox);
   evas_object_size_hint_weight_set(sl, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sl, EVAS_HINT_FILL, 0.5);
   elm_slider_span_size_set(sl, 120);
   elm_slider_unit_format_set(sl, "%1.2f");
   elm_slider_indicator_format_set(sl, "%1.2f");
   elm_slider_min_max_set(sl, 0.25, 5.0);
   elm_slider_value_set(sl, elm_config_scale_get());
   elm_box_pack_end(bx, sl);
   evas_object_show(sl);
   evas_object_smart_callback_add(sl, "changed", _scale_round, NULL);
   evas_object_smart_callback_add(sl, "delay,changed", _scale_change, NULL);

   lbl = elm_label_add(opbox);
   evas_object_size_hint_weight_set(lbl, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lbl, 0.0, 0.0);
   elm_object_text_set(lbl, _("Select prefered size so that this text is readable."));
   elm_box_pack_end(bx, lbl);
   evas_object_show(lbl);

   lbl = elm_label_add(opbox);
   evas_object_size_hint_weight_set(lbl, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lbl, 0.0, 0.0);
   elm_object_text_set(lbl, _("The scale configuration can also be changed through <hilight>elementary</hilight>'s configuration panel."));
   elm_box_pack_end(bx, lbl);
   evas_object_show(lbl);
}
