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

static Evas_Object *_inwin = NULL,
                   *_list = NULL,
                   *_bg_grid = NULL,
                   *_parent = NULL,
                   *_bx = NULL;
static Ecore_Timer *_seltimer = NULL;
static Eina_List *_backgroundlist = NULL,
                 *_pathlist = NULL;

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
             int i, ret = 0;

             for (i = 0; extn_edj[i]; i++)
               {
                  if (eina_str_has_extension(item->path, extn_edj[i]))
                    return media_add(obj, item->path, config,
                                     MEDIA_BG, &ret);
               }
             return media_add(obj, item->path, config, MEDIA_THUMB, &ret);
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
        // no background
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
   evas_object_del(_inwin);
   _inwin = NULL;
}
/*
 * Methods for the genlist
 */
static char *
_item_label_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   Wallpaper_Path_Item *item = data;
   if (!item->path) return NULL;
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
        if (!path) continue;
        wpi = calloc(1, sizeof(Wallpaper_Path_Item));
        if (wpi)
          {
             wpi->path = eina_stringshare_add(path);
             if (wpi->path)
               {
                  elm_genlist_item_append(list, itc, wpi, NULL,
                                          ELM_GENLIST_ITEM_NONE,
                                          NULL, NULL);
                  _pathlist = eina_list_append(_pathlist, wpi);
               }
             else free(wpi);
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
   evas_object_del(_list);
   EINA_LIST_FREE(_pathlist, item)
     {
        if (item->path) eina_stringshare_del(item->path);
        free(item);
     }
   _list = elm_genlist_add(_inwin);
   _fill_path_list(config->wallpaper_paths, _list);
   elm_box_pack_start(elm_win_inwin_content_get(_inwin), _list);
   evas_object_show(_list);
   _renew_gengrid_backgrounds(data);
}

static void
_delete_path_click(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Elm_Object_Item *selected = elm_genlist_selected_item_get(_list);
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
             evas_object_del(_list);
             EINA_LIST_FREE(_pathlist, item)
               {
                  if (item->path) eina_stringshare_del(item->path);
                  free(item);
               }
             _list = elm_genlist_add(_inwin);
             _fill_path_list(config->wallpaper_paths, _list);
             elm_box_pack_start(elm_win_inwin_content_get(_inwin), _list);
             evas_object_show(_list);
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

   _inwin = o = elm_win_inwin_add(parent);
   evas_object_show(o);

   bx = elm_box_add(_inwin);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_win_inwin_content_set(o, bx);
   evas_object_show(bx);

   _list = o = elm_genlist_add(_inwin);
   _fill_path_list(config->wallpaper_paths, o);
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_box_add(_inwin);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_box_horizontal_set(o, EINA_TRUE);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   bx2 = o;

   bx = o = elm_box_add(_inwin);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.0);
   elm_box_horizontal_set(o, EINA_TRUE);
   elm_box_pack_end(bx2, o);
   evas_object_show(o);

   o = elm_fileselector_button_add(_inwin);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.0);
   elm_fileselector_button_inwin_mode_set(o, EINA_TRUE);
   elm_fileselector_button_folder_only_set(o, EINA_TRUE);
   elm_object_text_set(o, "Add path");
   evas_object_smart_callback_add(o, "file,chosen", _file_is_chosen, data);
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_button_add(_inwin);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.0);
   elm_object_text_set(o, "Delete path");
   elm_box_pack_end(bx, o);
   evas_object_smart_callback_add(o, "clicked", _delete_path_click, data);
   evas_object_show(o);

   o = elm_button_add(_inwin);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.0);
   elm_object_text_set(o, "Done");
   elm_box_pack_end(bx2, o);
   evas_object_smart_callback_add(o, "clicked", _done_click, _inwin);
   evas_object_show(o);
}

