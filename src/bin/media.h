#ifndef _MEDIA_H__
#define _MEDIA_H__ 1

#define MEDIA_SIZE_MASK    0x000f
#define MEDIA_OPTIONS_MASK 0x00f0
// enum list types
#define MEDIA_BG           0x0000
#define MEDIA_POP          0x0001
#define MEDIA_STRETCH      0x0002
#define MEDIA_THUMB        0x0003
// bitmask for options - on or off
#define MEDIA_RECOVER      0x0010
#define MEDIA_SAVE         0x0020

#define TYPE_UNKNOWN -1
#define TYPE_IMG      0
#define TYPE_SCALE    1
#define TYPE_EDJE     2
#define TYPE_MOV      3
#define TYPE_THUMB    4
#define TYPE_ICON     5

#include "config.h"

Evas_Object *media_add(Evas_Object *parent, const char *src, const Config *config, int mode, int *type);
void media_mute_set(Evas_Object *obj, Eina_Bool mute);
void media_play_set(Evas_Object *obj, Eina_Bool play);
void media_position_set(Evas_Object *obj, double pos);
void media_volume_set(Evas_Object *obj, double vol);
void media_stop(Evas_Object *obj);
const char *media_get(const Evas_Object *obj);
int media_src_type_get(const char *src);

#endif
