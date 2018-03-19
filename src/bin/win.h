#ifndef _WIN_H__
#define _WIN_H__ 1

#include "config.h"

typedef struct _Win   Win;
typedef struct _Term  Term;



Eina_Bool main_term_popup_exists(const Term *term);
void term_unfocus(Term *term);
void term_focus(Term *term);

Evas_Object *term_termio_get(const Term *term);
Evas_Object *term_miniview_get(const Term *term);
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

Term *term_new(Win *wn, Config *config, const char *cmd,
               Eina_Bool login_shell, const char *cd, int size_w, int size_h,
               Eina_Bool hold, const char *title);
int win_term_set(Win *wn, Term *term);

Eina_List *
terms_from_win_object(Evas_Object *win);

Evas_Object *win_base_get(const Win *wn);
Evas_Object *win_evas_object_get(const Win *win);
Eina_List * win_terms_get(const Win *wn);
Config *win_config_get(const Win *wn);
void win_term_swallow(Win *wn, Term *term);
void win_add_split(Win *wn, Term *term);
void win_sizing_handle(Win *wn);
void win_toggle_visible_group(Win *wn);
void win_toggle_all_group(Win *wn);
Eina_Bool win_is_group_action_handled(Win *wn);
Eina_Bool win_is_group_input(Win *wn);


void term_ref(Term *term);
void term_unref(Term *term);
Term *term_next_get(const Term *term);
Term *term_prev_get(const Term *term);
void term_next(Term *term);
void term_prev(Term *term);
Win * term_win_get(const Term *term);

void term_up(Term *term);
void term_down(Term *term);
void term_left(Term *term);
void term_right(Term *term);

const char *
term_preedit_str_get(Term *term);
Ecore_IMF_Context *
term_imf_context_get(Term *term);

Eina_Bool term_is_visible(Term *term);

void win_font_size_set(Win *wn, int new_size);
void win_font_update(Term *term);

Eina_Bool
term_has_popmedia(const Term *term);
void
term_popmedia_close(Term *term);

typedef Eina_Bool (*For_Each_Term)(Term *term, void *data);

Eina_Bool
for_each_term_do(Win *wn, For_Each_Term cb, void *data);

void
term_apply_shine(Term *term, int shine);
void background_set_shine(Config *config, Evas_Object *bg);

#endif
