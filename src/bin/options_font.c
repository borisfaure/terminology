#include "private.h"

#include <Elementary.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_font.h"
#include "utils.h"

#define TEST_STRING "oislOIS.015!|,"

static Evas_Object *op_fontslider, *op_fontlist, *op_fsml, *op_fbig;

typedef struct _Font Font;

struct _Font
{
   Elm_Object_Item *item;
   const char *pretty_name;
   const char *full_name;
   Evas_Object *term;
   Eina_Bool bitmap : 1;
};

static Eina_List *fonts = NULL;
static Eina_Hash *fonthash = NULL;
static Evas_Coord tsize_w = 0, tsize_h = 0;
static int expecting_resize = 0;

static void
_update_sizing(Evas_Object *term)
{
   Evas_Coord mw = 1, mh = 1, w, h;

   termio_config_update(term);
   evas_object_size_hint_min_get(term, &mw, &mh);
   if (mw < 1) mw = 1;
   if (mh < 1) mh = 1;
   w = tsize_w / mw;
   h = tsize_h / mh;
   evas_object_size_hint_request_set(term, w * mw, h * mh);
   expecting_resize = 1;
}

static int
_parse_font_name(const char *full_name,
                 const char **name, const char **pretty_name)
{
   char buf[4096];
   size_t style_len = 0;
   size_t font_len = 0;
   char *style = NULL;
   char *s;
   unsigned int len;

   *pretty_name = NULL;
   *name = NULL;

   font_len = strlen(full_name);
   if (font_len >= sizeof(buf))
     return -1;
   style = strchr(full_name, ':');
   if (style)
     font_len = (size_t)(style - full_name);

   s = strchr(full_name, ',');
   if (s && style && s < style)
     font_len = s - full_name;

#define STYLE_STR ":style="
   if (style && strncmp(style, STYLE_STR, strlen(STYLE_STR)) == 0)
     {
        style += strlen(STYLE_STR);
        s = strchr(style, ',');
        style_len = (s == NULL) ? strlen(style) : (size_t)(s - style);
     }

   s = buf;
   memcpy(s, full_name, font_len);
   s += font_len;
   len = font_len;
   if (style)
     {
        memcpy(s, STYLE_STR, strlen(STYLE_STR));
        s += strlen(STYLE_STR);
        len += strlen(STYLE_STR);

        memcpy(s, style, style_len);
        s += style_len;
        len += style_len;
     }
     *s = '\0';
   *name = eina_stringshare_add_length(buf, len);
#undef STYLE_STR

   /* unescape the dashes */
   s = buf;
   len = 0;
   while ( (size_t)(s - buf) < sizeof(buf) &&
           font_len > 0 )
     {
        if (*full_name != '\\')
          {
             *s++ = *full_name;
          }
        full_name++;
        font_len--;
        len++;
     }
   /* copy style */
   if (style_len > 0 && ((sizeof(buf) - (s - buf)) > style_len + 3 ))
     {
        *s++ = ' ';
        *s++ = '(';
        memcpy(s, style, style_len);
        s += style_len;
        *s++ = ')';

        len += 3 + style_len;
     }
     *s = '\0';

     *pretty_name = eina_stringshare_add_length(buf, len);
     return 0;
}

static void
_cb_op_font_sel(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Font *f = data;
   Config *config = termio_config_get(f->term);
   if ((config->font.name) && (!strcmp(f->full_name, config->font.name)))
     return;
   if (config->font.name) eina_stringshare_del(config->font.name);
   config->font.name = eina_stringshare_add(f->full_name);
   config->font.bitmap = f->bitmap;
   _update_sizing(f->term);
   elm_object_disabled_set(op_fsml, f->bitmap);
   elm_object_disabled_set(op_fontslider, f->bitmap);
   elm_object_disabled_set(op_fbig, f->bitmap);
   config_save(config, NULL);
}

