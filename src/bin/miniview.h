#ifndef TERMINOLOGY_MINIVIEW_H_
#define TERMINOLOGY_MINIVIEW_H_ 1

#include "termpty.h"

Evas_Object *
term_miniview_get(const Term *term);

Evas_Object * miniview_add(Evas_Object *parent, Evas_Object *termio);

void miniview_redraw(const Evas_Object *obj);
void miniview_position_offset(const Evas_Object *obj, int by, Eina_Bool sanitize);
Eina_Bool miniview_handle_key(Evas_Object *obj, const Evas_Event_Key_Down *ev);

void miniview_init(void);
void miniview_shutdown(void);
#endif
