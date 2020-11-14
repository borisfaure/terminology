#ifndef _TYTEST_H__
#define _TYTEST_H__ 1

#if defined(BINARY_TYTEST)
#define evas_object_textgrid_palette_get  test_textgrid_palette_get
void
test_textgrid_palette_get(const Evas_Object *obj,
                          Evas_Textgrid_Palette pal,
                          int idx,
                          int *r,
                          int *g,
                          int *b,
                          int *a);

#define evas_object_textgrid_cellrow_get  tytest_textgrid_cellrow_get
Evas_Textgrid_Cell *
tytest_textgrid_cellrow_get(Evas_Object *obj, int y);

#define edje_object_color_class_get  tytest_edje_object_color_class_get
int
tytest_edje_object_color_class_get(Evas_Object *termio EINA_UNUSED, const char *key,
                        int *r, int *g, int *b, int *a,
                        int *r1 EINA_UNUSED, int *g1 EINA_UNUSED, int *b1 EINA_UNUSED, int *a1 EINA_UNUSED,
                        int *r2 EINA_UNUSED, int *g2 EINA_UNUSED, int *b2 EINA_UNUSED, int *a2 EINA_UNUSED);
#define edje_object_color_class_set  tytest_edje_object_color_class_set
int
tytest_edje_object_color_class_set(Evas_Object *termio EINA_UNUSED, const char *key,
                                 int r, int g, int b, int a,
                                 int r1 EINA_UNUSED, int g1 EINA_UNUSED, int b1 EINA_UNUSED, int a1 EINA_UNUSED,
                                 int r2 EINA_UNUSED, int g2 EINA_UNUSED, int b2 EINA_UNUSED, int a2 EINA_UNUSED);

void
tytest_termio_resize(int w, int h);
#endif

#if defined(BINARY_TYTEST) || defined(ENABLE_TEST_UI)
void
test_pointer_canvas_xy_get(int *mx,
                           int *my);

void test_set_mouse_pointer(int mx, int my);
#endif
#endif
