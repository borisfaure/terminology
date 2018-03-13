#include "private.h"

#include <Elementary.h>
#include <Ethumb_Client.h>
#include <Emotion.h>
#include <Efreet.h>
#include <stdlib.h>
#include <unistd.h>
#include "media.h"
#include "config.h"
#include "utils.h"

typedef struct _Media Media;

struct _Media
{
   Evas_Object_Smart_Clipped_Data __clipped_data;
   Evas_Object *clip, *o_img, *o_tmp, *o_ctrl, *o_busy, *o_event;
   Ecore_Timer *anim;
   Ecore_Timer *smooth_timer;
   Ecore_Job *restart_job;
   Ecore_Con_Url *url;
   Ecore_Event_Handler *url_prog_hand, *url_compl_hand;
   Ethumb_Client_Async *et_req;
   const char *src;
   const char *ext;
   const char *realf;
   const Config *config;
   double download_perc;
   int tmpfd;
   int w, h;
   int iw, ih;
   int sw, sh;
   int fr, frnum, loops;
   int mode;
   Media_Type type;
   int resizes;
   struct {
      Evas_Coord x, y;
      unsigned char down : 1;
   } down;
   unsigned char nosmooth : 1;
   unsigned char downloading : 1;
   unsigned char queued : 1;
   unsigned char pos_drag : 1;
};

static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

#include "extns.h"

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

//////////////////////// thumb

static Ethumb_Client *et_client = NULL;
static Eina_Bool et_connected = EINA_FALSE;
static Eina_List *et_queue = NULL;

static void _et_init(void);
static int _type_thumb_init2(Evas_Object *obj);

static void
_et_disconnect(void *_data EINA_UNUSED, Ethumb_Client *c)
{
   if (c != et_client) return;
   ethumb_client_disconnect(et_client);
   et_connected = EINA_FALSE;
   et_client = NULL;
   if (et_queue) _et_init();
}

static void
_et_connect(void *_data EINA_UNUSED, Ethumb_Client *c, Eina_Bool ok)
{
   if (ok)
     {
        Evas_Object *o;

        et_connected = EINA_TRUE;
        ethumb_client_on_server_die_callback_set(c, _et_disconnect,
                                                 NULL, NULL);
        EINA_LIST_FREE(et_queue, o)
          _type_thumb_init2(o);
     }
   else
     et_client = NULL;
}

static void
_et_init(void)
{
   if (et_client) return;

   ethumb_client_init();
   et_client = ethumb_client_connect(_et_connect, NULL, NULL);
}

static void
_type_thumb_calc(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
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
        x += ((w - iw) / 2);
        y += ((h - ih) / 2);
        w = iw;
        h = ih;
     }
   evas_object_move(sd->o_img, x, y);
   evas_object_resize(sd->o_img, w, h);
}

static void
_cb_thumb_preloaded(void *data,
                    Evas *_e EINA_UNUSED,
                    Evas_Object *_obj EINA_UNUSED,
                    void *_event EINA_UNUSED)
{
   Media *sd = evas_object_smart_data_get(data);
   Evas_Coord ox, oy, ow, oh;
   if (!sd) return;

   evas_object_geometry_get(data, &ox, &oy, &ow, &oh);
   _type_thumb_calc(data, ox, oy, ow, oh);
   evas_object_show(sd->o_img);
   evas_object_show(sd->clip);
}

static void
_et_done(Ethumb_Client *_c EINA_UNUSED,
         const char *file, const char *key, void *data)
{
   Evas_Object *obj = data;
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;

//   if (c != et_client) return;
   sd->et_req = NULL;
   evas_object_event_callback_add(sd->o_img, EVAS_CALLBACK_IMAGE_PRELOADED,
                                  _cb_thumb_preloaded, obj);
   evas_object_image_file_set(sd->o_img, file, key);
   evas_object_image_size_get(sd->o_img, &(sd->iw), &(sd->ih));
   evas_object_image_preload(sd->o_img, EINA_FALSE);
}

static void
_et_error(Ethumb_Client *_c EINA_UNUSED, void *data)
{
   Evas_Object *obj = data;
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;

//   if (c != et_client) return;
   sd->et_req = NULL;
}

