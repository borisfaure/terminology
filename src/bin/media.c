#include <Elementary.h>
#include "config.h"

#define TYPE_IMG   0
#define TYPE_SCALE 1
#define TYPE_EDJE  2
#define TYPE_MOV   3

typedef struct _Media Media;

struct _Media
{
   Evas_Object_Smart_Clipped_Data __clipped_data;
   Evas_Object *clip, *o_img;
   Ecore_Timer *anim;
   const char *src;
   int iw, ih;
   int fr, frnum;
   int mode, type;
};

static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _meida_sc = EVAS_SMART_CLASS_INIT_NULL;

static const char *extn_img[] =
{
   ".png", ".jpg", ".jpeg", ".jpe", ".jfif", ".tif", ".tiff", ".gif",
   ".bmp", ".ico", ".ppm", ".pgm", ".pbm", ".pnm", ".xpm", ".psd", ".wbmp",
   ".cur", ".xcf", ".xcf.gz", ".arw", ".cr2", ".crw", ".dcr", ".dng", ".k25",
   ".kdc", ".erf", ".mrw", ".nef", ".nrf", ".nrw", ".orf", ".raw", ".rw2",
   ".rw2", ".pef", ".raf", ".sr2", ".srf", ".x3f",
   NULL
};

static const char *extn_scale[] =
{
   ".svg", ".svgz", ".svg.gz", ".ps", ".psd", 
   NULL
};

static const char *extn_edj[] =
{
   ".edj",
   NULL
};

static const char *extn_mov[] =
{
   ".asf", ".avi", ".bdm", ".bdmv", ".clpi", ".cpi", ".dv", ".fla", ".flv",
   ".m1v", ".m2t", ".m2v", ".m4v", ".mkv", ".mov", ".mp2", ".mp2ts", ".mp4",
   ".mpe", ".mpeg", ".mpg", ".mpl", ".mpls", ".mts", ".mxf", ".nut", ".nuv",
   ".ogg", ".ogm", ".ogv", ".qt", ".rm", ".rmj", ".rmm", ".rms", ".rmvb",
   ".rmx", ".rv", ".swf", ".ts", ".weba", ".webm", ".wmv", ".3g2", ".3gp",
   ".3gp2", ".3gpp", ".3gpp2", ".3p2", ".264",
   NULL
};

static Eina_Bool
_is_fmt(const char *f, const char **extn)
{
   int i, len, l;
   
   len = strlen(f);
   for (i = 0; extn[i]; i++)
     {
        l = strlen(extn[i]);
        if (len < l) continue;
        if (!strcasecmp(extn[i], f + len - l)) return EINA_TRUE;
     }
   return EINA_FALSE;
}

//////////////////////// img
static void
_cb_img_preloaded(void *data, Evas *e, Evas_Object *obj, void *event)
{
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   evas_object_show(sd->o_img);
   printf("preloaded\n");
}

static Eina_Bool
_cb_img_frame(void *data)
{
   double t;
   int fr;
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return EINA_FALSE;
   sd->fr++;
   fr = ((sd->fr - 1) % (sd->frnum)) + 1;
   evas_object_image_animated_frame_set(sd->o_img, fr);
   t = evas_object_image_animated_frame_duration_get(sd->o_img, fr, 0);
   ecore_timer_interval_set(sd->anim, t);
//   sd->anim = ecore_timer_add(t, _cb_img_frame, data);
   return EINA_TRUE;
}

static void
_type_img_anim_handle(Evas_Object *obj)
{
   double t;
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (!evas_object_image_animated_get(sd->o_img)) return;
   sd->fr = 1;
   sd->frnum = evas_object_image_animated_frame_count_get(sd->o_img);
   if (sd->frnum < 2) return;
   t = evas_object_image_animated_frame_duration_get(sd->o_img, sd->fr, 0);
   sd->anim = ecore_timer_add(t, _cb_img_frame, obj);
}

static void
_type_img_init(Evas_Object *obj)
{
   Evas_Object *o;
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   sd->type = TYPE_IMG;
   o = sd->o_img = evas_object_image_filled_add(evas_object_evas_get(obj));
   evas_object_smart_member_add(o, obj);
   evas_object_clip_set(o, sd->clip);
   evas_object_event_callback_add(o, EVAS_CALLBACK_IMAGE_PRELOADED,
                                  _cb_img_preloaded, obj);
   evas_object_image_file_set(o, sd->src, NULL);
   evas_object_image_size_get(o, &(sd->iw), &(sd->ih));
   printf("start preload\n");
   evas_object_image_preload(o, EINA_FALSE);
   printf("start preload done\n");
   _type_img_anim_handle(obj);
}

