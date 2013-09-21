#include <Elementary.h>
#include "win.h"
#include "config.h"
#include "main.h"
#include "app_server.h"

Evas_Object *
tg_win_add(const char *name, const char *role, const char *title, const char *icon_name)
{
   Evas_Object *win, *o;
   char buf[4096];

   if (!name) name = "main";
   if (!title) title = "Terminology";
   if (!icon_name) icon_name = "Terminology";
   
   win = elm_win_add(NULL, name, ELM_WIN_BASIC);
   elm_win_title_set(win, title);
   elm_win_icon_name_set(win, icon_name);
   if (role) elm_win_role_set(win, role);
   
   evas_object_smart_callback_add(win, "delete,request",
                                  app_server_win_del_request_cb, win);

   elm_win_autodel_set(win, EINA_TRUE);
   
   o = evas_object_image_add(evas_object_evas_get(win));
   snprintf(buf, sizeof(buf), "%s/images/terminology.png",
            elm_app_data_dir_get());
   evas_object_image_file_set(o, buf, NULL);
   elm_win_icon_object_set(win, o);

   return win;
}
