#include "private.h"

#include <Elementary.h>
#include <assert.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_colors.h"


static const char mapping[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11 };
static const char *mapping_names[] =
  {
     gettext_noop("Default"),
     gettext_noop("Black"),
     gettext_noop("Red"),
     gettext_noop("Green"),
     gettext_noop("Yellow"),
     gettext_noop("Blue"),
     gettext_noop("Magenta"),
     gettext_noop("Cyan"),
     gettext_noop("White"),
     gettext_noop("Inverse"),
     gettext_noop("Inverse Base")
  };

typedef struct _Colors_Ctx {
     Elm_Object_Item *colitem[4][11];
     Evas_Object *colorsel;
     Elm_Object_Item *curitem;
     Evas_Object *colpal[4];
     Evas_Object *label;
     Evas_Object *reset;
     Config *config;
     Evas_Object *term;
} Colors_Ctx;

static void
_cb_op_use_custom_chg(void *data,
                      Evas_Object *obj,
                      void *_event EINA_UNUSED)
{
   Colors_Ctx *ctx = data;
   Evas_Object *term = ctx->term;
   Config *config = ctx->config;
   Eina_Bool state = EINA_FALSE;
   int i;

   state = elm_check_state_get(obj);
   elm_object_disabled_set(ctx->colorsel, !state);

   for (i = 0; i < 4; i++)
     elm_object_disabled_set(ctx->colpal[i], !state);

   elm_object_disabled_set(ctx->label, !state);
   config->colors_use = state;
   termio_config_update(term);
   config_save(config, NULL);
}

static void
_cb_op_color_item_sel(void *data,
                      Evas_Object *_obj EINA_UNUSED,
                      void *event)
{
   Colors_Ctx *ctx = data;
   Elm_Object_Item *it = event;
   int r = 0, g = 0, b = 0, a = 0;
   int i, j;

   ctx->curitem = it;
   elm_colorselector_palette_item_color_get(it, &r, &g, &b, &a);
   elm_colorselector_color_set(ctx->colorsel, r, g, b, a);
   for (j = 0; j < 4; j++)
     {
        for (i = 0; i < 11; i++)
          {
             if (ctx->colitem[j][i] == it)
               elm_object_text_set(ctx->label,
#if HAVE_GETTEXT && ENABLE_NLS
                                   gettext(mapping_names[i])
#else
                                   mapping_names[i]
#endif
                                   );
          }
     }
}

static void
_cb_op_color_chg(void *data,
                 Evas_Object *obj,
                 void *_event EINA_UNUSED)
{
   Colors_Ctx *ctx = data;
   Config *config = ctx->config;
   int r = 0, g = 0, b = 0, a = 0, rr = 0, gg = 0, bb = 0, aa = 0;
   int i, j;

   elm_colorselector_palette_item_color_get(ctx->curitem, &rr, &gg, &bb, &aa);
   elm_colorselector_color_get(obj, &r, &g, &b, &a);
   if ((r != rr) || (g != gg) || (b != bb) || (a != aa))
     {
        if (ctx->curitem)
          elm_colorselector_palette_item_color_set(ctx->curitem, r, g, b, a);
        elm_object_disabled_set(ctx->reset, EINA_FALSE);
        for (j = 0; j < 4; j++)
          {
             for (i = 0; i < 11; i++)
               {
                  if (ctx->colitem[j][i] == ctx->curitem)
                    {
                       config->colors[(j * 12) + mapping[i]].r = r;
                       config->colors[(j * 12) + mapping[i]].g = g;
                       config->colors[(j * 12) + mapping[i]].b = b;
                       config->colors[(j * 12) + mapping[i]].a = a;
                       termio_config_update(ctx->term);
                       config_save(config, NULL);
                       return;
                    }
               }
          }
     }
}

static void
_cb_op_reset(void *data,
             Evas_Object *_obj EINA_UNUSED,
             void *_event EINA_UNUSED)
{
   Colors_Ctx *ctx = data;
   Evas_Object *term = ctx->term;
   Config *config = ctx->config;
   int r = 0, g = 0, b = 0, a = 0;
   int i, j;

   for (j = 0; j < 4; j++)
     {
        for (i = 0; i < 12; i++)
          {
             unsigned char rr = 0, gg = 0, bb = 0, aa = 0;

             colors_standard_get(j, i, &rr, &gg, &bb, &aa);
             config->colors[(j * 12) + i].r = rr;
             config->colors[(j * 12) + i].g = gg;
             config->colors[(j * 12) + i].b = bb;
             config->colors[(j * 12) + i].a = aa;
          }
        for (i = 0; i < 11; i++)
          {
             elm_colorselector_palette_item_color_set
               (ctx->colitem[j][i],
                config->colors[(j * 12) + mapping[i]].r,
                config->colors[(j * 12) + mapping[i]].g,
                config->colors[(j * 12) + mapping[i]].b,
                config->colors[(j * 12) + mapping[i]].a);
          }
     }
   elm_object_disabled_set(ctx->reset, EINA_TRUE);
   elm_colorselector_palette_item_color_get(ctx->curitem, &r, &g, &b, &a);
   elm_colorselector_color_set(ctx->colorsel, r, g, b, a);
   termio_config_update(term);
   config_save(config, NULL);
}

