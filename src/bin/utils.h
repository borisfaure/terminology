#ifndef _UTILS_H__
#define _UTILS_H__

#include <Eina.h>
#include "config.h"

Eina_Bool homedir_get(char *buf, size_t size);
void open_url(const Config *config, const char *url);

#endif