static int
_type_thumb_init2(Evas_Object *obj)
{
   Media *sd = evas_object_smart_data_get(obj);
   Eina_Bool ok;

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, -1);

   if ((sd->realf) && (sd->realf[0] != '/'))
     {
        /* TODO: Listen for theme cache changes */
        static const char *icon_theme = NULL;
        const char *fl = NULL;

        if (!icon_theme)
          {
             Efreet_Icon_Theme *theme;

             theme = efreet_icon_theme_find(getenv("E_ICON_THEME"));
             if (!theme)
               {
                  const char **itr;
                  static const char *themes[] = {
                       "Human", "oxygen", "gnome", "hicolor", NULL
                  };
                  for (itr = themes; *itr; itr++)
                    {
                       theme = efreet_icon_theme_find(*itr);
                       if (!theme) continue;
                       //try to fetch the icon, if we dont find it, continue at other themes
                       icon_theme = eina_stringshare_add(theme->name.internal);
                       fl = efreet_icon_path_find(icon_theme, sd->realf, sd->iw);
                       if (fl) break;
                    }
               }
             else
               {
                  icon_theme = eina_stringshare_add(theme->name.internal);
               }
          }
        if (!fl)
          fl = efreet_icon_path_find(icon_theme, sd->realf, sd->iw);
        ok = ethumb_client_file_set(et_client, fl, NULL);
        if (!ok)
          return -1;
     }
   else
     {
        ok = ethumb_client_file_set(et_client, sd->realf, NULL);
        if (!ok)
          return -1;
     }

   sd->et_req = ethumb_client_thumb_async_get(et_client, _et_done,
                                              _et_error, obj);
   sd->queued = EINA_FALSE;
   return 0;
}

static int
_type_thumb_init(Evas_Object *obj)
{
   Evas_Object *o;
   Media *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, -1);

   sd->type = MEDIA_TYPE_THUMB;
   _et_init();
   o = sd->o_img = evas_object_image_filled_add(evas_object_evas_get(obj));
   evas_object_image_load_orientation_set(o, EINA_TRUE);
   evas_object_smart_member_add(o, obj);
   evas_object_clip_set(o, sd->clip);
   evas_object_raise(sd->o_event);
   sd->iw = 64;
   sd->ih = 64;
   if (!et_connected)
     {
        et_queue = eina_list_append(et_queue, obj);
        sd->queued = EINA_TRUE;
        return 0;
     }
   return _type_thumb_init2(obj);
}

//////////////////////// img
static void
_cb_img_preloaded(void *data,
                  Evas *_e EINA_UNUSED,
                  Evas_Object *_obj EINA_UNUSED,
                  void *_event EINA_UNUSED)
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
   if ((sd->fr >= sd->frnum) && (fr == 1))
     {
        int loops;

        if (evas_object_image_animated_loop_type_get(sd->o_img) ==
            EVAS_IMAGE_ANIMATED_HINT_NONE)
          {
             sd->anim = NULL;
             return EINA_FALSE;
          }
        sd->loops++;
        loops = evas_object_image_animated_loop_count_get(sd->o_img);
        if (loops != 0) // loop == 0 -> loop forever
          {
             if (loops < sd->loops)
               {
                  sd->anim = NULL;
                  return EINA_FALSE;
               }
          }
     }
   evas_object_image_animated_frame_set(sd->o_img, fr);
   t = evas_object_image_animated_frame_duration_get(sd->o_img, fr, 0);
   ecore_timer_interval_set(sd->anim, t);
   return EINA_TRUE;
}

static int
_type_img_anim_handle(Evas_Object *obj)
{
   double t;
   Media *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, -1);

   if (!evas_object_image_animated_get(sd->o_img))
     return 0;

   sd->fr = 1;
   sd->frnum = evas_object_image_animated_frame_count_get(sd->o_img);
   if (sd->frnum < 2)
     return 0;

   t = evas_object_image_animated_frame_duration_get(sd->o_img, sd->fr, 0);
   sd->anim = ecore_timer_add(t, _cb_img_frame, obj);
   return 0;
}

