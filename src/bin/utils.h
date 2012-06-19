#ifndef _UTILS_H__
#define _UTILS_H__

#include <Evas.h>
#include "config.h"

Eina_Bool theme_apply(Evas_Object *edje, const Config *config, const char *group);

void theme_reload(Evas_Object *edje);

void theme_auto_reload_enable(Evas_Object *edje);

#endif
