#ifndef _TERMCMD_H__
#define _TERMCMD_H__ 1

#include "config.h"

Eina_Bool termcmd_watch(Evas_Object *obj, Evas_Object *win, Evas_Object *bg, const char *cmd);
Eina_Bool termcmd_do(Evas_Object *obj, Evas_Object *win, Evas_Object *bg, const char *cmd);

#endif
