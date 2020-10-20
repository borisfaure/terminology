#ifndef _OPTIONS_THEMEPV_H__
#define _OPTIONS_THEMEPV_H__ 1
#include "colors.h"

Evas_Object *
options_theme_preview_add(Evas_Object *parent,
                          const Config *config,
                          const char *file,
                          const Color_Scheme *cs,
                          Evas_Coord w,
                          Evas_Coord h);

#endif
