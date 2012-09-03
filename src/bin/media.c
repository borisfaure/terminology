#include "private.h"

#include <Elementary.h>
#include <Emotion.h>
#include <stdlib.h>
#include <unistd.h>
#include "media.h"
#include "config.h"
#include "utils.h"

typedef struct _Media Media;

struct _Media
{
   Evas_Object_Smart_Clipped_Data __clipped_data;
   Evas_Object *clip, *o_img, *o_tmp, *o_ctrl, *o_busy;
   Ecore_Timer *anim;
   Ecore_Timer *smooth_timer;
   Ecore_Job *restart_job;
   Ecore_Con_Url *url;
   Ecore_Event_Handler *url_prog_hand, *url_compl_hand;
   const char *src;
   const char *ext;
   const char *realf;
   const Config *config;
   double download_perc;
   int tmpfd;
   int w, h;
   int iw, ih;
   int sw, sh;
   int fr, frnum;
   int mode, type;
   int resizes;
   Eina_Bool nosmooth : 1;
   Eina_Bool downloading : 1;
};

static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _meida_sc = EVAS_SMART_CLASS_INIT_NULL;

static const char *extn_img[] =
{
   ".png", ".jpg", ".jpeg", ".jpe", ".jfif", ".tif", ".tiff", ".gif",
   ".bmp", ".ico", ".ppm", ".pgm", ".pbm", ".pnm", ".xpm", ".psd", ".wbmp",
   ".cur", ".xcf", ".xcf.gz", ".arw", ".cr2", ".crw", ".dcr", ".dng", ".k25",
   ".kdc", ".erf", ".mrw", ".nef", ".nrf", ".nrw", ".orf", ".raw", ".rw2",
   ".rw2", ".pef", ".raf", ".sr2", ".srf", ".x3f", ".webp", 
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

static const char *
_is_fmt(const char *f, const char **extn)
{
   int i, len, l;
   
   len = strlen(f);
   for (i = 0; extn[i]; i++)
     {
        l = strlen(extn[i]);
        if (len < l) continue;
        if (!strcasecmp(extn[i], f + len - l)) return extn[i];
     }
   return NULL;
}

//////////////////////// img
static void
_cb_img_preloaded(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   evas_object_show(sd->o_img);
   evas_object_show(sd->clip);
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
   evas_object_image_file_set(o, sd->realf, NULL);
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
        int iw = 1, ih = 1;
        
        if (sd->mode == MEDIA_BG)
          {
             iw = w;
             ih = (sd->ih * w) / sd->iw;
             if (ih < h)
               {
                  ih = h;
                  iw = (sd->iw * h) / sd->ih;
                  if (iw < w) iw = w;
               }
          }
        else if (sd->mode == MEDIA_POP)
          {
             iw = w;
             ih = (sd->ih * w) / sd->iw;
             if (ih > h)
               {
                  ih = h;
                  iw = (sd->iw * h) / sd->ih;
                  if (iw > w) iw = w;
               }
             if ((iw > sd->iw) || (ih > sd->ih))
               {
                  iw = sd->iw;
                  ih = sd->ih;
               }
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
   if (!sd->o_tmp)
     {
        evas_object_show(sd->o_img);
        evas_object_show(sd->clip);
     }
   else
     {
        evas_object_del(sd->o_img);
        sd->o_img = sd->o_tmp;
        sd->o_tmp = NULL;
        evas_object_show(sd->o_img);
        evas_object_show(sd->clip);
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
   evas_object_image_file_set(o, sd->realf, NULL);
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
        int iw = 1, ih = 1;
        
        if (sd->mode == MEDIA_BG)
          {
             iw = w;
             ih = (sd->ih * w) / sd->iw;
             if (ih < h)
               {
                  ih = h;
                  iw = (sd->iw * h) / sd->ih;
                  if (iw < w) iw = w;
               }
          }
        else if (sd->mode == MEDIA_POP)
          {
             iw = w;
             ih = (sd->ih * w) / sd->iw;
             if (ih > h)
               {
                  ih = h;
                  iw = (sd->iw * h) / sd->ih;
                  if (iw > w) iw = w;
               }
          }
        x += ((w - iw) / 2);
        y += ((h - ih) / 2);
        w = iw;
        h = ih;
     }
   if (!sd->nosmooth)
     {
        Evas_Coord lw, lh;
        
        lw = w;
        lh = h;
        if (lw < 256) lw = 256;
        if (lh < 256) lh = 256;
        if ((lw != sd->sw) || (lh != sd->sh))
          {
             o = sd->o_tmp = evas_object_image_filled_add(evas_object_evas_get(obj));
             evas_object_smart_member_add(o, obj);
             evas_object_clip_set(o, sd->clip);
             evas_object_event_callback_add(o, EVAS_CALLBACK_IMAGE_PRELOADED,
                                            _cb_scale_preloaded, obj);
             evas_object_image_file_set(o, sd->realf, NULL);
             evas_object_image_load_size_set(sd->o_tmp, lw, lh);
             evas_object_image_preload(o, EINA_FALSE);
          }
        sd->sw = lw;
        sd->sh = lh;
     }
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
   evas_object_show(sd->clip);
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
        if (edje_object_file_set(o, sd->realf, groups[i]))
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
   double len, pos;

   if (!sd) return;
   evas_object_geometry_get(data, &ox, &oy, &ow, &oh);
   evas_object_show(sd->o_img);
   evas_object_show(sd->clip);
   _type_mov_calc(data, ox, oy, ow, oh);

   len = emotion_object_play_length_get(sd->o_img);
   pos = emotion_object_position_get(sd->o_img);
   pos /= len;
   edje_object_part_drag_value_set(sd->o_ctrl, "terminology.posdrag", pos, pos);
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
_cb_media_play(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   media_play_set(data, EINA_TRUE);
}

static void
_cb_media_pause(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   media_play_set(data, EINA_FALSE);
}

static void
_cb_media_stop(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   media_stop(data);
}

static void
_cb_media_vol(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   double vx, vy;
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   edje_object_part_drag_value_get(sd->o_ctrl, "terminology.voldrag", &vx, &vy);
   media_volume_set(data, vx + vy);
}

static void
_cb_media_pos(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   double vx, vy;
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   edje_object_part_drag_value_get(sd->o_ctrl, "terminology.posdrag", &vx, &vy);
   media_position_set(data, vx + vy);
}

static void
_type_mov_init(Evas_Object *obj)
{
   Evas_Object *o;
   double vol;
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
   emotion_object_file_set(o, sd->realf);
   evas_object_smart_member_add(o, obj);
   evas_object_clip_set(o, sd->clip);

   o = sd->o_ctrl = edje_object_add(evas_object_evas_get(obj));
   theme_apply(o, sd->config, "terminology/mediactrl");
   vol = emotion_object_audio_volume_get(sd->o_img);
   edje_object_part_drag_value_set(o, "terminology.voldrag", vol, vol);
   edje_object_signal_callback_add(o, "play", "",
                                   _cb_media_play, obj);
   edje_object_signal_callback_add(o, "pause", "",
                                   _cb_media_pause, obj);
   edje_object_signal_callback_add(o, "stop", "",
                                   _cb_media_stop, obj);
   edje_object_signal_callback_add(o, "drag", "terminology.posdrag",
                                   _cb_media_pos, obj);
   edje_object_signal_callback_add(o, "drag", "terminology.voldrag",
                                   _cb_media_vol, obj);
   /* TODO where to stack the object in the ui? controls cannot be part of
    * the 'media smart obj' becouse controls need to be on top of the term obj.
    * 
    * I think we need to swallow inside the bg object. but how to
    * retrive the edje bg object from here?
    * */
   evas_object_show(o);

   media_position_set(obj, 0.0);
   media_play_set(obj, EINA_TRUE);
   if (sd->config->mute) media_mute_set(obj, EINA_TRUE);
}

static void
_type_mov_calc(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;

   evas_object_move(sd->o_ctrl, x, y);
   evas_object_resize(sd->o_ctrl, w, h);

   emotion_object_size_get(sd->o_img, &(sd->iw), &(sd->ih));
   if ((w <= 0) || (h <= 0) || (sd->iw <= 0) || (sd->ih <= 0))
     {
        w = 1;
        h = 1;
     }
   else
     {
        int iw = 1, ih = 1;
        double ratio;
        
        ratio = emotion_object_ratio_get(sd->o_img);
        if (ratio > 0.0) sd->iw = (sd->ih * ratio) + 0.5;
        else ratio = (double)sd->iw / (double)sd->ih;

        if (sd->mode == MEDIA_BG)
          {
             iw = w;
             ih = w / ratio;
             if (ih < h)
               {
                  ih = h;
                  iw = h * ratio;
                  if (iw < w) iw = w;
               }
          }
        else if (sd->mode == MEDIA_POP)
          {
             iw = w;
             ih = w / ratio;
             if (ih > h)
               {
                  ih = h;
                  iw = h * ratio;
                  if (iw > w) iw = w;
               }
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
}

static void
_smart_del(Evas_Object *obj)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->url)
     {
        ecore_event_handler_del(sd->url_prog_hand);
        ecore_event_handler_del(sd->url_compl_hand);
        ecore_con_url_free(sd->url);
        ecore_con_url_shutdown();
        ecore_con_shutdown();
     }
   sd->url = NULL;
   sd->url_prog_hand = NULL;
   sd->url_compl_hand = NULL;
   if (sd->tmpfd >= 0)
     {
        unlink(sd->realf);
        close(sd->tmpfd);
     }
   if (sd->src) eina_stringshare_del(sd->src);
   if (sd->realf) eina_stringshare_del(sd->realf);
   if (sd->clip) evas_object_del(sd->clip);
   if (sd->o_img) evas_object_del(sd->o_img);
   if (sd->o_tmp) evas_object_del(sd->o_tmp);
   if (sd->o_ctrl) evas_object_del(sd->o_ctrl);
   if (sd->o_busy) evas_object_del(sd->o_busy);
   if (sd->anim) ecore_timer_del(sd->anim);
   if (sd->smooth_timer) sd->smooth_timer = ecore_timer_del(sd->smooth_timer);
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

static Eina_Bool
_unsmooth_timeout(void *data)
{
   Media *sd = evas_object_smart_data_get(data);
   Evas_Coord ox, oy, ow, oh;

   if (!sd) return EINA_FALSE;
   evas_object_geometry_get(data, &ox, &oy, &ow, &oh);
   sd->smooth_timer = NULL;
   sd->nosmooth = EINA_FALSE;
   if ((sd->type == TYPE_IMG) || (sd->type == TYPE_SCALE))
     {
        evas_object_image_smooth_scale_set(sd->o_img, !sd->nosmooth);
        if (sd->o_tmp)
          evas_object_image_smooth_scale_set(sd->o_tmp, !sd->nosmooth);
        if (sd->type == TYPE_SCALE) _type_scale_calc(data, ox, oy, ow, oh);
     }
   else if (sd->type == TYPE_MOV)
     emotion_object_smooth_scale_set(sd->o_img, !sd->nosmooth);
   return EINA_FALSE;
}

static void
_smooth_handler(Evas_Object *obj)
{
   Media *sd = evas_object_smart_data_get(obj);
   double interval;
   
   if (!sd) return;
   interval = ecore_animator_frametime_get();
   if (interval <= 0.0) interval = 1.0/60.0;
   if (!sd->nosmooth)
     {
        if (sd->resizes >= 2)
          {
             sd->nosmooth = EINA_TRUE;
             sd->resizes = 0;
             if ((sd->type == TYPE_IMG) || (sd->type == TYPE_SCALE))
               {
                  evas_object_image_smooth_scale_set(sd->o_img, !sd->nosmooth);
                  if (sd->o_tmp)
                    evas_object_image_smooth_scale_set(sd->o_tmp, !sd->nosmooth);
               }
             else if (sd->type == TYPE_MOV)
               emotion_object_smooth_scale_set(sd->o_img, !sd->nosmooth);
             if (sd->smooth_timer)
               sd->smooth_timer = ecore_timer_del(sd->smooth_timer);
             sd->smooth_timer = ecore_timer_add(interval * 10,
                                                _unsmooth_timeout, obj);
          }
     }
   else
     {
        if (sd->smooth_timer)
          sd->smooth_timer = ecore_timer_del(sd->smooth_timer);
        sd->smooth_timer = ecore_timer_add(interval * 10,
                                           _unsmooth_timeout, obj);
     }
}

static void
_smart_calculate(Evas_Object *obj)
{
   Media *sd = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh;

   if (!sd) return;
   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   if ((ow != sd->w) || (oh != sd->h)) sd->resizes++;
   else sd->resizes = 0;
   _smooth_handler(obj);
   sd->w = ow;
   sd->h = oh;
   if (sd->type == TYPE_IMG) _type_img_calc(obj, ox, oy, ow, oh);
   else if (sd->type == TYPE_SCALE) _type_scale_calc(obj, ox, oy, ow, oh);
   else if (sd->type == TYPE_EDJE) _type_edje_calc(obj, ox, oy, ow, oh);
   else if (sd->type == TYPE_MOV) _type_mov_calc(obj, ox, oy, ow, oh);
   evas_object_move(sd->clip, ox, oy);
   evas_object_resize(sd->clip, ow, oh);
   if (sd->o_busy)
     {
        evas_object_move(sd->o_busy, ox, oy);
        evas_object_resize(sd->o_busy, ow, oh);
     }
}

static void
_smart_move(Evas_Object *obj, Evas_Coord x __UNUSED__, Evas_Coord y __UNUSED__)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   evas_object_smart_changed(obj);
}

static Eina_Bool
_url_prog_cb(void *data, int type __UNUSED__, void *event_info)
{
   Ecore_Con_Event_Url_Progress *ev = event_info;
   Evas_Object *obj = data;
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return EINA_TRUE;
   if (ev->url_con != sd->url) return EINA_TRUE;
   if (ev->down.total > 0.0)
     {
        double perc;
        
        if (!sd->downloading)
          edje_object_signal_emit(sd->o_busy, "downloading", "terminology");
        sd->downloading = EINA_TRUE;
        perc = ev->down.now / ev->down.total;
        if (perc != sd->download_perc)
          {
             Edje_Message_Float msg;
             
             sd->download_perc = perc;
             msg.val = perc;
             edje_object_message_send(sd->o_busy, EDJE_MESSAGE_FLOAT, 1, &msg);
          }
     }
   return EINA_FALSE;
}

static Eina_Bool
_url_compl_cb(void *data, int type __UNUSED__, void *event_info)
{
   Ecore_Con_Event_Url_Complete *ev = event_info;
   Evas_Object *obj = data;
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return EINA_TRUE;
   if (ev->url_con != sd->url) return EINA_TRUE;
   
   edje_object_signal_emit(sd->o_busy, "done", "terminology");
   ecore_event_handler_del(sd->url_prog_hand);
   ecore_event_handler_del(sd->url_compl_hand);
   ecore_con_url_free(sd->url);
   ecore_con_url_shutdown();
   ecore_con_shutdown();
   sd->url = NULL;
   sd->url_prog_hand = NULL;
   sd->url_compl_hand = NULL;
   
   if      (_is_fmt(sd->src, extn_img))
     _type_img_init(obj);
   else if (_is_fmt(sd->src, extn_scale))
     _type_scale_init(obj);
   else if (_is_fmt(sd->src, extn_edj))
     _type_edje_init(obj);
   else if (_is_fmt(sd->src, extn_mov))
     _type_mov_init(obj);
   evas_object_raise(sd->o_busy);
   _smart_calculate(obj);
   return EINA_FALSE;
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
   sd->tmpfd = -1;

   if (link_is_url(sd->src))
     {
        const char *ext = NULL;
        char *tbuf;
        
        if (!strncasecmp(sd->src, "www.", 4))
          {
             tbuf = alloca(strlen(sd->src) + 10);
             strcpy(tbuf, "http://");
             strcat(tbuf, sd->src);
          }
        else if (!strncasecmp(sd->src, "ftp.", 4))
          {
             tbuf = alloca(strlen(sd->src) + 10);
             strcpy(tbuf, "ftp://");
             strcat(tbuf, sd->src);
          }
        else
          tbuf = (char *)sd->src;
        if      ((ext = _is_fmt(src, extn_img)))
          sd->ext = ext;
        else if ((ext = _is_fmt(src, extn_scale)))
          sd->ext = ext;
        else if ((ext = _is_fmt(src, extn_edj)))
          sd->ext = ext;
        else if ((ext = _is_fmt(src, extn_mov)))
          sd->ext = ext;
        if (sd->ext)
          {
             char buf[4096];
             
             snprintf(buf, sizeof(buf), "/tmp/tmngyXXXXXX%s", sd->ext);
             sd->tmpfd = mkstemps(buf, strlen(sd->ext));
             if (sd->tmpfd >= 0)
               {
                  ecore_con_init();
                  ecore_con_url_init();
                  sd->url = ecore_con_url_new(tbuf);
                  if (!sd->url)
                    {
                       unlink(buf);
                       close(sd->tmpfd);
                       ecore_con_url_shutdown();
                       ecore_con_shutdown();
                    }
                  else
                    {
                       ecore_con_url_fd_set(sd->url, sd->tmpfd);
                       if (!ecore_con_url_get(sd->url))
                         {
                            unlink(buf);
                            close(sd->tmpfd);
                            ecore_con_url_shutdown();
                            ecore_con_shutdown();
                            sd->url = NULL;
                         }
                       else
                         {
                            Evas_Object *o;
                            
                            o = sd->o_busy = edje_object_add(evas_object_evas_get(obj));
                            evas_object_smart_member_add(o, obj);
                            theme_apply(o, sd->config, "terminology/mediabusy");
                            evas_object_show(o);
                            edje_object_signal_emit(o, "busy", "terminology");
                            
                            sd->realf = eina_stringshare_add(buf);
                            sd->url_prog_hand = ecore_event_handler_add
                              (ECORE_CON_EVENT_URL_PROGRESS, _url_prog_cb, obj);
                            sd->url_compl_hand = ecore_event_handler_add
                              (ECORE_CON_EVENT_URL_COMPLETE, _url_compl_cb, obj);
                         }
                    }
               }
          }
     }

   if (!sd->url) sd->realf = eina_stringshare_add(sd->src);
   
   if      (_is_fmt(sd->src, extn_img))
     {
        if (!sd->url) _type_img_init(obj);
        if (type) *type = TYPE_IMG;
     }
   else if (_is_fmt(sd->src, extn_scale))
     {
        if (!sd->url) _type_scale_init(obj);
        if (type) *type = TYPE_SCALE;
     }
   else if (_is_fmt(sd->src, extn_edj))
     {
        if (!sd->url) _type_edje_init(obj);
        if (type) *type = TYPE_EDJE;
     }
   else if (_is_fmt(sd->src, extn_mov))
     {
        if (!sd->url) _type_mov_init(obj);
        if (type) *type = TYPE_MOV;
     }
   return obj;
}

void
media_mute_set(Evas_Object *obj, Eina_Bool mute)
{
   Media *sd = evas_object_smart_data_get(obj);
   if ((!sd) || (sd->type != TYPE_MOV)) return;
   emotion_object_audio_mute_set(sd->o_img, mute);
   if (mute)
      edje_object_signal_emit(sd->o_ctrl, "mute,set", "terminology");
   else
      edje_object_signal_emit(sd->o_ctrl, "mute,unset", "terminology");
}

void
media_play_set(Evas_Object *obj, Eina_Bool play)
{
   Media *sd = evas_object_smart_data_get(obj);
   if ((!sd) || (sd->type != TYPE_MOV)) return;
   emotion_object_play_set(sd->o_img, play);
   if (play)
      edje_object_signal_emit(sd->o_ctrl, "play,set", "terminology");
   else
      edje_object_signal_emit(sd->o_ctrl, "pause,set", "terminology");
}

void
media_stop(Evas_Object *obj)
{
   Media *sd = evas_object_smart_data_get(obj);
   if ((!sd) || (sd->type != TYPE_MOV)) return;

   evas_object_del(obj);
}

void
media_position_set(Evas_Object *obj, double pos)
{
   double len;
   Media *sd = evas_object_smart_data_get(obj);
   if ((!sd) || (sd->type != TYPE_MOV)) return;
   len = emotion_object_play_length_get(sd->o_img);
   emotion_object_position_set(sd->o_img, len * pos);
}

void
media_volume_set(Evas_Object *obj, double vol)
{
   Media *sd = evas_object_smart_data_get(obj);
   if ((!sd) || (sd->type != TYPE_MOV)) return;
   emotion_object_audio_volume_set(sd->o_img, vol);
   edje_object_part_drag_value_set(sd->o_ctrl, "terminology.voldrag", vol, vol);
}

int
media_src_type_get(const char *src)
{
   int type = TYPE_UNKNOWN;
   
   if      (_is_fmt(src, extn_img))   type = TYPE_IMG;
   else if (_is_fmt(src, extn_scale)) type = TYPE_SCALE;
   else if (_is_fmt(src, extn_edj))   type = TYPE_EDJE;
   else if (_is_fmt(src, extn_mov))   type = TYPE_MOV;
   return type;
}
