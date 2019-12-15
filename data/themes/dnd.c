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

static int _tab_active_idx = 0;

static Tab_Item _tab_items[4] = {
       {.title="tab 1", .has_bell=EINA_FALSE,},
       {.title="tab 2", .has_bell=EINA_FALSE,},
       {.title="tab 3", .has_bell=EINA_FALSE,},
       {.title="tab 4", .has_bell=EINA_FALSE,},
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
_cb_tab_bar_change(void *data,
                   Evas *_e EINA_UNUSED,
                   Evas_Object *obj,
                   void *_info EINA_UNUSED)
{
   Evas_Coord w, h;

   evas_object_geometry_get(obj, NULL, NULL, &w, &h);
   ERR("tab bar w:%d h:%d", w, h);
   //evas_object_size_hint_min_set(_tab_bar, w, h);
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
   //evas_object_size_hint_min_set(_main_tab, w, h);
}

static void
_tab_bar_setup(void)
{
   Evas_Coord w = 0, h = 0;

   _main_tab = edje_object_add(_evas);
   if (!edje_object_file_set(_main_tab, _edje_file, "tab_main"))
     printf("failed to set file %s.\n", _edje_file);
   edje_object_part_swallow(_bg, "terminology.tab_main",
                            _main_tab);
   edje_object_part_text_set(_main_tab, "terminology.tab.title",
                             "foo bar 42 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
   evas_object_size_hint_weight_set(_main_tab, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(_main_tab, EVAS_HINT_FILL, EVAS_HINT_FILL);

   edje_object_size_min_calc(_main_tab, &w, &h);
   evas_object_size_hint_min_set(_main_tab, w, h);
   evas_object_event_callback_add(_main_tab, EVAS_CALLBACK_MOVE,
                                  _cb_main_tab_change, NULL);
   evas_object_event_callback_add(_main_tab, EVAS_CALLBACK_RESIZE,
                                  _cb_main_tab_change, NULL);
   evas_object_show(_main_tab);

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

   rect = evas_object_rectangle_add(_evas);
   evas_object_color_set(rect, 0, 0, 0, 0);
   edje_object_part_swallow(_bg, "terminology.content", rect);
   evas_object_size_hint_min_set(rect, WIDTH, HEIGHT - 32);
   evas_object_size_hint_weight_set(rect, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(rect, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(rect);


   _tab_bar_setup();

   /* TODO */

   ecore_evas_show(_ee);

   elm_run();

   ecore_evas_free(_ee);

   return EXIT_SUCCESS;
}

ELM_MAIN()

