#include <Elementary.h>
#include "config.h"

typedef struct _Media Media;

struct _Media
{
   Evas_Object_Smart_Clipped_Data __clipped_data;
   Evas_Object *event;
};

static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _meida_sc = EVAS_SMART_CLASS_INIT_NULL;

static void _smart_calculate(Evas_Object *obj);

static void
_smart_add(Evas_Object *obj)
{
   Media *sd;
   Evas_Object_Smart_Clipped_Data *cd;
   Evas_Object *o;
   char buf[4096];
   
   _meida_sc.add(obj);
   cd = evas_object_smart_data_get(obj);
   if (!cd) return;
   sd = calloc(1, sizeof(Media));
   if (!sd) return;
   sd->__clipped_data = *cd;
   free(cd);
   evas_object_smart_data_set(obj, sd);
   
   o = evas_object_rectangle_add(evas_object_evas_get(obj));
   evas_object_smart_member_add(o, obj);
   sd->event = o;
   evas_object_color_set(o, 128, 0, 0, 128);
   evas_object_show(o);
}

static void
_smart_del(Evas_Object *obj)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->event) evas_object_del(sd->event);
   _meida_sc.del(obj);
   evas_object_smart_data_set(obj, NULL);
}

static void
_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   Media *sd = evas_object_smart_data_get(obj);
   Evas_Coord ow, oh;
   if (!sd) return;
   evas_object_geometry_get(obj, NULL, NULL, &ow, &oh);
   if ((ow == w) && (oh == h)) return;
   evas_object_smart_changed(obj);
   evas_object_resize(sd->event, ow, oh);
}

static void
_smart_calculate(Evas_Object *obj)
{
   Media *sd = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh;

   if (!sd) return;

   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   evas_object_move(sd->event, ox, oy);
   evas_object_resize(sd->event, ow, oh);
}

static void
_smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   evas_object_smart_changed(obj);
}

static void
_smart_init(void)
{
   static Evas_Smart_Class sc;

   evas_object_smart_clipped_smart_set(&_meida_sc);
   sc           = _meida_sc;
   sc.name      = "media";
   sc.version   = EVAS_SMART_CLASS_VERSION;
   sc.add       = _smart_add;
   sc.del       = _smart_del;
   sc.resize    = _smart_resize;
   sc.move      = _smart_move;
   sc.calculate = _smart_calculate;
   _smart = evas_smart_class_new(&sc);
}

Evas_Object *
media_add(Evas_Object *parent, const char *src, int mode)
{
   Evas *e;
   Evas_Object *obj;
   Media *sd;

   EINA_SAFETY_ON_NULL_RETURN_VAL(parent, NULL);
   e = evas_object_evas_get(parent);
   if (!e) return NULL;

   if (!_smart) _smart_init();
   obj = evas_object_smart_add(e, _smart);
   sd = evas_object_smart_data_get(obj);
   if (!sd) return obj;
   return obj;
}
