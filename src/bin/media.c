#include "private.h"

#include <Elementary.h>
#include <Emotion.h>
#include "media.h"
#include "config.h"
#include "utils.h"

typedef struct _Media Media;

struct _Media
{
   Evas_Object_Smart_Clipped_Data __clipped_data;
   Evas_Object *clip, *o_img, *o_tmp;
   Ecore_Timer *anim;
   Ecore_Job *restart_job;
   const char *src;
   const Config *config;
   int iw, ih;
   int sw, sh;
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
   ".svg", ".svgz", ".svg.gz", ".ps", ".ps.gz", ".pdf",
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
_cb_img_preloaded(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   evas_object_show(sd->o_img);
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
   evas_object_image_preload(o, EINA_FALSE);
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
_cb_scale_preloaded(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   if (!sd->o_tmp) evas_object_show(sd->o_img);
   else
     {
        evas_object_del(sd->o_img);
        sd->o_img = sd->o_tmp;
        sd->o_tmp = NULL;
        evas_object_show(sd->o_img);
     }
}

static void
_type_scale_init(Evas_Object *obj)
{
   Evas_Object *o;
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   sd->type = TYPE_SCALE;
   o = sd->o_img = evas_object_image_filled_add(evas_object_evas_get(obj));
   evas_object_smart_member_add(o, obj);
   evas_object_clip_set(o, sd->clip);
   evas_object_event_callback_add(o, EVAS_CALLBACK_IMAGE_PRELOADED,
                                  _cb_scale_preloaded, obj);
   evas_object_image_file_set(o, sd->src, NULL);
   evas_object_image_size_get(o, &(sd->iw), &(sd->ih));
   evas_object_image_preload(o, EINA_FALSE);
}

static void
_type_scale_calc(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   Evas_Object *o;
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
   if ((w != sd->sw) || (h != sd->sh))
     {
        o = sd->o_tmp = evas_object_image_filled_add(evas_object_evas_get(obj));
        evas_object_smart_member_add(o, obj);
        evas_object_clip_set(o, sd->clip);
        evas_object_event_callback_add(o, EVAS_CALLBACK_IMAGE_PRELOADED,
                                       _cb_scale_preloaded, obj);
        evas_object_image_file_set(o, sd->src, NULL);
        evas_object_image_load_size_set(sd->o_tmp, w, h);
        evas_object_image_preload(o, EINA_FALSE);
     }
   sd->sw = w;
   sd->sh = h;
   if (sd->o_tmp)
     {
        evas_object_move(sd->o_tmp, x, y);
        evas_object_resize(sd->o_tmp, w, h);
     }
   evas_object_move(sd->o_img, x, y);
   evas_object_resize(sd->o_img, w, h);
}

//////////////////////// edj
static void
_cb_edje_preloaded(void *data, Evas_Object *obj __UNUSED__, const char *sig __UNUSED__, const char *src __UNUSED__)
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
             theme_auto_reload_enable(o);
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
static void _type_mov_calc(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);

static void
_cb_mov_frame_decode(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Media *sd = evas_object_smart_data_get(data);
   Evas_Coord ox, oy, ow, oh;

   if (!sd) return;
   evas_object_geometry_get(data, &ox, &oy, &ow, &oh);
   evas_object_show(sd->o_img);
   _type_mov_calc(data, ox, oy, ow, oh);
}

static void
_cb_mov_frame_resize(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Media *sd = evas_object_smart_data_get(data);
   Evas_Coord ox, oy, ow, oh;

   if (!sd) return;
   evas_object_geometry_get(data, &ox, &oy, &ow, &oh);
   _type_mov_calc(data, ox, oy, ow, oh);
}

static void
_cb_mov_len_change(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
}

static void
_cb_mov_restart(void *data)
{
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   sd->restart_job = NULL;
   emotion_object_position_set(sd->o_img, 0.0);
   emotion_object_play_set(sd->o_img, EINA_TRUE);
}

static void
_cb_mov_decode_stop(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   if (sd->restart_job) ecore_job_del(sd->restart_job);
   sd->restart_job = ecore_job_add(_cb_mov_restart, data);
}

static void
_cb_mov_progress(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   DBG("progress: '%s' '%3.3f",
       emotion_object_progress_info_get(sd->o_img),
       emotion_object_progress_status_get(sd->o_img));
}

static void
_cb_mov_ref(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   DBG("ref: '%s' num '%i'",
       emotion_object_ref_file_get(sd->o_img),
       emotion_object_ref_num_get(sd->o_img));
}

static void
_type_mov_init(Evas_Object *obj)
{
   Evas_Object *o;
   char *modules[] =
     {
        NULL,
        "gstreamer",
        "xine",
        "generic"
     };
   char *mod = NULL;
        
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   sd->type = TYPE_MOV;
   emotion_init();
   o = sd->o_img = emotion_object_add(evas_object_evas_get(obj));
   if ((sd->config->vidmod >= 0) && 
       (sd->config->vidmod < (int)EINA_C_ARRAY_LENGTH(modules)))
     mod = modules[sd->config->vidmod];
   if (!emotion_object_init(o, mod))
     {
        ERR("can't init emotion module '%s'", mod);
        evas_object_del(sd->o_img);
        sd->o_img = NULL;
        return;
     }
   evas_object_smart_callback_add(o, "frame_decode",
                                  _cb_mov_frame_decode, obj);
   evas_object_smart_callback_add(o, "frame_resize",
                                  _cb_mov_frame_resize, obj);
   evas_object_smart_callback_add(o, "length_change",
                                  _cb_mov_len_change, obj);
   evas_object_smart_callback_add(o, "decode_stop",
                                  _cb_mov_decode_stop, obj);
   evas_object_smart_callback_add(o, "progress_change",
                                  _cb_mov_progress, obj);
   evas_object_smart_callback_add(o, "ref_change",
                                  _cb_mov_ref, obj);
   emotion_object_file_set(o, sd->src);
   evas_object_smart_member_add(o, obj);
   evas_object_clip_set(o, sd->clip);
   emotion_object_position_set(o, 0.0);
   emotion_object_play_set(o, EINA_TRUE);
   if (sd->config->mute) emotion_object_audio_mute_set(o, EINA_TRUE);
}

static void
_type_mov_calc(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   emotion_object_size_get(sd->o_img, &(sd->iw), &(sd->ih));
   if ((w <= 0) || (h <= 0) || (sd->iw <= 0) || (sd->ih <= 0))
     {
        w = 1;
        h = 1;
     }
   else
     {
        int iw, ih;
        double ratio;
        
        ratio = emotion_object_ratio_get(sd->o_img);
        if (ratio > 0.0) sd->iw = (sd->ih * ratio) + 0.5;
        else ratio = (double)sd->iw / (double)sd->ih;
        
        iw = w;
        ih = w / ratio;
        if (ih < h)
          {
             ih = h;
             iw = h * ratio;
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

static void _smart_calculate(Evas_Object *obj);

static void
_smart_add(Evas_Object *obj)
{
   Media *sd;
   Evas_Object_Smart_Clipped_Data *cd;
   Evas_Object *o;

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
   if (sd->o_tmp) evas_object_del(sd->o_tmp);
   if (sd->anim) ecore_timer_del(sd->anim);
   if (sd->restart_job) ecore_job_del(sd->restart_job);
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
_smart_move(Evas_Object *obj, Evas_Coord x __UNUSED__, Evas_Coord y __UNUSED__)
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
media_add(Evas_Object *parent, const char *src, const Config *config, int mode, int *type)
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
   sd->config = config;
   sd->mode = mode;
   if      (_is_fmt(src, extn_img))
     {
        _type_img_init(obj);
        if (type) *type = TYPE_IMG;
     }
   else if (_is_fmt(src, extn_scale))
     {
        _type_scale_init(obj);
        if (type) *type = TYPE_SCALE;
     }
   else if (_is_fmt(src, extn_edj))
     {
        _type_edje_init(obj);
        if (type) *type = TYPE_EDJE;
     }
   else if (_is_fmt(src, extn_mov))
     {
        _type_mov_init(obj);
        if (type) *type = TYPE_MOV;
     }
   return obj;
}

void
media_mute_set(Evas_Object *obj, Eina_Bool mute)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->type != TYPE_MOV) return;
   emotion_object_audio_mute_set(sd->o_img, mute);
}
