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
   Evas_Object *term;
} 
Background_Item;

typedef struct _Wallpaper_Path_Item
{
   const char *path;
} 
Wallpaper_Path_Item;

static void _renew_gengrid_backgrounds(Evas_Object *term);

static Evas_Object *inwin, *list, *bg_grid, *parent, *bx;
static Ecore_Timer *seltimer = NULL;
static Eina_List *backgroundlist = NULL;
static Eina_List *pathlist = NULL;

static char *
_grid_text_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   Background_Item *item = data;
   const char *s;

   if (!item->path) return strdup("None");
   s = ecore_file_file_get(item->path);
   if (s) return strdup(s);
   return NULL;
}

static Evas_Object *
_grid_content_get(void *data, Evas_Object *obj, const char *part)
{
   Background_Item *item = data;
   Evas_Object *o, *oe;
   Config *config = termio_config_get(item->term);
   char path[PATH_MAX];

   if (!strcmp(part, "elm.swallow.icon"))
     {
        if (item->path)
          {
             return media_add(obj, item->path, config, MEDIA_THUMB, TYPE_IMG);
          }
        else
          {
             if (!config->theme) return NULL;
             snprintf(path, PATH_MAX, "%s/themes/%s", elm_app_data_dir_get(), 
                                                      config->theme);
             o = elm_layout_add(obj);
             oe = elm_layout_edje_get(o);
             edje_object_file_set(oe, path, "terminology/background");
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
   Config *config = termio_config_get(item->term);   

   if (!config) return;
   if (!item->path)
     {
        //no background
        eina_stringshare_del(config->background);
        config->background = NULL;
        config_save(config, NULL);
        main_media_update(config);
     }
   else
     {
        eina_stringshare_replace(&(config->background), item->path);
        config_save(config, NULL);
        main_media_update(config);
     }
}
/*
 * Method to open the in windows
 */
static void
_done_click(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   evas_object_del(inwin);
   inwin = NULL;
}
/*
 * Methods for the genlist
 */
static char *
_item_label_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   Wallpaper_Path_Item *item = data;
   return strdup(item->path);
}

static void 
_fill_path_list(Eina_List *paths, Evas_Object *list)
{
   Eina_List *node = NULL;
   char *path;
   Wallpaper_Path_Item *wpi = NULL;
   Elm_Genlist_Item_Class *itc;

   itc = elm_genlist_item_class_new();
   itc->item_style = "default";
   itc->func.text_get = _item_label_get;
   itc->func.content_get = NULL;
   itc->func.state_get = NULL;
   itc->func.del = NULL;
   EINA_LIST_FOREACH(paths, node, path)
     {
        wpi = calloc(1, sizeof(Wallpaper_Path_Item));
        if (wpi)
          {
             wpi->path = eina_stringshare_add(path);
             elm_genlist_item_append(list, itc, wpi, NULL, 
                                     ELM_GENLIST_ITEM_NONE, 
                                     NULL, NULL);
             pathlist = eina_list_append(pathlist, wpi); 
          }
     }
   elm_gengrid_item_class_free(itc);
   evas_object_size_hint_weight_set(list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
}

static void
_file_is_chosen(void *data, Evas_Object *obj EINA_UNUSED, void *event)
{
   Eina_List *node;
   char *saved_path;
   Config *config = termio_config_get(data);
   Wallpaper_Path_Item *item;

   EINA_LIST_FOREACH(config->wallpaper_paths, node, saved_path)
     if (!strcmp(event, saved_path)) return;   

   config->wallpaper_paths = eina_list_append(config->wallpaper_paths, 
                                              eina_stringshare_add(event));
   config_save(config, NULL);
   main_media_update(config);
   evas_object_del(list);
   EINA_LIST_FREE(pathlist, item)
     {
        eina_stringshare_del(item->path);
        free(item);
     }
   list = elm_genlist_add(inwin);
   _fill_path_list(config->wallpaper_paths, list);
   elm_box_pack_start(elm_win_inwin_content_get(inwin), list);
   evas_object_show(list);
   _renew_gengrid_backgrounds(data);
}

static void
_delete_path_click(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Elm_Object_Item *selected = elm_genlist_selected_item_get(list);
   Config *config = termio_config_get(data);
   Wallpaper_Path_Item *item;

   if (selected)
     {
        item = elm_object_item_data_get(selected);
        if (item)
          {
             config->wallpaper_paths = eina_list_remove(config->wallpaper_paths,
                                                        item->path);
             config_save(config, NULL);
             main_media_update(config);
             evas_object_del(list);
             EINA_LIST_FREE(pathlist, item)
               {
                  eina_stringshare_del(item->path);
                  free(item);
               }
             list = elm_genlist_add(inwin);
             _fill_path_list(config->wallpaper_paths, list);
             elm_box_pack_start(elm_win_inwin_content_get(inwin), list);
             evas_object_show(list);
             _renew_gengrid_backgrounds(data);
          }
     }
}

static void
_path_edit_click(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Config *config = termio_config_get(data);
   Evas_Object *parent = elm_object_top_widget_get(obj);
   Evas_Object *o, *bx, *bx2;

   inwin = o = elm_win_inwin_add(parent);
   evas_object_show(o);

   bx = elm_box_add(inwin);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_win_inwin_content_set(o, bx);
   evas_object_show(bx);
   
   list = o = elm_genlist_add(inwin);
   _fill_path_list(config->wallpaper_paths, o);
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_box_add(inwin);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_box_horizontal_set(o, EINA_TRUE);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   bx2 = o;

   bx = o = elm_box_add(inwin);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.0);
   elm_box_horizontal_set(o, EINA_TRUE);
   elm_box_pack_end(bx2, o);
   evas_object_show(o);

   o = elm_fileselector_button_add(inwin);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.0);
   elm_fileselector_button_inwin_mode_set(o, EINA_TRUE);
   elm_fileselector_button_folder_only_set(o, EINA_TRUE);
   elm_object_text_set(o, "Add path");
   evas_object_smart_callback_add(o, "file,chosen", _file_is_chosen, data);
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_button_add(inwin);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.0);
   elm_object_text_set(o, "Delete path");
   elm_box_pack_end(bx, o);
   evas_object_smart_callback_add(o, "clicked", _delete_path_click, data);
   evas_object_show(o);

   o = elm_button_add(inwin);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.0);
   elm_object_text_set(o, "Done");   
   elm_box_pack_end(bx2, o);
   evas_object_smart_callback_add(o, "clicked", _done_click, inwin);
   evas_object_show(o);
}