static void
_type_img_calc(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if ((w <= 0) || (h <= 0) || (sd->iw <= 0) || (sd->ih <= 0))
     {
        w = 1;
        h = 1;
     }
   else
     {
        int iw, ih;
        
        iw = w;
        ih = (sd->ih * w) / sd->iw;
        if (ih < h)
          {
             ih = h;
             iw = (sd->iw * h) / sd->ih;
             if (iw < w) iw = w;
          }
        x += ((w - iw) / 2);
        y += ((h - ih) / 2);
        w = iw;
        h = ih;
     }
   evas_object_move(sd->o_img, x, y);
   evas_object_resize(sd->o_img, w, h);
}

//////////////////////// scalable img
static void
_type_scale_init(Evas_Object *obj)
{
   Evas_Object *o;
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   sd->type = TYPE_SCALE;
   // XXX
   o = sd->o_img = evas_object_image_filled_add(evas_object_evas_get(obj));
   evas_object_smart_member_add(o, obj);
   evas_object_clip_set(o, sd->clip);
   evas_object_show(o);
   evas_object_image_file_set(o, sd->src, NULL);
}

static void
_type_scale_calc(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   evas_object_move(sd->o_img, x, y);
   evas_object_resize(sd->o_img, w, h);
}

//////////////////////// edj
static void
_cb_edje_preloaded(void *data, Evas_Object *obj, const char *sig, const char *src)
{
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   evas_object_show(sd->o_img);
}

static void
_type_edje_init(Evas_Object *obj)
{
   Evas_Object *o;
   int i;
   const char *groups[] =
     {
        "terminology/backgroud",
        "e/desktop/background",
        NULL
     };
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   sd->type = TYPE_EDJE;
   o = sd->o_img = edje_object_add(evas_object_evas_get(obj));
   evas_object_smart_member_add(o, obj);
   evas_object_clip_set(o, sd->clip);
   for (i = 0; groups[i]; i++)
     {
        if (edje_object_file_set(o, sd->src, groups[i]))
          {
             edje_object_signal_callback_add(o, "preload,done", "",
                                             _cb_edje_preloaded, obj);
             edje_object_preload(o, EINA_FALSE);
             return;
          }
     }
}

static void
_type_edje_calc(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   evas_object_move(sd->o_img, x, y);
   evas_object_resize(sd->o_img, w, h);
}

//////////////////////// movie
static void
_type_mov_init(Evas_Object *obj)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   sd->type = TYPE_MOV;
}

static void
_type_mov_calc(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   evas_object_move(sd->o_img, x, y);
   evas_object_resize(sd->o_img, w, h);
}

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
   sd->clip = o;
   evas_object_color_set(o, 255, 255, 255, 255);
   evas_object_show(o);
}

static void
_smart_del(Evas_Object *obj)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->clip) evas_object_del(sd->clip);
   if (sd->o_img) evas_object_del(sd->o_img);
   if (sd->anim) ecore_timer_del(sd->anim);
   _meida_sc.del(obj);
   evas_object_smart_data_set(obj, NULL);
}

static void
_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   Media *sd = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh;
   if (!sd) return;
   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   if ((ow == w) && (oh == h)) return;
   evas_object_smart_changed(obj);
   evas_object_resize(sd->clip, ow, oh);
}

static void
_smart_calculate(Evas_Object *obj)
{
   Media *sd = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh;

   if (!sd) return;
   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   if (sd->type == TYPE_IMG) _type_img_calc(obj, ox, oy, ow, oh);
   else if (sd->type == TYPE_SCALE) _type_scale_calc(obj, ox, oy, ow, oh);
   else if (sd->type == TYPE_EDJE) _type_edje_calc(obj, ox, oy, ow, oh);
   else if (sd->type == TYPE_MOV) _type_mov_calc(obj, ox, oy, ow, oh);
   evas_object_move(sd->clip, ox, oy);
   evas_object_resize(sd->clip, ow, oh);
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
   
   sd->src = eina_stringshare_add(src);
   sd->mode = mode;
   if      (_is_fmt(src, extn_img))   _type_img_init(obj);
   else if (_is_fmt(src, extn_scale)) _type_scale_init(obj);
   else if (_is_fmt(src, extn_edj))   _type_edje_init(obj);
   else if (_is_fmt(src, extn_mov))   _type_mov_init(obj);
   return obj;
}
