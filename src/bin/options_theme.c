#include "private.h"

#include <Elementary.h>
#include <Efreet.h>
#include <assert.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_theme.h"
#include "options_themepv.h"
#include "utils.h"
#include "main.h"

typedef struct _Theme_Ctx
{
   Evas_Object *term;
   Config *config;
   Evas_Object *op_themelist;
   Eina_List *themes;
   Ecore_Timer *seltimer;
} Theme_Ctx;

typedef struct _Theme Theme;
struct _Theme
{
   Elm_Object_Item *item;
   const char *name;
   Theme_Ctx *ctx;
};

static char *
_cb_op_theme_text_get(void *data,
                      Evas_Object *_obj EINA_UNUSED,
                      const char *_part EINA_UNUSED)
{
   Theme *t = data;
   char buf[4096], *p;

   eina_strlcpy(buf, t->name, sizeof(buf));
   p = strrchr(buf, '.');
   if (p)
     *p = 0;

   return strdup(buf);
}

static Evas_Object *
_cb_op_theme_content_get(void *data, Evas_Object *obj, const char *part)
{
   Theme *t = data;

   if (!strcmp(part, "elm.swallow.icon"))
     {
        Evas_Object *o;
        Config *config = t->ctx->config;

        if (config)
          {
             o = options_theme_preview_add(obj, config,
                                           theme_path_get(t->name),
                                           128 * elm_config_scale_get(),
                                           64 * elm_config_scale_get());
             return o;
          }
     }

   return NULL;
}

static void
_cb_op_theme_sel(void *data,
                 Evas_Object *_obj EINA_UNUSED,
                 void *_event EINA_UNUSED)
{
   Theme *t = data;
   Config *config = t->ctx->config;

   if ((config->theme) && (!strcmp(t->name, config->theme)))
     return;

   eina_stringshare_replace(&(config->theme), t->name);
   config_save(config, NULL);
   change_theme(termio_win_get(t->ctx->term), config);
}

static int
_cb_op_theme_sort(const void *d1, const void *d2)
{
   return strcasecmp(d1, d2);
}

static Eina_Bool
_cb_sel_item(void *data)
{
   Theme *t = data;

   if (t)
     {
        elm_gengrid_item_selected_set(t->item, EINA_TRUE);
        elm_gengrid_item_bring_in(t->item, ELM_GENGRID_ITEM_SCROLLTO_MIDDLE);
        t->ctx->seltimer = NULL;
     }
   return EINA_FALSE;
}

static void
_parent_del_cb(void *data,
               Evas *_e EINA_UNUSED,
               Evas_Object *_obj EINA_UNUSED,
               void *_event_info EINA_UNUSED)
{
   Theme_Ctx *ctx = data;
   Theme *t;

   ctx->op_themelist = NULL;

   if (ctx->seltimer)
     {
        ecore_timer_del(ctx->seltimer);
        ctx->seltimer = NULL;
     }
   EINA_LIST_FREE(ctx->themes, t)
     {
        eina_stringshare_del(t->name);
        free(t);
     }

   free(ctx);
}

void
options_theme(Evas_Object *opbox, Evas_Object *term)
{
   Evas_Object *o, *box, *fr;
   Elm_Gengrid_Item_Class *it_class;
   Eina_List *files, *userfiles, *l, *l_next;
   char buf[4096], *file;
   const char *config_dir = efreet_config_home_get(),
              *data_dir = elm_app_data_dir_get();
   Config *config = termio_config_get(term);
   Eina_Bool to_skip = EINA_FALSE;
   double scale = elm_config_scale_get();
   Theme_Ctx *ctx;

   ctx = calloc(1, sizeof(*ctx));
   assert(ctx);

   ctx->config = config;
   ctx->term = term;

   fr = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Theme"));
   elm_box_pack_end(opbox, o);
   evas_object_show(o);

   evas_object_event_callback_add(fr, EVAS_CALLBACK_DEL,
                                  _parent_del_cb, ctx);

   box = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(o, EINA_FALSE);
   elm_object_content_set(fr, o);
   evas_object_show(o);

   it_class = elm_gengrid_item_class_new();
   it_class->item_style = "thumb";
   it_class->func.text_get = _cb_op_theme_text_get;
   it_class->func.content_get = _cb_op_theme_content_get;

   ctx->op_themelist = o = elm_gengrid_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_gengrid_item_size_set(o, scale * 160, scale * 180);

   snprintf(buf, sizeof(buf), "%s/themes", data_dir);
   files = ecore_file_ls(buf);
   if (files)
     files = eina_list_sort(files, eina_list_count(files),
                            _cb_op_theme_sort);

   snprintf(buf, sizeof(buf), "%s/terminology/themes", config_dir);
   userfiles = ecore_file_ls(buf);
   if (userfiles)
     userfiles = eina_list_sort(userfiles, eina_list_count(userfiles),
                            _cb_op_theme_sort);

   if (files && userfiles)
     files = eina_list_sorted_merge(files, userfiles, _cb_op_theme_sort);
   else if (userfiles)
     files = userfiles;

   if (ctx->seltimer)
     {
        ecore_timer_del(ctx->seltimer);
        ctx->seltimer = NULL;
     }

   EINA_LIST_FOREACH_SAFE(files, l, l_next, file)
     {
        const char *ext = strchr(file, '.');

        if (!((config) && (file[0] != '.') &&
              ((ext) && (!strcasecmp(".edj", ext)))))
          {
             free(file);
             files = eina_list_remove_list(files, l);
          }
     }

   EINA_LIST_FOREACH_SAFE(files, l, l_next, file)
     {
        Theme *t;
        if (to_skip == EINA_TRUE)
          {
             to_skip = EINA_FALSE;
             goto end_loop;
          }

        if (l_next && l_next->data && !strcmp(file, l_next->data))
          {
             to_skip = EINA_TRUE;
          }

        t = calloc(1, sizeof(Theme));
        if (!t)
          break;
        t->name = eina_stringshare_add(file);
        t->ctx = ctx;
        t->item = elm_gengrid_item_append(o, it_class, t,
                                          _cb_op_theme_sel, t);
        if (t->item)
          {
             ctx->themes = eina_list_append(ctx->themes, t);
             if ((config) && (config->theme) &&
                 (!strcmp(config->theme, t->name)))
               {
                  if (ctx->seltimer)
                    ecore_timer_del(ctx->seltimer);
                  ctx->seltimer = ecore_timer_add(0.2, _cb_sel_item, t);
               }
          }
        else
          {
             eina_stringshare_del(t->name);
             free(t);
          }
     end_loop:
        files = eina_list_remove_list(files, l);
        free(file);
     }

   elm_gengrid_item_class_free(it_class);

   elm_box_pack_end(box, o);
   evas_object_size_hint_weight_set(opbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(opbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(o);
}
