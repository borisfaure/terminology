#include "private.h"

#include <Elementary.h>
#include <assert.h>
#include "config.h"
#include "termio.h"
#include "media.h"
#include "options.h"
#include "options_background.h"
#include "extns.h"
#include "media.h"
#include "main.h"
#include <sys/stat.h>

typedef struct _Background_Ctx {
     Config *config;
     Evas_Object *frame;
     Evas_Object *flip;
     Evas_Object *bg_grid;
     Evas_Object *term;
     Evas_Object *entry;
     Evas_Object *bubble;
     Evas_Object *op_trans;
     Evas_Object *op_opacity;
     Evas_Object *op_shine_slider;

     Eina_Stringshare *system_path;
     Eina_Stringshare *user_path;
     Eina_List *background_list;
     Ecore_Timer *bubble_disappear;
} Background_Ctx;

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


static void
_cb_op_shine_sel(void *data,
                 Evas_Object *obj,
                 void *_event EINA_UNUSED)
{
   Background_Ctx *ctx = data;
   Config *config = ctx->config;
   Term *term = termio_term_get(ctx->term);
   Win *wn = term_win_get(term);
   int shine = elm_slider_value_get(obj);
   Eina_List *l, *wn_list;

   if (config->shine == shine)
       return;

   wn_list = win_terms_get(wn);
   EINA_LIST_FOREACH(wn_list, l, term)
     {
        term_apply_shine(term, shine);
     }
}

static void
_cb_op_video_trans_chg(void *data,
                       Evas_Object *obj,
                       void *_event EINA_UNUSED)
{
   Background_Ctx *ctx = data;
   Config *config = ctx->config;

   config->translucent = elm_check_state_get(obj);
   elm_object_disabled_set(ctx->op_opacity, !config->translucent);
   main_trans_update(config);
   config_save(config, NULL);
}

static void
_cb_op_video_opacity_chg(void *data,
                         Evas_Object *obj,
                         void *_event EINA_UNUSED)
{
   Background_Ctx *ctx = data;
   Config *config = ctx->config;

   config->opacity = elm_slider_value_get(obj);
   if (!config->translucent)
     return;
   main_trans_update(config);
   config_save(config, NULL);
}

static void
_cb_fileselector(void *data,
                 Evas_Object *obj,
                 void *event)
{
   Background_Ctx *ctx = data;

   if (event)
     {
        elm_object_text_set(ctx->entry, elm_fileselector_path_get(obj));
        elm_flip_go_to(ctx->flip, EINA_TRUE, ELM_FLIP_PAGE_LEFT);
     }
   else
     {
        elm_flip_go_to(ctx->flip, EINA_TRUE, ELM_FLIP_PAGE_LEFT);
     }
}