static int
_type_img_init(Evas_Object *obj)
{
   Evas_Object *o;
   Media *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, -1);

   o = sd->o_img = evas_object_image_filled_add(evas_object_evas_get(obj));
   evas_object_smart_member_add(o, obj);
   evas_object_clip_set(o, sd->clip);
   evas_object_raise(sd->o_event);
   evas_object_event_callback_add(o, EVAS_CALLBACK_IMAGE_PRELOADED,
                                  _cb_img_preloaded, obj);
   evas_object_image_load_orientation_set(o, EINA_TRUE);
   evas_object_image_file_set(o, sd->realf, NULL);
   evas_object_image_size_get(o, &(sd->iw), &(sd->ih));
   evas_object_image_preload(o, EINA_FALSE);
   return _type_img_anim_handle(obj);
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

        if ((sd->mode & MEDIA_SIZE_MASK) == MEDIA_BG)
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
        else if ((sd->mode & MEDIA_SIZE_MASK) == MEDIA_POP)
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
        else if ((sd->mode & MEDIA_SIZE_MASK) == MEDIA_STRETCH)
          {
             iw = w;
             ih = h;
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
_cb_scale_preloaded(void *data,
                    Evas *_e EINA_UNUSED,
                    Evas_Object *_obj EINA_UNUSED,
                    void *_event EINA_UNUSED)
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

static int
_type_scale_init(Evas_Object *obj)
{
   Evas_Object *o;
   Media *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, -1);

   o = sd->o_img = evas_object_image_filled_add(evas_object_evas_get(obj));
   evas_object_smart_member_add(o, obj);
   evas_object_clip_set(o, sd->clip);
   evas_object_raise(sd->o_event);
   evas_object_event_callback_add(o, EVAS_CALLBACK_IMAGE_PRELOADED,
                                  _cb_scale_preloaded, obj);
   evas_object_image_load_orientation_set(o, EINA_TRUE);
   evas_object_image_file_set(o, sd->realf, NULL);
   evas_object_image_size_get(o, &(sd->iw), &(sd->ih));
   evas_object_image_preload(o, EINA_FALSE);

   return 0;
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

        if ((sd->mode & MEDIA_SIZE_MASK) == MEDIA_BG)
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
        else if ((sd->mode & MEDIA_SIZE_MASK) == MEDIA_POP)
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
        else if ((sd->mode & MEDIA_SIZE_MASK) == MEDIA_STRETCH)
          {
             iw = w;
             ih = h;
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
             evas_object_raise(sd->o_event);
             evas_object_event_callback_add(o, EVAS_CALLBACK_IMAGE_PRELOADED,
                                            _cb_scale_preloaded, obj);
             evas_object_image_load_orientation_set(o, EINA_TRUE);
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
_cb_edje_preloaded(void *data,
                   Evas_Object *_obj EINA_UNUSED,
                   const char *_sig EINA_UNUSED,
                   const char *_src EINA_UNUSED)
{
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   evas_object_show(sd->o_img);
   evas_object_show(sd->clip);
}

static int
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

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, -1);

   o = sd->o_img = edje_object_add(evas_object_evas_get(obj));
   evas_object_smart_member_add(o, obj);
   evas_object_clip_set(o, sd->clip);
   evas_object_raise(sd->o_event);
   for (i = 0; groups[i]; i++)
     {
        if (edje_object_file_set(o, sd->realf, groups[i]))
          {
             edje_object_signal_callback_add(o, "preload,done", "",
                                             _cb_edje_preloaded, obj);
             edje_object_preload(o, EINA_FALSE);
             theme_auto_reload_enable(o);
             return 0;
          }
     }
   return 0;
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
_cb_mov_frame_decode(void *data,
                     Evas_Object *_obj EINA_UNUSED,
                     void *_event EINA_UNUSED)
{
   Media *sd = evas_object_smart_data_get(data);
   Evas_Coord ox, oy, ow, oh;
   double len, pos;

   if (!sd) return;
   evas_object_geometry_get(data, &ox, &oy, &ow, &oh);
   evas_object_show(sd->o_img);
   evas_object_show(sd->clip);
   _type_mov_calc(data, ox, oy, ow, oh);

   if (sd->pos_drag) return;
   len = emotion_object_play_length_get(sd->o_img);
   pos = emotion_object_position_get(sd->o_img);
   pos /= len;
   edje_object_part_drag_value_set(sd->o_ctrl, "terminology.posdrag", pos, pos);
}

static void
_cb_mov_frame_resize(void *data,
                     Evas_Object *_obj EINA_UNUSED,
                     void *_event EINA_UNUSED)
{
   Media *sd = evas_object_smart_data_get(data);
   Evas_Coord ox, oy, ow, oh;

   if (!sd) return;
   evas_object_geometry_get(data, &ox, &oy, &ow, &oh);
   _type_mov_calc(data, ox, oy, ow, oh);
}

static void
_cb_mov_len_change(void *data,
                   Evas_Object *_obj EINA_UNUSED,
                   void *_event EINA_UNUSED)
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
_cb_mov_decode_stop(void *data,
                    Evas_Object *_obj EINA_UNUSED,
                    void *_event EINA_UNUSED)
{
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   if (sd->restart_job) ecore_job_del(sd->restart_job);
   sd->restart_job = ecore_job_add(_cb_mov_restart, data);
   evas_object_smart_callback_call(data, "loop", NULL);
}

static void
_cb_mov_progress(void *data,
                 Evas_Object *_obj EINA_UNUSED,
                 void *_event EINA_UNUSED)
{
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   DBG("progress: '%s' '%3.3f",
       emotion_object_progress_info_get(sd->o_img),
       emotion_object_progress_status_get(sd->o_img));
}

static void
_cb_mov_ref(void *data,
            Evas_Object *_obj EINA_UNUSED,
            void *_event EINA_UNUSED)
{
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   DBG("ref: '%s' num '%i'",
       emotion_object_ref_file_get(sd->o_img),
       emotion_object_ref_num_get(sd->o_img));
}

static void
_cb_media_play(void *data,
               Evas_Object *_obj EINA_UNUSED,
               const char *_emission EINA_UNUSED,
               const char *_source EINA_UNUSED)
{
   media_play_set(data, EINA_TRUE);
}

static void
_cb_media_pause(void *data,
                Evas_Object *_obj EINA_UNUSED,
                const char *_emission EINA_UNUSED,
                const char *_source EINA_UNUSED)
{
   media_play_set(data, EINA_FALSE);
}

static void
_cb_media_stop(void *data,
               Evas_Object *_obj EINA_UNUSED,
               const char *_emission EINA_UNUSED,
               const char *_source EINA_UNUSED)
{
   media_stop(data);
}

static void
_cb_media_vol(void *data,
              Evas_Object *_obj EINA_UNUSED,
              const char *_emission EINA_UNUSED,
              const char *_source EINA_UNUSED)
{
   double vx, vy;
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   edje_object_part_drag_value_get(sd->o_ctrl, "terminology.voldrag", &vx, &vy);
   media_volume_set(data, vx + vy);
}

static void
_cb_media_pos_drag_start(void *data,
                         Evas_Object *_obj EINA_UNUSED,
                         const char *_emission EINA_UNUSED,
                         const char *_source EINA_UNUSED)
{
   double vx, vy;
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   sd->pos_drag = 1;
   edje_object_part_drag_value_get(sd->o_ctrl, "terminology.posdrag", &vx, &vy);
   media_position_set(data, vx + vy);
}

static void
_cb_media_pos_drag_stop(void *data,
                        Evas_Object *_obj EINA_UNUSED,
                        const char *_emission EINA_UNUSED,
                        const char *_source EINA_UNUSED)
{
   double pos, len;
   Media *sd = evas_object_smart_data_get(data);
   if (!sd || !sd->pos_drag) return;
   sd->pos_drag = 0;
   len = emotion_object_play_length_get(sd->o_img);
   pos = emotion_object_position_get(sd->o_img);
   pos /= len;
   edje_object_part_drag_value_set(sd->o_ctrl, "terminology.posdrag", pos, pos);
}

static void
_cb_media_pos(void *data,
              Evas_Object *_obj EINA_UNUSED,
              const char *_emission EINA_UNUSED,
              const char *_source EINA_UNUSED)
{
   double vx, vy;
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   edje_object_part_drag_value_get(sd->o_ctrl, "terminology.posdrag", &vx, &vy);
   media_position_set(data, vx + vy);
}

static int
_type_mov_init(Evas_Object *obj)
{
   Evas_Object *o;
   double vol;
   char *modules[] =
     {
        NULL,
        "gstreamer",
        "xine",
        "vlc",
        "gstreamer1"
    };
   char *mod = NULL;
   Media *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, -1);

   emotion_init();
   o = sd->o_img = emotion_object_add(evas_object_evas_get(obj));
   if ((sd->config->vidmod >= 0) &&
       (sd->config->vidmod < (int)EINA_C_ARRAY_LENGTH(modules)))
     mod = modules[sd->config->vidmod];
   if (!emotion_object_init(o, mod))
     {
        ERR(_("Could not Initialize the emotion module '%s'"), mod);
        evas_object_del(sd->o_img);
        sd->o_img = NULL;
        return -1;
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
   if (((sd->mode & MEDIA_OPTIONS_MASK) & MEDIA_RECOVER)
       && (sd->type == MEDIA_TYPE_MOV) && (sd->o_img))
     emotion_object_last_position_load(sd->o_img);
   else
     media_position_set(obj, 0.0);
   evas_object_smart_member_add(o, obj);
   evas_object_clip_set(o, sd->clip);
   evas_object_raise(sd->o_event);

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
   edje_object_signal_callback_add(o, "pos,drag,start", "",
                                   _cb_media_pos_drag_start, obj);
   edje_object_signal_callback_add(o, "pos,drag,stop", "",
                                   _cb_media_pos_drag_stop, obj);
   edje_object_signal_callback_add(o, "pos,drag", "",
                                   _cb_media_pos, obj);
   edje_object_signal_callback_add(o, "vol,drag", "",
                                   _cb_media_vol, obj);
   evas_object_clip_set(o, sd->clip);
   evas_object_show(o);

   media_play_set(obj, EINA_TRUE);
   if (sd->config->mute)
     media_mute_set(obj, EINA_TRUE);
   if (sd->config->visualize)
     media_visualize_set(obj, EINA_TRUE);

   return 0;
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

        if ((sd->mode & MEDIA_SIZE_MASK) == MEDIA_BG)
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
        else if ((sd->mode & MEDIA_SIZE_MASK) == MEDIA_POP)
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
        else if ((sd->mode & MEDIA_SIZE_MASK) == MEDIA_STRETCH)
          {
             iw = w;
             ih = h;
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
   Evas_Object *o;

   sd = calloc(1, sizeof(Media));
   EINA_SAFETY_ON_NULL_RETURN(sd);
   evas_object_smart_data_set(obj, sd);

   _parent_sc.add(obj);

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
   if (((sd->mode & MEDIA_OPTIONS_MASK) & MEDIA_SAVE)
       && (sd->type == MEDIA_TYPE_MOV) && (sd->o_img))
     emotion_object_last_position_save(sd->o_img);
   if (sd->url)
     {
        ecore_event_handler_del(sd->url_prog_hand);
        ecore_event_handler_del(sd->url_compl_hand);
        ecore_con_url_free(sd->url);
     }
   sd->url = NULL;
   sd->url_prog_hand = NULL;
   sd->url_compl_hand = NULL;
   if (sd->tmpfd >= 0)
     {
        if (sd->realf) unlink(sd->realf);
        close(sd->tmpfd);
     }
   if (sd->src) eina_stringshare_del(sd->src);
   if (sd->realf) eina_stringshare_del(sd->realf);
   if (sd->clip) evas_object_del(sd->clip);
   if (sd->o_img) evas_object_del(sd->o_img);
   if (sd->o_tmp) evas_object_del(sd->o_tmp);
   if (sd->o_ctrl) evas_object_del(sd->o_ctrl);
   if (sd->o_busy) evas_object_del(sd->o_busy);
   if (sd->o_event) evas_object_del(sd->o_event);
   if (sd->anim) ecore_timer_del(sd->anim);
   if (sd->smooth_timer) sd->smooth_timer = ecore_timer_del(sd->smooth_timer);
   if (sd->restart_job) ecore_job_del(sd->restart_job);
   if ((et_client) && (sd->et_req))
     ethumb_client_thumb_async_cancel(et_client, sd->et_req);
   if (sd->queued) et_queue = eina_list_remove(et_queue, obj);
   sd->et_req = NULL;

   _parent_sc.del(obj);
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
   if ((sd->type == MEDIA_TYPE_IMG) || (sd->type == MEDIA_TYPE_SCALE))
     {
        evas_object_image_smooth_scale_set(sd->o_img, !sd->nosmooth);
        if (sd->o_tmp)
          evas_object_image_smooth_scale_set(sd->o_tmp, !sd->nosmooth);
        if (sd->type == MEDIA_TYPE_SCALE) _type_scale_calc(data, ox, oy, ow, oh);
     }
   else if (sd->type == MEDIA_TYPE_MOV)
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
             if ((sd->type == MEDIA_TYPE_IMG) || (sd->type == MEDIA_TYPE_SCALE))
               {
                  evas_object_image_smooth_scale_set(sd->o_img, !sd->nosmooth);
                  if (sd->o_tmp)
                    evas_object_image_smooth_scale_set(sd->o_tmp, !sd->nosmooth);
               }
             else if (sd->type == MEDIA_TYPE_MOV)
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
   switch (sd->type)
     {
      case MEDIA_TYPE_IMG: _type_img_calc(obj, ox, oy, ow, oh); break;
      case MEDIA_TYPE_SCALE: _type_scale_calc(obj, ox, oy, ow, oh); break;
      case MEDIA_TYPE_EDJE: _type_edje_calc(obj, ox, oy, ow, oh); break;
      case MEDIA_TYPE_MOV: _type_mov_calc(obj, ox, oy, ow, oh); break;
      case MEDIA_TYPE_THUMB: _type_thumb_calc(obj, ox, oy, ow, oh); break;
      case MEDIA_TYPE_UNKNOWN:
         return;
     }
   evas_object_move(sd->clip, ox, oy);
   evas_object_resize(sd->clip, ow, oh);
   if (sd->o_busy)
     {
        evas_object_move(sd->o_busy, ox, oy);
        evas_object_resize(sd->o_busy, ow, oh);
     }
   if (sd->o_event)
     {
        evas_object_move(sd->o_event, ox, oy);
        evas_object_resize(sd->o_event, ow, oh);
     }
}

static void
_smart_move(Evas_Object *obj,
            Evas_Coord _x EINA_UNUSED,
            Evas_Coord _y EINA_UNUSED)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   evas_object_smart_changed(obj);
}

static Eina_Bool
_url_prog_cb(void *data,
             int _type EINA_UNUSED,
             void *event_info)
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
_url_compl_cb(void *data,
              int _type EINA_UNUSED,
              void *event_info)
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
   sd->url = NULL;
   sd->url_prog_hand = NULL;
   sd->url_compl_hand = NULL;

   switch (sd->type)
     {
      case MEDIA_TYPE_IMG:
         _type_img_init(obj);
         break;
      case MEDIA_TYPE_SCALE:
         _type_scale_init(obj);
         break;
      case MEDIA_TYPE_EDJE:
         _type_edje_init(obj);
         break;
      case MEDIA_TYPE_MOV:
         _type_mov_init(obj);
         break;
      default:
         break;
     }
   evas_object_raise(sd->o_busy);
   evas_object_raise(sd->o_event);
   _smart_calculate(obj);
   return EINA_FALSE;
}

