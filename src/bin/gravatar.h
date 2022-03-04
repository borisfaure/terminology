#ifndef TERMINOLOGY_GRAVATAR_H_
#define TERMINOLOGY_GRAVATAR_H_ 1

#include "config.h"

void
gravatar_tooltip(Evas_Object *obj, const Config *config, const char *email);

void gravatar_init(void);
void gravatar_shutdown(void);

#endif
