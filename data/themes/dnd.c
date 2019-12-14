/**
 * edje_cc -id images/ dnd.edc && gcc -o dnd dnd.c `pkg-config --libs --cflags evas ecore ecore-evas edje`
 */

#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Edje.h>

#define WIDTH  400
#define HEIGHT 400

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
   Evas_Object *bg;
   Evas_Object *edje_obj;
   int          w;
   int          h;

   bg = ecore_evas_data_get(ee, "background");
   edje_obj = ecore_evas_data_get(ee, "edje_obj");

   ecore_evas_geometry_get(ee, NULL, NULL, &w, &h);
   evas_object_resize(bg, w, h);
   evas_object_resize(edje_obj, w, h);
}



int
main(int argc EINA_UNUSED, char *argv[] EINA_UNUSED)
{
   const char  *edje_file = "dnd.edj";
   Ecore_Evas  *ee;
   Evas        *evas;
   Evas_Object *bg;
   Evas_Object *edje_obj;

   assert(ecore_evas_init());
   assert(edje_init());
   ee = ecore_evas_new(NULL, 0, 0, WIDTH, HEIGHT, NULL);
   assert(ee);

   ecore_evas_callback_destroy_set(ee, _on_destroy);
   ecore_evas_callback_resize_set(ee, _on_canvas_resize);
   ecore_evas_title_set(ee, "Tab Drag Test");

   evas = ecore_evas_get(ee);

   edje_obj = edje_object_add(evas);

   if (!edje_object_file_set(edje_obj, edje_file, "main"))
     printf("failed to set file %s.\n", edje_file);

   evas_object_move(edje_obj, 0, 0);
   evas_object_resize(edje_obj, WIDTH, HEIGHT);
   evas_object_show(edje_obj);
   ecore_evas_data_set(ee, "edje_obj", edje_obj);

   ecore_evas_show(ee);

   ecore_main_loop_begin();

   ecore_evas_free(ee);
   ecore_evas_shutdown();
   edje_shutdown();

   return EXIT_SUCCESS;

 shutdown_ecore_evas:
   ecore_evas_shutdown();

   return EXIT_FAILURE;
}
