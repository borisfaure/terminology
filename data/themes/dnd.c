/**
 * edje_cc -id images/ dnd.edc && gcc -o dnd dnd.c `pkg-config --libs --cflags elementary`
 */

#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Edje.h>
#include <Elementary.h>

#define CRITICAL(...) EINA_LOG_CRIT(__VA_ARGS__)
#define ERR(...)      EINA_LOG_ERR(__VA_ARGS__)
#define WRN(...)      EINA_LOG_WARN(__VA_ARGS__)
#define INF(...)      EINA_LOG_INFO(__VA_ARGS__)
#define DBG(...)      EINA_LOG_DBG(__VA_ARGS__)

#define WIDTH  400
#define HEIGHT 400

typedef struct _Tab_Item Tab_Item;
struct _Tab_Item {
     const char *title;
     Eina_Bool has_bell;
};

static const char  *_edje_file = "dnd.edj";
static Ecore_Evas  *_ee = NULL;
static Evas        *_evas = NULL;
static Evas_Object *_win = NULL;
static Evas_Object *_bg = NULL;
static Evas_Object *_tab_bar= NULL;
static Evas_Object *_main_tab = NULL;
static Evas_Object *_left_box = NULL;
static Evas_Object *_right_box = NULL;
static Evas_Object *_spacer = NULL;
static int _tab_active_idx = 2;

#define NB_TABS  4
static Tab_Item _tab_items[NB_TABS] = {
       {
          .title="tab 1 alpha bravo charlie delta",
          .has_bell=EINA_FALSE,
       },
       {
          .title="tab 2 echo foxtrot golf hotel",
          .has_bell=EINA_FALSE,
       },
       {
          .title="tab 3 india juliett kilo lima",
          .has_bell=EINA_FALSE,
       },
       {
          .title="tab 4 mike november oscar papa",
          .has_bell=EINA_FALSE,
       },
};




/* here just to keep our example's window size and background image's
 * size in synchrony */
static void
_on_canvas_resize(void *data,
                  Evas *_e EINA_UNUSED,
                  Evas_Object *obj,
                  void *_info EINA_UNUSED)
{
   int          w;
   int          h;

   ecore_evas_geometry_get(_ee, NULL, NULL, &w, &h);
   evas_object_resize(_bg, w, h);

   evas_object_geometry_get(_tab_bar, NULL, NULL, &w, &h);
   ERR("tab bar w:%d h:%d", w, h);
   evas_object_geometry_get(_main_tab, NULL, NULL, &w, &h);
   ERR("main tab w:%d h:%d", w, h);
   evas_object_size_hint_min_get(_main_tab, &w, &h);
   ERR("main tab min w:%d h:%d", w, h);
}

static void
_cb_main_tab_change(void *data,
                   Evas *_e EINA_UNUSED,
                   Evas_Object *obj,
                   void *_info EINA_UNUSED)
{
   Evas_Coord w, h;

   evas_object_geometry_get(obj, NULL, NULL, &w, &h);
   ERR("main tab w:%d h:%d", w, h);
   evas_object_size_hint_min_set(_spacer, 1, h);
}

static void
_tab_bar_clear(void)
{
   elm_box_clear(_left_box);
   elm_box_clear(_right_box);
}

