#ifndef _CONTROLS_H__
#define _CONTROLS_H__ 1

void controls_show(Evas_Object *win, Evas_Object *base, Evas_Object *bg,
                   Evas_Object *term,
                   void (*donecb) (void *data), void *donedata);

void controls_init(void);
void controls_shutdown(void);

#endif
