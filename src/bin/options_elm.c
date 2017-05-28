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

void
options_elm(Evas_Object *opbox, Evas_Object *_term EINA_UNUSED)
{
   Evas_Object *o, *fr, *bx, *bt, *en;

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
}
