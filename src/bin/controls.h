#ifndef _CONTROLS_H__
#define _CONTROLS_H__ 1

void controls_toggle(Evas_Object *win, Evas_Object *bg, Evas_Object *term,
                     void (*donecb) (void *data), void *donedata);

#endif
