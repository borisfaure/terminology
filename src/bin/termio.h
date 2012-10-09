#ifndef _TERMIO_H__
#define _TERMIO_H__ 1

#include "config.h"

Evas_Object *termio_add(Evas_Object *parent, Config *config, const char *cmd, Eina_Bool login_shell, const char *cd, int w, int h);
void         termio_win_set(Evas_Object *obj, Evas_Object *win);
void         termio_theme_set(Evas_Object *obj, Evas_Object *theme);
Evas_Object *termio_theme_get(Evas_Object *obj);
char        *termio_selection_get(Evas_Object *obj, int c1x, int c1y, int c2x, int c2y);
void         termio_config_update(Evas_Object *obj);
Config      *termio_config_get(const Evas_Object *obj);
void         termio_copy_clipboard(Evas_Object *obj);
void         termio_paste_clipboard(Evas_Object *obj);
const char  *termio_link_get(const Evas_Object *obj);
void         termio_mouseover_suspend_pushpop(Evas_Object *obj, int dir);
void         termio_size_get(Evas_Object *obj, int *w, int *h);
int          termio_scroll_get(Evas_Object *obj);
void         termio_font_size_set(Evas_Object *obj, int size);
void         termio_grid_size_set(Evas_Object *obj, int w, int h);
pid_t        termio_pid_get(const Evas_Object *obj);
Eina_Bool    termio_cwd_get(const Evas_Object *obj, char *buf, size_t size);

#endif