static void
_cb_op_fontsize_sel(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   int size = elm_slider_value_get(obj) + 0.5;

   if (config->font.size == size) return;
   config->font.size = size;
   _update_sizing(term);
   elm_genlist_realized_items_update(op_fontlist);
   config_save(config, NULL);
}

static int
_cb_op_font_sort(const void *d1, const void *d2)
{
   return strcasecmp(d1, d2);
}

static void
_cb_op_font_preview_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *o;
   Ecore_Timer *timer = evas_object_data_get(obj, "delay");
        
   if (timer)
     {
        ecore_timer_del(timer);
        evas_object_data_del(obj, "delay");
     }

   o = edje_object_part_swallow_get(obj, "terminology.text.preview");
   if (o)
     {
        evas_object_del(o);
     }
}

static Eina_Bool
_cb_op_font_preview_delayed_eval(void *data)
{
   Evas_Object *obj = data;
   Font *f;
   Evas_Object *o;
   Evas_Coord ox, oy, ow, oh, vx, vy, vw, vh;
   Config *config;
   
   if (!evas_object_visible_get(obj)) goto done;
   if (edje_object_part_swallow_get(obj, "terminology.text.preview")) goto done;
   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   if ((ow < 2) || (oh < 2)) goto done;
   evas_output_viewport_get(evas_object_evas_get(obj), &vx, &vy, &vw, &vh);
   f = evas_object_data_get(obj, "font");
   if (!f) goto done;
   config = termio_config_get(f->term);
   if (!config) goto done;
   if (ELM_RECTS_INTERSECT(ox, oy, ow, oh, vx, vy, vw, vh))
     {
        char buf[4096];
        
        o = evas_object_text_add(evas_object_evas_get(obj));
        evas_object_color_set(o, 0, 0, 0, 255);
        evas_object_text_text_set(o, TEST_STRING);
        evas_object_scale_set(o, elm_config_scale_get());
        if (f->bitmap)
          {
             snprintf(buf, sizeof(buf), "%s/fonts/%s",
                      elm_app_data_dir_get(), f->full_name);
             evas_object_text_font_set(o, buf, config->font.size);
          }
        else
          evas_object_text_font_set(o, f->full_name, config->font.size);
        evas_object_geometry_get(o, NULL, NULL, &ow, &oh);
        evas_object_size_hint_min_set(o, ow, oh);
        edje_object_part_swallow(obj, "terminology.text.preview", o);
     }
done:
   evas_object_data_del(obj, "delay");
   return EINA_FALSE;
}

static void
_cb_op_font_preview_eval(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event EINA_UNUSED)
{
   Font *f = data;
   Evas_Coord ox, oy, ow, oh, vx, vy, vw, vh;
   
   if (!evas_object_visible_get(obj)) return;
   if (edje_object_part_swallow_get(obj, "terminology.text.preview")) return;
   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   if ((ow < 2) || (oh < 2)) return;
   evas_output_viewport_get(evas_object_evas_get(obj), &vx, &vy, &vw, &vh);
   if (ELM_RECTS_INTERSECT(ox, oy, ow, oh, vx, vy, vw, vh))
     {
        Ecore_Timer *timer;
        double rnd = 0.2;

        timer = evas_object_data_get(obj, "delay");
        if (timer) return;
        else evas_object_data_set(obj, "font", f);
        rnd += (double)(rand() % 100) / 500.0;
        timer = ecore_timer_add(rnd, _cb_op_font_preview_delayed_eval, obj);
        evas_object_data_set(obj, "delay", timer);
     }
}


