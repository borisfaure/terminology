#include <Elementary.h>
#include "win.h"
#include "config.h"

Evas_Object *
tg_win_add(void)
{
   Evas_Object *win, *o;
   char buf[4096];
   
   win = elm_win_add(NULL, "main", ELM_WIN_BASIC);
   elm_win_autodel_set(win, EINA_TRUE);
   
   elm_win_title_set(win, "Terminology");
   elm_win_icon_name_set(win, "Terminology");
   
   o = evas_object_image_add(evas_object_evas_get(win));
   snprintf(buf, sizeof(buf), "%s/icons/terminology.png",
            elm_app_data_dir_get());
   evas_object_image_file_set(o, buf, NULL);
   elm_win_icon_object_set(win, o);
   
   return win;
}
