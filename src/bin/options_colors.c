#include "private.h"

#include <Elementary.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_colors.h"

void
options_colors(Evas_Object *opbox, Evas_Object *term __UNUSED__)
{
   Evas_Object *o;

   o = elm_label_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, "Not Implemented Yet.");
   evas_object_show(o);
   elm_box_pack_end(opbox, o);
}
