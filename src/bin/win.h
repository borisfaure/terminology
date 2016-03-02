#ifndef _WIN_H__
#define _WIN_H__ 1

#include "config.h"

typedef struct _Win   Win;
typedef struct _Term  Term;



Eina_Bool main_term_popup_exists(const Term *term);
void term_unfocus(Term *term);

Evas_Object *term_termio_get(Term *term);
Evas_Object *term_miniview_get(Term *term);
void term_miniview_toggle(Term *term);
void term_set_title(Term *term);
void term_miniview_hide(Term *term);
Eina_Bool term_tab_go(Term *term, int tnum);

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
void windows_update(void);

Term *term_new(Win *wn, Config *config, const char *cmd, Eina_Bool login_shell, const char *cd, int size_w, int size_h, Eina_Bool hold);
int win_term_set(Win *wn, Term *term);

Eina_List *
terms_from_win_object(Evas_Object *win);

Evas_Object *win_base_get(Win *wn);
Evas_Object *win_evas_object_get(Win *win);
Eina_List * win_terms_get(Win *wn);
Config *win_config_get(Win *wn);
void win_term_swallow(Win *wn, Term *term);
void win_add_split(Win *wn, Term *term);
void win_sizing_handle(Win *wn);

void term_ref(Term *term);
void term_unref(Term *term);
Term *term_next_get(Term *term);
Term *term_prev_get(Term *term);
void term_next(Term *term);
void term_prev(Term *term);
Win * term_win_get(Term *term);

void
win_font_size_set(Win *wn, int new_size);

Eina_Bool
term_has_popmedia(const Term *term);
void
term_popmedia_close(Term *term);

typedef Eina_Bool (*For_Each_Term)(Term *term, void *data);

Eina_Bool
for_each_term_do(Win *wn, For_Each_Term cb, void *data);


#endif
