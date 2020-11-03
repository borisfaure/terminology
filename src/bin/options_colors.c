#include "private.h"

#include <Elementary.h>
#include <assert.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "colors.h"
#include "options_colors.h"
#include "options_themepv.h"

typedef struct _Color_Scheme_Ctx
{
   Evas_Object *term;
   Config *config;
   Evas_Coord pv_width;
   Evas_Coord pv_height;
   Eina_List *cs_infos;
} Color_Scheme_Ctx;

typedef struct _Color_Scheme_Info
{
   Color_Scheme_Ctx *ctx;
   Elm_Object_Item *item;
   Color_Scheme *cs;
} Color_Scheme_Info;

static char *
_cb_op_cs_name_get(void *data,
                   Evas_Object *obj EINA_UNUSED,
                   const char *part EINA_UNUSED)
{
   Color_Scheme_Info *csi = data;
   Color_Scheme *cs = csi->cs;

   return strdup(cs->md.name);
}

static Evas_Object *
_cb_op_cs_content_get(void *data, Evas_Object *obj, const char *part)
{
   Color_Scheme_Info *csi = data;

   if (strncmp(part, "elm.swallow.icon", sizeof("elm.swallow.icon") -1) == 0)
     {
        Config *config = csi->ctx->config;
        Evas_Object *o;

        if (!config)
          return NULL;

        o = options_theme_preview_add(obj, config,
                                      NULL,
                                      csi->cs,
                                      csi->ctx->pv_width,
                                      csi->ctx->pv_height,
                                      EINA_TRUE);
        return o;
     }

   return NULL;
}

static void
_cb_op_color_scheme_sel(void *data,
                        Evas_Object *_obj EINA_UNUSED,
                        void *_event EINA_UNUSED)
{
   Color_Scheme_Info *csi = data;
   Config *config = csi->ctx->config;

   ERR("TODO: csi sel %p cfg:%p", csi, config);
#if 0
   if ((config->theme) && (!strcmp(t->name, config->theme)))
     return;

   eina_stringshare_replace(&(config->theme), t->name);
   config_save(config);
   change_theme(termio_win_get(t->ctx->term), config);
#endif
}

static void
_parent_del_cb(void *data,
               Evas *_e EINA_UNUSED,
               Evas_Object *_obj EINA_UNUSED,
               void *_event_info EINA_UNUSED)
{
   Color_Scheme_Ctx *ctx = data;
   Color_Scheme_Info *csi;

   EINA_LIST_FREE(ctx->cs_infos, csi)
     {
        free(csi->cs);
        free(csi);
     }
   free(ctx);
}

void
options_colors(Evas_Object *opbox, Evas_Object *term)
{
   Evas_Object *o, *box, *fr;
   Elm_Gengrid_Item_Class *it_class;
   Eina_List *schemes, *l, *l_next;
   Color_Scheme *cs;
   Color_Scheme_Ctx *ctx;
   Config *config = termio_config_get(term);
   int chw = 10, chh = 10;

   termio_character_size_get(term, &chw, &chh);
   ctx = calloc(1, sizeof(*ctx));
   assert(ctx);

   ctx->config = config;
   ctx->term = term;
   ctx->pv_width = (COLOR_MODE_PREVIEW_WIDTH+0) * chw;
   ctx->pv_height = (COLOR_MODE_PREVIEW_HEIGHT+0) * chh;
   ERR("w:%d, h:%d", ctx->pv_width, ctx->pv_height);
   if (ctx->pv_width >= ctx->pv_height)
     ctx->pv_height = ctx->pv_width;
   else
     ctx->pv_width = ctx->pv_height;

   fr = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Color schemes"));
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
   it_class->func.text_get = _cb_op_cs_name_get;
   it_class->func.content_get = _cb_op_cs_content_get;

   o = elm_gengrid_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   //elm_gengrid_item_size_set(o, ctx->pv_width + 10, ctx->pv_height + 10);

   schemes = color_scheme_list();

   EINA_LIST_FOREACH_SAFE(schemes, l, l_next, cs)
     {
        Color_Scheme_Info *csi;

        csi = calloc(1, sizeof(Color_Scheme_Info));
        if (!csi)
          break;
        csi->ctx = ctx;
        csi->cs = cs;
        csi->item = elm_gengrid_item_append(o, it_class, csi,
                                          _cb_op_color_scheme_sel, csi);
        if (csi->item)
          {
             ctx->cs_infos = eina_list_append(ctx->cs_infos, csi);

#if 0
             if ((config) && (config->theme) &&
                 (!strcmp(config->theme, t->name)))
               {
                  if (ctx->seltimer)
                    ecore_timer_del(ctx->seltimer);
                  ctx->seltimer = ecore_timer_add(0.2, _cb_sel_item, t);
               }
#endif
          }
        else
          {
             free(csi);
             free(cs);
          }
        schemes = eina_list_remove_list(schemes, l);
     }

   elm_gengrid_item_class_free(it_class);

   elm_box_pack_end(box, o);
   evas_object_size_hint_weight_set(opbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(opbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(o);
}
