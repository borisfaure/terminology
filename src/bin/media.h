#define MEDIA_BG 0

#define TYPE_IMG   0
#define TYPE_SCALE 1
#define TYPE_EDJE  2
#define TYPE_MOV   3

Evas_Object *media_add(Evas_Object *parent, const char *src, int mode, int *type);
void media_mute_set(Evas_Object *obj, Eina_Bool mute);
