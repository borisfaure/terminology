#ifndef _MAIN_H__
#define _MAIN_H__ 1

#include "config.h"

void main_split_h(Evas_Object *win, Evas_Object *term);
void main_split_v(Evas_Object *win, Evas_Object *term);
void main_close(Evas_Object *win, Evas_Object *term);

void main_trans_update(const Config *config);
void main_media_update(const Config *config);
void main_media_mute_update(const Config *config);
void main_config_sync(const Config *config);

#endif
