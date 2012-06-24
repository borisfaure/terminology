#ifndef _TERMIO_H__
#define _TERMIO_H__ 1

#include "config.h"

Evas_Object *termio_add(Evas_Object *parent, Config *config, const char *cmd, int w, int h);
void         termio_win_set(Evas_Object *obj, Evas_Object *win);
char        *termio_selection_get(Evas_Object *obj, int c1x, int c1y, int c2x, int c2y);
void         termio_config_update(Evas_Object *obj);
Config      *termio_config_get(const Evas_Object *obj);
void         termio_copy_clipboard(Evas_Object *obj);
void         termio_paste_clipboard(Evas_Object *obj);

#endif
