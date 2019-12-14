/**
 * edje_cc -id images/ dnd.edc && gcc -o dnd dnd.c `pkg-config --libs --cflags evas ecore ecore-evas edje`
 */

#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Edje.h>

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
static Evas_Object *_bg = NULL;
static Evas_Object *_tab_bar= NULL;
static Evas_Object *_main_tab = NULL;
static Evas_Object *_spacer = NULL;

static Tab_Item _tab_items[4] = {
       {.title="tab 1", .has_bell=EINA_FALSE,},
       {.title="tab 2", .has_bell=EINA_FALSE,},
       {.title="tab 3", .has_bell=EINA_FALSE,},
       {.title="tab 4", .has_bell=EINA_FALSE,},
};



static void
_on_destroy(Ecore_Evas *ee EINA_UNUSED)
{
   ecore_main_loop_quit();
}

/* here just to keep our example's window size and background image's
 * size in synchrony */
static void
_on_canvas_resize(Ecore_Evas *ee)
{
   int          w;
   int          h;

   ecore_evas_geometry_get(ee, NULL, NULL, &w, &h);
   evas_object_resize(_bg, w, h);
}

static void
_tabbar_setup(void)
{
   Evas_Coord w = 0, h = 0;

   _tab_bar = edje_object_add(_evas);
   if (!edje_object_file_set(_tab_bar, _edje_file, "tab_bar"))
     printf("failed to set file %s.\n", _edje_file);


   _spacer = evas_object_rectangle_add(_evas);
   evas_object_color_set(_spacer, 0, 0, 0, 0);
   //elm_coords_finger_size_adjust(1, &w, 1, &h);
   w = h = 32;
   evas_object_size_hint_min_set(_spacer, w, h);
   edje_object_part_swallow(_tab_bar, "terminology.tab_spacer",
                            _spacer);

   _main_tab = edje_object_add(_evas);
   if (!edje_object_file_set(_main_tab, _edje_file, "main_tab"))
     printf("failed to set file %s.\n", _edje_file);
   edje_object_part_swallow(_tab_bar, "terminology.main_tab",
                            _main_tab);
   evas_object_show(_main_tab);
   edje_object_part_text_set(_main_tab, "terminology.tab.title",
                             "foo bar 42");


   edje_object_part_swallow(_bg, "terminology.tab_bar",
                            _tab_bar);
   evas_object_show(_tab_bar);
   edje_object_signal_emit(_bg, "tab_bar,on", "terminology");
   _main_tab = edje_object_add(_evas);
}


int
main(int argc EINA_UNUSED, char *argv[] EINA_UNUSED)
{

   assert(ecore_evas_init());
   assert(edje_init());
   _ee = ecore_evas_new(NULL, 0, 0, WIDTH, HEIGHT, NULL);
   assert(_ee);

   ecore_evas_callback_destroy_set(_ee, _on_destroy);
   ecore_evas_callback_resize_set(_ee, _on_canvas_resize);
   ecore_evas_title_set(_ee, "Tab Drag Test");

   _evas = ecore_evas_get(_ee);

   _bg = edje_object_add(_evas);

   if (!edje_object_file_set(_bg, _edje_file, "main"))
     printf("failed to set file %s.\n", _edje_file);

   evas_object_move(_bg, 0, 0);
   evas_object_resize(_bg, WIDTH, HEIGHT);
   evas_object_show(_bg);
   ecore_evas_data_set(_ee, "bg", _bg);


   _tabbar_setup();

   /* TODO */

   ecore_evas_show(_ee);

   ecore_main_loop_begin();

   ecore_evas_free(_ee);
   ecore_evas_shutdown();
   edje_shutdown();

   return EXIT_SUCCESS;
}
