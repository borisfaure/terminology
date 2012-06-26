#ifndef _MEDIA_H__
#define _MEDIA_H__ 1

#define MEDIA_BG 0

#define TYPE_IMG   0
#define TYPE_SCALE 1
#define TYPE_EDJE  2
#define TYPE_MOV   3

#include "config.h"

Evas_Object *media_add(Evas_Object *parent, const char *src, const Config *config, int mode, int *type);
void media_mute_set(Evas_Object *obj, Eina_Bool mute);
void media_play_set(Evas_Object *obj, Eina_Bool play);
void media_position_set(Evas_Object *obj, double pos);
void media_volume_set(Evas_Object *obj, double vol);
void media_stop(Evas_Object *obj);

#endif
