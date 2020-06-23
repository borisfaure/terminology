#ifndef _THEME_H__
#define _THEME_H__

#include <Evas.h>
#include "config.h"

Eina_Bool theme_apply(Evas_Object *edje, const Config *config, const char *group);
Eina_Bool theme_apply_elm(Evas_Object *edje, const Config *config, const char *group);
Eina_Bool theme_apply_default(Evas_Object *edje, const Config *config, const char *group);
void theme_reload(Evas_Object *edje);
void theme_auto_reload_enable(Evas_Object *edje);
const char *theme_path_get(const char *name);

Eina_Bool utils_need_scale_wizard(void);

#endif
