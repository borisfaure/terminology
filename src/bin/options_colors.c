#include "private.h"

#include <Elementary.h>
#include <assert.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "colors.h"
#include "options_colors.h"
#include "options_themepv.h"
#include "utils.h"

typedef struct tag_Color_Scheme_Ctx
{
   Evas_Object *term;
   Config *config;
   Evas_Coord pv_width;
   Evas_Coord pv_height;
   Eina_List *cs_infos;
   Ecore_Timer *seltimer;
   Evas_Object *ctxpopup;
} Color_Scheme_Ctx;

typedef struct tag_Color_Scheme_Info
{
   Color_Scheme_Ctx *ctx;
   Elm_Object_Item *item;
   Color_Scheme *cs;
   const char *tooltip;
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

static void
_cb_ctxp_del(void *data,
             Evas *_e EINA_UNUSED,
             Evas_Object *_obj EINA_UNUSED,
             void *_event EINA_UNUSED)
{
   Color_Scheme_Info *csi = data;
   EINA_SAFETY_ON_NULL_RETURN(csi);
   csi->ctx->ctxpopup = NULL;
}

static void
_cb_ctxp_dismissed(void *data,
                   Evas_Object *obj,
                   void *_event EINA_UNUSED)
{
   Color_Scheme_Info *csi = data;
   EINA_SAFETY_ON_NULL_RETURN(csi);
   csi->ctx->ctxpopup = NULL;
   evas_object_del(obj);
}

static void
_cb_ctxp_open_website(void *data,
                      Evas_Object *obj,
                      void *_event EINA_UNUSED)
{
   Color_Scheme_Info *csi = data;
   Color_Scheme_Ctx *ctx;

   EINA_SAFETY_ON_NULL_RETURN(csi);
   ctx = csi->ctx;

   open_url(ctx->config, csi->cs->md.website);

   ctx->ctxpopup = NULL;
   evas_object_del(obj);
}

static void
_handle_mouse_down(void *data,
                   Evas *_e EINA_UNUSED,
                   Evas_Object *obj,
                   void *event)
{
   Evas_Event_Mouse_Down *ev = event;
   Color_Scheme_Info *csi = data;
   Evas_Object *ctxp;

   if (ev->button != 3 || csi->cs->md.website == NULL ||
       csi->cs->md.website[0] == '\0')
     return;

   ctxp = elm_ctxpopup_add(obj);
   csi->ctx->ctxpopup = ctxp;

   elm_ctxpopup_item_append(ctxp, _("Open website"), NULL,
                            _cb_ctxp_open_website, csi);
   evas_object_move(ctxp, ev->canvas.x, ev->canvas.y);
   evas_object_show(ctxp);
   evas_object_smart_callback_add(ctxp, "dismissed",
                                  _cb_ctxp_dismissed, csi);
   evas_object_event_callback_add(ctxp, EVAS_CALLBACK_DEL,
                                  _cb_ctxp_del, csi);
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
        if (!csi->tooltip)
          {
             csi->tooltip = eina_stringshare_printf(
                _("<b>Author: </b>%s<br/>"
                "<b>Website: </b>%s<br/>"
                "<b>License: </b>%s"),
                csi->cs->md.author, csi->cs->md.website, csi->cs->md.license);
          }
        elm_object_tooltip_text_set(o, csi->tooltip);
        evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                       _handle_mouse_down, csi);
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
   Color_Scheme *cs = csi->cs;
   Config *config = csi->ctx->config;

   if ((config->color_scheme_name) &&
       (!strcmp(cs->md.name, config->color_scheme_name)))
     return;

   eina_stringshare_replace(&(config->color_scheme_name), cs->md.name);
   free((void*)config->color_scheme);
   config->color_scheme = color_scheme_dup(cs);

   config_save(config);
   change_theme(termio_win_get(csi->ctx->term), config);
}

static Eina_Bool
_cb_sel_item(void *data)
{
   Color_Scheme_Info *csi = data;

   if (csi)
     {
        elm_gengrid_item_selected_set(csi->item, EINA_TRUE);
        elm_gengrid_item_bring_in(csi->item, ELM_GENGRID_ITEM_SCROLLTO_MIDDLE);
        csi->ctx->seltimer = NULL;
     }
   return EINA_FALSE;
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
        eina_stringshare_del(csi->tooltip);
        free(csi);
     }
   ecore_timer_del(ctx->seltimer);
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

   if (config && config->theme)
     {
        char theme[4096];
        const char *start = strrchr(config->theme, '/');
        const char *end;
        size_t len;

        if (start)
          start++;
        else
          start = config->theme;

        end = strrchr(start, '.');
        if (!end)
          end = start + strlen(start) - 1;
        len = end - start;
        if (len < sizeof(theme))
          {
             char buf[4096 + 1024];

             strncpy(theme, start, len);
             theme[len] = '\0';
             o = elm_label_add(opbox);
             evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0);
             evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
             snprintf(buf, sizeof(buf), _("Using theme <hilight>%s</hilight>"),
                      theme);
             elm_object_text_set(o, buf);
             elm_box_pack_end(box, o);
             evas_object_show(o);
          }
     }


   it_class = elm_gengrid_item_class_new();
   it_class->item_style = "thumb";
   it_class->func.text_get = _cb_op_cs_name_get;
   it_class->func.content_get = _cb_op_cs_content_get;

   o = elm_gengrid_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

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

             if ((config) && (config->color_scheme_name) &&
                 (!strcmp(config->color_scheme_name, cs->md.name)))
               {
                  if (ctx->seltimer)
                    ecore_timer_del(ctx->seltimer);
                  ctx->seltimer = ecore_timer_add(0.2, _cb_sel_item, csi);
               }
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
