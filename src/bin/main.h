#ifndef _MAIN_H__
#define _MAIN_H__ 1

#include "config.h"

typedef struct _Win   Win;
typedef struct _Term  Term;
typedef struct _Split Split;

void main_new(Evas_Object *win, Evas_Object *term);
void main_new_with_dir(Evas_Object *win, Evas_Object *term, const char *wdir);
void main_split_h(Evas_Object *win, Evas_Object *term);
void main_split_v(Evas_Object *win, Evas_Object *term);
void main_close(Evas_Object *win, Evas_Object *term);

void main_trans_update(const Config *config);
void main_media_update(const Config *config);
void main_media_mute_update(const Config *config);
void main_config_sync(const Config *config);

void change_theme(Evas_Object *win, Config *config);

void main_term_focus(Term *term);

Win *main_term_win_get(Term *term);
Evas_Object *main_win_evas_object_get(Win *wn);
Eina_List *main_win_terms_get(Win *wn);
Evas_Object *main_term_evas_object_get(Term *term);

#endif
