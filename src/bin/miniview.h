#ifndef _MINIVIEW_H__
#define _MINIVIEW_H__ 1

void miniview_resize(Evas_Object *obj, Termpty *pty, int w, int h);
void miniview_move(Evas_Object *obj, int x, int y);
void miniview_update_scroll(Evas_Object *obj, int scroll_position);
Evas_Object * miniview_add(Evas_Object *parent, int fontw, int fonth, Termpty *pty,
                           int scroll_position, int x, int y, int w, int h);
void miniview_hide(Evas_Object *obj);
#endif
