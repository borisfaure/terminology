#ifndef _MAIN_H__
#define _MAIN_H__ 1

#include "config.h"

Config * main_config_get(void);
void main_new(Evas_Object *win, Evas_Object *term);
void main_new_with_dir(Evas_Object *win, Evas_Object *term, const char *wdir);
void main_split_h(Evas_Object *win, Evas_Object *term, char *cmd);
void main_split_v(Evas_Object *win, Evas_Object *term, char *cmd);
void main_close(Evas_Object *win, Evas_Object *term);

void main_trans_update(const Config *config);
void main_media_update(const Config *config);
void main_media_mute_update(const Config *config);
void main_media_visualize_update(const Config *config);
void main_config_sync(const Config *config);

void change_theme(Evas_Object *win, Config *config);
#endif
