#ifndef _TYTEST_H__
#define _TYTEST_H__ 1

#if defined(ENABLE_TESTS)
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

void
tytest_termio_resize(int w, int h);
#endif

#if defined(ENABLE_TESTS) || defined(ENABLE_TEST_UI)
void
test_pointer_canvas_xy_get(int *mx,
                           int *my);

void test_set_mouse_pointer(int mx, int my);
#endif

#endif