static void
_tab_bar_fill(void)
{
   int i;
   _tab_bar_clear();

   for (i = 0; i < NB_TABS; i++)
     {
        Tab_Item *item = _tab_items + i;
        Evas_Object *tab;

        if (i == _tab_active_idx)
          {
             double step, tab_orig;

             edje_object_part_text_set(_main_tab, "terminology.tab.title",
                                       item->title);

             if (NB_TABS > 1)
               {
                  step = 1.0 / (NB_TABS);
                  tab_orig = (double)(i) / (double)(NB_TABS - 1);
               }
             else
               {
                  step = 1.0;
                  tab_orig = 0.0;
               }

             edje_object_part_drag_size_set(_bg, "terminology.main_tab",
                                            step, 0.0);
             edje_object_part_drag_value_set(_bg, "terminology.main_tab",
                                             tab_orig, 0.0);
             continue;
          }
        /* inactive tab */
        if ((i > 0 && i < _tab_active_idx) ||
            (i > _tab_active_idx + 1 && i < NB_TABS - 1))
          {
             ERR("ADD SEPARATOR");
             /* add separator */
             Evas_Object *sep = elm_layout_add(_win);
             elm_layout_file_set(sep, _edje_file, "tab_separator");
             evas_object_size_hint_weight_set(sep, 0.0, EVAS_HINT_EXPAND);
             evas_object_size_hint_fill_set(sep, 0.0, EVAS_HINT_FILL);
             if (i < _tab_active_idx)
               elm_box_pack_end(_left_box, sep);
             else
               elm_box_pack_end(_right_box, sep);
             evas_object_show(sep);
          }
        tab = elm_layout_add(_win);
        elm_layout_file_set(tab, _edje_file, "tab_inactive");
        elm_layout_text_set(tab, "terminology.tab.title", item->title);
        evas_object_size_hint_weight_set(tab, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_fill_set(tab, EVAS_HINT_FILL, EVAS_HINT_FILL);
        if (i < _tab_active_idx)
          elm_box_pack_end(_left_box, tab);
        else
          elm_box_pack_end(_right_box, tab);
        evas_object_show(tab);

     }
}

static void
_on_drag(void *data EINA_UNUSED,
         Evas_Object *o,
         const char *emission EINA_UNUSED,
         const char *source EINA_UNUSED)
{
   double val;

   edje_object_part_drag_value_get(o, "terminology.main_tab", &val, NULL);
   ERR("value changed to: %0.3f", val);
}

static void
_on_bg_resize(void *data,
              Evas *_e EINA_UNUSED,
              Evas_Object *obj,
              void *_info EINA_UNUSED)
{
}

static void
_on_title(void *data EINA_UNUSED,
         Evas_Object *o EINA_UNUSED,
         const char *emission,
         const char *source)
{
   ERR("Received signal '%s' from '%s'", emission, source);
}

static void
_tab_bar_setup(void)
{
   Evas_Coord w = 0, h = 0;

   _spacer = evas_object_rectangle_add(_evas);
   evas_object_color_set(_spacer, 0, 0, 0, 0);
   elm_coords_finger_size_adjust(1, &w, 1, &h);
   evas_object_size_hint_min_set(_spacer, w, h);
   edje_object_part_swallow(_bg, "terminology.tab",
                            _spacer);
   evas_object_show(_spacer);

   _main_tab = edje_object_add(_evas);
   if (!edje_object_file_set(_main_tab, _edje_file, "tab_main"))
     printf("failed to set file %s.\n", _edje_file);
   edje_object_part_swallow(_bg, "terminology.main_tab",
                            _main_tab);
   edje_object_part_text_set(_main_tab, "terminology.tab.title",
                             "foo bar 42");
   evas_object_size_hint_weight_set(_main_tab, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(_main_tab, EVAS_HINT_FILL, EVAS_HINT_FILL);
   edje_object_signal_callback_add(_bg, "drag", "terminology.main_tab",
                                   _on_drag, NULL);
   edje_object_signal_callback_add(_main_tab, "tab,title", "*",
                                   _on_title, NULL);

   edje_object_size_min_calc(_main_tab, &w, &h);
   ERR("min: %d %d", w, h);
   evas_object_size_hint_min_set(_main_tab, w, h);
   evas_object_event_callback_add(_main_tab, EVAS_CALLBACK_MOVE,
                                  _cb_main_tab_change, NULL);
   evas_object_event_callback_add(_main_tab, EVAS_CALLBACK_RESIZE,
                                  _cb_main_tab_change, NULL);
   evas_object_show(_main_tab);

   _left_box = elm_box_add(_win);
   assert(_left_box);
   elm_box_horizontal_set(_left_box, EINA_TRUE);
   elm_box_homogeneous_set(_left_box, EINA_FALSE);
   evas_object_size_hint_weight_set(_left_box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(_left_box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   edje_object_part_swallow(_bg, "terminology.tabs_left", _left_box);

   _right_box = elm_box_add(_win);
   assert(_right_box);
   elm_box_horizontal_set(_right_box, EINA_TRUE);
   elm_box_homogeneous_set(_right_box, EINA_FALSE);
   evas_object_size_hint_weight_set(_right_box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(_right_box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   edje_object_part_swallow(_bg, "terminology.tabs_right", _right_box);


   edje_object_signal_emit(_bg, "tab_bar,on", "terminology");
   evas_object_show(_tab_bar);
}

EAPI_MAIN int
elm_main(int argc EINA_UNUSED, char **argv EINA_UNUSED)
{
   Evas_Object *rect;

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
   _win = elm_win_util_standard_add("tabtest", "Tab Drag Test");
   elm_win_autodel_set(_win, EINA_TRUE);

   /* and now just resize the window to a size you want. normally widgets
    * will determine the initial size though */
   evas_object_resize(_win, 400, 400);
   /* and show the window */
   evas_object_show(_win);
   _evas = evas_object_evas_get(_win);
   assert(_evas);
   _ee = ecore_evas_ecore_evas_get(_evas);
   assert(_ee);

   evas_object_event_callback_add(_win, EVAS_CALLBACK_RESIZE,
                                  _on_canvas_resize, NULL);

   _bg = edje_object_add(_evas);

   if (!edje_object_file_set(_bg, _edje_file, "main"))
     printf("failed to set file %s.\n", _edje_file);

   evas_object_move(_bg, 0, 0);
   evas_object_resize(_bg, WIDTH, HEIGHT);
   evas_object_show(_bg);
   ecore_evas_data_set(_ee, "bg", _bg);

   evas_object_event_callback_add(_bg, EVAS_CALLBACK_RESIZE,
                                  _on_bg_resize, NULL);

   rect = evas_object_rectangle_add(_evas);
   evas_object_color_set(rect, 0, 0, 0, 0);
   edje_object_part_swallow(_bg, "terminology.content", rect);
   evas_object_size_hint_min_set(rect, WIDTH, HEIGHT - 32);
   evas_object_size_hint_weight_set(rect, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(rect, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(rect);


   _tab_bar_setup();
   _tab_bar_fill();

   /* TODO */

   ecore_evas_show(_ee);

   elm_run();

   ecore_evas_free(_ee);

   return EXIT_SUCCESS;
}

ELM_MAIN()