static Evas_Object *
_cb_op_font_content_get(void *data, Evas_Object *obj, const char *part)
{
   Font *f = data;
   if (!strcmp(part, "elm.swallow.icon"))
     {
        Evas_Object *o;
        Config *config = termio_config_get(f->term);
        
        o = edje_object_add(evas_object_evas_get(obj));
        theme_apply(o, config, "terminology/fontpreview");
        theme_auto_reload_enable(o);
        evas_object_size_hint_min_set(o,
                                      96 * elm_config_scale_get(),
                                      40 * elm_config_scale_get());
        evas_object_event_callback_add(o, EVAS_CALLBACK_MOVE,
                                       _cb_op_font_preview_eval, f);
        evas_object_event_callback_add(o, EVAS_CALLBACK_RESIZE,
                                       _cb_op_font_preview_eval, f);
        evas_object_event_callback_add(o, EVAS_CALLBACK_SHOW,
                                       _cb_op_font_preview_eval, f);
        evas_object_event_callback_add(o, EVAS_CALLBACK_DEL,
                                       _cb_op_font_preview_del, f);
        return o;
     }
   return NULL;
}

static char *
_cb_op_font_text_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   Font *f = data;
   return strdup(f->pretty_name);
}

static char *
_cb_op_font_group_text_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   return strdup(data);
}

static void
_cb_term_resize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   if (expecting_resize)
     {
        expecting_resize = 0;
        return;
     }
   evas_object_geometry_get(term, NULL, NULL, &tsize_w, &tsize_h);
}

static void
_cb_font_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   evas_object_event_callback_del_full(term, EVAS_CALLBACK_RESIZE, 
                                       _cb_term_resize, term);
}

void
options_font_clear(void)
{
   Font *f;

   op_fontslider = NULL;
   op_fontlist = NULL;
   op_fsml = NULL;
   op_fbig = NULL;
   
   EINA_LIST_FREE(fonts, f)
     {
        eina_stringshare_del(f->full_name);
        eina_stringshare_del(f->pretty_name);
        free(f);
     }
   if (fonthash)
     {
        eina_hash_free(fonthash);
        fonthash = NULL;
     }
}

