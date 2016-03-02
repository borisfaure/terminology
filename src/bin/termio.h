#ifndef _TERMIO_H__
#define _TERMIO_H__ 1

#include "config.h"
#include "main.h"
#include "col.h"
#include "termpty.h"
#include "win.h"

Evas_Object *termio_add(Evas_Object *parent, Config *config, const char *cmd, Eina_Bool login_shell, const char *cd, int w, int h, Term *term);
void         termio_win_set(Evas_Object *obj, Evas_Object *win);
void         termio_theme_set(Evas_Object *obj, Evas_Object *theme);
char        *termio_selection_get(Evas_Object *obj,
                                  int c1x, int c1y, int c2x, int c2y,
                                  size_t *len, Eina_Bool right_trim);
Eina_Bool    termio_selection_exists(const Evas_Object *obj);
void termio_scroll_delta(Evas_Object *obj, int delta, int by_page);
void termio_scroll_set(Evas_Object *obj, int scroll);
void termio_scroll(Evas_Object *obj, int direction, int start_y, int end_y);
void termio_content_change(Evas_Object *obj, Evas_Coord x, Evas_Coord y, int n);

void         termio_config_update(Evas_Object *obj);
Config      *termio_config_get(const Evas_Object *obj);
Eina_Bool    termio_take_selection(Evas_Object *obj, Elm_Sel_Type);
void         termio_paste_selection(Evas_Object *obj, Elm_Sel_Type);
const char  *termio_link_get(const Evas_Object *obj);
void         termio_mouseover_suspend_pushpop(Evas_Object *obj, int dir);
void         termio_event_feed_mouse_in(Evas_Object *obj);
void         termio_size_get(Evas_Object *obj, int *w, int *h);
int          termio_scroll_get(Evas_Object *obj);
void         termio_font_size_set(Evas_Object *obj, int size);
void         termio_grid_size_set(Evas_Object *obj, int w, int h);
pid_t        termio_pid_get(const Evas_Object *obj);
Eina_Bool    termio_cwd_get(const Evas_Object *obj, char *buf, size_t size);
Evas_Object *termio_textgrid_get(Evas_Object *obj);
Evas_Object *termio_win_get(Evas_Object *obj);
const char  *termio_title_get(Evas_Object *obj);
void         termio_user_title_set(Evas_Object *obj, const char *title);
const char  *termio_icon_name_get(Evas_Object *obj);
void         termio_media_mute_set(Evas_Object *obj, Eina_Bool mute);
void         termio_media_visualize_set(Evas_Object *obj, Eina_Bool visualize);
void         termio_config_set(Evas_Object *obj, Config *config);
Config      *termio_config_get(const Evas_Object *obj);

Termpty *termio_pty_get(Evas_Object *obj);
Evas_Object * termio_miniview_get(Evas_Object *obj);
Term* termio_term_get(Evas_Object *obj);

#endif
