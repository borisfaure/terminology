#ifndef _MINIVIEW_H__
#define _MINIVIEW_H__ 1

void miniview_update_scroll(Evas_Object *obj, int scroll_position);
Evas_Object * miniview_add(Evas_Object *parent, Evas_Object *termio);

void miniview_init(void);
void miniview_shutdown(void);
#endif