/* make color palettes wrap back. :) works with elm git. */
static void
_cb_op_scroller_resize(void *data,
                       Evas *_e EINA_UNUSED,
                       Evas_Object *_obj EINA_UNUSED,
                       void *_event EINA_UNUSED)
{
   Colors_Ctx *ctx = data;
   int i;

   for (i = 0; i < 4; i++)
     evas_object_resize(ctx->colpal[i], 1, 1);
}

static void
_parent_del_cb(void *data,
               Evas *_e EINA_UNUSED,
               Evas_Object *_obj EINA_UNUSED,
               void *_event_info EINA_UNUSED)
{
   Colors_Ctx *ctx = data;

   free(ctx);
}

void
options_colors(Evas_Object *opbox, Evas_Object *term)
{
   Config *config = termio_config_get(term);
   Evas_Object *o, *fr, *bx, *sc, *bx2, *bx3, *bx4;
   int i, j;
   int r = 0, g = 0, b = 0, a = 0;
   Colors_Ctx *ctx;

   ctx = calloc(1, sizeof(*ctx));
   assert(ctx);

   ctx->config = config;
   ctx->term = term;

   fr = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Colors"));
   elm_box_pack_end(opbox, o);
   evas_object_show(o);

   evas_object_event_callback_add(fr, EVAS_CALLBACK_DEL,
                                  _parent_del_cb, ctx);

   bx = o = elm_box_add(opbox);
   elm_box_horizontal_set(o, EINA_TRUE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_object_content_set(fr, o);
   evas_object_show(o);

   sc = o = elm_scroller_add(opbox);
   evas_object_event_callback_add(o, EVAS_CALLBACK_RESIZE,
                                  _cb_op_scroller_resize, ctx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   bx3 = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_object_content_set(sc, o);
   evas_object_show(o);

   for (j = 0; j < 4; j++)
     {
        o = elm_label_add(opbox);
        if (j == 0) elm_object_text_set(o, _("Normal"));
        else if (j == 1) elm_object_text_set(o, _("Bright/Bold"));
        else if (j == 2) elm_object_text_set(o, _("Intense"));
        else if (j == 3) elm_object_text_set(o, _("Intense Bright/Bold"));
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
        elm_box_pack_end(bx3, o);
        evas_object_show(o);

        ctx->colpal[j] = o = elm_colorselector_add(opbox);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
        elm_colorselector_mode_set(o, ELM_COLORSELECTOR_PALETTE);
        for (i = 0; i < 11; i++)
          {
             Elm_Object_Item *it;

             it = elm_colorselector_palette_color_add
               (o,
                config->colors[(j * 12) + mapping[i]].r,
                config->colors[(j * 12) + mapping[i]].g,
                config->colors[(j * 12) + mapping[i]].b,
                config->colors[(j * 12) + mapping[i]].a);
             ctx->colitem[j][i] = it;
          }
        evas_object_smart_callback_add(o, "color,item,selected",
                                       _cb_op_color_item_sel, ctx);
        elm_box_pack_end(bx3, o);
        evas_object_show(o);
        if (j == 1)
          {
             o = elm_separator_add(opbox);
             evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
             evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
             elm_separator_horizontal_set(o, EINA_TRUE);
             elm_box_pack_end(bx3, o);
             evas_object_show(o);
          }
     }

   ctx->curitem = ctx->colitem[0][0];

   bx2 = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   ctx->label = o = elm_label_add(opbox);
   elm_object_text_set(o,
#if HAVE_GETTEXT && ENABLE_NLS
                       gettext(mapping_names[0])
#else
                       mapping_names[0]
#endif
                       );
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(bx2, o);
   evas_object_show(o);

   ctx->colorsel = o = elm_colorselector_add(opbox);
   elm_colorselector_palette_item_color_get(ctx->colitem[0][0], &r, &g, &b, &a);
   elm_colorselector_color_set(o, r, g, b, a);
   elm_colorselector_mode_set(o, ELM_COLORSELECTOR_COMPONENTS);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(bx2, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed", _cb_op_color_chg, ctx);

   bx4 = o = elm_box_add(opbox);
   elm_box_horizontal_set(o, EINA_TRUE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(bx2, o);
   evas_object_show(o);

   o = elm_check_add(opbox);
   evas_object_size_hint_weight_set(o, 1.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, _("Use"));
   elm_check_state_set(o, config->colors_use);
   elm_box_pack_end(bx4, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed", _cb_op_use_custom_chg, ctx);

   ctx->reset = o = elm_button_add(opbox);
   elm_object_disabled_set(o, EINA_TRUE);
   evas_object_size_hint_weight_set(o, 1.0, 0.0);
   evas_object_size_hint_align_set(o, 1.0, 0.5);
   elm_object_text_set(o, _("Reset"));
   elm_box_pack_end(bx4, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "clicked", _cb_op_reset, ctx);
}
