#ifndef _TYTEST_H__
#define _TYTEST_H__ 1

#if defined(ENABLE_TESTS)
#define evas_object_textgrid_palette_get test_textgrid_palette_get
void
test_textgrid_palette_get(const Evas_Object *obj,
                          Evas_Textgrid_Palette pal,
                          int idx,
                          int *r,
                          int *g,
                          int *b,
                          int *a);
#endif
#endif
