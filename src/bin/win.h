#ifndef _WIN_H__
#define _WIN_H__ 1

#include "config.h"
#include "term_container.h"

Eina_Bool main_term_popup_exists(const Term *term);
void main_term_focus(Term *term);

Evas_Object *main_term_evas_object_get(Term *term);
Evas_Object *term_miniview_get(Term *term);
void term_miniview_toggle(Term *term);
void term_miniview_hide(Term *term);

void split_horizontally(Evas_Object *win, Evas_Object *term, const char *cmd);
void split_vertically(Evas_Object *win, Evas_Object *term, const char *cmd);

Win *
win_new(const char *name, const char *role, const char *title,
             const char *icon_name, Config *config,
             Eina_Bool fullscreen, Eina_Bool iconic,
             Eina_Bool borderless, Eina_Bool override,
             Eina_Bool maximized);
void win_free(Win *wn);
void windows_free(void);

Term *term_new(Win *wn, Config *config, const char *cmd, Eina_Bool login_shell, const char *cd, int size_w, int size_h, Eina_Bool hold);

Eina_List *
terms_from_win_object(Evas_Object *win);

Evas_Object *win_base_get(Win *wn);
Evas_Object *win_evas_object_get(Win *win);
Eina_List * win_terms_get(Win *wn);
Config *win_config_get(Win *wn);
int win_term_set(Win *wn, Term *term);
void win_sizing_handle(Win *wn);

void term_next(Term *term);
void term_prev(Term *term);
Win * term_win_get(Term *term);

#endif
