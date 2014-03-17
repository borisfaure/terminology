#ifndef _COL_H__
#define _COL_H__ 1

#include <Evas.h>
#include "config.h"

void colors_term_init(Evas_Object *textgrid, Evas_Object *bg, Config *config);
void colors_standard_get(int set, int col, unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a);

#endif