void
options_font(Evas_Object *opbox, Evas_Object *term)
{
   Evas_Object *o, *bx, *fr, *bx0;
   char buf[4096], *file, *fname;
   Eina_List *files, *fontlist, *l;
   Font *f;
   Elm_Object_Item *it, *sel_it = NULL, *sel_it2 = NULL, *grp_it = NULL;
   Elm_Genlist_Item_Class *it_class, *it_group;
   Config *config = termio_config_get(term);

   options_font_clear();
   
   fr = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Font"));
   elm_box_pack_end(opbox, o);
   evas_object_show(o);
   
   bx0 = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(fr, o);
   evas_object_show(o);
   
   bx = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.5);
   elm_box_horizontal_set(o, EINA_TRUE);
   
   op_fsml = o = elm_label_add(opbox);
   elm_object_text_set(o, "<font_size=6>A</font_size>");
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   op_fontslider = o = elm_slider_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_slider_span_size_set(o, 40);
   elm_slider_unit_format_set(o, "%1.0f");
   elm_slider_indicator_format_set(o, "%1.0f");
   elm_slider_min_max_set(o, 5, 45);
   elm_slider_value_set(o, config->font.size);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   
   evas_object_smart_callback_add(o, "delay,changed",
                                  _cb_op_fontsize_sel, term);

   op_fbig = o = elm_label_add(opbox);
   elm_object_text_set(o, "<font_size=24>A</font_size>");
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   
   elm_box_pack_end(bx0, bx);
   evas_object_show(bx);
   
   it_class = elm_genlist_item_class_new();
   it_class->item_style = "end_icon";
   it_class->func.text_get = _cb_op_font_text_get;
   it_class->func.content_get = _cb_op_font_content_get;
   
   it_group = elm_genlist_item_class_new();
   it_group->item_style = "group_index";
   it_group->func.text_get = _cb_op_font_group_text_get;

   op_fontlist = o = elm_genlist_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_genlist_mode_set(o, ELM_LIST_COMPRESS);
   elm_genlist_homogeneous_set(o, EINA_TRUE);

   /* Bitmaps */
   snprintf(buf, sizeof(buf), "%s/fonts", elm_app_data_dir_get());
   files = ecore_file_ls(buf);
   if (files)
     {
        grp_it = elm_genlist_item_append(o, it_group, _("Bitmap"), NULL,
                                         ELM_GENLIST_ITEM_GROUP,
                                         NULL, NULL);
        elm_genlist_item_select_mode_set(grp_it,
                                         ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
     }
   EINA_LIST_FREE(files, file)
     {
        char *s;
        f = calloc(1, sizeof(Font));
        f->full_name = eina_stringshare_add(file);
        s = strchr(file, '.');
        if (s != NULL) *s = '\0';
        f->pretty_name = eina_stringshare_add(file);
        f->term = term;
        f->bitmap = EINA_TRUE;
        fonts = eina_list_append(fonts, f);

        f->item = it = elm_genlist_item_append(o, it_class, f, grp_it,
                                     ELM_GENLIST_ITEM_NONE,
                                     _cb_op_font_sel, f);
        if ((!sel_it) && (config->font.bitmap) && (config->font.name) &&
            ((!strcmp(config->font.name, f->full_name)) ||
             (!strcmp(config->font.name, file))))
          {
             sel_it = it;
             elm_object_disabled_set(op_fsml, EINA_TRUE);
             elm_object_disabled_set(op_fontslider, EINA_TRUE);
             elm_object_disabled_set(op_fbig, EINA_TRUE);
          }
        free(file);
     }

   /* Standard fonts */
   fontlist = evas_font_available_list(evas_object_evas_get(opbox));
   fontlist = eina_list_sort(fontlist, eina_list_count(fontlist),
                             _cb_op_font_sort);
   fonthash = eina_hash_string_superfast_new(NULL);
   if (fonts)
     {
        grp_it = elm_genlist_item_append(o, it_group, _("Standard"), NULL,
                                         ELM_GENLIST_ITEM_GROUP,
                                         NULL, NULL);
        elm_genlist_item_select_mode_set(grp_it,
                                         ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
     }
   EINA_LIST_FOREACH(fontlist, l, fname)
     {
        if (!eina_hash_find(fonthash, fname))
          {
             f = calloc(1, sizeof(Font));
             if (_parse_font_name(fname, &f->full_name, &f->pretty_name) <0)
               {
                  free(f);
                  continue;
               }
             f->term = term;
             f->bitmap = EINA_FALSE;
             eina_hash_add(fonthash, eina_stringshare_add(fname), f);
             fonts = eina_list_append(fonts, f);
             f->item = it = elm_genlist_item_append(o, it_class, f, grp_it,
                                          ELM_GENLIST_ITEM_NONE,
                                          _cb_op_font_sel, f);
             if ((!sel_it) && (!config->font.bitmap) && (config->font.name))
               {
                  char *s = strchr(fname, ':');
                  size_t len;

                  len = (s == NULL) ? strlen(fname) : (size_t)(s - fname);
                  if (!strcmp(config->font.name, f->full_name))
                    sel_it = it;
                  else if ((!sel_it2) &&
                           (!strncmp(config->font.name, fname, len)))
                    sel_it2 = it;
               }
          }
     }

   if (fontlist)
     evas_font_available_list_free(evas_object_evas_get(opbox), fontlist);

   if (!sel_it && sel_it2)
      sel_it = sel_it2;
   if (sel_it)
     {
        elm_genlist_item_selected_set(sel_it, EINA_TRUE);
        elm_genlist_item_show(sel_it, ELM_GENLIST_ITEM_SCROLLTO_TOP);
     }

   elm_genlist_item_class_free(it_class);
   elm_genlist_item_class_free(it_group);

   elm_box_pack_end(bx0, o);
   evas_object_size_hint_weight_set(opbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(opbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(o);

   expecting_resize = 0;
   evas_object_geometry_get(term, NULL, NULL, &tsize_w, &tsize_h);
   evas_object_event_callback_add(term, EVAS_CALLBACK_RESIZE,
                                  _cb_term_resize, term);
   evas_object_event_callback_add(opbox, EVAS_CALLBACK_DEL,
                                  _cb_font_del, term);
}
