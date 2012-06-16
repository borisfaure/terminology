#include <Elementary.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_font.h"

static Evas_Object *op_fontslider, *op_fontlist;

typedef struct _Font Font;

struct _Font
{
   const char *name;
   Evas_Object *term;
   Eina_Bool bitmap : 1;
};

static Eina_List *fonts = NULL;
static Eina_Hash *fonthash = NULL;

static void
_update_sizing(Evas_Object *term)
{
   Evas_Coord ow = 0, oh = 0, mw = 1, mh = 1, w, h;

   evas_object_data_del(term, "sizedone");
   termio_config_update(term);
   evas_object_geometry_get(term, NULL, NULL, &ow, &oh);
   evas_object_size_hint_min_get(term, &mw, &mh);
   if (mw < 1) mw = 1;
   if (mh < 1) mh = 1;
   w = ow / mw;
   h = oh / mh;
   evas_object_data_del(term, "sizedone");
   evas_object_size_hint_request_set(term, w * mw, h * mh);
}

static void
_cb_op_font_sel(void *data, Evas_Object *obj, void *event)
{
   Font *f = data;
   if ((config->font.name) && (!strcmp(f->name, config->font.name)))
     return;
   if (config->font.name) eina_stringshare_del(config->font.name);
   config->font.name = eina_stringshare_add(f->name);
   config->font.bitmap = f->bitmap;
   _update_sizing(f->term);
   config_save();
}

static void
_cb_op_fontsize_sel(void *data, Evas_Object *obj, void *event)
{
   int size  = elm_slider_value_get(obj) + 0.5;

   if (config->font.size == size) return;
   config->font.size = size;
   _update_sizing(data);
   config_save();
}

static int
_cb_op_font_sort(const void *d1, const void *d2)
{
   return strcasecmp(d1, d2);
}

void
options_font(Evas_Object *opbox, Evas_Object *term)
{
   Evas_Object *o;
   char buf[4096], *file, *fname, *s;
   Eina_List *files, *fontlist, *l;
   Font *f;
   Elm_Object_Item *it, *sel_it = NULL;
   
   EINA_LIST_FREE(fonts, f)
     {
        eina_stringshare_del(f->name);
        free(f);
     }
   if (fonthash)
     {
        eina_hash_free(fonthash);
        fonthash = NULL;
     }

   op_fontslider = o = elm_slider_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_slider_span_size_set(o, 160);
   elm_slider_unit_format_set(o, "%1.0f");
   elm_slider_indicator_format_set(o, "%1.0f");
   elm_slider_min_max_set(o, 5, 45);
   elm_slider_value_set(o, config->font.size);
   elm_box_pack_end(opbox, o);
   evas_object_show(o);

   evas_object_smart_callback_add(o, "delay,changed",
                                  _cb_op_fontsize_sel, term);

   op_fontlist = o = elm_list_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

   elm_list_select_mode_set(o, ELM_OBJECT_SELECT_MODE_DEFAULT);

   evas_event_freeze(evas_object_evas_get(opbox));
   edje_freeze();

   snprintf(buf, sizeof(buf), "%s/fonts", elm_app_data_dir_get());
   files = ecore_file_ls(buf);
   EINA_LIST_FREE(files, file)
     {
        f = calloc(1, sizeof(Font));
        f->name = eina_stringshare_add(file);
        f->term = term;
        f->bitmap = EINA_TRUE;
        fonts = eina_list_append(fonts, f);
        it = elm_list_item_append(o, f->name, NULL, NULL, _cb_op_font_sel, f);
        if ((config->font.bitmap) && (config->font.name) && 
            (!strcmp(config->font.name, f->name)))
          {
             elm_list_item_selected_set(it, EINA_TRUE);
             sel_it = it;
          }
        free(file);
     }

   fontlist = evas_font_available_list(evas_object_evas_get(opbox));
   fontlist = eina_list_sort(fontlist, eina_list_count(fontlist),
                             _cb_op_font_sort);
   fonthash = eina_hash_string_superfast_new(NULL);

   if ((files) && (fontlist))
     {
        it = elm_list_item_append(o, "", NULL, NULL, NULL, NULL);
        elm_list_item_separator_set(it, EINA_TRUE);
     }
   
   EINA_LIST_FOREACH(fontlist, l, fname)
     {
        snprintf(buf, sizeof(buf), "%s", fname);
        s = strchr(buf, ':');
        if (s) *s = 0;
        fname = buf;
        if (!eina_hash_find(fonthash, fname))
          {
             f = calloc(1, sizeof(Font));
             f->name = eina_stringshare_add(fname);
             f->term = term;
             f->bitmap = EINA_FALSE;
             eina_hash_add(fonthash, fname, f);
             fonts = eina_list_append(fonts, f);
             it = elm_list_item_append(o, f->name, NULL, NULL, _cb_op_font_sel, f);
             if ((!config->font.bitmap) && (config->font.name) && 
                 (!strcmp(config->font.name, f->name)))
               {
                  elm_list_item_selected_set(it, EINA_TRUE);
                  sel_it = it;
               }
          }
     }
   if (fontlist)
     evas_font_available_list_free(evas_object_evas_get(opbox), fontlist);

   elm_list_go(o);
   if (sel_it) elm_list_item_show(sel_it);
   
   edje_thaw();
   evas_event_thaw(evas_object_evas_get(opbox));

   elm_box_pack_end(opbox, o);
   evas_object_show(o);
}