static void
_mouse_down_cb(void *data,
               Evas *_e EINA_UNUSED,
               Evas_Object *_obj EINA_UNUSED,
               void *event)
{
   Evas_Event_Mouse_Down *ev = event;
   Media *sd = evas_object_smart_data_get(data);
   if (!sd) return;

   if (sd->down.down) return;
   if (ev->button != 1) return;
   sd->down.x = ev->canvas.x;
   sd->down.y = ev->canvas.y;
   sd->down.down = EINA_TRUE;
}

static void
_mouse_up_cb(void *data,
             Evas *_e EINA_UNUSED,
             Evas_Object *_obj EINA_UNUSED,
             void *event)
{
   Evas_Event_Mouse_Up *ev = event;
   Media *sd = evas_object_smart_data_get(data);
   Evas_Coord dx, dy;
   if (!sd) return;

   if (!sd->down.down) return;
   sd->down.down = EINA_FALSE;
   dx = abs(ev->canvas.x - sd->down.x);
   dy = abs(ev->canvas.y - sd->down.y);
   if ((dx <= elm_config_finger_size_get()) &&
       (dy <= elm_config_finger_size_get()))
     evas_object_smart_callback_call(data, "clicked", NULL);
}

static void
_smart_init(void)
{
   static Evas_Smart_Class sc;

   evas_object_smart_clipped_smart_set(&_parent_sc);
   sc           = _parent_sc;
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
media_add(Evas_Object *parent, const char *src, const Config *config, int mode,
          Media_Type type)
{
   Evas *e;
   Evas_Object *obj = NULL;
   Media *sd = NULL;

   EINA_SAFETY_ON_NULL_RETURN_VAL(parent, NULL);
   e = evas_object_evas_get(parent);
   if (!e)
     {
        ERR("can not get evas");
        return NULL;
     }

   if (!_smart) _smart_init();
   obj = evas_object_smart_add(e, _smart);
   sd = evas_object_smart_data_get(obj);
   if (!sd) return obj;

   sd->src = eina_stringshare_add(src);
   sd->config = config;
   sd->mode = mode;
   sd->tmpfd = -1;
   sd->type = type;

#if HAVE_MKSTEMPS
   if (link_is_url(sd->src) && (type != MEDIA_TYPE_MOV))
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
        else
          {
             switch (type)
               {
                case MEDIA_TYPE_IMG:
                   sd->ext = ".png";
                   break;
                case MEDIA_TYPE_SCALE:
                   sd->ext = ".svg";
                   break;
                case MEDIA_TYPE_EDJE:
                   sd->ext = ".edj";
                   break;
                case MEDIA_TYPE_MOV:
                case MEDIA_TYPE_UNKNOWN:
                case MEDIA_TYPE_THUMB:
                   break;
               }
          }
        if (sd->ext)
          {
             char buf[4096];

             snprintf(buf, sizeof(buf), "/tmp/tmngyXXXXXX%s", sd->ext);
             sd->tmpfd = mkstemps(buf, strlen(sd->ext));
             if (sd->tmpfd >= 0)
               {
                  sd->url = ecore_con_url_new(tbuf);
                  if (!sd->url)
                    {
                       unlink(buf);
                       close(sd->tmpfd);
                    }
                  else
                    {
                       ecore_con_url_fd_set(sd->url, sd->tmpfd);
                       if (!ecore_con_url_get(sd->url))
                         {
                            unlink(buf);
                            close(sd->tmpfd);
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
             else
               {
                  ERR(_("Function %s failed: %s"), "mkstemps()", strerror(errno));
               }
          }
     }
#endif

   if (!sd->url)
     sd->realf = eina_stringshare_add(sd->src);

   if ((mode & MEDIA_SIZE_MASK) == MEDIA_THUMB)
     {
        // XXX: handle sd->url being true?
        if (_type_thumb_init(obj) < 0)
          {
             ERR("failed to init '%s'", src);
             goto err;
          }
     }
   else
     {
        switch (type)
          {
           case MEDIA_TYPE_IMG:
             if (!sd->url && (_type_img_init(obj) < 0))
               {
                  ERR("failed to init '%s'", src);
                  goto err;
               }
             break;
           case MEDIA_TYPE_SCALE:
             if (!sd->url && (_type_scale_init(obj) < 0))
               {
                  ERR("failed to init '%s'", src);
                  goto err;
               }
             break;
           case MEDIA_TYPE_EDJE:
             if (!sd->url && (_type_edje_init(obj) < 0))
               {
                  ERR("failed to init '%s'", src);
                  goto err;
               }
             break;
           case MEDIA_TYPE_MOV:
             if (!sd->url && (_type_mov_init(obj) < 0))
               {
                  ERR("failed to init '%s'", src);
                  goto err;
               }
             break;
           default:
             break;
          }
     }

   sd->o_event = evas_object_rectangle_add(e);
   evas_object_color_set(sd->o_event, 0, 0, 0, 0);
   evas_object_repeat_events_set(sd->o_event, EINA_TRUE);
   evas_object_smart_member_add(sd->o_event, obj);
   evas_object_clip_set(sd->o_event, sd->clip);
   evas_object_show(sd->o_event);
   evas_object_event_callback_add(sd->o_event, EVAS_CALLBACK_MOUSE_DOWN,
                                  _mouse_down_cb, obj);
   evas_object_event_callback_add(sd->o_event, EVAS_CALLBACK_MOUSE_UP,
                                  _mouse_up_cb, obj);

   return obj;

err:
   if (obj)
     evas_object_del(obj);
   return NULL;
}

void
media_mute_set(Evas_Object *obj, Eina_Bool mute)
{
   Media *sd = evas_object_smart_data_get(obj);
   if ((!sd) || (sd->type != MEDIA_TYPE_MOV)) return;
   emotion_object_audio_mute_set(sd->o_img, mute);
   if (mute)
      edje_object_signal_emit(sd->o_ctrl, "mute,set", "terminology");
   else
      edje_object_signal_emit(sd->o_ctrl, "mute,unset", "terminology");
}

void
media_visualize_set(Evas_Object *obj, Eina_Bool visualize)
{
   Media *sd = evas_object_smart_data_get(obj);
   if ((!sd) || (sd->type != MEDIA_TYPE_MOV)) return;
   if (visualize)
     {
        /*
         * FIXME: configure visualizing type, not hard coded one
         */
        if (!emotion_object_vis_supported(sd->o_img, EMOTION_VIS_LIBVISUAL_INFINITE))
          ERR(_("Media visualizing is not supported"));
        else
          emotion_object_vis_set(sd->o_img, EMOTION_VIS_LIBVISUAL_INFINITE);
     }
   else
     emotion_object_vis_set(sd->o_img, EMOTION_VIS_NONE);
}

void
media_play_set(Evas_Object *obj, Eina_Bool play)
{
   Media *sd = evas_object_smart_data_get(obj);
   if ((!sd) || (sd->type != MEDIA_TYPE_MOV)) return;
   emotion_object_play_set(sd->o_img, play);
   if (play)
     {
        evas_object_smart_callback_call(obj, "play", NULL);
        edje_object_signal_emit(sd->o_ctrl, "play,set", "terminology");
     }
   else
     {
        evas_object_smart_callback_call(obj, "pause", NULL);
        edje_object_signal_emit(sd->o_ctrl, "pause,set", "terminology");
     }
}

Eina_Bool
media_play_get(const Evas_Object *obj)
{
   Media *sd = evas_object_smart_data_get(obj);
   if ((!sd) || (sd->type != MEDIA_TYPE_MOV)) return EINA_FALSE;
   return emotion_object_play_get(sd->o_img);
}

void
media_stop(Evas_Object *obj)
{
   Media *sd = evas_object_smart_data_get(obj);
   if ((!sd) || (sd->type != MEDIA_TYPE_MOV)) return;
   evas_object_smart_callback_call(obj, "stop", NULL);
   evas_object_del(obj);
}

void
media_position_set(Evas_Object *obj, double pos)
{
   double len;
   Media *sd = evas_object_smart_data_get(obj);
   if ((!sd) || (sd->type != MEDIA_TYPE_MOV)) return;
   len = emotion_object_play_length_get(sd->o_img);
   emotion_object_position_set(sd->o_img, len * pos);
}

void
media_volume_set(Evas_Object *obj, double vol)
{
   Media *sd = evas_object_smart_data_get(obj);
   if ((!sd) || (sd->type != MEDIA_TYPE_MOV)) return;
   emotion_object_audio_volume_set(sd->o_img, vol);
   edje_object_part_drag_value_set(sd->o_ctrl, "terminology.voldrag", vol, vol);
}

const char *
media_get(const Evas_Object *obj)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return NULL;
   return sd->realf;
}

Media_Type
media_src_type_get(const char *src)
{
   Media_Type type = MEDIA_TYPE_UNKNOWN;

   if      (_is_fmt(src, extn_img))   type = MEDIA_TYPE_IMG;
   else if (_is_fmt(src, extn_scale)) type = MEDIA_TYPE_SCALE;
   else if (_is_fmt(src, extn_edj))   type = MEDIA_TYPE_EDJE;
   else if (_is_fmt(src, extn_mov))   type = MEDIA_TYPE_MOV;
   return type;
}

Evas_Object *
media_control_get(const Evas_Object *obj)
{
   Media *sd = evas_object_smart_data_get(obj);
   if (!sd) return NULL;
   return sd->o_ctrl;
}

void
media_unknown_handle(const char *handler, const char *src)
{
   const char *cmd;
   char buf[PATH_MAX];
   char *escaped;

   cmd = "xdg-open";
   escaped = ecore_file_escape_name(src);
   if (!escaped)
     return;
   if (handler && *handler)
     cmd = handler;
   snprintf(buf, sizeof(buf), "%s %s", cmd, escaped);
   free(escaped);

   ecore_exe_run(buf, NULL);
}
