#include <Elementary.h>
#include "win.h"
#include "termio.h"
#include "config.h"

const char *cmd = NULL;
static Evas_Object *win, *bg, *term;
static Evas_Object
  *op_frame, *op_box, *op_toolbar, *op_opbox,
  *op_fontslider, *op_fontlist;
static Eina_Bool op_out = EINA_FALSE;

static void
_cb_focus_in(void *data, Evas_Object *obj, void *event)
{
   edje_object_signal_emit(bg, "focus,in", "terminology");
   elm_object_focus_set(data, EINA_TRUE);
}

static void
_cb_focus_out(void *data, Evas_Object *obj, void *event)
{
   edje_object_signal_emit(bg, "focus,out", "terminology");
   elm_object_focus_set(data, EINA_FALSE);
}

static void
_cb_size_hint(void *data, Evas *e, Evas_Object *obj, void *event)
{
   Evas_Coord mw, mh, rw, rh, w = 0, h = 0;

   evas_object_size_hint_min_get(obj, &mw, &mh);
   evas_object_size_hint_request_get(obj, &rw, &rh);

   edje_object_size_min_calc(bg, &w, &h);
   evas_object_size_hint_min_set(bg, w, h);
   elm_win_size_base_set(win, w - mw, h - mh);
   elm_win_size_step_set(win, mw, mh);
   if (!evas_object_data_get(obj, "sizedone"))
     {
        evas_object_resize(win, w - mw + rw, h - mh + rh);
        evas_object_data_set(obj, "sizedone", obj);
     }
}

typedef struct _Font Font;

struct _Font
{
   const char *name;
   Eina_Bool bitmap : 1;
};

static Eina_List *fonts = NULL;
static Eina_Hash *fonthash = NULL;

static void
_update_sizing(void)
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
   _update_sizing();
}

static void
_cb_op_fontsize_sel(void *data, Evas_Object *obj, void *event)
{
   int size  =elm_slider_value_get(obj) + 0.5;

   if (config->font.size == size) return;
   config->font.size = size;
   _update_sizing();
}

static int
_cb_op_font_sort(const void *d1, const void *d2)
{
   return strcasecmp(d1, d2);
}

