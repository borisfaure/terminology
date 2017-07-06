#ifndef _MEDIA_H__
#define _MEDIA_H__ 1

#define MEDIA_SIZE_MASK    0x000f
#define MEDIA_OPTIONS_MASK 0x00f0
// enum list types
#define MEDIA_BG           0x0000
#define MEDIA_POP          0x0001
#define MEDIA_STRETCH      0x0002
#define MEDIA_THUMB        0x0003
#define MEDIA_TOOLTIP      0x0004
// bitmask for options - on or off
#define MEDIA_RECOVER      0x0010
#define MEDIA_SAVE         0x0020


typedef enum _Media_Type Media_Type;

enum _Media_Type {
     MEDIA_TYPE_UNKNOWN,
     MEDIA_TYPE_IMG,
     MEDIA_TYPE_SCALE,
     MEDIA_TYPE_EDJE,
     MEDIA_TYPE_MOV,
     MEDIA_TYPE_THUMB,
};

#include "config.h"

Evas_Object *media_add(Evas_Object *parent, const char *src, const Config *config, int mode, Media_Type type);
void media_mute_set(Evas_Object *obj, Eina_Bool mute);
void media_play_set(Evas_Object *obj, Eina_Bool play);
Eina_Bool media_play_get(const Evas_Object *obj);
void media_position_set(Evas_Object *obj, double pos);
void media_volume_set(Evas_Object *obj, double vol);
void media_visualize_set(Evas_Object *obj, Eina_Bool visualize);
void media_stop(Evas_Object *obj);
const char *media_get(const Evas_Object *obj);
Media_Type media_src_type_get(const char *src);
Evas_Object *media_control_get(const Evas_Object *obj);
void media_unknown_handle(const char *handler, const char *src);

#endif
