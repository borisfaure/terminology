#ifndef TERMINOLOGY_CONTROLS_H_
#define TERMINOLOGY_CONTROLS_H_ 1

void controls_show(Evas_Object *win, Evas_Object *base, Evas_Object *bg,
                   Evas_Object *term,
                   void (*donecb) (void *data), void *donedata);

void controls_init(void);
void controls_shutdown(void);

#endif
