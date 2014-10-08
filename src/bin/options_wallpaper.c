#include "private.h"

#include <Elementary.h>
#include "config.h"
#include "termio.h"
#include "media.h"
#include "options.h"
#include "options_wallpaper.h"
#include "extns.h"
#include "media.h"
#include "main.h"
#include <sys/stat.h>

typedef struct _Background_Item
{
   const char *path;
   Eina_Bool selected;
   Elm_Object_Item *item;
} Background_Item;

typedef struct _Insert_Gen_Grid_Item_Notify
{
   Elm_Gengrid_Item_Class *class;
   Background_Item *item;
} Insert_Gen_Grid_Item_Notify;


static Eina_Stringshare *_system_path,
                        *_user_path;

static Evas_Object *_bg_grid = NULL, 
                   *_term = NULL, 
                   *_entry = NULL, 
                   *_flip = NULL, 
                   *_bubble = NULL;

static Eina_List *_backgroundlist = NULL;

static Ecore_Timer *_bubble_disappear;

static Ecore_Thread *_thread;

static void
_cb_fileselector(void *data EINA_UNUSED, Evas_Object *obj, void* event)
{

  if (event) 
    {
      elm_object_text_set(_entry, elm_fileselector_path_get(obj));
      elm_flip_go_to(_flip, EINA_TRUE, ELM_FLIP_PAGE_LEFT);
    }
  else
    {
       elm_flip_go_to(_flip, EINA_TRUE, ELM_FLIP_PAGE_LEFT);
    }
}

static Eina_Bool
_cb_timer_bubble_disappear(void *data EINA_UNUSED)
{
   evas_object_del(_bubble);
   _bubble_disappear = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_bubble_show(char *text)
{
   Evas_Object *opbox = elm_object_top_widget_get(_bg_grid);
   Evas_Object *o;
   int x = 0, y = 0, w , h;

   evas_object_geometry_get(_bg_grid, &x, &y, &w ,&h);
   if (_bubble_disappear)
     {
        ecore_timer_del(_bubble_disappear);
        _cb_timer_bubble_disappear(NULL);
     }
 
   _bubble = elm_bubble_add(opbox);
   elm_bubble_pos_set(_bubble, ELM_BUBBLE_POS_BOTTOM_RIGHT);
   evas_object_resize(_bubble, 200, 50);
   evas_object_move(_bubble, (x + w - 200), (y + h - 50));
   evas_object_show(_bubble);

   o = elm_label_add(_bubble);
   elm_object_text_set(o, text);
   elm_object_content_set(_bubble, o);

  _bubble_disappear = ecore_timer_add(2.0, _cb_timer_bubble_disappear, NULL);
}

static char *
_grid_text_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   Background_Item *item = data;
   const char *s;

   if (!item->path) return strdup(_("None"));
   s = ecore_file_file_get(item->path);
   if (s) return strdup(s);
   return NULL;
}

static Evas_Object *
_grid_content_get(void *data, Evas_Object *obj, const char *part)
{
   Background_Item *item = data;
   Evas_Object *o, *oe;
   Config *config = termio_config_get(_term);
   char path[PATH_MAX];

   if (!strcmp(part, "elm.swallow.icon"))
     {
        if (item->path)
          {
             int i;
             Media_Type type;
             for (i = 0; extn_edj[i]; i++)
               {
                  if (eina_str_has_extension(item->path, extn_edj[i]))
                    return media_add(obj, item->path, config,
                                     MEDIA_BG, MEDIA_TYPE_EDJE);
               }
             type = media_src_type_get(item->path);
             return media_add(obj, item->path, config, MEDIA_THUMB, type);
          }
        else
          {
             if (!config->theme) return NULL;
             snprintf(path, PATH_MAX, "%s/themes/%s", elm_app_data_dir_get(),
                                                      config->theme);
             o = elm_layout_add(obj);
             oe = elm_layout_edje_get(o);
             if (!edje_object_file_set(oe, path, "terminology/background"))
               {
                  evas_object_del(o);
                  return NULL;
               }
             evas_object_show(o);
             return o;
         }
     }
   return NULL;
}

static void
_item_selected(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Background_Item *item = data;
   Config *config = termio_config_get(_term);

   if (!config) return;
   if (!item->path)
     {
        // no background
        eina_stringshare_del(config->background);
        config->background = NULL;
        config_save(config, NULL);
        main_media_update(config);
     }
   else if (eina_stringshare_replace(&(config->background), item->path))
     {
        config_save(config, NULL);
        main_media_update(config);
     }
}

