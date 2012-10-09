#include "private.h"

#include <Elementary.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_theme.h"
#include "utils.h"

static Evas_Object *op_themelist;

static Eina_List *themes = NULL;

typedef struct _Theme Theme;
struct _Theme
{
   Elm_Object_Item *item;
   const char *name;
   Evas_Object *term;
};

static char *
_cb_op_theme_text_get(void *data, Evas_Object *obj __UNUSED__, const char *part __UNUSED__)
{
   Theme *t = data;
   char buf[4096], *p;

   eina_strlcpy(buf, t->name, sizeof(buf));
   p = strrchr(buf, '.');
   if (p) *p = 0;

   return strdup(buf);
}

static Evas_Object *
_cb_op_theme_content_get(void *data, Evas_Object *obj, const char *part)
{
   Theme *t = data;
   char buf[4096];

   if (!strcmp(part, "elm.swallow.icon"))
     {
        Evas_Object *o;
        Config *config = termio_config_get(t->term);
	snprintf(buf, sizeof(buf), "%s/themes/%s", elm_app_data_dir_get(), t->name);
        o = edje_object_add(evas_object_evas_get(obj));
	if (!edje_object_file_set(o, buf, "terminology/background"))
	    return NULL;

        evas_object_size_hint_min_set(o,
                                      96 * elm_config_scale_get(),
                                      40 * elm_config_scale_get());
        return o;
     }

   return NULL;
}

static void
_cb_op_theme_sel(void *data, Evas_Object *obj, void *event __UNUSED__)
{
   Theme *t = data;
   Config *config = termio_config_get(t->term);
   Evas_Object *edje = termio_theme_get(t->term);

   if ((config->theme) && (!strcmp(t->name, config->theme)))
     return;

   eina_stringshare_replace(&(config->theme), t->name);
   config_save(config, NULL);
   if (!theme_apply(edje, config, "terminology/background"))
     ERR("Couldn't find terminology theme!");
}

static int
_cb_op_theme_sort(const void *d1, const void *d2)
{
   return strcasecmp(d1, d2);
}

void
options_theme(Evas_Object *opbox, Evas_Object *term)
{
   Evas_Object *o, *box, *fr;
   Elm_Genlist_Item_Class *it_class;
   Eina_List *files;
   char buf[4096], *file;
   Theme *t;

   options_theme_clear();

   fr = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, "Theme");
   elm_box_pack_end(opbox, o);
   evas_object_show(o);

   box = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(o, EINA_TRUE);
   elm_object_content_set(fr, o);
   evas_object_show(o);

   it_class = elm_genlist_item_class_new();
   it_class->item_style = "end_icon";
   it_class->func.text_get = _cb_op_theme_text_get;
   it_class->func.content_get = _cb_op_theme_content_get;

   op_themelist = o = elm_genlist_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_genlist_mode_set(o, ELM_LIST_COMPRESS);
   elm_genlist_homogeneous_set(o, EINA_TRUE);

   snprintf(buf, sizeof(buf), "%s/themes", elm_app_data_dir_get());
   files = ecore_file_ls(buf);
   if (files)
     files = eina_list_sort(files, eina_list_count(files),
                            _cb_op_theme_sort);

   EINA_LIST_FREE(files, file)
     {
        t = calloc(1, sizeof(Theme));
        t->name = eina_stringshare_add(file);
        t->term = term;
        t->item = elm_genlist_item_append(o, it_class, t, NULL,
					  ELM_GENLIST_ITEM_NONE,
					  _cb_op_theme_sel, t);
        themes = eina_list_append(themes, t);
        free(file);
     }

   elm_genlist_item_class_free(it_class);

   elm_box_pack_end(box, o);
   evas_object_size_hint_weight_set(opbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(opbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(o);
}

void
options_theme_clear(void)
{
   Theme *t;

   op_themelist = NULL;

   EINA_LIST_FREE(themes, t)
     {
        eina_stringshare_del(t->name);
        free(t);
     }
}