static Eina_List*
_rec_read_directorys(Eina_List *list, char *root_path, Evas_Object *term)
{
   Eina_List *childs = ecore_file_ls(root_path);
   char *file_name, path[PATH_MAX];
   int i, j;
   Background_Item *item;

   if (!childs) return list;
   EINA_LIST_FREE(childs, file_name)
     {
        snprintf(path, PATH_MAX, "%s/%s", root_path, file_name);
        if ((!ecore_file_is_dir(path)) && (file_name[0] != '.'))
          {
             const char **extns[5] =
               { extn_img, extn_scale, extn_edj, extn_mov, NULL };

             for (j = 0; extns[j]; j++)
               {
                  const char **ex = extns[j];

                  for (i = 0; ex[i]; i++)
                    {
                       if (eina_str_has_extension(file_name, ex[i]))
                         {
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

   EINA_LIST_FREE(_backgroundlist, item)
     {
        if (item->path) eina_stringshare_del(item->path);
        free(item);
     }
   // first of all append the None !!
   item = calloc(1, sizeof(Background_Item));
   item->path = NULL;
   item->term = term;
   _backgroundlist = eina_list_append(_backgroundlist, item);
   // append the standard directory
   snprintf(path, sizeof(path), "%s/backgrounds", elm_app_data_dir_get());
   _backgroundlist = _rec_read_directorys(_backgroundlist, path, term);
   // append the Home background directory if this directory exists
   home_dir = getenv("HOME");
   if (home_dir)
     {
        snprintf(path, sizeof(path), "%s/.e/e/backgrounds", home_dir);
        _backgroundlist = _rec_read_directorys(_backgroundlist, path, term);
     }
   // Now append all the directorys which are stored
   EINA_LIST_FOREACH(config->wallpaper_paths, node, path_iterate)
     {
        _backgroundlist = _rec_read_directorys(_backgroundlist, path_iterate, term);
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
   _seltimer = NULL;
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

   if (_bg_grid) evas_object_del(_bg_grid);

   _bg_grid = o = elm_gengrid_add(_parent);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_gengrid_item_size_set(o, elm_config_scale_get() * 100,
                                elm_config_scale_get() * 80);
   _read_directorys(term);
   _backgroundlist = eina_list_sort(_backgroundlist,
                                   eina_list_count(_backgroundlist),
                                   _cb_path_sort);
   EINA_LIST_FOREACH(_backgroundlist, node, item)
     {
        item->item = elm_gengrid_item_append(_bg_grid, item_class, item,
                                             _item_selected, item);
        if ((!item->path) && (!config->background))
          {
             if (!_seltimer) ecore_timer_del(_seltimer);
             _seltimer = ecore_timer_add(0.2, _cb_selection_timer, item->item);
          }
        else if ((item->path) && (config->background))
          {
             if (strcmp(item->path, config->background) == 0)
               {
                  if (!_seltimer) ecore_timer_del(_seltimer);
                  _seltimer = ecore_timer_add(0.2, _cb_selection_timer, item->item);
               }
          }
     }
   elm_box_pack_start(_bx, o);
   evas_object_show(o);
   elm_gengrid_item_class_free(item_class);
}

void
options_wallpaper(Evas_Object *opbox, Evas_Object *term)
{
   Evas_Object *frame, *o;

   _parent = opbox;

   frame = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, "Background");
   evas_object_show(o);
   elm_box_pack_end(opbox, o);

   _bx = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_object_content_set(frame, o);
   evas_object_show(o);
   _renew_gengrid_backgrounds(term);

   o = elm_button_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_object_text_set(o, "Edit paths");
   elm_box_pack_end(_bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "clicked", _path_edit_click, term);

}
void
options_wallpaper_clear(void)
{
   Background_Item *item;
   Wallpaper_Path_Item *wpi;

   EINA_LIST_FREE(_backgroundlist, item)
     {
        if (item->path) eina_stringshare_del(item->path);
        free(item);
     }
   EINA_LIST_FREE(_pathlist, wpi)
     {
        if (wpi->path) eina_stringshare_del(wpi->path);
        free(wpi);
     }
}
