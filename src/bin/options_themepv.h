#ifndef TERMINOLOGY_OPTIONS_THEMEPV_H_
#define TERMINOLOGY_OPTIONS_THEMEPV_H_ 1
#include "colors.h"

Evas_Object *
options_theme_preview_add(Evas_Object *parent,
                          const Config *config,
                          const char *file,
                          const Color_Scheme *cs,
                          Evas_Coord w,
                          Evas_Coord h,
                          Eina_Bool colors_mode);
#define COLOR_MODE_PREVIEW_WIDTH   39
#define COLOR_MODE_PREVIEW_HEIGHT  20
#endif