static Eina_Bool
_cb_timer_bubble_disappear(void *data)
{
   Background_Ctx *ctx = data;
   evas_object_del(ctx->bubble);
   ctx->bubble = NULL;
   ctx->bubble_disappear = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_bubble_show(Background_Ctx *ctx, char *text)
{
   Evas_Object *opbox = elm_object_top_widget_get(ctx->bg_grid);
   Evas_Object *o;
   int x = 0, y = 0, w , h;

   evas_object_geometry_get(ctx->bg_grid, &x, &y, &w ,&h);
   if (ctx->bubble_disappear)
     {
        ecore_timer_del(ctx->bubble_disappear);
        _cb_timer_bubble_disappear(ctx);
     }

   ctx->bubble = elm_bubble_add(opbox);
   elm_bubble_pos_set(ctx->bubble, ELM_BUBBLE_POS_BOTTOM_RIGHT);
   evas_object_resize(ctx->bubble, 200, 50);
   evas_object_move(ctx->bubble, (x + w - 200), (y + h - 50));
   evas_object_show(ctx->bubble);

   o = elm_label_add(ctx->bubble);
   elm_object_text_set(o, text);
   elm_object_content_set(ctx->bubble, o);

  ctx->bubble_disappear = ecore_timer_add(2.0,
                                          _cb_timer_bubble_disappear, ctx);
}

static char *
_grid_text_get(void *data,
               Evas_Object *_obj EINA_UNUSED,
               const char *_part EINA_UNUSED)
{
   Background_Item *item = data;
   const char *s;

   if (!item->path)
     return strdup(_("None"));
   s = ecore_file_file_get(item->path);
   if (s)
     return strdup(s);
   return NULL;
}

static Evas_Object *
_grid_content_get(void *data, Evas_Object *obj, const char *part)
{
   Background_Ctx *ctx = evas_object_data_get(obj, "ctx");
   assert(ctx);
   Background_Item *item = data;
   Evas_Object *o, *oe;
   Config *config = ctx->config;
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
             if (!config->theme)
               return NULL;
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
_item_selected(void *data,
               Evas_Object *obj,
               void *_event EINA_UNUSED)
{
   Background_Ctx *ctx = evas_object_data_get(obj, "ctx");
   Background_Item *item = data;
   Config *config = ctx->config;

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
_insert_gengrid_item(Background_Ctx *ctx,
                     Insert_Gen_Grid_Item_Notify *msg_data)
{
   Insert_Gen_Grid_Item_Notify *insert_msg = msg_data;
   Config *config = ctx->config;

   if (insert_msg && insert_msg->item && insert_msg->class && config)
     {
        Background_Item *item = insert_msg->item;
        Elm_Gengrid_Item_Class *item_class = insert_msg->class;

        item->item = elm_gengrid_item_append(ctx->bg_grid, item_class, item,
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
_rec_read_directorys(Background_Ctx *ctx, Eina_List *list,
                     const char *root_path, Elm_Gengrid_Item_Class *class)
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
                                 if (notify)
                                   {
                                      //insert item to gengrid
                                      notify->class = class;
                                      notify->item = item;
                                      //ecore_thread_feedback(th, notify);
                                      _insert_gengrid_item(ctx, notify);
                                   }
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
_refresh_directory(Background_Ctx *ctx, const char* data)
{
   Background_Item *item;
   Elm_Gengrid_Item_Class *item_class;

   elm_gengrid_clear(ctx->bg_grid);

   if (ctx->background_list)
     {
        EINA_LIST_FREE(ctx->background_list, item)
          {
             if (item->path)
               eina_stringshare_del(item->path);
             free(item);

          }
        ctx->background_list = NULL;
     }
   item_class = elm_gengrid_item_class_new();
   item_class->func.text_get = _grid_text_get;
   item_class->func.content_get = _grid_content_get;

   item = calloc(1, sizeof(Background_Item));
   ctx->background_list = eina_list_append(ctx->background_list, item);

   //Insert None Item
   Insert_Gen_Grid_Item_Notify *notify = calloc(1,
                                         sizeof(Insert_Gen_Grid_Item_Notify));
   if (notify)
     {
        notify->class = item_class;
        notify->item = item;

        _insert_gengrid_item(ctx, notify);
     }

   ctx->background_list = _rec_read_directorys(ctx, ctx->background_list, data,
                                          item_class);

   elm_gengrid_item_class_free(item_class);
}

static void
_gengrid_refresh_samples(Background_Ctx *ctx, const char *path)
{
   if(!ecore_file_exists(path))
      return;
   _refresh_directory(ctx, path);
}

static void
_cb_entry_changed(void *data,
                  Evas_Object *parent,
                  void *_event EINA_UNUSED)
{
   Background_Ctx *ctx = data;
   const char *path = elm_object_text_get(parent);
   _gengrid_refresh_samples(ctx, path);
}

static void
_cb_hoversel_select(Background_Ctx *ctx, const Eina_Stringshare *path)
{
   Evas_Object *o;
   if (path)
     {
        elm_flip_go_to(ctx->flip, EINA_TRUE, ELM_FLIP_PAGE_LEFT);
        elm_object_text_set(ctx->entry, path);
     }
   else
     {
        o = elm_object_part_content_get(ctx->flip, "back");
        elm_fileselector_path_set(o, elm_object_text_get(ctx->entry));
        elm_flip_go_to(ctx->flip, EINA_FALSE, ELM_FLIP_PAGE_RIGHT);
     }
}

static void
_cb_hoversel_select_system(void *data,
                           Evas_Object *obj EINA_UNUSED,
                           void *event_info EINA_UNUSED)
{
   Background_Ctx *ctx = data;

   _cb_hoversel_select(ctx, ctx->system_path);
}
static void
_cb_hoversel_select_user(void *data,
                         Evas_Object *obj EINA_UNUSED,
                         void *event_info EINA_UNUSED)
{
   Background_Ctx *ctx = data;

   _cb_hoversel_select(ctx, ctx->user_path);
}

static void
_cb_hoversel_select_none(void *data,
                         Evas_Object *obj EINA_UNUSED,
                         void *event_info EINA_UNUSED)
{
   Background_Ctx *ctx = data;
   _cb_hoversel_select(ctx, NULL);
}

static void
_system_background_dir_init(Background_Ctx *ctx)
{
   char path[PATH_MAX];

   snprintf(path, PATH_MAX, "%s/backgrounds/", elm_app_data_dir_get());
   if (ctx->system_path)
     eina_stringshare_replace(&ctx->system_path, path);
   else
     ctx->system_path = eina_stringshare_add(path);
}

static const char*
_user_background_dir_init(Background_Ctx *ctx)
{
   char path[PATH_MAX], *user;

   user = getenv("HOME");
   if(!user)
      return NULL;
   snprintf(path, PATH_MAX, "%s/.config/terminology/background/", user);
   if (!ecore_file_exists(path))
     ecore_file_mkpath(path);
   if (!ctx->user_path)
     ctx->user_path = eina_stringshare_add(path);
   else
     eina_stringshare_replace(&ctx->user_path, path);
   return ctx->user_path;
}

static const char*
_import_background(Background_Ctx *ctx, const char* background)
{
   char path[PATH_MAX];
   const char *filename = ecore_file_file_get(background);

   if (!filename)
     return NULL;
   if (!_user_background_dir_init(ctx))
     return NULL;
   snprintf(path, PATH_MAX, "%s/%s", ctx->user_path, filename);
   if (!ecore_file_cp(background, path))
     return NULL;
   return eina_stringshare_add(path);
}

static void
_cb_grid_doubleclick(void *data,
                     Evas_Object *_obj EINA_UNUSED,
                     void *_event EINA_UNUSED)
{
   Background_Ctx *ctx = data;
   Config *config = ctx->config;
   char *config_background_dir = ecore_file_dir_get(config->background);

   if (!ctx->user_path) {
     if (!_user_background_dir_init(ctx))
       return;
   }
   if (!config->background)
     return;
   if (strncmp(config_background_dir, ctx->user_path,
               strlen(config_background_dir)) == 0)
     {
        _bubble_show(ctx, _("Source file is target file"));
        free(config_background_dir);
        return;
     }

   const char *newfile = _import_background(ctx, config->background);

   if (newfile)
     {
        eina_stringshare_replace(&(config->background), newfile);
        config_save(config, NULL);
        main_media_update(config);
        eina_stringshare_del(newfile);
        _bubble_show(ctx, _("Picture imported"));
        elm_object_text_set(ctx->entry, config_background_dir);
     }
   else
     {
       _bubble_show(ctx, _("Failed"));
     }
   free(config_background_dir);
}

static void
_parent_del_cb(void *data,
               Evas *_e EINA_UNUSED,
               Evas_Object *_obj EINA_UNUSED,
               void *_event_info EINA_UNUSED)
{
   Background_Ctx *ctx = data;
   Background_Item *item;

   EINA_LIST_FREE(ctx->background_list, item)
     {
        if (item->path)
          eina_stringshare_del(item->path);
        free(item);
     }
   ctx->background_list = NULL;
   if (ctx->user_path)
     {
        eina_stringshare_del(ctx->user_path);
        ctx->user_path = NULL;
     }
   if (ctx->system_path)
     {
        eina_stringshare_del(ctx->system_path);
        ctx->system_path = NULL;
     }
     free(ctx);
}


void
options_background(Evas_Object *opbox, Evas_Object *term)
{
   Evas_Object *o, *bx, *bx_front;
   Config *config = termio_config_get(term);
   char path[PATH_MAX], *config_background_dir;
   Background_Ctx *ctx;

   ctx = calloc(1, sizeof(*ctx));
   assert(ctx);

   ctx->config = config;
   ctx->term = term;

   ctx->frame = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Background"));
   evas_object_show(o);
   elm_box_pack_end(opbox, o);

   evas_object_event_callback_add(ctx->frame, EVAS_CALLBACK_DEL,
                                  _parent_del_cb, ctx);

   bx = o = elm_box_add(ctx->frame);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(ctx->frame, bx);
   evas_object_show(o);

   o = elm_label_add(opbox);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, _("Shine:"));
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   ctx->op_shine_slider = o = elm_slider_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_slider_span_size_set(o, 40);
   elm_slider_unit_format_set(o, "%1.0f");
   elm_slider_indicator_format_set(o, "%1.0f");
   elm_slider_min_max_set(o, 0, 255);
   elm_slider_step_set(o, 1);
   elm_slider_value_set(o, config->shine);
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   evas_object_smart_callback_add(o, "delay,changed",
                                  _cb_op_shine_sel, ctx);



   ctx->op_trans = o = elm_check_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Translucent"));
   elm_check_state_set(o, config->translucent);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_trans_chg, ctx);

   ctx->op_opacity = o = elm_slider_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_slider_span_size_set(o, 40);
   elm_slider_unit_format_set(o, _("%1.0f%%"));
   elm_slider_indicator_format_set(o, _("%1.0f%%"));
   elm_slider_min_max_set(o, 0, 100);
   elm_slider_value_set(o, config->opacity);
   elm_object_disabled_set(o, !config->translucent);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_video_opacity_chg, ctx);

   o = elm_separator_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_separator_horizontal_set(o, EINA_TRUE);
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_hoversel_add(opbox);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_object_text_set(o, _("Select Path"));
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   snprintf(path, PATH_MAX, "%s/backgrounds/", elm_app_data_dir_get());
   _system_background_dir_init(ctx);
   elm_hoversel_item_add(o, _("System"), NULL, ELM_ICON_NONE,
                         _cb_hoversel_select_system, ctx);
   if (_user_background_dir_init(ctx))
     elm_hoversel_item_add(o, _("User"), NULL, ELM_ICON_NONE,
                           _cb_hoversel_select_user, ctx);
   //In the other case it has failed, so dont show the user item
   elm_hoversel_item_add(o, _("Other"), NULL, ELM_ICON_NONE,
                         _cb_hoversel_select_none, ctx);

   ctx->flip = o = elm_flip_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   bx_front = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_box_horizontal_set(o, EINA_FALSE);
   elm_object_part_content_set(ctx->flip, "front", o);
   evas_object_show(o);

   ctx->entry = o = elm_entry_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_scroller_policy_set(o, ELM_SCROLLER_POLICY_OFF,
                           ELM_SCROLLER_POLICY_OFF);
   evas_object_smart_callback_add(ctx->entry, "changed",
                                  _cb_entry_changed, ctx);
   elm_box_pack_end(bx_front, o);
   evas_object_show(o);

   ctx->bg_grid = o = elm_gengrid_add(opbox);
   evas_object_data_set(ctx->bg_grid, "ctx", ctx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(o, "clicked,double",
                                  _cb_grid_doubleclick, ctx);
   elm_gengrid_item_size_set(o, elm_config_scale_get() * 100,
                                elm_config_scale_get() * 80);
   elm_box_pack_end(bx_front, o);
   evas_object_show(o);

   o = elm_label_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_object_text_set(o, _("Click on a picture to use it as background"));
   elm_box_pack_end(bx_front, o);
   evas_object_show(o);



   o = elm_fileselector_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_part_content_set(ctx->flip, "back", o);
   elm_fileselector_folder_only_set(o, EINA_TRUE);
   evas_object_smart_callback_add(o, "done", _cb_fileselector, ctx);
   evas_object_show(o);

   if (config->background)
     {
        config_background_dir = ecore_file_dir_get(config->background);
        elm_object_text_set(ctx->entry, config_background_dir);
        free(config_background_dir);
     }
   else
     {
        elm_object_text_set(ctx->entry, ctx->system_path);
     }
}
