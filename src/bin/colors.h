#ifndef _COLORS_H__
#define _COLORS_H__ 1

#include <Evas.h>
#include "config.h"

void
colors_term_init(Evas_Object *textgrid,
                 const Evas_Object *bg,
                 const Config *config);
void
colors_standard_get(int set,
                    int col,
                    unsigned char *r,
                    unsigned char *g,
                    unsigned char *b,
                    unsigned char *a);

void
colors_256_get(int col,
               unsigned char *r,
               unsigned char *g,
               unsigned char *b,
               unsigned char *a);


void
color_scheme_apply(Evas_Object *edje,
                   const Config *config);

#endif
