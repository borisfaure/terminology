#ifndef _MINIVIEW_H__
#define _MINIVIEW_H__ 1


Evas_Object * miniview_add(Evas_Object *parent, Evas_Object *termio);

void miniview_redraw(Evas_Object *obj, int columns, int rows);

void miniview_init(void);
void miniview_shutdown(void);
#endif
