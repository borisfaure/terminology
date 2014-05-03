#ifndef _MINIVIEW_H__
#define _MINIVIEW_H__ 1

#include "termpty.h"

Evas_Object * miniview_add(Evas_Object *parent, Evas_Object *termio);

void miniview_redraw(Evas_Object *obj);
void miniview_push_history(Evas_Object *obj, Termcell *cells, int cells_len);

void miniview_init(void);
void miniview_shutdown(void);
#endif