static Eina_List*
_rec_read_directorys(Eina_List *list, char *root_path, Evas_Object *term)
{
   Eina_List *childs = ecore_file_ls(root_path);
   char *file_name, *ext;
   int i;
   char path[PATH_MAX];
   Background_Item *item;

   if (!childs) return list;
   EINA_LIST_FREE(childs, file_name)
     {
        snprintf(path, PATH_MAX, "%s/%s", root_path, file_name);
        ext = strchr(file_name, '.');
        if ((!ecore_file_is_dir(path)) && (file_name[0] != '.') && (ext))
          {
             for (i = 0; extn_img[i]; i++)
               {
                  if (strcasecmp(extn_img[i], ext)) continue;
                  item = calloc(1, sizeof(Background_Item));
                  if (item)
                    {
                       item->path = eina_stringshare_add(path);
                       item->term = term;
                       list = eina_list_append(list, item);
                    }
                  break;
               }
          }
        else
          {
             list = _rec_read_directorys(list, path, term);
          }
        free(file_name);
     }
   return list;
}

static void 
_read_directorys(Evas_Object *term)
{
   Config *config = termio_config_get(term);
   char path[PATH_MAX];  
   Background_Item *item; 
   Eina_List *node;
   char *path_iterate;
   char *home_dir;

   if (backgroundlist)
     {
        EINA_LIST_FREE(backgroundlist, item)
          {
             if (item->path) eina_stringshare_del(item->path);
             free(item);
          }
     }
   //first of all append the None !!
   item = calloc(1, sizeof(Background_Item));
   item->path = NULL;
   item->term = term; 
   backgroundlist = eina_list_append(backgroundlist, item); 
   //append the standart directory
   snprintf(path, sizeof(path), "%s/backgrounds/", elm_app_data_dir_get());
   backgroundlist = _rec_read_directorys(backgroundlist, path, term);
   //append the Home background directory if this directory exists
   home_dir = getenv("HOME");
   if(home_dir)
     {
        snprintf(path, sizeof(path), "%s/.e/e/backgrounds/", home_dir);
        backgroundlist = _rec_read_directorys(backgroundlist, path, term);
     }
   //Now append all the directorys which are stored
   EINA_LIST_FOREACH(config->wallpaper_paths, node, path_iterate)
     {
        backgroundlist = _rec_read_directorys(backgroundlist, path_iterate, term); 
     }   
}

