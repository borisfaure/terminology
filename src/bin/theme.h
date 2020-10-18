#ifndef _THEME_H__
#define _THEME_H__

#include <Evas.h>
#include "config.h"
#include "colors.h"

Eina_Bool
theme_apply(Evas_Object *obj,
            const Config *config,
            const char *group,
            const char *theme_file,
            const Color_Scheme *cs,
            Eina_Bool is_elm_layout);
void theme_reload(Evas_Object *edje);
void theme_auto_reload_enable(Evas_Object *edje);
const char *theme_path_get(const char *name);
const char *theme_path_default_get(void);

Eina_Bool utils_need_scale_wizard(void);

#endif
