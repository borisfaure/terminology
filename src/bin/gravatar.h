#ifndef _GRAVATAR_H__
#define _GRAVATAR_H__ 1

#include "config.h"

void
gravatar_tooltip(Evas_Object *obj, const Config *config, char *email);

void gravatar_init(void);
void gravatar_shutdown(void);

#endif
