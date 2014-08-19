#ifndef _MINIVIEW_H__
#define _MINIVIEW_H__ 1

#include "termpty.h"

Evas_Object * miniview_add(Evas_Object *parent, Evas_Object *termio);

void miniview_redraw(Evas_Object *obj);
void miniview_position_offset(Evas_Object *obj, int by, Eina_Bool sanitize);
Eina_Bool miniview_handle_key(Evas_Object *obj, const Evas_Event_Key_Down *ev);

void miniview_init(void);
void miniview_shutdown(void);
#endif
