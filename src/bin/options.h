#ifndef _TERMINOLOGY_OPTIONS_H__
#define _TERMINOLOGY_OPTIONS_H__ 1

void options_toggle(Evas_Object *win, Evas_Object *bg, Evas_Object *term,
                    void (*donecb) (void *data), void *donedata);
Eina_Bool options_active_get(void);

#endif