static void
_cb_op_font(void *data, Evas_Object *obj, void *event)
{
   Evas_Object *o;
   char buf[4096], *file, *fname, *s;
   Eina_List *files, *fontlist, *l;
   Font *f;
   Elm_Object_Item *it, *sel_it;
   
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
   elm_box_clear(op_opbox);

   op_fontslider = o = elm_slider_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_slider_span_size_set(o, 160);
   elm_slider_unit_format_set(o, "%1.0f");
   elm_slider_indicator_format_set(o, "%1.0f");
   elm_slider_min_max_set(o, 5, 45);
   elm_slider_value_set(o, config->font.size);
   elm_box_pack_end(op_opbox, o);
   evas_object_show(o);

   evas_object_smart_callback_add(o, "delay,changed",
                                  _cb_op_fontsize_sel, NULL);

   op_fontlist = o = elm_list_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

   elm_list_select_mode_set(o, ELM_OBJECT_SELECT_MODE_DEFAULT);

   evas_event_freeze(evas_object_evas_get(win));
   edje_freeze();

   snprintf(buf, sizeof(buf), "%s/fonts", elm_app_data_dir_get());
   files = ecore_file_ls(buf);
   EINA_LIST_FREE(files, file)
     {
        f = calloc(1, sizeof(Font));
        f->name = eina_stringshare_add(file);
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

   fontlist = evas_font_available_list(evas_object_evas_get(win));
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
     evas_font_available_list_free(evas_object_evas_get(win), fontlist);

   elm_list_go(o);
   if (sel_it) elm_list_item_show(sel_it);
   
   edje_thaw();
   evas_event_thaw(evas_object_evas_get(win));

   elm_box_pack_end(op_opbox, o);
   evas_object_show(o);
}

static void
_cb_op_theme(void *data, Evas_Object *obj, void *event)
{
   Evas_Object *o;

   elm_box_clear(op_opbox);
   // XXX: not done yet
}

static void
_cb_op_behavior(void *data, Evas_Object *obj, void *event)
{
   Evas_Object *o;

   elm_box_clear(op_opbox);
   // XXX: not done yet
}

static void
_cb_options(void *data, Evas_Object *obj, void *event)
{
   Evas_Object *o;

   if (!op_frame)
     {
        Elm_Object_Item *it_fn, *it_th, *it_bh;

        op_frame = o = elm_frame_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(o, "Options");

        op_box = o = elm_box_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
        elm_object_content_set(op_frame, o);
        evas_object_show(o);

        op_toolbar = o = elm_toolbar_add(win);
        elm_object_style_set(o, "item_horizontal");
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
        elm_toolbar_icon_size_set(o, 16);
        elm_toolbar_shrink_mode_set(o, ELM_TOOLBAR_SHRINK_NONE);
        elm_toolbar_select_mode_set(o, ELM_OBJECT_SELECT_MODE_DEFAULT);
        elm_toolbar_menu_parent_set(o, win);
        elm_toolbar_homogeneous_set(o, EINA_FALSE);

        it_fn = elm_toolbar_item_append(o, "preferences-desktop-font", "Font",
                                        _cb_op_font, NULL);
        it_th = elm_toolbar_item_append(o, "preferences-desktop-theme", "Theme",
                                        _cb_op_theme, NULL);
        it_bh = elm_toolbar_item_append(o, "system-run", "Behavior",
                                        _cb_op_behavior, NULL);

        elm_box_pack_end(op_box, o);
        evas_object_show(o);

        op_opbox = o = elm_box_add(win);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_box_pack_end(op_box, o);
        evas_object_show(o);

        elm_toolbar_item_selected_set(it_fn, EINA_TRUE);

        evas_smart_objects_calculate(evas_object_evas_get(win));
        edje_object_part_swallow(bg, "terminology.options", op_frame);
        evas_object_show(o);
     }
   if (!op_out)
     {
        edje_object_signal_emit(bg, "options,show", "terminology");
        op_out = EINA_TRUE;
     }
   else
     {
        edje_object_signal_emit(bg, "options,hide", "terminology");
        op_out = EINA_FALSE;
        elm_object_focus_set(term, EINA_TRUE);
     }
}

EAPI_MAIN int
elm_main(int argc, char **argv)
{
   int i;
   Evas_Object *o;
   char buf[4096];

   config_init();
   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
   elm_app_compile_bin_dir_set(PACKAGE_BIN_DIR);
   elm_app_compile_data_dir_set(PACKAGE_DATA_DIR);
   elm_app_info_set(elm_main, "terminology", "themes/default.edj");

   for (i = 1; i < argc; i++)
     {
        if ((!strcmp(argv[i], "-e")) && (i < (argc - 1)))
          {
             i++;
             cmd = argv[i];
          }
     }

   win = tg_win_add();

   bg = o = edje_object_add(evas_object_evas_get(win));
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   snprintf(buf, sizeof(buf), "%s/themes/%s",
            elm_app_data_dir_get(), config->theme);
   edje_object_file_set(o, buf, "terminology/background");
   elm_win_resize_object_add(win, o);
   evas_object_show(o);

   term = o = termio_add(win, cmd, 80, 24);
   termio_win_set(o, win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_event_callback_add(o, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                  _cb_size_hint, win);
   edje_object_part_swallow(bg, "terminology.content", o);
   evas_object_smart_callback_add(o, "options", _cb_options, NULL);
   evas_object_show(o);

   evas_object_smart_callback_add(win, "focus,in", _cb_focus_in, term);
   evas_object_smart_callback_add(win, "focus,out", _cb_focus_out, term);
   _cb_size_hint(win, evas_object_evas_get(win), term, NULL);

   evas_object_show(win);

   elm_run();
   elm_shutdown();
   config_shutdown();
   return 0;
}
ELM_MAIN()
