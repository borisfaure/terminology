#ifndef _UTILS_H__
#define _UTILS_H__

#include <Evas.h>
#include "config.h"

Eina_Bool theme_apply(Evas_Object *edje, const Config *config, const char *group);
Eina_Bool theme_apply_default(Evas_Object *edje, const Config *config, const char *group);
void theme_reload(Evas_Object *edje);
void theme_auto_reload_enable(Evas_Object *edje);
const char *theme_path_get(const char *name);

Eina_Bool homedir_get(char *buf, size_t size);

Eina_Bool link_is_protocol(const char *str);
Eina_Bool link_is_url(const char *str);
Eina_Bool link_is_email(const char *str);

#define casestartswith(str, constref) \
  (!strncasecmp(str, constref, sizeof(constref) - 1))

#endif