static void
_insert_gengrid_item(Insert_Gen_Grid_Item_Notify *msg_data)
{
   Insert_Gen_Grid_Item_Notify *insert_msg = msg_data;
   Config *config = termio_config_get(_term);

   if (insert_msg && insert_msg->item && insert_msg->class && config)
     {
        Background_Item *item = insert_msg->item;
        Elm_Gengrid_Item_Class *item_class = insert_msg->class;

        item->item = elm_gengrid_item_append(_bg_grid, item_class, item,
                                        _item_selected, item);
        if ((!item->path) && (!config->background))
          {
             elm_gengrid_item_selected_set(item->item, EINA_TRUE);
             elm_gengrid_item_bring_in(item->item,
                                       ELM_GENGRID_ITEM_SCROLLTO_MIDDLE);
          }
        else if ((item->path) && (config->background))
          {
             if (strcmp(item->path, config->background) == 0)
               {
                  elm_gengrid_item_selected_set(item->item, EINA_TRUE);
                  elm_gengrid_item_bring_in(item->item,
                                            ELM_GENGRID_ITEM_SCROLLTO_MIDDLE);
               }
          }
     }
   free(msg_data);
}

static Eina_List*
_rec_read_directorys(Eina_List *list, const char *root_path, 
                     Elm_Gengrid_Item_Class *class)
{
   Eina_List *childs = ecore_file_ls(root_path);
   char *file_name, path[PATH_MAX];
   int i, j;
   Background_Item *item;
   const char **extns[5] =
   { extn_img, extn_scale, extn_edj, extn_mov, NULL };
   const char **ex;
   Insert_Gen_Grid_Item_Notify *notify; 


   if (!childs) return list;
   EINA_LIST_FREE(childs, file_name)
     {
        snprintf(path, PATH_MAX, "%s/%s", root_path, file_name);
        if ((!ecore_file_is_dir(path)) && (file_name[0] != '.'))
          {
             //file is found, search for correct file endings ! 
             for (j = 0; extns[j]; j++)
               {
                  ex = extns[j];
                  for (i = 0; ex[i]; i++)
                    {
                       if (eina_str_has_extension(file_name, ex[i]))
                         {
                            //File is found and valid
                            item = calloc(1, sizeof(Background_Item));
                            if (item)
                              {
                                 notify = calloc(1, 
                                          sizeof(Insert_Gen_Grid_Item_Notify));
                                 item->path = eina_stringshare_add(path);
                                 list = eina_list_append(list, item);
                                 //insert item to gengrid
                                 notify->class = class;
                                 notify->item = item;
                                 //ecore_thread_feedback(th, notify);
                                 _insert_gengrid_item(notify);
                              }
                            break;
                         }
                    }
               }
          }
        free(file_name);
     }
   return list;
}

static void
_refresh_directory(const char* data)
{
   Background_Item *item;
   Elm_Gengrid_Item_Class *item_class;

   // This will run elm_gengrid_clear 
   elm_gengrid_clear(_bg_grid);

   if (_backgroundlist)
     {
        EINA_LIST_FREE(_backgroundlist, item)
          {
             if (item->path)
               eina_stringshare_del(item->path);
             free(item);

          }
        _backgroundlist = NULL;
     }
   item_class = elm_gengrid_item_class_new();
   item_class->func.text_get = _grid_text_get;
   item_class->func.content_get = _grid_content_get;

   item = calloc(1, sizeof(Background_Item));
   _backgroundlist = eina_list_append(_backgroundlist, item); 

   //Insert None Item
   Insert_Gen_Grid_Item_Notify *notify = calloc(1, 
                                         sizeof(Insert_Gen_Grid_Item_Notify));
   notify->class = item_class;
   notify->item = item;

   _insert_gengrid_item(notify);

   _backgroundlist = _rec_read_directorys(_backgroundlist, data,
                                          item_class);

   elm_gengrid_item_class_free(item_class);
   _thread = NULL;
}

static void
_gengrid_refresh_samples(const char *path)
{
   if(!ecore_file_exists(path))
      return;
   _refresh_directory(path);
}

static void
_cb_entry_changed(void *data EINA_UNUSED, Evas_Object *parent,
                  void *event EINA_UNUSED)
{
   const char *path = elm_object_text_get(parent);
   _gengrid_refresh_samples(path);
}

static void
_cb_hoversel_select(void *data, Evas_Object *hoversel EINA_UNUSED, 
                    void *event EINA_UNUSED)
{
   Evas_Object *o;
   char *path = data;
   if (path)
     {
        elm_object_text_set(_entry, path);
     }
   else
     {
        o = elm_object_part_content_get(_flip, "back");
        elm_fileselector_path_set(o, elm_object_text_get(_entry));
        elm_flip_go_to(_flip, EINA_FALSE, ELM_FLIP_PAGE_RIGHT);
     } 
}

static void
_system_background_dir_init(void)
{ 
   char path[PATH_MAX];

   snprintf(path, PATH_MAX, "%s/backgrounds/", elm_app_data_dir_get());
   if (_system_path)
     eina_stringshare_replace(&_system_path, path);
   else
     _system_path = eina_stringshare_add(path);
}