static int
_cb_path_sort(const void *d1, const void *d2)
{
   const Background_Item *item1 = d1;
   const Background_Item *item2 = d2;

   if (!item1->path) return -1;
   if (!item2->path) return 1;
   return strcasecmp(item1->path, item2->path);
}

static Eina_Bool
_cb_selection_timer(void *data)
{
   Elm_Object_Item *item = data;
   
   elm_gengrid_item_selected_set(item, EINA_TRUE);     
   elm_gengrid_item_bring_in(item, ELM_GENLIST_ITEM_SCROLLTO_MIDDLE);
   seltimer = NULL;
   return EINA_FALSE;
}

static void 
_renew_gengrid_backgrounds(Evas_Object *term)
{
   Background_Item *item;
   Eina_List *node;
   Config *config = termio_config_get(term);
   Evas_Object *o;
   Elm_Gengrid_Item_Class *item_class; 

   item_class = elm_gengrid_item_class_new();
   item_class->func.text_get = _grid_text_get;
   item_class->func.content_get = _grid_content_get;
   
   if (bg_grid) evas_object_del(bg_grid);
   
   bg_grid = o = elm_gengrid_add(parent);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_gengrid_item_size_set(o, elm_config_scale_get() * 100, 
                                elm_config_scale_get() * 80);
   _read_directorys(term);
   backgroundlist = eina_list_sort(backgroundlist,
                                   eina_list_count(backgroundlist),
                                   _cb_path_sort);
   EINA_LIST_FOREACH(backgroundlist, node, item)
     {
        item->item = elm_gengrid_item_append(bg_grid, item_class, item, 
                                             _item_selected, item);
        if ((!item->path) && (!config->background))
          {
             if (!seltimer) ecore_timer_del(seltimer);
             seltimer = ecore_timer_add(0.2, _cb_selection_timer, item->item);
          }
        else if ((item->path) && (config->background))
          {
             if (strcmp(item->path, config->background) == 0)
               {
                  if (!seltimer) ecore_timer_del(seltimer);
                  seltimer = ecore_timer_add(0.2, _cb_selection_timer, item->item);
               }
          }
     }
   elm_box_pack_start(bx, o);
   evas_object_show(o);
   elm_gengrid_item_class_free(item_class);
}

void
options_wallpaper(Evas_Object *opbox, Evas_Object *term EINA_UNUSED)
{
   Evas_Object *frame, *o;

   bg_grid = NULL;
   parent = opbox;
  
   frame = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, "Background");  
   evas_object_show(o);
   elm_box_pack_end(opbox, o);

   bx = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_object_content_set(frame, o);
   evas_object_show(o);
   _renew_gengrid_backgrounds(term);

   o = elm_button_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_object_text_set(o, "Edit paths");
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "clicked", _path_edit_click, term);

}
void
options_wallpaper_clear(void)
{
   Background_Item *item;
   Wallpaper_Path_Item *wpi;
   
   EINA_LIST_FREE(backgroundlist, item)
     {
        if (item->path) eina_stringshare_del(item->path);
        free(item);
     } 
   backgroundlist = NULL; 
   if (pathlist)
     {
        EINA_LIST_FREE(pathlist, wpi)
          {
             eina_stringshare_del(wpi->path);
             free(wpi);
          }
        pathlist = NULL;
     }
}