static const char*
_user_background_dir_init(void){
   char path[PATH_MAX], *user;

   user = getenv("HOME");
   if(!user)
      return NULL;
   snprintf(path, PATH_MAX, "%s/.config/terminology/background/", user);
   if (!ecore_file_exists(path))
     ecore_file_mkpath(path);
   if (!_user_path)
     _user_path = eina_stringshare_add(path);
   else
     eina_stringshare_replace(&_user_path, path);
   return _user_path; 
}

static const char*
_import_background(const char* background)
{
   char path[PATH_MAX];
   const char *filename = ecore_file_file_get(background);

   if (!filename)
     return NULL;
   if (!_user_background_dir_init())
     return NULL;
   snprintf(path, PATH_MAX, "%s/%s", _user_path, filename);
   if (!ecore_file_cp(background, path))
     return NULL;
   return eina_stringshare_add(path);
}

static void
_cb_grid_doubleclick(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, 
                     void *event EINA_UNUSED)
{
   Config *config = termio_config_get(_term);
   char *config_background_dir = ecore_file_dir_get(config->background);

   if (!_user_path) {
     if (!_user_background_dir_init())
       return;
   }
   if (!config->background) 
     return;
   if (strncmp(config_background_dir, _user_path, 
               strlen(config_background_dir)) == 0)
     {
        _bubble_show(_("Source file is target file"));
        free(config_background_dir);
        return;
     }

   const char *newfile = _import_background(config->background);

   if (newfile)
     {
        eina_stringshare_replace(&(config->background), newfile);
        config_save(config, NULL);
        main_media_update(config);
        eina_stringshare_del(newfile);
        _bubble_show(_("Picture imported"));
        elm_object_text_set(_entry, config_background_dir);
     }
   else
     {
       _bubble_show(_("Failed"));
     }
   free(config_background_dir);
}

void
options_wallpaper(Evas_Object *opbox, Evas_Object *term)
{
   Evas_Object *frame, *o, *bx, *bx2;
   Config *config = termio_config_get(term);
   char path[PATH_MAX], *config_background_dir;

   _term = term;

   frame = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Background"));
   evas_object_show(o);
   elm_box_pack_end(opbox, o);

   _flip = o = elm_flip_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(frame, o);
   evas_object_show(o);

   o = elm_fileselector_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_part_content_set(_flip, "back", o);
   elm_fileselector_folder_only_set(o, EINA_TRUE);
   evas_object_smart_callback_add(o, "done", _cb_fileselector, NULL);
   evas_object_show(o);

   bx = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_part_content_set(_flip, "front", bx);
   evas_object_show(o);

   bx2 = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_box_horizontal_set(o, EINA_TRUE);
   elm_box_pack_start(bx, o);
   evas_object_show(o);

   _entry = o = elm_entry_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_scroller_policy_set(o, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
   evas_object_smart_callback_add(_entry, "changed", _cb_entry_changed, NULL);
   elm_box_pack_start(bx2, o);
   evas_object_show(o); 

   o = elm_hoversel_add(opbox);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_object_text_set(o, _("Select Path"));
   elm_box_pack_end(bx2, o);
   evas_object_show(o);

   snprintf(path, PATH_MAX, "%s/backgrounds/", elm_app_data_dir_get());
   _system_background_dir_init();
   elm_hoversel_item_add(o, _("System"), NULL, ELM_ICON_NONE, _cb_hoversel_select ,
                         _system_path);
   if (_user_background_dir_init())
     elm_hoversel_item_add(o, _("User"), NULL, ELM_ICON_NONE, _cb_hoversel_select ,
                           _user_path);
   //In the other case it has failed, so dont show the user item
   elm_hoversel_item_add(o, _("Other"), NULL, ELM_ICON_NONE, _cb_hoversel_select , 
                         NULL);

   _bg_grid = o = elm_gengrid_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(o, "clicked,double", _cb_grid_doubleclick, NULL);
   elm_gengrid_item_size_set(o, elm_config_scale_get() * 100,
                                elm_config_scale_get() * 80);
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_label_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_object_text_set(o, _("Double click on a picture to import it"));
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   if (config->background)
     {
        config_background_dir = ecore_file_dir_get(config->background);
        elm_object_text_set(_entry, config_background_dir);
        free(config_background_dir); 
     }
   else
     {
        elm_object_text_set(_entry, _system_path);
     }
}

void
options_wallpaper_clear(void)
{
   Background_Item *item;

   EINA_LIST_FREE(_backgroundlist, item)
     {
        if (item->path) eina_stringshare_del(item->path);
        free(item);
     }
   _backgroundlist = NULL;
   if (_user_path)
     {
        eina_stringshare_del(_user_path);
        _user_path = NULL;
     }
   if (_system_path)
     {
        eina_stringshare_del(_system_path);
        _system_path = NULL;
     }
}
