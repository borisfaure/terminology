#include "private.h"

#include <Elementary.h>
#include <Ecore_Input.h>

#include "termio.h"
#include "termiolink.h"
#include "termpty.h"
#include "termcmd.h"
#include "termptydbl.h"
#include "utf8.h"
#include "col.h"
#include "keyin.h"
#include "config.h"
#include "utils.h"
#include "media.h"
#include "dbus.h"
#include "miniview.h"
#include "gravatar.h"

#if defined (__MacOSX__) || (defined (__MACH__) && defined (__APPLE__))
# include <sys/proc_info.h>
# include <libproc.h>
#endif

typedef struct _Termio Termio;

struct _Termio
{
   Evas_Object_Smart_Clipped_Data __clipped_data;
   struct {
      const char *name;
      int size;
      int chw, chh;
   } font;
   struct {
      int w, h;
      Evas_Object *obj;
   } grid;
   struct {
        Evas_Object *top, *bottom, *theme;
   } sel;
   struct {
      Evas_Object *obj;
      int x, y;
   } cursor;
   struct {
      int cx, cy;
      int button;
   } mouse;
   struct {
      char *string;
      int x1, y1, x2, y2;
      int suspend;
      Eina_List *objs;
      struct {
         Evas_Object *dndobj;
         Evas_Coord x, y;
         unsigned char down : 1;
         unsigned char dnd : 1;
         unsigned char dndobjdel : 1;
      } down;
   } link;
   Evas_Object *ctxpopup;
   int zoom_fontsize_start;
   int scroll;
   Evas_Object *self;
   Evas_Object *event;
   Term *term;

   Termpty *pty;
   Ecore_Animator *anim;
   Ecore_Timer *delayed_size_timer;
   Ecore_Timer *link_do_timer;
   Ecore_Timer *mouse_selection_scroll_timer;
   Ecore_Job *mouse_move_job;
   Ecore_Timer *mouseover_delay;
   Evas_Object *win, *theme, *glayer;
   Config *config;
   const char *sel_str;
   const char *preedit_str;
   Eina_List *cur_chids;
   Ecore_Job *sel_reset_job;
   double set_sel_at;
   Elm_Sel_Type sel_type;
   Keys_Handler khdl;
   unsigned char jump_on_change : 1;
   unsigned char jump_on_keypress : 1;
   unsigned char have_sel : 1;
   unsigned char noreqsize : 1;
   unsigned char didclick : 1;
   unsigned char moved : 1;
   unsigned char bottom_right : 1;
   unsigned char top_left : 1;
   unsigned char reset_sel : 1;
   double gesture_zoom_start_size;
};

#define INT_SWAP(_a, _b) do {    \
    int _swap = _a; _a = _b; _b = _swap; \
} while (0)


static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

static Eina_List *terms = NULL;

static void _sel_set(Termio *sd, Eina_Bool enable);
static void _remove_links(Termio *sd, Evas_Object *obj);
static void _smart_update_queue(Evas_Object *obj, Termio *sd);
static void _smart_apply(Evas_Object *obj);
static void _smart_size(Evas_Object *obj, int w, int h, Eina_Bool force);
static void _smart_calculate(Evas_Object *obj);
static void _take_selection_text(Termio *sd, Elm_Sel_Type type, const char *text);
static void _smart_xy_to_cursor(Termio *sd, Evas_Coord x, Evas_Coord y, int *cx, int *cy);
static Eina_Bool _mouse_in_selection(Termio *sd, int cx, int cy);


/* {{{ Helpers */

void
termio_scroll(Evas_Object *obj, int direction, int start_y, int end_y)
{
   Termpty *ty;
   Termio *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN(sd);

   if ((!sd->jump_on_change) && // if NOT scroll to bottom on updates
       (sd->scroll > 0))
     {
        Evas_Object *mv = term_miniview_get(sd->term);
        if (mv) miniview_position_offset(mv, direction, EINA_FALSE);
        // adjust scroll position for added scrollback
        sd->scroll -= direction;
     }
   ty = sd->pty;
   if (ty->selection.is_active)
     {
        int sel_start_y, sel_end_y;

        sel_start_y = ty->selection.start.y;
        sel_end_y = ty->selection.end.y;

        if (!ty->selection.is_top_to_bottom)
             INT_SWAP(sel_start_y, sel_end_y);
        if (start_y <= sel_start_y &&
            sel_end_y <= end_y)
          {
             ty->selection.orig.y += direction;
             ty->selection.start.y += direction;
             ty->selection.end.y += direction;
             sel_start_y += direction;
             sel_end_y += direction;
             if (!(start_y <= sel_start_y &&
                 sel_end_y <= end_y))
               {
                  _sel_set(sd, EINA_FALSE);
               }
          }
        else if (!((start_y > sel_end_y) ||
                   (end_y < sel_start_y)))
          {
             _sel_set(sd, EINA_FALSE);
          }
        else if (sd->scroll > 0)
          {
             ty->selection.orig.y += direction;
             ty->selection.start.y += direction;
             ty->selection.end.y += direction;
          }
     }
   if (sd->link.string)
     {
        if (sd->link.y1 <= end_y && sd->link.y2 >= start_y)
          _remove_links(sd, obj);
     }
}

void
termio_content_change(Evas_Object *obj, Evas_Coord x, Evas_Coord y,
                      int n)
{
   Termpty *ty;
   int start_x, start_y, end_x, end_y;
   Termio *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN(sd);
   ty = sd->pty;

   if (sd->link.string)
     {
        int _y = y + (x + n) / ty->w;

        start_x = sd->link.x1;
        start_y = sd->link.y1;
        end_x   = sd->link.x2;
        end_y   = sd->link.y2;

        y = MAX(y, start_y);
        for (; y <= MIN(_y, end_y); y++)
          {
             int d = MIN(n, ty->w - x);
             if (!((x > end_x) || (x + d < start_x)))
               {
                  _remove_links(sd, obj);
                  break;
               }
             n -= d;
             x = 0;
          }
     }

   if (!ty->selection.is_active) return;

   start_x = sd->pty->selection.start.x;
   start_y = sd->pty->selection.start.y;
   end_x   = sd->pty->selection.end.x;
   end_y   = sd->pty->selection.end.y;

   if (!sd->pty->selection.is_top_to_bottom)
     {
        INT_SWAP(start_y, end_y);
        INT_SWAP(start_x, end_x);
     }
   if (ty->selection.is_box)
     {
        int _y = y + (x + n) / ty->w;

        y = MAX(y, start_y);
        for (; y <= MIN(_y, end_y); y++)
          {
             int d = MIN(n, ty->w - x);
             if (!((x > end_x) || (x + d < start_x)))
               {
                  _sel_set(sd, EINA_FALSE);
                  break;
               }
             n -= d;
             x = 0;
          }
     }
   else
     {
        int sel_len;
        Termcell *cells_changed, *cells_selection;

        sel_len = end_x - start_x + ty->w * (end_y - start_y);
        cells_changed = &(TERMPTY_SCREEN(ty, x, y));
        cells_selection = &(TERMPTY_SCREEN(ty, start_x, start_y));

        if (!((cells_changed > (cells_selection + sel_len)) ||
             (cells_selection > (cells_changed + n))))
          {
             _sel_set(sd, EINA_FALSE);
          }
     }
}


static void
_win_obj_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event EINA_UNUSED)
{
   Termio *sd = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN(sd);
   if (obj == sd->win)
     {
        evas_object_event_callback_del_full(sd->win, EVAS_CALLBACK_DEL,
                                            _win_obj_del, data);
        sd->win = NULL;
     }
}


void
termio_theme_set(Evas_Object *obj, Evas_Object *theme)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   if (theme) sd->theme = theme;
}

void
termio_mouseover_suspend_pushpop(Evas_Object *obj, int dir)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   sd->link.suspend += dir;
   if (sd->link.suspend < 0) sd->link.suspend = 0;
   if (sd->link.suspend)
     {
        if (sd->anim) ecore_animator_del(sd->anim);
        sd->anim = NULL;
     }
   _smart_update_queue(obj, sd);
}

void
termio_size_get(Evas_Object *obj, int *w, int *h)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   if (w) *w = sd->grid.w;
   if (h) *h = sd->grid.h;
}

int
termio_scroll_get(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, 0);
   return sd->scroll;
}

void
termio_scroll_delta(Evas_Object *obj, int delta, int by_page)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (by_page)
     {
        int by = sd->grid.h - 2;
        if (by > 1)
          delta *= by;
     }
   sd->scroll += delta;
   if (delta <= 0 && sd->scroll < 0)
       sd->scroll = 0;
   _smart_update_queue(obj, sd);
   miniview_position_offset(term_miniview_get(sd->term), -delta, EINA_TRUE);
}

void
termio_scroll_set(Evas_Object *obj, int scroll)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   sd->scroll = scroll;
   _remove_links(sd, obj);
   _smart_apply(obj);
}

const char *
termio_title_get(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);
   if (sd->pty->prop.user_title)
     return sd->pty->prop.user_title;
   return sd->pty->prop.title;
}

void
termio_user_title_set(Evas_Object *obj, const char *title)
{
    Termio *sd = evas_object_smart_data_get(obj);
    EINA_SAFETY_ON_NULL_RETURN(sd);

    if (sd->pty->prop.user_title)
      eina_stringshare_del(sd->pty->prop.user_title);

    sd->pty->prop.user_title = eina_stringshare_add(title);
    if (sd->pty->cb.set_title.func)
      sd->pty->cb.set_title.func(sd->pty->cb.set_title.data);
}

const char *
termio_icon_name_get(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);
   return sd->pty->prop.icon;
}

void
termio_media_mute_set(Evas_Object *obj, Eina_Bool mute)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Eina_List *l;
   Termblock *blk;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   EINA_LIST_FOREACH(sd->pty->block.active, l, blk)
     {
        if (blk->obj && !blk->edje)
          media_mute_set(blk->obj, mute);
     }
}

void
termio_media_visualize_set(Evas_Object *obj, Eina_Bool visualize)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Eina_List *l;
   Termblock *blk;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   EINA_LIST_FOREACH(sd->pty->block.active, l, blk)
     {
        if (blk->obj && !blk->edje)
          media_visualize_set(blk->obj, visualize);
     }
}

Eina_Bool
termio_selection_exists(const Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EINA_FALSE);
   return sd->pty->selection.is_active;
}

Termpty *
termio_pty_get(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);

   return sd->pty;
}

Evas_Object *
termio_miniview_get(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);

   return term_miniview_get(sd->term);
}

Term*
termio_term_get(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);

   return sd->term;
}

static void
_font_size_set(Evas_Object *obj, int size)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Config *config;
   EINA_SAFETY_ON_NULL_RETURN(sd);

   config = sd->config;

   if (size < 5) size = 5;
   else if (size > 100) size = 100;
   if (config)
     {
        config->temporary = EINA_TRUE;
        config->font.size = size;
        sd->noreqsize = 1;
        termio_config_update(obj);
        sd->noreqsize = 0;
        evas_object_data_del(obj, "sizedone");
     }
}

void
termio_font_size_set(Evas_Object *obj, int size)
{
   _font_size_set(obj, size);
}

void
termio_grid_size_set(Evas_Object *obj, int w, int h)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord mw = 1, mh = 1;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (w < 1) w = 1;
   if (h < 1) h = 1;
   evas_object_size_hint_min_get(obj, &mw, &mh);
   evas_object_data_del(obj, "sizedone");
   evas_object_size_hint_request_set(obj, mw * w, mh * h);
}

pid_t
termio_pid_get(const Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, 0);
   return termpty_pid_get(sd->pty);
}

Eina_Bool
termio_cwd_get(const Evas_Object *obj, char *buf, size_t size)
{
   Termio *sd = evas_object_smart_data_get(obj);
   pid_t pid;

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EINA_FALSE);

   pid = termpty_pid_get(sd->pty);

#if defined (__MacOSX__) || (defined (__MACH__) && defined (__APPLE__))

   struct proc_vnodepathinfo vpi;

   if (proc_pidinfo(pid, PROC_PIDVNODEPATHINFO, 0, &vpi, sizeof(vpi)) <= 0)
     {
        ERR(_("Could not get working directory of pid %i: %s"),
            pid, strerror(errno));
        return EINA_FALSE;
     }
   memcpy(buf, vpi.pvi_cdir.vip_path, size);

#else

   char procpath[PATH_MAX];
   ssize_t siz;

   snprintf(procpath, sizeof(procpath), "/proc/%ld/cwd", (long) pid);
   if ((siz = readlink(procpath, buf, size)) < 1)
     {
        ERR(_("Could not load working directory %s: %s"),
            procpath, strerror(errno));
        return EINA_FALSE;
     }
   buf[siz] = 0;

#endif

   return EINA_TRUE;
}

Evas_Object *
termio_textgrid_get(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);

   return sd->grid.obj;
}

Evas_Object *
termio_win_get(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);

   return sd->win;
}


/* }}} */
/* {{{ Config */

void
termio_config_update(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord w, h, ow = 0, oh = 0;
   char buf[4096];

   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (sd->font.name) eina_stringshare_del(sd->font.name);
   sd->font.name = NULL;

   if (sd->config->font.bitmap)
     {
        snprintf(buf, sizeof(buf), "%s/fonts/%s",
                 elm_app_data_dir_get(), sd->config->font.name);
        sd->font.name = eina_stringshare_add(buf);
     }
   else
     sd->font.name = eina_stringshare_add(sd->config->font.name);
   sd->font.size = sd->config->font.size;

   sd->jump_on_change = sd->config->jump_on_change;
   sd->jump_on_keypress = sd->config->jump_on_keypress;

   termpty_backlog_size_set(sd->pty, sd->config->scrollback);
   sd->scroll = 0;

   if (evas_object_focus_get(obj))
     {
        edje_object_signal_emit(sd->cursor.obj, "focus,out", "terminology");
        if (sd->config->disable_cursor_blink)
          edje_object_signal_emit(sd->cursor.obj, "focus,in,noblink", "terminology");
        else
          edje_object_signal_emit(sd->cursor.obj, "focus,in", "terminology");
     }

   colors_term_init(sd->grid.obj, sd->theme, sd->config);

   evas_object_scale_set(sd->grid.obj, elm_config_scale_get());
   evas_object_textgrid_font_set(sd->grid.obj, sd->font.name, sd->font.size);
   evas_object_textgrid_cell_size_get(sd->grid.obj, &w, &h);

   if (w < 1) w = 1;
   if (h < 1) h = 1;
   sd->font.chw = w;
   sd->font.chh = h;

   evas_object_geometry_get(obj, NULL, NULL, &ow, &oh);
   _smart_size(obj, ow / w, oh / h, EINA_TRUE);
}

Config *
termio_config_get(const Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);
   return sd->config;
}

void
termio_config_set(Evas_Object *obj, Config *config)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord w = 2, h = 2;

   sd->config = config;

   sd->jump_on_change = config->jump_on_change;
   sd->jump_on_keypress = config->jump_on_keypress;

   if (config->font.bitmap)
     {
        char buf[PATH_MAX];
        snprintf(buf, sizeof(buf), "%s/fonts/%s",
                 elm_app_data_dir_get(), config->font.name);
        sd->font.name = eina_stringshare_add(buf);
     }
   else
     sd->font.name = eina_stringshare_add(config->font.name);
   sd->font.size = config->font.size;

   evas_object_scale_set(sd->grid.obj, elm_config_scale_get());
   evas_object_textgrid_font_set(sd->grid.obj, sd->font.name, sd->font.size);
   evas_object_textgrid_size_get(sd->grid.obj, &w, &h);
   if (w < 1) w = 1;
   if (h < 1) h = 1;
   evas_object_textgrid_size_set(sd->grid.obj, w, h);
   evas_object_textgrid_cell_size_get(sd->grid.obj, &w, &h);

   if (w < 1) w = 1;
   if (h < 1) h = 1;
   sd->font.chw = w;
   sd->font.chh = h;

   theme_apply(sd->cursor.obj, config, "terminology/cursor");
   theme_auto_reload_enable(sd->cursor.obj);
   evas_object_resize(sd->cursor.obj, sd->font.chw, sd->font.chh);
   evas_object_show(sd->cursor.obj);

   theme_apply(sd->sel.theme, config, "terminology/selection");
   theme_auto_reload_enable(sd->sel.theme);
   edje_object_part_swallow(sd->sel.theme, "terminology.top_left", sd->sel.top);
   edje_object_part_swallow(sd->sel.theme, "terminology.bottom_right", sd->sel.bottom);
}

/* }}} */
/* {{{ Links */

static Eina_Bool
_should_inline(const Evas_Object *obj)
{
   const Config *config = termio_config_get(obj);
   const Evas *e;
   const Evas_Modifier *mods;

   if (!config->helper.inline_please) return EINA_FALSE;

   e = evas_object_evas_get(obj);
   mods = evas_key_modifier_get(e);

   if (evas_key_modifier_is_set(mods, "Control"))  return EINA_FALSE;

   return EINA_TRUE;
}

const char  *
termio_link_get(const Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);
   return sd->link.string;
}

static void
_activate_link(Evas_Object *obj, Eina_Bool may_inline)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Config *config;
   char buf[PATH_MAX], *s, *escaped;
   const char *path = NULL, *cmd = NULL;
   Eina_Bool url = EINA_FALSE, email = EINA_FALSE, handled = EINA_FALSE;
   int type;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   config = sd->config;
   if (!config) return;
   if (!config->active_links) return;
   if (!sd->link.string) return;
   if (link_is_url(sd->link.string))
     {
        if (casestartswith(sd->link.string, "file://"))
          // TODO: decode string: %XX -> char
          path = sd->link.string + sizeof("file://") - 1;
        else
          url = EINA_TRUE;
     }
   else if (sd->link.string[0] == '/')
     path = sd->link.string;
   else if (link_is_email(sd->link.string))
     email = EINA_TRUE;

   if (url && casestartswith(sd->link.string, "mailto:"))
     {
        email = EINA_TRUE;
        url = EINA_FALSE;
     }

   s = eina_str_escape(sd->link.string);
   if (!s) return;
   if (email)
     {
        const char *p = s;

        // run mail client
        cmd = "xdg-email";

        if ((config->helper.email) &&
            (config->helper.email[0]))
          cmd = config->helper.email;

        if (casestartswith(s, "mailto:"))
          p += sizeof("mailto:") - 1;

        escaped = ecore_file_escape_name(p);
        if (escaped)
          {
             snprintf(buf, sizeof(buf), "%s %s", cmd, escaped);
             free(escaped);
          }
     }
   else if (path)
     {
        // locally accessible file
        cmd = "xdg-open";

        escaped = ecore_file_escape_name(s);
        if (escaped)
          {
             type = media_src_type_get(sd->link.string);
             if (may_inline && _should_inline(obj))
               {
                  if ((type == MEDIA_TYPE_IMG) ||
                      (type == MEDIA_TYPE_SCALE) ||
                      (type == MEDIA_TYPE_EDJE))
                    {
                       evas_object_smart_callback_call(obj, "popup", NULL);
                       handled = EINA_TRUE;
                    }
                  else if (type == MEDIA_TYPE_MOV)
                    {
                       evas_object_smart_callback_call(obj, "popup", NULL);
                       handled = EINA_TRUE;
                    }
               }
             if (!handled)
               {
                  if ((type == MEDIA_TYPE_IMG) ||
                      (type == MEDIA_TYPE_SCALE) ||
                      (type == MEDIA_TYPE_EDJE))
                    {
                       if ((config->helper.local.image) &&
                           (config->helper.local.image[0]))
                         cmd = config->helper.local.image;
                    }
                  else if (type == MEDIA_TYPE_MOV)
                    {
                       if ((config->helper.local.video) &&
                           (config->helper.local.video[0]))
                         cmd = config->helper.local.video;
                    }
                  else
                    {
                       if ((config->helper.local.general) &&
                           (config->helper.local.general[0]))
                         cmd = config->helper.local.general;
                    }
                  snprintf(buf, sizeof(buf), "%s %s", cmd, escaped);
                  free(escaped);
               }
          }
     }
   else if (url)
     {
        // remote file needs ecore-con-url
        cmd = "xdg-open";

        escaped = ecore_file_escape_name(s);
        if (escaped)
          {
             type = media_src_type_get(sd->link.string);
             if (may_inline && _should_inline(obj))
               {
                  evas_object_smart_callback_call(obj, "popup", NULL);
                  handled = EINA_TRUE;
               }
             if (!handled)
               {
                  if ((type == MEDIA_TYPE_IMG) ||
                      (type == MEDIA_TYPE_SCALE) ||
                      (type == MEDIA_TYPE_EDJE))
                    {
                       if ((config->helper.url.image) &&
                           (config->helper.url.image[0]))
                         cmd = config->helper.url.image;
                    }
                  else if (type == MEDIA_TYPE_MOV)
                    {
                       if ((config->helper.url.video) &&
                           (config->helper.url.video[0]))
                         cmd = config->helper.url.video;
                    }
                  else
                    {
                       if ((config->helper.url.general) &&
                           (config->helper.url.general[0]))
                         cmd = config->helper.url.general;
                    }
                  snprintf(buf, sizeof(buf), "%s %s", cmd, escaped);
                  free(escaped);
               }
          }
     }
   else
     {
        free(s);
        return;
     }
   free(s);
   if (!handled) ecore_exe_run(buf, NULL);
}

static void
_cb_ctxp_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
             void *event EINA_UNUSED)
{
   Termio *sd = data;
   EINA_SAFETY_ON_NULL_RETURN(sd);
   sd->ctxpopup = NULL;
}

static void
_cb_ctxp_dismissed(void *data EINA_UNUSED, Evas_Object *obj,
                   void *event EINA_UNUSED)
{
   Termio *sd = data;
   EINA_SAFETY_ON_NULL_RETURN(sd);
   sd->ctxpopup = NULL;
   evas_object_del(obj);
}

static void
_cb_ctxp_link_preview(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Termio *sd = evas_object_smart_data_get(term);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   _activate_link(term, EINA_TRUE);

   sd->ctxpopup = NULL;
   evas_object_del(obj);
}

static void
_cb_ctxp_link_open(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Termio *sd = evas_object_smart_data_get(term);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   _activate_link(term, EINA_FALSE);

   sd->ctxpopup = NULL;
   evas_object_del(obj);
}

static void
_cb_ctxp_link_copy(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Termio *sd = evas_object_smart_data_get(term);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   EINA_SAFETY_ON_NULL_RETURN(sd->link.string);
   _take_selection_text(sd, ELM_SEL_TYPE_CLIPBOARD, sd->link.string);

   sd->ctxpopup = NULL;
   evas_object_del(obj);
}

static void
_cb_link_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Mouse_Down *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (ev->button == 1)
     {
        sd->link.down.down = EINA_TRUE;
        sd->link.down.x = ev->canvas.x;
        sd->link.down.y = ev->canvas.y;
     }
   else if (ev->button == 3)
     {
        Evas_Object *ctxp;

        if (sd->pty->selection.is_active)
          {
             int cx = 0, cy = 0;

             _smart_xy_to_cursor(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);
             if (_mouse_in_selection(sd, cx, cy))
               return;
          }

        ctxp = elm_ctxpopup_add(sd->win);
        sd->ctxpopup = ctxp;

        if (sd->config->helper.inline_please)
          {
             int type = media_src_type_get(sd->link.string);

             if ((type == MEDIA_TYPE_IMG) ||
                 (type == MEDIA_TYPE_SCALE) ||
                 (type == MEDIA_TYPE_EDJE) ||
                 (type == MEDIA_TYPE_MOV))
               elm_ctxpopup_item_append(ctxp, _("Preview"), NULL,
                                        _cb_ctxp_link_preview, sd->self);
          }
        elm_ctxpopup_item_append(ctxp, _("Open"), NULL, _cb_ctxp_link_open,
                                 sd->self);
        elm_ctxpopup_item_append(ctxp, _("Copy"), NULL, _cb_ctxp_link_copy,
                                 sd->self);
        evas_object_move(ctxp, ev->canvas.x, ev->canvas.y);
        evas_object_show(ctxp);
        evas_object_smart_callback_add(ctxp, "dismissed",
                                       _cb_ctxp_dismissed, sd);
        evas_object_event_callback_add(ctxp, EVAS_CALLBACK_DEL,
                                       _cb_ctxp_del, sd);
     }
}

static Eina_Bool
_cb_link_up_delay(void *data)
{
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EINA_FALSE);

   sd->link_do_timer = NULL;
   if (!sd->didclick) _activate_link(data, EINA_TRUE);
   sd->didclick = EINA_FALSE;
   return EINA_FALSE;
}

static void
_cb_link_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Mouse_Up *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   if ((ev->button == 1) && (sd->link.down.down))
     {
        Evas_Coord dx, dy, finger_size;

        dx = abs(ev->canvas.x - sd->link.down.x);
        dy = abs(ev->canvas.y - sd->link.down.y);
        finger_size = elm_config_finger_size_get();

        if ((dx <= finger_size) && (dy <= finger_size))
          {
             if (sd->link_do_timer)
               ecore_timer_reset(sd->link_do_timer);
             else
               sd->link_do_timer = ecore_timer_add(0.2, _cb_link_up_delay, data);
          }
        sd->link.down.down = EINA_FALSE;
     }
}

#if !((ELM_VERSION_MAJOR == 1) && (ELM_VERSION_MINOR < 8))
static void
_cb_link_drag_move(void *data, Evas_Object *obj, Evas_Coord x, Evas_Coord y, Elm_Xdnd_Action action)
{
   const Evas_Modifier *em = NULL;
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   DBG("dnd %i %i act %i", x, y, action);
   em = evas_key_modifier_get(evas_object_evas_get(sd->event));
   if (em)
     {
        if (evas_key_modifier_is_set(em, "Control"))
          elm_drag_action_set(obj, ELM_XDND_ACTION_COPY);
        else
          elm_drag_action_set(obj, ELM_XDND_ACTION_MOVE);
     }
}

static void
_cb_link_drag_accept(void *data, Evas_Object *obj EINA_UNUSED, Eina_Bool doaccept)
{
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   DBG("dnd accept: %i", doaccept);
}

static void
_cb_link_drag_done(void *data, Evas_Object *obj EINA_UNUSED)
{
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   DBG("dnd done");
   sd->link.down.dnd = EINA_FALSE;
   if ((sd->link.down.dndobjdel) && (sd->link.down.dndobj))
     evas_object_del(sd->link.down.dndobj);
   sd->link.down.dndobj = NULL;
}

static Evas_Object *
_cb_link_icon_new(void *data, Evas_Object *par, Evas_Coord *xoff, Evas_Coord *yoff)
{
   Evas_Object *icon;
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);

   icon = elm_button_add(par);
   elm_object_text_set(icon, sd->link.string);
   *xoff = 0;
   *yoff = 0;
   return icon;
}
#endif

static void
_cb_link_move(void *data, Evas *e EINA_UNUSED,
              Evas_Object *obj
#if ((ELM_VERSION_MAJOR == 1) && (ELM_VERSION_MINOR < 8))
              EINA_UNUSED
#endif
              , void *event)
{
   Evas_Event_Mouse_Move *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   Evas_Coord dx, dy;
   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (!sd->link.down.down) return;
   dx = abs(ev->cur.canvas.x - sd->link.down.x);
   dy = abs(ev->cur.canvas.y - sd->link.down.y);
   if ((sd->config->drag_links) &&
       (sd->link.string) &&
       ((dx > elm_config_finger_size_get()) ||
           (dy > elm_config_finger_size_get())))
     {
        sd->link.down.down = EINA_FALSE;
        sd->link.down.dnd = EINA_TRUE;
#if !((ELM_VERSION_MAJOR == 1) && (ELM_VERSION_MINOR < 8))
        DBG("dnd start %s %i %i", sd->link.string,
               evas_key_modifier_is_set(ev->modifiers, "Control"),
               evas_key_modifier_is_set(ev->modifiers, "Shift"));
        if (evas_key_modifier_is_set(ev->modifiers, "Control"))
          elm_drag_start(obj, ELM_SEL_FORMAT_IMAGE, sd->link.string,
                         ELM_XDND_ACTION_COPY,
                         _cb_link_icon_new, data,
                         _cb_link_drag_move, data,
                         _cb_link_drag_accept, data,
                         _cb_link_drag_done, data);
        else
          elm_drag_start(obj, ELM_SEL_FORMAT_IMAGE, sd->link.string,
                         ELM_XDND_ACTION_MOVE,
                         _cb_link_icon_new, data,
                         _cb_link_drag_move, data,
                         _cb_link_drag_accept, data,
                         _cb_link_drag_done, data);
        sd->link.down.dndobj = obj;
        sd->link.down.dndobjdel = EINA_FALSE;
#endif
     }
}

static void
_update_link(Evas_Object *obj, Termio *sd,
             Eina_Bool same_link, Eina_Bool same_geom)
{
   Evas_Coord ox, oy, ow, oh;
   Evas_Object *o;
   Eina_Bool popup_exists;
   int y;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (!same_link)
     {
        // check link and re-probe/fetch create popup preview
     }

   if (same_geom)
     return;

   if (sd->link.suspend)
     return;
   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   EINA_LIST_FREE(sd->link.objs, o)
     {
        if (sd->link.down.dndobj == o)
          {
             sd->link.down.dndobjdel = EINA_TRUE;
             evas_object_hide(o);
          }
        else
          evas_object_del(o);
     }
   if (!sd->link.string)
     return;

   popup_exists = main_term_popup_exists(sd->term);
   if ((!popup_exists) &&
       ((sd->link.string[0] == '/') || (link_is_url(sd->link.string))))
     {
        Evas_Coord _x = ox, _y = oy;
        uint64_t xwin;

        _x += sd->mouse.cx * sd->font.chw;
        _y += sd->mouse.cy * sd->font.chh;
#if (ELM_VERSION_MAJOR > 1) || (ELM_VERSION_MINOR >= 8)
        xwin = elm_win_window_id_get(sd->win);
# if (ELM_VERSION_MAJOR > 1) || ((ELM_VERSION_MAJOR == 1) && (ELM_VERSION_MINOR > 8)) // not a typo
        if (strstr(ecore_evas_engine_name_get(ecore_evas_ecore_evas_get(evas_object_evas_get(sd->win))), "wayland"))
          xwin = ((uint64_t)xwin << 32) + (uint64_t)getpid();
# endif
#else
        xwin = elm_win_xwindow_get(sd->win);
#endif
        ty_dbus_link_mousein(xwin, sd->link.string, _x, _y);
     }
   for (y = sd->link.y1; y <= sd->link.y2; y++)
     {
        o = elm_layout_add(obj);
        evas_object_smart_member_add(o, obj);
        theme_apply(elm_layout_edje_get(o), sd->config, "terminology/link");

        if (y == sd->link.y1)
          {
             evas_object_move(o, ox + (sd->link.x1 * sd->font.chw),
                              oy + (y * sd->font.chh));
             if (sd->link.y1 == sd->link.y2)
               evas_object_resize(o,
                                  ((sd->link.x2 - sd->link.x1 + 1) * sd->font.chw),
                                  sd->font.chh);
             else
               evas_object_resize(o,
                                  ((sd->grid.w - sd->link.x1) * sd->font.chw),
                                  sd->font.chh);
          }
        else if (y == sd->link.y2)
          {
             evas_object_move(o, ox, oy + (y * sd->font.chh));
             evas_object_resize(o,
                                ((sd->link.x2 + 1) * sd->font.chw),
                                sd->font.chh);
          }
        else
          {
             evas_object_move(o, ox, oy + (y * sd->font.chh));
             evas_object_resize(o, (sd->grid.w * sd->font.chw),
                                sd->font.chh);
          }

        sd->link.objs = eina_list_append(sd->link.objs, o);
        elm_object_cursor_set(o, "hand2");
        evas_object_show(o);
        evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                       _cb_link_down, obj);
        evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_UP,
                                       _cb_link_up, obj);
        evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_MOVE,
                                       _cb_link_move, obj);
        if ((!popup_exists) && link_is_email(sd->link.string))
          {
             gravatar_tooltip(o, sd->config, sd->link.string);
          }
     }
}

static void
_remove_links(Termio *sd, Evas_Object *obj)
{
   Eina_Bool same_link = EINA_FALSE, same_geom = EINA_FALSE;

   if (sd->link.string)
     {
        if ((sd->link.string[0] == '/') || (link_is_url(sd->link.string)))
          {
             Evas_Coord ox, oy;
             uint64_t xwin;

             evas_object_geometry_get(obj, &ox, &oy, NULL, NULL);

             ox += sd->mouse.cx * sd->font.chw;
             oy += sd->mouse.cy * sd->font.chh;
#if (ELM_VERSION_MAJOR > 1) || (ELM_VERSION_MINOR >= 8)
                       xwin = elm_win_window_id_get(sd->win);
# if (ELM_VERSION_MAJOR > 1) || ((ELM_VERSION_MAJOR == 1) && (ELM_VERSION_MINOR > 8)) // not a typo
                       if (strstr(ecore_evas_engine_name_get(ecore_evas_ecore_evas_get(evas_object_evas_get(sd->win))), "wayland"))
                         xwin = ((uint64_t)xwin << 32) + (uint64_t)getpid();
# endif
#else
                       xwin = elm_win_xwindow_get(sd->win);
#endif
             ty_dbus_link_mouseout(xwin, sd->link.string, ox, oy);
          }
        free(sd->link.string);
        sd->link.string = NULL;
     }
   sd->link.x1 = -1;
   sd->link.y1 = -1;
   sd->link.x2 = -1;
   sd->link.y2 = -1;
   sd->link.suspend = EINA_FALSE;
   _update_link(obj, sd, same_link, same_geom);
}

/* }}} */
/* {{{ Blocks */

static void
_smart_media_clicked(void *data, Evas_Object *obj, void *info EINA_UNUSED)
{
//   Termio *sd = evas_object_smart_data_get(data);
   Termblock *blk;
   const char *file = media_get(obj);
   if (!file) return;
   blk = evas_object_data_get(obj, "blk");
   if (blk)
     {
        if (blk->link)
          {
             int type = media_src_type_get(blk->link);
             Config *config = termio_config_get(data);

             if (config)
               {
                  if ((!config->helper.inline_please) ||
                      (!((type == MEDIA_TYPE_IMG)  || (type == MEDIA_TYPE_SCALE) ||
                         (type == MEDIA_TYPE_EDJE) || (type == MEDIA_TYPE_MOV))))
                    {
                       const char *cmd = NULL;

                       file = blk->link;
                       if ((config->helper.local.general) &&
                           (config->helper.local.general[0]))
                         cmd = config->helper.local.general;
                       if (cmd)
                         {
                            char buf[PATH_MAX], *escaped;

                            escaped = ecore_file_escape_name(file);
                            if (escaped)
                              {
                                 snprintf(buf, sizeof(buf), "%s %s", cmd, escaped);
                                 ecore_exe_run(buf, NULL);
                                 free(escaped);
                              }
                            return;
                         }
                    }
                  file = blk->link;
               }
          }
     }
   evas_object_smart_callback_call(data, "popup", (void *)file);
}

static void
_smart_media_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *info EINA_UNUSED)
{
   Termblock *blk = data;

   if (blk->obj == obj) blk->obj = NULL;
}

static void
_smart_media_play(void *data, Evas_Object *obj, void *info EINA_UNUSED)
{
   Termblock *blk = data;

   if (blk->obj != obj) return;

   blk->mov_state = MOVIE_STATE_PLAY;
}

static void
_smart_media_pause(void *data, Evas_Object *obj, void *info EINA_UNUSED)
{
   Termblock *blk = data;

   if (blk->obj != obj) return;

   blk->mov_state = MOVIE_STATE_PAUSE;
}

static void
_smart_media_stop(void *data, Evas_Object *obj, void *info EINA_UNUSED)
{
   Termblock *blk = data;

   if (blk->obj != obj) return;

   blk->mov_state = MOVIE_STATE_STOP;
}

static void
_block_edje_signal_cb(void *data, Evas_Object *obj EINA_UNUSED, const char *sig, const char *src)
{
   Termblock *blk = data;
   Termio *sd = evas_object_smart_data_get(blk->pty->obj);
   char *buf = NULL, *chid = NULL;
   int buflen = 0;
   Eina_List *l;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   if ((!blk->chid) || (!sd->cur_chids)) return;
   EINA_LIST_FOREACH(sd->cur_chids, l, chid)
     {
        if (!(!strcmp(blk->chid, chid))) break;
        chid = NULL;
     }
   if (!chid) return;
   if ((!strcmp(sig, "drag")) ||
       (!strcmp(sig, "drag,start")) ||
       (!strcmp(sig, "drag,stop")) ||
       (!strcmp(sig, "drag,step")) ||
       (!strcmp(sig, "drag,set")))
     {
        int v1, v2;
        double f1 = 0.0, f2 = 0.0;

        edje_object_part_drag_value_get(blk->obj, src, &f1, &f2);
        v1 = (int)(f1 * 1000.0);
        v2 = (int)(f2 * 1000.0);
        buf = alloca(strlen(src) + strlen(blk->chid) + 256);
        buflen = sprintf(buf, "%c};%s\n%s\n%s\n%i\n%i", 0x1b,
                         blk->chid, sig, src, v1, v2);
        termpty_write(sd->pty, buf, buflen + 1);
     }
   else
     {
        buf = alloca(strlen(sig) + strlen(src) + strlen(blk->chid) + 128);
        buflen = sprintf(buf, "%c}signal;%s\n%s\n%s", 0x1b,
                         blk->chid, sig, src);
        termpty_write(sd->pty, buf, buflen + 1);
     }
}

static void
_block_edje_message_cb(void *data, Evas_Object *obj EINA_UNUSED, Edje_Message_Type type, int id, void *msg)
{
   Termblock *blk = data;
   Termio *sd = evas_object_smart_data_get(blk->pty->obj);
   char *chid = NULL, buf[4096];
   Eina_List *l;
   int buflen;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   if ((!blk->chid) || (!sd->cur_chids)) return;
   EINA_LIST_FOREACH(sd->cur_chids, l, chid)
     {
        if (!(!strcmp(blk->chid, chid))) break;
        chid = NULL;
     }
   if (!chid) return;
   switch (type)
     {
      case EDJE_MESSAGE_STRING:
          {
             Edje_Message_String *m = msg;

             buflen = sprintf(buf, "%c}message;%s\n%i\nstring\n%s", 0x1b,
                              blk->chid, id, m->str);
             termpty_write(sd->pty, buf, buflen + 1);
          }
        break;
      case EDJE_MESSAGE_INT:
          {
             Edje_Message_Int *m = msg;

             buflen = snprintf(buf, sizeof(buf),
                               "%c}message;%s\n%i\nint\n%i", 0x1b,
                               blk->chid, id, m->val);
             termpty_write(sd->pty, buf, buflen + 1);
          }
        break;
      case EDJE_MESSAGE_FLOAT:
          {
             Edje_Message_Float *m = msg;

             buflen = snprintf(buf, sizeof(buf),
                               "%c}message;%s\n%i\nfloat\n%i", 0x1b,
                               blk->chid, id, (int)(m->val * 1000.0));
             termpty_write(sd->pty, buf, buflen + 1);
          }
        break;
      case EDJE_MESSAGE_STRING_SET:
          {
             Edje_Message_String_Set *m = msg;
             int i;
             char zero[1] = { 0 };

             buflen = snprintf(buf, sizeof(buf),
                               "%c}message;%s\n%i\nstring_set\n%i", 0x1b,
                               blk->chid, id, m->count);
             termpty_write(sd->pty, buf, buflen);
             for (i = 0; i < m->count; i++)
               {
                  termpty_write(sd->pty, "\n", 1);
                  termpty_write(sd->pty, m->str[i], strlen(m->str[i]));
               }
             termpty_write(sd->pty, zero, 1);
         }
        break;
      case EDJE_MESSAGE_INT_SET:
          {
             Edje_Message_Int_Set *m = msg;
             int i;
             char zero[1] = { 0 };

             buflen = snprintf(buf, sizeof(buf),
                               "%c}message;%s\n%i\nint_set\n%i", 0x1b,
                               blk->chid, id, m->count);
             termpty_write(sd->pty, buf, buflen);
             for (i = 0; i < m->count; i++)
               {
                  termpty_write(sd->pty, "\n", 1);
                  buflen = snprintf(buf, sizeof(buf), "%i", m->val[i]);
                  termpty_write(sd->pty, buf, buflen);
               }
             termpty_write(sd->pty, zero, 1);
          }
        break;
      case EDJE_MESSAGE_FLOAT_SET:
          {
             Edje_Message_Float_Set *m = msg;
             int i;
             char zero[1] = { 0 };

             buflen = snprintf(buf, sizeof(buf),
                               "%c}message;%s\n%i\nfloat_set\n%i", 0x1b,
                               blk->chid, id, m->count);
             termpty_write(sd->pty, buf, buflen);
             for (i = 0; i < m->count; i++)
               {
                  termpty_write(sd->pty, "\n", 1);
                  buflen = snprintf(buf, sizeof(buf), "%i", (int)(m->val[i] * 1000.0));
                  termpty_write(sd->pty, buf, buflen);
               }
             termpty_write(sd->pty, zero, 1);
          }
        break;
      case EDJE_MESSAGE_STRING_INT:
          {
             Edje_Message_String_Int *m = msg;

             buflen = snprintf(buf, sizeof(buf),
                               "%c}message;%s\n%i\nstring_int\n%s\n%i", 0x1b,
                               blk->chid, id, m->str, m->val);
             termpty_write(sd->pty, buf, buflen + 1);
          }
        break;
      case EDJE_MESSAGE_STRING_FLOAT:
          {
             Edje_Message_String_Float *m = msg;

             buflen = snprintf(buf, sizeof(buf),
                               "%c}message;%s\n%i\nstring_float\n%s\n%i", 0x1b,
                               blk->chid, id, m->str, (int)(m->val * 1000.0));
             termpty_write(sd->pty, buf, buflen + 1);
          }
        break;
      case EDJE_MESSAGE_STRING_INT_SET:
          {
             Edje_Message_String_Int_Set *m = msg;
             int i;
             char zero[1] = { 0 };

             buflen = snprintf(buf, sizeof(buf),
                               "%c}message;%s\n%i\nstring_int_set\n%i", 0x1b,
                               blk->chid, id, m->count);
             termpty_write(sd->pty, buf, buflen);
             termpty_write(sd->pty, "\n", 1);
             termpty_write(sd->pty, m->str, strlen(m->str));
             for (i = 0; i < m->count; i++)
               {
                  termpty_write(sd->pty, "\n", 1);
                  buflen = snprintf(buf, sizeof(buf), "%i", m->val[i]);
                  termpty_write(sd->pty, buf, buflen);
               }
             termpty_write(sd->pty, zero, 1);
          }
        break;
      case EDJE_MESSAGE_STRING_FLOAT_SET:
          {
             Edje_Message_String_Float_Set *m = msg;
             int i;
             char zero[1] = { 0 };

             buflen = snprintf(buf, sizeof(buf),
                               "%c}message;%s\n%i\nstring_float_set\n%i", 0x1b,
                               blk->chid, id, m->count);
             termpty_write(sd->pty, buf, buflen);
             termpty_write(sd->pty, "\n", 1);
             termpty_write(sd->pty, m->str, strlen(m->str));
             for (i = 0; i < m->count; i++)
               {
                  termpty_write(sd->pty, "\n", 1);
                  buflen = snprintf(buf, sizeof(buf), "%i", (int)(m->val[i] * 1000.0));
                  termpty_write(sd->pty, buf, buflen);
               }
             termpty_write(sd->pty, zero, 1);
          }
        break;
      default:
        break;
     }
}

static void
_block_edje_cmds(Termpty *ty, Termblock *blk, Eina_List *cmds, Eina_Bool created)
{
   Eina_List *l;
   char *s;

#define ISCMD(cmd) !strcmp(s, cmd)
#define GETS(var) l = l->next; if (!l) return; var = l->data
#define GETI(var) l = l->next; if (!l) return; var = atoi(l->data)
#define GETF(var) l = l->next; if (!l) return; var = (double)atoi(l->data) / 1000.0
   l = cmds;
   while (l)
     {
        s = l->data;

        /////////////////////////////////////////////////////////////////////
        if (ISCMD("text")) // set text part
          {
             char *prt, *txt;

             GETS(prt);
             GETS(txt);
             edje_object_part_text_set(blk->obj, prt, txt);
          }
        /////////////////////////////////////////////////////////////////////
        else if (ISCMD("emit")) // emit signal
          {
             char *sig, *src;

             GETS(sig);
             GETS(src);
             edje_object_signal_emit(blk->obj, sig, src);
          }
        /////////////////////////////////////////////////////////////////////
        else if (ISCMD("drag")) // set dragable
          {
             char *prt, *val;
             double v1, v2;

             GETS(prt);
             GETS(val);
             GETF(v1);
             GETF(v2);
             if (!strcmp(val, "value"))
               edje_object_part_drag_value_set(blk->obj, prt, v1, v2);
             else if (!strcmp(val, "size"))
               edje_object_part_drag_size_set(blk->obj, prt, v1, v2);
             else if (!strcmp(val, "step"))
               edje_object_part_drag_step_set(blk->obj, prt, v1, v2);
             else if (!strcmp(val, "page"))
               edje_object_part_drag_page_set(blk->obj, prt, v1, v2);
          }
        /////////////////////////////////////////////////////////////////////
        else if (ISCMD("message")) // send message
          {
             int id;
             char *typ;

             GETI(id);
             GETS(typ);
             if (!strcmp(typ, "string"))
               {
                  Edje_Message_String *m;

                  m = alloca(sizeof(Edje_Message_String));
                  GETS(m->str);
                  edje_object_message_send(blk->obj, EDJE_MESSAGE_STRING,
                                           id, m);
               }
             else if (!strcmp(typ, "int"))
               {
                  Edje_Message_Int *m;

                  m = alloca(sizeof(Edje_Message_Int));
                  GETI(m->val);
                  edje_object_message_send(blk->obj, EDJE_MESSAGE_INT,
                                           id, m);
               }
             else if (!strcmp(typ, "float"))
               {
                  Edje_Message_Float *m;

                  m = alloca(sizeof(Edje_Message_Float));
                  GETF(m->val);
                  edje_object_message_send(blk->obj, EDJE_MESSAGE_FLOAT,
                                           id, m);
               }
             else if (!strcmp(typ, "string_set"))
               {
                  Edje_Message_String_Set *m;
                  int i, count;

                  GETI(count);
                  m = alloca(sizeof(Edje_Message_String_Set) +
                             ((count - 1) * sizeof(char *)));
                  m->count = count;
                  for (i = 0; i < m->count; i++)
                    {
                       GETS(m->str[i]);
                    }
                  edje_object_message_send(blk->obj,
                                           EDJE_MESSAGE_STRING_SET,
                                           id, m);
               }
             else if (!strcmp(typ, "int_set"))
               {
                  Edje_Message_Int_Set *m;
                  int i, count;

                  GETI(count);
                  m = alloca(sizeof(Edje_Message_Int_Set) +
                             ((count - 1) * sizeof(int)));
                  m->count = count;
                  for (i = 0; i < m->count; i++)
                    {
                       GETI(m->val[i]);
                    }
                  edje_object_message_send(blk->obj,
                                           EDJE_MESSAGE_INT_SET,
                                           id, m);
               }
             else if (!strcmp(typ, "float_set"))
               {
                  Edje_Message_Float_Set *m;
                  int i, count;

                  GETI(count);
                  m = alloca(sizeof(Edje_Message_Float_Set) +
                             ((count - 1) * sizeof(double)));
                  m->count = count;
                  for (i = 0; i < m->count; i++)
                    {
                       GETF(m->val[i]);
                    }
                  edje_object_message_send(blk->obj,
                                           EDJE_MESSAGE_FLOAT_SET,
                                           id, m);
               }
             else if (!strcmp(typ, "string_int"))
               {
                  Edje_Message_String_Int *m;

                  m = alloca(sizeof(Edje_Message_String_Int));
                  GETS(m->str);
                  GETI(m->val);
                  edje_object_message_send(blk->obj, EDJE_MESSAGE_STRING_INT,
                                           id, m);
               }
             else if (!strcmp(typ, "string_float"))
               {
                  Edje_Message_String_Float *m;

                  m = alloca(sizeof(Edje_Message_String_Float));
                  GETS(m->str);
                  GETF(m->val);
                  edje_object_message_send(blk->obj, EDJE_MESSAGE_STRING_FLOAT,
                                           id, m);
               }
             else if (!strcmp(typ, "string_int_set"))
               {
                  Edje_Message_String_Int_Set *m;
                  int i, count;

                  GETI(count);
                  m = alloca(sizeof(Edje_Message_String_Int_Set) +
                             ((count - 1) * sizeof(int)));
                  GETS(m->str);
                  m->count = count;
                  for (i = 0; i < m->count; i++)
                    {
                       GETI(m->val[i]);
                    }
                  edje_object_message_send(blk->obj,
                                           EDJE_MESSAGE_STRING_INT_SET,
                                           id, m);
               }
             else if (!strcmp(typ, "string_float_set"))
               {
                  Edje_Message_String_Float_Set *m;
                  int i, count;

                  GETI(count);
                  m = alloca(sizeof(Edje_Message_String_Float_Set) +
                             ((count - 1) * sizeof(double)));
                  GETS(m->str);
                  m->count = count;
                  for (i = 0; i < m->count; i++)
                    {
                       GETF(m->val[i]);
                    }
                  edje_object_message_send(blk->obj,
                                           EDJE_MESSAGE_STRING_FLOAT_SET,
                                           id, m);
               }
          }
        /////////////////////////////////////////////////////////////////////
        else if (ISCMD("chid")) // set callback channel id
          {
             char *chid;

             GETS(chid);
             if (!blk->chid)
               {
                  blk->chid = eina_stringshare_add(chid);
                  termpty_block_chid_update(ty, blk);
               }
             if (created)
               {
                  edje_object_signal_callback_add(blk->obj, "*", "*",
                                                  _block_edje_signal_cb,
                                                  blk);
                  edje_object_message_handler_set(blk->obj,
                                                  _block_edje_message_cb,
                                                  blk);
               }
          }
        if (l) l = l->next;
     }
}

static void
_block_edje_activate(Evas_Object *obj, Termblock *blk)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Eina_Bool ok = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   if ((!blk->path) || (!blk->link)) return;
   blk->obj = edje_object_add(evas_object_evas_get(obj));
   if (blk->path[0] == '/')
     {
        ok = edje_object_file_set(blk->obj, blk->path, blk->link);
     }
   else if (!strcmp(blk->path, "THEME"))
     {
        ok = edje_object_file_set(blk->obj,
                                  config_theme_path_default_get
                                  (sd->config),
                                  blk->link);
     }
   else
     {
        char path[PATH_MAX], home[PATH_MAX];

        if (homedir_get(home, sizeof(home)))
          {
             snprintf(path, sizeof(path), "%s/.terminology/objlib/%s",
                      home, blk->path);
             ok = edje_object_file_set(blk->obj, path, blk->link);
          }
        if (!ok)
          {
             snprintf(path, sizeof(path), "%s/objlib/%s",
                      elm_app_data_dir_get(), blk->path);
             ok = edje_object_file_set(blk->obj, path, blk->link);
          }
     }
   evas_object_smart_member_add(blk->obj, obj);
   evas_object_stack_above(blk->obj, sd->event);
   evas_object_show(blk->obj);
   evas_object_data_set(blk->obj, "blk", blk);

   if (ok)
     {
        _block_edje_cmds(sd->pty, blk, blk->cmds, EINA_TRUE);
     }
   else
     {
        ERR("failed to activate textblock of id %d", blk->id);
     }
}

static void
_block_media_activate(Evas_Object *obj, Termblock *blk)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Media_Type type;
   int media = MEDIA_STRETCH;
   Evas_Object *mctrl;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (blk->scale_stretch)
     media = MEDIA_STRETCH;
   else if (blk->scale_center)
     media = MEDIA_POP;
   else if (blk->scale_fill)
     media = MEDIA_BG;
   else if (blk->thumb)
     media = MEDIA_THUMB;

   if (!blk->was_active_before || blk->mov_state == MOVIE_STATE_STOP)
     media |= MEDIA_SAVE;
   else
     media |= MEDIA_RECOVER | MEDIA_SAVE;

   type = media_src_type_get(blk->path);
   blk->obj = media_add(obj, blk->path, sd->config, media, type);

   if (type == MEDIA_TYPE_MOV)
     media_play_set(blk->obj, blk->mov_state == MOVIE_STATE_PLAY);

   evas_object_event_callback_add
     (blk->obj, EVAS_CALLBACK_DEL, _smart_media_del, blk);
   evas_object_smart_callback_add(blk->obj, "play", _smart_media_play, blk);
   evas_object_smart_callback_add(blk->obj, "pause", _smart_media_pause, blk);
   evas_object_smart_callback_add(blk->obj, "stop", _smart_media_stop, blk);

   blk->type = type;
   evas_object_smart_member_add(blk->obj, obj);

   mctrl = media_control_get(blk->obj);
   if (mctrl)
     {
        evas_object_smart_member_add(mctrl, obj);
        evas_object_stack_above(mctrl, sd->event);
     }

   evas_object_stack_above(blk->obj, sd->grid.obj);
   evas_object_show(blk->obj);
   evas_object_data_set(blk->obj, "blk", blk);

   if (blk->thumb)
     {
        evas_object_smart_callback_add(blk->obj, "clicked",
                                       _smart_media_clicked, obj);
     }
}

static void
_block_activate(Evas_Object *obj, Termblock *blk)
{
   Termio *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (blk->active)
     return;
   blk->active = EINA_TRUE;
   if (blk->obj)
     return;
   if (blk->edje)
     _block_edje_activate(obj, blk);
   else
     _block_media_activate(obj, blk);

   blk->was_active_before = EINA_TRUE;
   if (!blk->was_active)
     sd->pty->block.active = eina_list_append(sd->pty->block.active, blk);
}

static void
_block_obj_del(Termblock *blk)
{
   if (!blk->obj) return;

   evas_object_event_callback_del_full
      (blk->obj, EVAS_CALLBACK_DEL,
       _smart_media_del, blk);
   evas_object_del(blk->obj);
   blk->obj = NULL;
}

/* }}} */
/* {{{ Keys */

static void
_smart_cb_key_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Key_Up *ev = event;
   Termio *sd = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN(sd);

   keyin_handle_up(&sd->khdl, ev);
}

static void
_smart_cb_key_down(void *data, Evas *e EINA_UNUSED,
                   Evas_Object *obj EINA_UNUSED, void *event)
{
   const Evas_Event_Key_Down *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   int ctrl, alt, shift, win, meta, hyper;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   EINA_SAFETY_ON_NULL_RETURN(ev->key);

   if (miniview_handle_key(term_miniview_get(sd->term), ev))
     return;

   if (term_has_popmedia(sd->term) && !strcmp(ev->key, "Escape"))
     {
        term_popmedia_close(sd->term);
        return;
     }

   ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   alt = evas_key_modifier_is_set(ev->modifiers, "Alt");
   shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   win = evas_key_modifier_is_set(ev->modifiers, "Super");
   meta = evas_key_modifier_is_set(ev->modifiers, "Meta") ||
      evas_key_modifier_is_set(ev->modifiers, "AltGr") ||
      evas_key_modifier_is_set(ev->modifiers, "ISO_Level3_Shift");
   hyper = evas_key_modifier_is_set(ev->modifiers, "Hyper");

   if (keyin_handle(&sd->khdl, sd->pty, ev, ctrl, alt, shift, win, meta, hyper))
     goto end;

   if (sd->jump_on_keypress)
     {
        if (!key_is_modifier(ev->key))
          {
             sd->scroll = 0;
             _smart_update_queue(data, sd);
          }
     }
end:
   if (sd->config->flicker_on_key)
     edje_object_signal_emit(sd->cursor.obj, "key,down", "terminology");
}

/* }}} */
/* {{{ Selection */

static Eina_Bool
_mouse_in_selection(Termio *sd, int cx, int cy)
{
   int start_x = 0, start_y = 0, end_x = 0, end_y = 0;

   if (!sd->pty->selection.is_active)
     return EINA_FALSE;

   start_x = sd->pty->selection.start.x;
   start_y = sd->pty->selection.start.y;
   end_x = sd->pty->selection.end.x;
   end_y = sd->pty->selection.end.y;

   if (!sd->pty->selection.is_top_to_bottom)
     {
        INT_SWAP(start_y, end_y);
        INT_SWAP(start_x, end_x);
     }
   if (sd->pty->selection.is_box)
     {
        if ((start_y <= cy) && (cy <= end_y) &&
            (start_x <= cx) && (cx <= end_x) )
          return EINA_TRUE;
     }
   else
     {
        if ((cy < start_y) || (cy > end_y))
          return EINA_FALSE;
        if (((cy == start_y) && (cx < start_x)) ||
            ((cy == end_y) && (cx > end_x)))
          return EINA_FALSE;
        return EINA_TRUE;
     }
     return EINA_FALSE;
}

struct termio_sb {
   char *buf;
   size_t len;
   size_t alloc;
};

static int
_sb_add(struct termio_sb *sb, const char *s, size_t len)
{
   size_t new_len = sb->len + len;

   if ((new_len >= sb->alloc) || !sb->buf)
     {
        size_t new_alloc = ((new_len + 15) / 16) * 24;
        char *new_buf;

        new_buf = realloc(sb->buf, new_alloc);
        if (new_buf == NULL)
          return -1;
        sb->buf = new_buf;
        sb->alloc = new_alloc;
     }
   memcpy(sb->buf + sb->len, s, len);
   sb->len += len;
   sb->buf[sb->len] = '\0';
   return 0;
}

/* unlike eina_strbuf_rtrim, only trims \t, \f, ' ' */
static void
_sb_spaces_rtrim(struct termio_sb *sb)
{
   if (!sb->buf)
     return;

   while (sb->len > 0)
     {
        char c = sb->buf[sb->len - 1];
        if ((c != ' ') && (c != '\t') && (c != '\f'))
            break;
        sb->len--;
     }
   sb->buf[sb->len] = '\0';
}


char *
termio_selection_get(Evas_Object *obj, int c1x, int c1y, int c2x, int c2y,
                     size_t *lenp,
                     Eina_Bool rtrim)
{
   Termio *sd = evas_object_smart_data_get(obj);
   struct termio_sb sb = {.buf = NULL, .len = 0, .alloc = 0};
   int x, y;

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);
   termpty_backlog_lock();
   for (y = c1y; y <= c2y; y++)
     {
        Termcell *cells;
        ssize_t w;
        int last0, v, start_x, end_x;

        w = 0;
        last0 = -1;
        cells = termpty_cellrow_get(sd->pty, y, &w);
        if (!cells || !w)
          {
             if (_sb_add(&sb, "\n", 1) < 0) goto err;
             continue;
          }
        if (w > sd->grid.w) w = sd->grid.w;
        if (y == c1y && c1x >= w)
          {
             if (rtrim)
               _sb_spaces_rtrim(&sb);
             if (_sb_add(&sb, "\n", 1) < 0) goto err;
             continue;
          }
        start_x = c1x;
        end_x = (c2x >= w) ? w - 1 : c2x;
        if (c1y != c2y)
          {
             if (y == c1y) end_x = w - 1;
             else if (y == c2y) start_x = 0;
             else
               {
                  start_x = 0;
                  end_x = w - 1;
               }
          }
        for (x = start_x; x <= end_x; x++)
          {
             if ((cells[x].codepoint == 0) && (cells[x].att.dblwidth))
               {
                  if (x < end_x) x++;
                  else break;
               }
             if (x >= w) break;
             if (cells[x].att.newline)
               {
                  last0 = -1;
                  if ((y != c2y) || (x != end_x))
                    {
                       if (rtrim)
                         _sb_spaces_rtrim(&sb);
                       if (_sb_add(&sb, "\n", 1) < 0) goto err;
                    }
                  break;
               }
             else if (cells[x].att.tab)
               {
                  if (_sb_add(&sb, "\t", 1) < 0) goto err;
                  x = ((x + 8) / 8) * 8;
                  x--; /* counter the ++ of the for loop */
               }
             else if (cells[x].codepoint == 0)
               {
                  if (last0 < 0) last0 = x;
               }
             else
               {
                  char txt[8];
                  int txtlen;

                  if (last0 >= 0)
                    {
                       v = x - last0 - 1;
                       last0 = -1;
                       while (v >= 0)
                         {
                            if (_sb_add(&sb, " ", 1) < 0) goto err;
                            v--;
                         }
                    }
                  txtlen = codepoint_to_utf8(cells[x].codepoint, txt);
                  if (txtlen > 0)
                    if (_sb_add(&sb, txt, txtlen) < 0) goto err;
                  if ((x == (w - 1)) &&
                      ((x != c2x) || (y != c2y)))
                    {
                       if (!cells[x].att.autowrapped)
                         {
                            if (rtrim)
                              _sb_spaces_rtrim(&sb);
                            if (_sb_add(&sb, "\n", 1) < 0) goto err;
                         }
                    }
               }
          }
        if (last0 >= 0)
          {
             if (y == c2y)
               {
                  Eina_Bool have_more = EINA_FALSE;

                  for (x = end_x + 1; x < w; x++)
                    {
                       if ((cells[x].codepoint == 0) &&
                           (cells[x].att.dblwidth))
                         {
                            if (x < (w - 1)) x++;
                            else break;
                         }
                       if (((cells[x].codepoint != 0) &&
                            (cells[x].codepoint != ' ')) ||
                           (cells[x].att.newline) ||
                           (cells[x].att.tab))
                         {
                            have_more = EINA_TRUE;
                            break;
                         }
                    }
                  if (!have_more)
                    {
                       if (rtrim)
                         _sb_spaces_rtrim(&sb);
                       if (_sb_add(&sb, "\n", 1) < 0) goto err;
                    }
                  else
                    {
                       for (x = last0; x <= end_x; x++)
                         {
                            if ((cells[x].codepoint == 0) &&
                                (cells[x].att.dblwidth))
                              {
                                 if (x < (w - 1)) x++;
                                 else break;
                              }
                            if (x >= w) break;
                            if (_sb_add(&sb, " ", 1) < 0) goto err;
                         }
                    }
               }
             else
               {
                  if (rtrim)
                    _sb_spaces_rtrim(&sb);
                  if (_sb_add(&sb, "\n", 1) < 0) goto err;
               }
          }
     }
   termpty_backlog_unlock();

   if (rtrim)
     _sb_spaces_rtrim(&sb);

   if (lenp)
     *lenp = sb.len;

   return sb.buf;
err:
   free(sb.buf);
   return NULL;
}


static void
_sel_set(Termio *sd, Eina_Bool enable)
{
   if (sd->pty->selection.is_active == enable) return;
   sd->pty->selection.is_active = enable;
   if (enable)
     evas_object_smart_callback_call(sd->win, "selection,on", NULL);
   else
     {
        evas_object_smart_callback_call(sd->win, "selection,off", NULL);
        sd->pty->selection.by_word = EINA_FALSE;
        sd->pty->selection.by_line = EINA_FALSE;
     }
}

static void _lost_selection(void *data, Elm_Sel_Type selection);

static void
_lost_selection_reset_job(void *data)
{
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   sd->sel_reset_job = NULL;
   elm_cnp_selection_set(sd->win, sd->sel_type,
                         ELM_SEL_FORMAT_TEXT,
                         sd->sel_str, strlen(sd->sel_str));
   elm_cnp_selection_loss_callback_set(sd->win, sd->sel_type,
                                       _lost_selection, data);
}

static void
_lost_selection(void *data, Elm_Sel_Type selection)
{
   Eina_List *l;
   Evas_Object *obj;
   double t = ecore_time_get();
   EINA_LIST_FOREACH(terms, l, obj)
     {
        Termio *sd = evas_object_smart_data_get(obj);
        if (!sd) continue;
        if ((t - sd->set_sel_at) < 0.2) /// hack
          {
             if ((sd->have_sel) && (sd->sel_str) && (!sd->reset_sel))
               {
                  sd->reset_sel = EINA_TRUE;
                  if (sd->sel_reset_job) ecore_job_del(sd->sel_reset_job);
                  sd->sel_reset_job = ecore_job_add
                    (_lost_selection_reset_job, data);
               }
             continue;
          }
        if (sd->have_sel)
          {
             if (sd->sel_str)
               {
                  eina_stringshare_del(sd->sel_str);
                  sd->sel_str = NULL;
               }
             _sel_set(sd, EINA_FALSE);
             elm_object_cnp_selection_clear(sd->win, selection);
             _smart_update_queue(obj, sd);
             sd->have_sel = EINA_FALSE;
          }
     }
}

static void
_take_selection_text(Termio *sd, Elm_Sel_Type type, const char *text)
{
   EINA_SAFETY_ON_NULL_RETURN(sd);

   text = eina_stringshare_add(text);

   sd->have_sel = EINA_FALSE;
   sd->reset_sel = EINA_FALSE;
   sd->set_sel_at = ecore_time_get(); // hack
   sd->sel_type = type;

   elm_cnp_selection_set(sd->win, type,
                         ELM_SEL_FORMAT_TEXT,
                         text,
                         eina_stringshare_strlen(text));
   elm_cnp_selection_loss_callback_set(sd->win, type,
                                       _lost_selection, sd->self);
   sd->have_sel = EINA_TRUE;
   if (sd->sel_str) eina_stringshare_del(sd->sel_str);
   sd->sel_str = text;
}

Eina_Bool
termio_take_selection(Evas_Object *obj, Elm_Sel_Type type)
{
   Termio *sd = evas_object_smart_data_get(obj);
   int start_x = 0, start_y = 0, end_x = 0, end_y = 0;
   char *s = NULL;
   size_t len = 0;

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EINA_FALSE);
   if (sd->pty->selection.is_active)
     {
        start_x = sd->pty->selection.start.x;
        start_y = sd->pty->selection.start.y;
        end_x = sd->pty->selection.end.x;
        end_y = sd->pty->selection.end.y;

        if (!sd->pty->selection.is_top_to_bottom)
          {
             INT_SWAP(start_y, end_y);
             INT_SWAP(start_x, end_x);
          }
     }
   else
     {
        if (sd->link.string)
          {
             len = strlen(sd->link.string);
             s = strndup(sd->link.string, len);
          }
        return EINA_FALSE;
     }

   if (sd->pty->selection.is_box)
     {
        int i;
        Eina_Strbuf *sb;

        sb = eina_strbuf_new();
        for (i = start_y; i <= end_y; i++)
          {
             /* TODO: use our own strbuf implementation */
             char *tmp = termio_selection_get(obj, start_x, i, end_x, i,
                                              &len, EINA_TRUE);

             if (tmp)
               {
                  eina_strbuf_append_length(sb, tmp, len);
                  if (len && tmp[len - 1] != '\n' && i != end_y)
                    eina_strbuf_append_char(sb, '\n');
                  free(tmp);
               }
          }
        len = eina_strbuf_length_get(sb);
        s = eina_strbuf_string_steal(sb);
        eina_strbuf_free(sb);
     }
   else
     {
        s = termio_selection_get(obj, start_x, start_y, end_x, end_y, &len,
                                 EINA_TRUE);
     }

   if (s)
     {
        if ((sd->win) && (len > 0))
          _take_selection_text(sd, type, s);
        free(s);
        return EINA_TRUE;
     }
   return EINA_FALSE;
}

static Eina_Bool
_getsel_cb(void *data, Evas_Object *obj EINA_UNUSED, Elm_Selection_Data *ev)
{
   Termio *sd = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EINA_FALSE);

   if (ev->format == ELM_SEL_FORMAT_TEXT)
     {
        char *tmp;

        if (ev->len <= 0) return EINA_TRUE;

        tmp = malloc(ev->len);
        if (tmp)
          {
             char *s = ev->data;
             size_t i;

             // apparently we have to convert \n into \r in terminal land.
             for (i = 0; i < ev->len && s[i]; i++)
               {
                  tmp[i] = s[i];
                  if (tmp[i] == '\n') tmp[i] = '\r';
               }
             if (i)
               {

                if (sd->pty->bracketed_paste)
                  termpty_write(sd->pty, "\x1b[200~",
                                sizeof("\x1b[200~") - 1);

                termpty_write(sd->pty, tmp, i);

                if (sd->pty->bracketed_paste)
                  termpty_write(sd->pty, "\x1b[201~",
                                sizeof("\x1b[201~") - 1);
               }

             free(tmp);
          }
     }
   else
     {
        const char *fmt = "UNKNOWN";
        switch (ev->format)
          {
           case ELM_SEL_FORMAT_TARGETS: fmt = "TARGETS"; break; /* shouldn't happen */
           case ELM_SEL_FORMAT_NONE: fmt = "NONE"; break;
           case ELM_SEL_FORMAT_TEXT: fmt = "TEXT"; break;
           case ELM_SEL_FORMAT_MARKUP: fmt = "MARKUP"; break;
           case ELM_SEL_FORMAT_IMAGE: fmt = "IMAGE"; break;
           case ELM_SEL_FORMAT_VCARD: fmt = "VCARD"; break;
           case ELM_SEL_FORMAT_HTML: fmt = "HTML"; break;
          }
        WRN(_("unsupported selection format '%s'"), fmt);
     }
   return EINA_TRUE;
}

void
termio_paste_selection(Evas_Object *obj, Elm_Sel_Type type)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   if (!sd->win) return;
   elm_cnp_selection_get(sd->win, type, ELM_SEL_FORMAT_TEXT,
                         _getsel_cb, obj);
}

static void
_sel_line(Termio *sd, int cy)
{
   int y;
   ssize_t w = 0;
   Termcell *cells;

   termpty_backlog_lock();

   _sel_set(sd, EINA_TRUE);
   sd->pty->selection.makesel = EINA_FALSE;

   sd->pty->selection.start.x = 0;
   sd->pty->selection.start.y = cy;
   sd->pty->selection.end.x = sd->grid.w - 1;
   sd->pty->selection.end.y = cy;

   y = cy;
   for (;;)
     {
        cells = termpty_cellrow_get(sd->pty, y - 1, &w);
        if (!cells || !cells[w-1].att.autowrapped) break;

        y--;
     }
   sd->pty->selection.start.y = y;
   y = cy;

   for (;;)
     {
        cells = termpty_cellrow_get(sd->pty, y, &w);
        if (!cells || !cells[w-1].att.autowrapped) break;

        sd->pty->selection.end.x = w - 1;
        y++;
     }
   sd->pty->selection.end.y = y;

   sd->pty->selection.by_line = EINA_TRUE;
   sd->pty->selection.is_top_to_bottom = EINA_TRUE;

   termpty_backlog_unlock();
}

static void
_sel_line_to(Termio *sd, int cy, Eina_Bool extend)
{
   int start_y, end_y, c_start_y, c_end_y,
       orig_y, orig_start_y, orig_end_y;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   /* Only change the end position */
   orig_y = sd->pty->selection.orig.y;
   start_y = sd->pty->selection.start.y;
   end_y   = sd->pty->selection.end.y;

   if (!sd->pty->selection.is_top_to_bottom)
     INT_SWAP(start_y, end_y);

   _sel_line(sd, cy);
   c_start_y = sd->pty->selection.start.y;
   c_end_y   = sd->pty->selection.end.y;

   _sel_line(sd, orig_y);
   orig_start_y = sd->pty->selection.start.y;
   orig_end_y   = sd->pty->selection.end.y;

   if (sd->pty->selection.is_box)
     {
        if (extend)
          {
             if (start_y <= cy && cy <= end_y )
               {
                  start_y = MIN(c_start_y, orig_start_y);
                  end_y = MAX(c_end_y, orig_end_y);
               }
             else
               {
                  if (c_end_y > end_y)
                    {
                       orig_y = start_y;
                       end_y = c_end_y;
                    }
                  if (c_start_y < start_y)
                    {
                       orig_y = end_y;
                       start_y = c_start_y;
                    }
               }
             goto end;
          }
        else
          {
             start_y = MIN(c_start_y, orig_start_y);
             end_y = MAX(c_end_y, orig_end_y);
             goto end;
          }
     }
   else
     {
        if (c_start_y < start_y)
          {
             /* orig is at bottom */
             if (extend)
               {
                  orig_y = end_y;
               }
             sd->pty->selection.is_top_to_bottom = EINA_FALSE;
             end_y = c_start_y;
             start_y = orig_end_y;
          }
        else if (c_end_y > end_y)
          {
             if (extend)
               {
                  orig_y = start_y;
               }
             sd->pty->selection.is_top_to_bottom = EINA_TRUE;
             start_y = orig_start_y;
             end_y = c_end_y;
          }
        else
          {
             if (c_start_y < orig_start_y)
               {
                  sd->pty->selection.is_top_to_bottom = EINA_FALSE;
                  start_y = orig_end_y;
                  end_y = c_start_y;
               }
             else
               {
                  sd->pty->selection.is_top_to_bottom = EINA_TRUE;
                  start_y = orig_start_y;
                  end_y = c_end_y;
               }
          }
     }
end:
   sd->pty->selection.orig.y = orig_y;
   sd->pty->selection.start.y = start_y;
   sd->pty->selection.end.y = end_y;
   if (sd->pty->selection.is_top_to_bottom)
     {
        sd->pty->selection.start.x = 0;
        sd->pty->selection.end.x = sd->grid.w - 1;
     }
   else
     {
        sd->pty->selection.end.x = 0;
        sd->pty->selection.start.x = sd->grid.w - 1;
     }
}

static Eina_Bool
_codepoint_is_wordsep(const Eina_Unicode g)
{
   /* TODO: use bitmaps to speed things up */
   // http://en.wikipedia.org/wiki/Asterisk
   // http://en.wikipedia.org/wiki/Comma
   // http://en.wikipedia.org/wiki/Interpunct
   // http://en.wikipedia.org/wiki/Bracket
   static const Eina_Unicode wordsep[] =
     {
       0,
       ' ',
       '!',
       '"',
       '#',
       '$',
       '\'',
       '(',
       ')',
       '*',
       ',',
       ';',
       '=',
       '?',
       '[',
       '\\',
       ']',
       '^',
       '`',
       '{',
       '|',
       '}',
       0x00a0,
       0x00ab,
       0x00b7,
       0x00bb,
       0x0294,
       0x02bb,
       0x02bd,
       0x02d0,
       0x0312,
       0x0313,
       0x0314,
       0x0315,
       0x0326,
       0x0387,
       0x055d,
       0x055e,
       0x060c,
       0x061f,
       0x066d,
       0x07fb,
       0x1363,
       0x1367,
       0x14fe,
       0x1680,
       0x1802,
       0x1808,
       0x180e,
       0x2000,
       0x2001,
       0x2002,
       0x2003,
       0x2004,
       0x2005,
       0x2006,
       0x2007,
       0x2008,
       0x2009,
       0x200a,
       0x200b,
       0x2018,
       0x2019,
       0x201a,
       0x201b,
       0x201c,
       0x201d,
       0x201e,
       0x201f,
       0x2022,
       0x2027,
       0x202f,
       0x2039,
       0x203a,
       0x203b,
       0x203d,
       0x2047,
       0x2048,
       0x2049,
       0x204e,
       0x205f,
       0x2217,
       0x225f,
       0x2308,
       0x2309,
       0x2420,
       0x2422,
       0x2423,
       0x2722,
       0x2723,
       0x2724,
       0x2725,
       0x2731,
       0x2732,
       0x2733,
       0x273a,
       0x273b,
       0x273c,
       0x273d,
       0x2743,
       0x2749,
       0x274a,
       0x274b,
       0x2a7b,
       0x2a7c,
       0x2cfa,
       0x2e2e,
       0x2e2e,
       0x3000,
       0x3001,
       0x3008,
       0x3009,
       0x300a,
       0x300b,
       0x300c,
       0x300c,
       0x300d,
       0x300d,
       0x300e,
       0x300f,
       0x3010,
       0x3011,
       0x301d,
       0x301e,
       0x301f,
       0x30fb,
       0xa60d,
       0xa60f,
       0xa6f5,
       0xe0a0,
       0xe0b0,
       0xe0b2,
       0xfe10,
       0xfe41,
       0xfe42,
       0xfe43,
       0xfe44,
       0xfe50,
       0xfe51,
       0xfe56,
       0xfe61,
       0xfe62,
       0xfe63,
       0xfeff,
       0xff02,
       0xff07,
       0xff08,
       0xff09,
       0xff0a,
       0xff0c,
       0xff1b,
       0xff1c,
       0xff1e,
       0xff1f,
       0xff3b,
       0xff3d,
       0xff5b,
       0xff5d,
       0xff62,
       0xff63,
       0xff64,
       0xff65,
       0xe002a
   };
   size_t imax = (sizeof(wordsep) / sizeof(wordsep[0])) - 1,
          imin = 0;
   size_t imaxmax = imax;

   if (g & 0x80000000)
     return EINA_TRUE;

   while (imax >= imin)
     {
        size_t imid = (imin + imax) / 2;

        if (wordsep[imid] == g) return EINA_TRUE;
        else if (wordsep[imid] < g) imin = imid + 1;
        else imax = imid - 1;
        if (imax > imaxmax) break;
     }
   return EINA_FALSE;
}

static Eina_Bool
_to_trim(Eina_Unicode codepoint, Eina_Bool right_trim)
{
   static const Eina_Unicode trim_chars[] =
     {
       ':',
       '<',
       '>',
       '.'
     };
   size_t i = 0, len;
   len = sizeof(trim_chars)/sizeof((trim_chars)[0]);
   if (right_trim)
     len--; /* do not right trim . */

   for (i = 0; i < len; i++)
     if (codepoint == trim_chars[i])
       return EINA_TRUE;
   return EINA_FALSE;
}

static void
_trim_sel_word(Termio *sd)
{
   Termpty *pty = sd->pty;
   Termcell *cells;
   int start = 0, end = 0, y = 0;
   ssize_t w;

   /* 1st step: trim from the start */
   start = pty->selection.start.x;
   for (y = pty->selection.start.y;
        y <= pty->selection.end.y;
        y++)
     {
        cells = termpty_cellrow_get(pty, y, &w);

        while (start < w && _to_trim(cells[start].codepoint, EINA_TRUE))
          start++;

        if (start < w)
          break;

        start = 0;
     }
   /* check validy of the selection */
   if ((y > pty->selection.end.y) ||
       ((y == pty->selection.end.y) &&
        (start > pty->selection.end.x)))
     {
        pty->selection.start.y = pty->selection.end.y;
        pty->selection.start.x = pty->selection.end.x;
        return;
     }
   pty->selection.start.y = y;
   pty->selection.start.x = start;

   /* 2nd step: trim from the end */
   end = pty->selection.end.x;
   for (y = pty->selection.end.y;
        y >= pty->selection.start.y;
        y--)
     {
        cells = termpty_cellrow_get(pty, y, &w);

        while (end >= 0 && _to_trim(cells[end].codepoint, EINA_FALSE))
          end--;

        if (end >= 0)
          break;
     }
   if (end < 0)
     {
        return;
     }
   /* check validy of the selection */
   if ((y < pty->selection.start.y) ||
       ((y == pty->selection.start.y) &&
        (end < pty->selection.start.x)))
     {
        pty->selection.end.x = pty->selection.start.x;
        pty->selection.end.y = pty->selection.start.y;
        return;
     }
   pty->selection.end.x = end;
   pty->selection.end.y = y;
}

static void
_sel_word(Termio *sd, int cx, int cy)
{
   Termcell *cells;
   int x, y;
   ssize_t w = 0;
   Eina_Bool done = EINA_FALSE;

   termpty_backlog_lock();

   _sel_set(sd, EINA_TRUE);
   sd->pty->selection.makesel = EINA_TRUE;
   sd->pty->selection.orig.x = cx;
   sd->pty->selection.orig.y = cy;
   sd->pty->selection.start.x = cx;
   sd->pty->selection.start.y = cy;
   sd->pty->selection.end.x = cx;
   sd->pty->selection.end.y = cy;
   x = cx;
   y = cy;

   if (sd->link.string &&
       (sd->link.x1 <= cx) && (cx <= sd->link.x2) &&
       (sd->link.y1 <= cy) && (cy <= sd->link.y2))
     {
        sd->pty->selection.start.x = sd->link.x1;
        sd->pty->selection.start.y = sd->link.y1;
        sd->pty->selection.end.x = sd->link.x2;
        sd->pty->selection.end.y = sd->link.y2;
        goto end;
     }
   cells = termpty_cellrow_get(sd->pty, y, &w);
   if (!cells) goto end;
   if (x >= w) x = w - 1;

   do
     {
        for (; x >= 0; x--)
          {
             if ((cells[x].codepoint == 0) && (cells[x].att.dblwidth) &&
                 (x > 0))
               x--;
             if (_codepoint_is_wordsep(cells[x].codepoint))
               {
                  done = EINA_TRUE;
                  break;
               }
             sd->pty->selection.start.x = x;
             sd->pty->selection.start.y = y;
          }
        if (!done)
          {
             Termcell *old_cells = cells;

             cells = termpty_cellrow_get(sd->pty, y - 1, &w);
             if (!cells || !cells[w-1].att.autowrapped)
               {
                  x = 0;
                  cells = old_cells;
                  done = EINA_TRUE;
               }
             else
               {
                  y--;
                  x = w - 1;
               }
          }
     }
   while (!done);

   done = EINA_FALSE;
   if (cy != y)
     {
        y = cy;
        cells = termpty_cellrow_get(sd->pty, y, &w);
        if (!cells) goto end;
     }
   x = cx;

   do
     {
        for (; x < w; x++)
          {
             if ((cells[x].codepoint == 0) && (cells[x].att.dblwidth) &&
                 (x < (w - 1)))
               {
                  sd->pty->selection.end.x = x;
                  x++;
               }
             if (_codepoint_is_wordsep(cells[x].codepoint))
               {
                  done = EINA_TRUE;
                  break;
               }
             sd->pty->selection.end.x = x;
             sd->pty->selection.end.y = y;
          }
        if (!done)
          {
             if (!cells[w - 1].att.autowrapped) goto end;
             y++;
             x = 0;
             cells = termpty_cellrow_get(sd->pty, y, &w);
             if (!cells) goto end;
          }
     }
   while (!done);

  end:

   sd->pty->selection.by_word = EINA_TRUE;
   sd->pty->selection.is_top_to_bottom = EINA_TRUE;
   _trim_sel_word(sd);

   termpty_backlog_unlock();
}

static void
_sel_word_to(Termio *sd, int cx, int cy, Eina_Bool extend)
{
   int start_x, start_y, end_x, end_y, orig_x, orig_y,
       c_start_x, c_start_y, c_end_x, c_end_y,
       orig_start_x, orig_start_y, orig_end_x, orig_end_y;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   orig_x = sd->pty->selection.orig.x;
   orig_y = sd->pty->selection.orig.y;
   start_x = sd->pty->selection.start.x;
   start_y = sd->pty->selection.start.y;
   end_x   = sd->pty->selection.end.x;
   end_y   = sd->pty->selection.end.y;

   if (!sd->pty->selection.is_top_to_bottom)
     {
        INT_SWAP(start_x, end_x);
        INT_SWAP(start_y, end_y);
     }

   _sel_word(sd, cx, cy);
   c_start_x = sd->pty->selection.start.x;
   c_start_y = sd->pty->selection.start.y;
   c_end_x   = sd->pty->selection.end.x;
   c_end_y   = sd->pty->selection.end.y;

   _sel_word(sd, orig_x, orig_y);
   orig_start_x = sd->pty->selection.start.x;
   orig_start_y = sd->pty->selection.start.y;
   orig_end_x   = sd->pty->selection.end.x;
   orig_end_y   = sd->pty->selection.end.y;

   if (sd->pty->selection.is_box)
     {
        if (extend)
          {
             /* special case: kind of line selection */
             if (c_start_y != c_end_y)
               {
                  start_x = 0;
                  end_x = sd->grid.w - 1;
                  if (start_y <= cy && cy <= end_y )
                    {
                       start_y = MIN(c_start_y, orig_start_y);
                       end_y = MAX(c_end_y, orig_end_y);
                    }
                  else
                    {
                       if (c_end_y > end_y)
                         {
                            orig_y = start_y;
                            end_y = c_end_y;
                         }
                       if (c_start_y < start_y)
                         {
                            orig_y = end_y;
                            start_y = c_start_y;
                         }
                    }
                  goto end;
               }
             if ((start_y <= cy && cy <= end_y ) &&
                 (start_x <= cx && cx <= end_x ))
               {
                  start_x = MIN(c_start_x, orig_start_x);
                  end_x = MAX(c_end_x, orig_end_x);
                  start_y = MIN(c_start_y, orig_start_y);
                  end_y = MAX(c_end_y, orig_end_y);
               }
             else
               {
                  if (c_end_x > end_x)
                    {
                       orig_x = start_x;
                       end_x = c_end_x;
                    }
                  if (c_start_x < start_x)
                    {
                       orig_x = end_x;
                       start_x = c_start_x;
                    }
                  if (c_end_y > end_y)
                    {
                       orig_y = start_y;
                       end_y = c_end_y;
                    }
                  if (c_start_y < start_y)
                    {
                       orig_y = end_y;
                       start_y = c_start_y;
                    }
                  end_x = MAX(c_end_x, end_x);
                  start_y = MIN(c_start_y, start_y);
                  end_y = MAX(c_end_y, end_y);
               }
          }
        else
          {
             /* special case: kind of line selection */
             if (c_start_y != c_end_y || orig_start_y != orig_end_y)
               {
                  start_x = 0;
                  end_x = sd->grid.w - 1;
                  start_y = MIN(c_start_y, orig_start_y);
                  end_y = MAX(c_end_y, orig_end_y);
                  goto end;
               }

             start_x = MIN(c_start_x, orig_start_x);
             end_x = MAX(c_end_x, orig_end_x);
             start_y = MIN(c_start_y, orig_start_y);
             end_y = MAX(c_end_y, orig_end_y);
          }
     }
   else
     {
        if (c_start_y < start_y ||
            (c_start_y == start_y &&
             c_start_x < start_x))
          {
             /* orig is at bottom */
             if (extend)
               {
                  orig_x = end_x;
                  orig_y = end_y;
               }
             sd->pty->selection.is_top_to_bottom = EINA_FALSE;
             end_x = c_start_x;
             end_y = c_start_y;
             start_x = orig_end_x;
             start_y = orig_end_y;
          }
        else if (c_end_y > end_y ||
                 (c_end_y == end_y && c_end_x >= end_x))
          {
             if (extend)
               {
                  orig_x = start_x;
                  orig_y = start_y;
               }
             sd->pty->selection.is_top_to_bottom = EINA_TRUE;
             start_x = orig_start_x;
             start_y = orig_start_y;
             end_x = c_end_x;
             end_y = c_end_y;
          }
        else
          {
             if (c_start_y < orig_start_y ||
                 (c_start_y == orig_start_y && c_start_x <= orig_start_x))
               {
                  sd->pty->selection.is_top_to_bottom = EINA_FALSE;
                  start_x = orig_end_x;
                  start_y = orig_end_y;
                  end_x = c_start_x;
                  end_y = c_start_y;
               }
             else
               {
                  sd->pty->selection.is_top_to_bottom = EINA_TRUE;
                  start_x = orig_start_x;
                  start_y = orig_start_y;
                  end_x = c_end_x;
                  end_y = c_end_y;
               }
          }
     }

end:
   sd->pty->selection.orig.x = orig_x;
   sd->pty->selection.orig.y = orig_y;
   sd->pty->selection.start.x = start_x;
   sd->pty->selection.start.y = start_y;
   sd->pty->selection.end.x = end_x;
   sd->pty->selection.end.y = end_y;
}

static void
_sel_to(Termio *sd, int cx, int cy, Eina_Bool extend)
{
   int start_x, start_y, end_x, end_y, orig_x, orig_y;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   orig_x = sd->pty->selection.orig.x;
   orig_y = sd->pty->selection.orig.y;
   start_x = sd->pty->selection.start.x;
   start_y = sd->pty->selection.start.y;
   end_x   = sd->pty->selection.end.x;
   end_y   = sd->pty->selection.end.y;

   if (sd->pty->selection.is_box)
     {
        if (!sd->pty->selection.is_top_to_bottom)
          INT_SWAP(start_y, end_y);
        if (start_x > end_x)
          INT_SWAP(start_x, end_x);

        if (cy < start_y)
          {
             start_y = cy;
          }
        else if (cy > end_y)
          {
             end_y = cy;
          }
        else
          {
             start_y = orig_y;
             end_y = cy;
          }

        if (cx < start_x)
          {
             start_x = cx;
          }
        else if (cx > end_x)
          {
             end_x = cx;
          }
        else
          {
             start_x = orig_x;
             end_x = cx;
          }
        sd->pty->selection.is_top_to_bottom = (end_y > start_y);
        if (sd->pty->selection.is_top_to_bottom)
          {
             if (start_x > end_x)
               INT_SWAP(start_x, end_x);
          }
        else
          {
             if (start_x < end_x)
               INT_SWAP(start_x, end_x);
          }
     }
   else
     {
        if (!sd->pty->selection.is_top_to_bottom)
          {
             INT_SWAP(start_x, end_x);
             INT_SWAP(start_y, end_y);
          }
        if (cy < start_y ||
            (cy == start_y &&
             cx < start_x))
          {
             /* orig is at bottom */
             if (sd->pty->selection.is_top_to_bottom && extend)
               {
                  orig_x = end_x;
                  orig_y = end_y;
               }
             sd->pty->selection.is_top_to_bottom = EINA_FALSE;
          }
        else if (cy > end_y ||
                 (cy == end_y && cx >= end_x))
          {
             if (!sd->pty->selection.is_top_to_bottom && extend)
               {
                  orig_x = start_x;
                  orig_y = start_y;
               }
             sd->pty->selection.is_top_to_bottom = EINA_TRUE;
          }
        else
          {
             sd->pty->selection.is_top_to_bottom =
                (cy > orig_y) || (cy == orig_y && cx > orig_x);
          }
        start_x = orig_x;
        start_y = orig_y;
        end_x = cx;
        end_y = cy;
     }

   sd->pty->selection.orig.x = orig_x;
   sd->pty->selection.orig.y = orig_y;
   sd->pty->selection.start.x = start_x;
   sd->pty->selection.start.y = start_y;
   sd->pty->selection.end.x = end_x;
   sd->pty->selection.end.y = end_y;
}

static void
_selection_dbl_fix(Termio *sd)
{
   int start_x, start_y, end_x, end_y;
   ssize_t w = 0;
   Termcell *cells;
   /* Only change the end position */

   EINA_SAFETY_ON_NULL_RETURN(sd);

   start_x = sd->pty->selection.start.x;
   start_y = sd->pty->selection.start.y;
   end_x   = sd->pty->selection.end.x;
   end_y   = sd->pty->selection.end.y;
   if (!sd->pty->selection.is_top_to_bottom)
     {
        INT_SWAP(start_y, end_y);
        INT_SWAP(start_x, end_x);
     }

   termpty_backlog_lock();
   cells = termpty_cellrow_get(sd->pty, end_y - sd->scroll, &w);
   if (cells)
     {
        // if sel2 after sel1
        if ((end_y > start_y) ||
            ((end_y == start_y) &&
                (end_x >= start_x)))
          {
             if (end_x < (w - 1))
               {
                  if ((cells[end_x].codepoint != 0) &&
                      (cells[end_x].att.dblwidth))
                    end_x++;
               }
          }
        // else sel1 after sel 2
        else
          {
             if (end_x > 0)
               {
                  if ((cells[end_x].codepoint == 0) &&
                      (cells[end_x].att.dblwidth))
                    end_x--;
               }
          }
     }
   cells = termpty_cellrow_get(sd->pty, start_y - sd->scroll, &w);
   if (cells)
     {
        // if sel2 after sel1
        if ((end_y > start_y) ||
            ((end_y == start_y) &&
                (end_x >= start_x)))
          {
             if (start_x > 0)
               {
                  if ((cells[start_x].codepoint == 0) &&
                      (cells[start_x].att.dblwidth))
                    start_x--;
               }
          }
        // else sel1 after sel 2
        else
          {
             if (start_x < (w - 1))
               {
                  if ((cells[start_x].codepoint != 0) &&
                      (cells[start_x].att.dblwidth))
                    start_x++;
               }
          }
     }
   termpty_backlog_unlock();

   if (!sd->pty->selection.is_top_to_bottom)
     {
        INT_SWAP(start_y, end_y);
        INT_SWAP(start_x, end_x);
     }
   sd->pty->selection.start.x = start_x;
   sd->pty->selection.start.y = start_y;
   sd->pty->selection.end.x = end_x;
   sd->pty->selection.end.y = end_y;
}

static void
_selection_newline_extend_fix(Evas_Object *obj)
{
   int start_x, start_y, end_x, end_y;
   Termio *sd;
   ssize_t w;

   sd = evas_object_smart_data_get(obj);

   if ((sd->top_left) || (sd->bottom_right) || (sd->pty->selection.is_box))
     return;

   termpty_backlog_lock();

   start_x = sd->pty->selection.start.x;
   start_y = sd->pty->selection.start.y;
   end_x   = sd->pty->selection.end.x;
   end_y   = sd->pty->selection.end.y;
   if (!sd->pty->selection.is_top_to_bottom)
     {
        INT_SWAP(start_y, end_y);
        INT_SWAP(start_x, end_x);
     }

   if ((end_y > start_y) ||
       ((end_y == start_y) &&
        (end_x >= start_x)))
     {
        /* going down/right */
        w = termpty_row_length(sd->pty, start_y);
        if (w < start_x)
          start_x = w;
        w = termpty_row_length(sd->pty, end_y);
        if (w <= end_x)
          end_x = sd->pty->w;
     }
   else
     {
        /* going up/left */
        w = termpty_row_length(sd->pty, end_y);
        if (w < end_x)
          end_x = w;
        w = termpty_row_length(sd->pty, start_y);
        if (w <= start_x)
          start_x = sd->pty->w;
     }

   if (!sd->pty->selection.is_top_to_bottom)
     {
        INT_SWAP(start_y, end_y);
        INT_SWAP(start_x, end_x);
     }
   sd->pty->selection.start.x = start_x;
   sd->pty->selection.start.y = start_y;
   sd->pty->selection.end.x = end_x;
   sd->pty->selection.end.y = end_y;

   termpty_backlog_unlock();
}

/* }}} */
/* {{{ Mouse */

void
termio_event_feed_mouse_in(Evas_Object *obj)
{
   Evas *e;
   Termio *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN(sd);
   e = evas_object_evas_get(obj);
   evas_event_feed_mouse_in(e, 0, NULL);
}

static void
_imf_cursor_set(Termio *sd)
{
   /* TODO */
   Evas_Coord cx, cy, cw, ch;
   evas_object_geometry_get(sd->cursor.obj, &cx, &cy, &cw, &ch);
   if (sd->khdl.imf)
     ecore_imf_context_cursor_location_set(sd->khdl.imf, cx, cy, cw, ch);
   if (sd->khdl.imf) ecore_imf_context_cursor_position_set
     (sd->khdl.imf, (sd->cursor.y * sd->grid.w) + sd->cursor.x);
   /*
    ecore_imf_context_cursor_position_set(sd->imf, 0); // how to get it?
    */
}

static void
_smart_cb_focus_in(void *data, Evas *e EINA_UNUSED,
                   Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (sd->config->disable_cursor_blink)
     edje_object_signal_emit(sd->cursor.obj, "focus,in,noblink", "terminology");
   else
     edje_object_signal_emit(sd->cursor.obj, "focus,in", "terminology");
   if (!sd->win) return;
   elm_win_keyboard_mode_set(sd->win, ELM_WIN_KEYBOARD_TERMINAL);
   if (sd->khdl.imf)
     {
        ecore_imf_context_input_panel_show(sd->khdl.imf);
        ecore_imf_context_reset(sd->khdl.imf);
        ecore_imf_context_focus_in(sd->khdl.imf);
        _imf_cursor_set(sd);
     }
}

static void
_smart_cb_focus_out(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
                    void *event EINA_UNUSED)
{
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   edje_object_signal_emit(sd->cursor.obj, "focus,out", "terminology");
   if (!sd->win) return;
   sd->pty->selection.last_click = 0;
   elm_win_keyboard_mode_set(sd->win, ELM_WIN_KEYBOARD_OFF);
   if (sd->khdl.imf)
     {
        ecore_imf_context_reset(sd->khdl.imf);
        _imf_cursor_set(sd);
        ecore_imf_context_focus_out(sd->khdl.imf);
        ecore_imf_context_input_panel_hide(sd->khdl.imf);
     }
   if (!sd->ctxpopup)
     _remove_links(sd, obj);
   term_unfocus(sd->term);
}

static void
_smart_mouseover_apply(Evas_Object *obj)
{
   char *s;
   int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
   Eina_Bool same_link = EINA_FALSE, same_geom = EINA_FALSE;
   Termio *sd = evas_object_smart_data_get(obj);
   Config *config;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   config = sd->config;
   if (!config->active_links) return;

   if ((sd->mouse.cx < 0) || (sd->mouse.cy < 0) ||
       (sd->link.suspend) || (!evas_object_focus_get(obj)))
     {
        _remove_links(sd, obj);
        return;
     }

   s = _termio_link_find(obj, sd->mouse.cx, sd->mouse.cy,
                         &x1, &y1, &x2, &y2);
   if (!s)
     {
        _remove_links(sd, obj);
        return;
     }

   if ((sd->link.string) && (!strcmp(sd->link.string, s)))
     same_link = EINA_TRUE;
   if (sd->link.string) free(sd->link.string);
   sd->link.string = s;

   if ((x1 == sd->link.x1) && (y1 == sd->link.y1) &&
       (x2 == sd->link.x2) && (y2 == sd->link.y2))
     same_geom = EINA_TRUE;
   if (((sd->link.suspend != 0) && (sd->link.objs)) ||
       ((sd->link.suspend == 0) && (!sd->link.objs)))
     same_geom = EINA_FALSE;
   sd->link.x1 = x1;
   sd->link.y1 = y1;
   sd->link.x2 = x2;
   sd->link.y2 = y2;
   _update_link(obj, sd, same_link, same_geom);
}

static void
_smart_xy_to_cursor(Termio *sd, Evas_Coord x, Evas_Coord y,
                    int *cx, int *cy)
{
   Evas_Coord ox, oy;
   EINA_SAFETY_ON_NULL_RETURN(sd);

   evas_object_geometry_get(sd->self, &ox, &oy, NULL, NULL);
   *cx = (x - ox) / sd->font.chw;
   *cy = (y - oy) / sd->font.chh;
   if (*cx < 0) *cx = 0;
   else if (*cx >= sd->grid.w) *cx = sd->grid.w - 1;
   if (*cy < 0) *cy = 0;
   else if (*cy >= sd->grid.h) *cy = sd->grid.h - 1;
}

static Eina_Bool
_rep_mouse_down(Termio *sd, Evas_Event_Mouse_Down *ev, int cx, int cy)
{
   char buf[64];
   Eina_Bool ret = EINA_FALSE;
   int btn;

   if (sd->pty->mouse_mode == MOUSE_OFF) return EINA_FALSE;
   if (!sd->mouse.button)
     {
        /* Need to remember the first button pressed for terminal handling */
        sd->mouse.button = ev->button;
     }

   btn = ev->button - 1;
   switch (sd->pty->mouse_ext)
     {
      case MOUSE_EXT_NONE:
        if ((cx < (0xff - ' ')) && (cy < (0xff - ' ')))
          {
             if (sd->pty->mouse_mode == MOUSE_X10)
               {
                  if (btn <= 2)
                    {
                       buf[0] = 0x1b;
                       buf[1] = '[';
                       buf[2] = 'M';
                       buf[3] = btn + ' ';
                       buf[4] = cx + 1 + ' ';
                       buf[5] = cy + 1 + ' ';
                       buf[6] = 0;
                       termpty_write(sd->pty, buf, strlen(buf));
                       ret = EINA_TRUE;
                    }
               }
             else
               {
                  int meta = evas_key_modifier_is_set(ev->modifiers, "Alt") ? 8 : 0;

                  if (btn > 2) btn = 0;
                  buf[0] = 0x1b;
                  buf[1] = '[';
                  buf[2] = 'M';
                  buf[3] = (btn | meta) + ' ';
                  buf[4] = cx + 1 + ' ';
                  buf[5] = cy + 1 + ' ';
                  buf[6] = 0;
                  termpty_write(sd->pty, buf, strlen(buf));
                  ret = EINA_TRUE;
               }
          }
        break;
      case MOUSE_EXT_UTF8: // ESC.[.M.BTN/FLGS.XUTF8.YUTF8
          {
             int meta = evas_key_modifier_is_set(ev->modifiers, "Alt") ? 8 : 0;
             int v, i;

             if (btn > 2) btn = 0;
             buf[0] = 0x1b;
             buf[1] = '[';
             buf[2] = 'M';
             buf[3] = (btn | meta) + ' ';
             i = 4;
             v = cx + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             v = cy + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             buf[i] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_SGR: // ESC.[.<.NUM.;.NUM.;.NUM.M
          {
             int meta = evas_key_modifier_is_set(ev->modifiers, "Alt") ? 8 : 0;

             if (btn > 2) btn = 0;
             snprintf(buf, sizeof(buf), "%c[<%i;%i;%iM", 0x1b,
                      (btn | meta), cx + 1, cy + 1);
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_URXVT: // ESC.[.NUM.;.NUM.;.NUM.M
          {
             int meta = evas_key_modifier_is_set(ev->modifiers, "Alt") ? 8 : 0;

             if (btn > 2) btn = 0;
             snprintf(buf, sizeof(buf), "%c[%i;%i;%iM", 0x1b,
                      (btn | meta) + ' ',
                      cx + 1, cy + 1);
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      default:
        break;
     }
   return ret;
}

static Eina_Bool
_rep_mouse_up(Termio *sd, Evas_Event_Mouse_Up *ev, int cx, int cy)
{
   char buf[64];
   Eina_Bool ret = EINA_FALSE;
   int meta;

   if ((sd->pty->mouse_mode == MOUSE_OFF) ||
       (sd->pty->mouse_mode == MOUSE_X10))
     return EINA_FALSE;
   if (sd->mouse.button == ev->button)
     sd->mouse.button = 0;

   meta = evas_key_modifier_is_set(ev->modifiers, "Alt") ? 8 : 0;

   switch (sd->pty->mouse_ext)
     {
      case MOUSE_EXT_NONE:
        if ((cx < (0xff - ' ')) && (cy < (0xff - ' ')))
          {
             buf[0] = 0x1b;
             buf[1] = '[';
             buf[2] = 'M';
             buf[3] = (3 | meta) + ' ';
             buf[4] = cx + 1 + ' ';
             buf[5] = cy + 1 + ' ';
             buf[6] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_UTF8: // ESC.[.M.BTN/FLGS.XUTF8.YUTF8
          {
             int v, i;

             buf[0] = 0x1b;
             buf[1] = '[';
             buf[2] = 'M';
             buf[3] = (3 | meta) + ' ';
             i = 4;
             v = cx + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             v = cy + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             buf[i] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_SGR: // ESC.[.<.NUM.;.NUM.;.NUM.m
          {
             int btn = ev->button - 1;
             if (btn > 2) btn = 0;
             snprintf(buf, sizeof(buf), "%c[<%i;%i;%im", 0x1b,
                      (btn | meta), cx + 1, cy + 1);
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_URXVT: // ESC.[.NUM.;.NUM.;.NUM.M
          {
             snprintf(buf, sizeof(buf), "%c[%i;%i;%iM", 0x1b,
                      (3 | meta) + ' ',
                      cx + 1, cy + 1);
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      default:
        break;
     }
   return ret;
}

static Eina_Bool
_rep_mouse_move(Termio *sd, Evas_Event_Mouse_Move *ev, int cx, int cy)
{
   char buf[64];
   Eina_Bool ret = EINA_FALSE;
   int btn, meta;

   if ((sd->pty->mouse_mode == MOUSE_OFF) ||
       (sd->pty->mouse_mode == MOUSE_X10) ||
       (sd->pty->mouse_mode == MOUSE_NORMAL))
     return EINA_FALSE;

   if ((!sd->mouse.button) && (sd->pty->mouse_mode == MOUSE_NORMAL_BTN_MOVE))
     return EINA_FALSE;

   btn = sd->mouse.button - 1;
   meta = evas_key_modifier_is_set(ev->modifiers, "Alt") ? 8 : 0;

   switch (sd->pty->mouse_ext)
     {
      case MOUSE_EXT_NONE:
        if ((cx < (0xff - ' ')) && (cy < (0xff - ' ')))
          {
             if (btn > 2) btn = 0;
             buf[0] = 0x1b;
             buf[1] = '[';
             buf[2] = 'M';
             buf[3] = (btn | meta | 32) + ' ';
             buf[4] = cx + 1 + ' ';
             buf[5] = cy + 1 + ' ';
             buf[6] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_UTF8: // ESC.[.M.BTN/FLGS.XUTF8.YUTF8
          {
             int v, i;

             if (btn > 2) btn = 0;
             buf[0] = 0x1b;
             buf[1] = '[';
             buf[2] = 'M';
             buf[3] = (btn | meta | 32) + ' ';
             i = 4;
             v = cx + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             v = cy + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             buf[i] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_SGR: // ESC.[.<.NUM.;.NUM.;.NUM.M
          {
             snprintf(buf, sizeof(buf), "%c[<%i;%i;%iM", 0x1b,
                      (btn | meta | 32), cx + 1, cy + 1);
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_URXVT: // ESC.[.NUM.;.NUM.;.NUM.M
          {
             if (btn > 2) btn = 0;
             snprintf(buf, sizeof(buf), "%c[%i;%i;%iM", 0x1b,
                      (btn | meta | 32) + ' ',
                      cx + 1, cy + 1);
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      default:
        break;
     }
   return ret;
}

static Eina_Bool
_smart_mouseover_delay(void *data)
{
   Termio *sd = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EINA_FALSE);
   sd->mouseover_delay = NULL;
   _smart_mouseover_apply(data);
   return EINA_FALSE;
}


static void
_smart_cb_mouse_move_job(void *data)
{
   Termio *sd = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN(sd);
   sd->mouse_move_job = NULL;
   if (sd->mouseover_delay)
     ecore_timer_reset(sd->mouseover_delay);
   else
     sd->mouseover_delay = ecore_timer_add(0.05, _smart_mouseover_delay, data);
}

static void
_edje_cb_bottom_right_in(void *data, Evas_Object *obj EINA_UNUSED,
                         const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Termio *sd = data;

   sd->bottom_right = EINA_TRUE;
}

static void
_edje_cb_top_left_in(void *data, Evas_Object *obj EINA_UNUSED,
                     const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Termio *sd = data;

   sd->top_left = EINA_TRUE;
}

static void
_edje_cb_bottom_right_out(void *data, Evas_Object *obj EINA_UNUSED,
                          const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Termio *sd = data;

   sd->bottom_right = EINA_FALSE;
}

static void
_edje_cb_top_left_out(void *data, Evas_Object *obj EINA_UNUSED,
                      const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Termio *sd = data;

   sd->top_left = EINA_FALSE;
}

static void
_handle_mouse_down_single_click(Termio *sd,
                                int cx, int cy,
                                int ctrl, int alt, int shift)
{
   sd->didclick = EINA_FALSE;
   /* SINGLE CLICK */
   if (sd->pty->selection.is_active &&
       (sd->top_left || sd->bottom_right))
     {
        /* stretch selection */
        int start_x, start_y, end_x, end_y;

        start_x = sd->pty->selection.start.x;
        start_y = sd->pty->selection.start.y;
        end_x   = sd->pty->selection.end.x;
        end_y   = sd->pty->selection.end.y;

        if (!sd->pty->selection.is_top_to_bottom)
          {
             INT_SWAP(start_y, end_y);
             INT_SWAP(start_x, end_x);
          }

        cy -= sd->scroll;

        sd->pty->selection.makesel = EINA_TRUE;

        if (sd->pty->selection.is_box)
          {
             if (end_x < start_x)
               INT_SWAP(end_x, start_x);
          }
        if (sd->top_left)
          {
             sd->pty->selection.orig.x = end_x;
             sd->pty->selection.orig.y = end_y;
             sd->pty->selection.is_top_to_bottom = EINA_FALSE;
          }
        else
          {
             /* sd->bottom_right */
             sd->pty->selection.orig.x = start_x;
             sd->pty->selection.orig.y = start_y;
             sd->pty->selection.is_top_to_bottom = EINA_TRUE;
          }

        sd->pty->selection.start.x = sd->pty->selection.orig.x;
        sd->pty->selection.start.y = sd->pty->selection.orig.y;
        sd->pty->selection.end.x = cx;
        sd->pty->selection.end.y = cy;
        _selection_dbl_fix(sd);
     }
   else if (!shift && !sd->pty->selection.is_active)
     {
        /* New selection */
        sd->moved = EINA_FALSE;
        _sel_set(sd, EINA_FALSE);
        sd->pty->selection.is_box = (ctrl || alt);
        sd->pty->selection.start.x = cx;
        sd->pty->selection.start.y = cy - sd->scroll;
        sd->pty->selection.orig.x = sd->pty->selection.start.x;
        sd->pty->selection.orig.y = sd->pty->selection.start.y;
        sd->pty->selection.end.x = cx;
        sd->pty->selection.end.y = cy - sd->scroll;
        sd->pty->selection.makesel = EINA_TRUE;
        sd->pty->selection.by_line = EINA_FALSE;
        sd->pty->selection.by_word = EINA_FALSE;
        _selection_dbl_fix(sd);
     }
   else if (shift && sd->pty->selection.is_active)
     {
        /* let cb_up handle it */
        /* do nothing */
        return;
     }
   else if (shift &&
            (time(NULL) - sd->pty->selection.last_click) <= 60)
     {
        sd->pty->selection.is_box = ctrl;
        _sel_to(sd, cx, cy - sd->scroll, EINA_FALSE);
        sd->pty->selection.is_active = EINA_TRUE;
        _selection_dbl_fix(sd);
     }
   else
     {
        sd->pty->selection.is_box = ctrl;
        sd->pty->selection.start.x = sd->pty->selection.end.x = cx;
        sd->pty->selection.orig.x = cx;
        sd->pty->selection.start.y = sd->pty->selection.end.y = cy - sd->scroll;
        sd->pty->selection.orig.y = cy - sd->scroll;
        sd->pty->selection.makesel = EINA_TRUE;
        sd->didclick = !sd->pty->selection.is_active;
        sd->pty->selection.is_active = EINA_FALSE;
        _sel_set(sd, EINA_FALSE);
     }
}

static void
_cb_ctxp_sel_copy(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Termio *sd = evas_object_smart_data_get(term);

   EINA_SAFETY_ON_NULL_RETURN(sd);

   termio_take_selection(data, ELM_SEL_TYPE_CLIPBOARD);

   sd->ctxpopup = NULL;
   evas_object_del(obj);
}

static void
_cb_ctxp_sel_open_as_url(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Termio *sd = evas_object_smart_data_get(term);
   char buf[PATH_MAX], *s = NULL, *escaped = NULL;
   const char *cmd;
   const char *prefix = "http://";
   Config *config;
   Eina_Strbuf *sb = NULL;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   config = sd->config;

   termio_take_selection(data, ELM_SEL_TYPE_PRIMARY);

   if (!sd->have_sel || !sd->sel_str)
     goto end;

   if (!(config->helper.url.general) ||
       !(config->helper.url.general[0]))
     goto end;
   cmd = config->helper.url.general;

   sb = eina_strbuf_new();
   if (!sb)
     goto end;
   eina_strbuf_append(sb, sd->sel_str);
   eina_strbuf_trim(sb);

   s = eina_str_escape(eina_strbuf_string_get(sb));
   if (!s)
     goto end;
   if (casestartswith(s, "http://") ||
        casestartswith(s, "https://") ||
        casestartswith(s, "ftp://") ||
        casestartswith(s, "mailto:"))
     prefix = "";

   escaped = ecore_file_escape_name(s);
   if (!escaped)
     goto end;

   snprintf(buf, sizeof(buf), "%s %s%s", cmd, prefix, escaped);

   WRN("trying to launch '%s'", buf);
   ecore_exe_run(buf, NULL);

end:
   eina_strbuf_free(sb);
   free(escaped);
   free(s);
   sd->ctxpopup = NULL;
   evas_object_del(obj);
}


static void
_handle_right_click(Evas_Object *obj, Evas_Event_Mouse_Down *ev, Termio *sd,
                    int cx, int cy)
{
   if (_mouse_in_selection(sd, cx, cy))
     {
        Evas_Object *ctxp;
        ctxp = elm_ctxpopup_add(sd->win);
        sd->ctxpopup = ctxp;

        elm_ctxpopup_item_append(ctxp, _("Copy"), NULL,
                                 _cb_ctxp_sel_copy, sd->self);
        elm_ctxpopup_item_append(ctxp, _("Open as URL"), NULL,
                                 _cb_ctxp_sel_open_as_url, sd->self);
        evas_object_move(ctxp, ev->canvas.x, ev->canvas.y);
        evas_object_show(ctxp);
        evas_object_smart_callback_add(ctxp, "dismissed",
                                       _cb_ctxp_dismissed, sd);
        evas_object_event_callback_add(ctxp, EVAS_CALLBACK_DEL,
                                       _cb_ctxp_del, sd);
        return;
     }
   if (!sd->link.string)
     evas_object_smart_callback_call(obj, "options", NULL);
}

static void
_smart_cb_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Mouse_Down *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   int cx = 0, cy = 0;
   int shift, ctrl, alt;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   alt = evas_key_modifier_is_set(ev->modifiers, "Alt");
   _smart_xy_to_cursor(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);

   if ((ev->button == 3) && ctrl)
     {
        _handle_right_click(data, ev, sd, cx, cy);
        return;
     }
   if (!shift && !ctrl)
     if (_rep_mouse_down(sd, ev, cx, cy))
       {
           if (sd->pty->selection.is_active)
             {
                _sel_set(sd, EINA_FALSE);
                _smart_update_queue(data, sd);
             }
          return;
       }
   if (ev->button == 1)
     {
        sd->pty->selection.makesel = EINA_TRUE;
        if (ev->flags & EVAS_BUTTON_TRIPLE_CLICK)
          {
             if (shift && sd->pty->selection.is_active)
               _sel_line_to(sd, cy - sd->scroll, EINA_TRUE);
             else
               _sel_line(sd, cy - sd->scroll);
             if (sd->pty->selection.is_active)
               {
                  termio_take_selection(data, ELM_SEL_TYPE_PRIMARY);
               }
             sd->didclick = EINA_TRUE;
          }
        else if (ev->flags & EVAS_BUTTON_DOUBLE_CLICK)
          {
             if (!sd->pty->selection.is_active && sd->didclick)
               sd->pty->selection.is_active = EINA_TRUE;
             if (shift && sd->pty->selection.is_active)
               _sel_word_to(sd, cx, cy - sd->scroll, EINA_TRUE);
             else
               _sel_word(sd, cx, cy - sd->scroll);
             if (sd->pty->selection.is_active)
               {
                  if (!termio_take_selection(data, ELM_SEL_TYPE_PRIMARY))
                    _sel_set(sd, EINA_FALSE);
               }
             sd->didclick = EINA_TRUE;
          }
        else
          {
             _handle_mouse_down_single_click(sd, cx, cy, ctrl, alt, shift);
          }
        _smart_update_queue(data, sd);
     }
   else if (ev->button == 2)
     {
        termio_paste_selection(data, ELM_SEL_TYPE_PRIMARY);
     }
   else if (ev->button == 3)
     {
        _handle_right_click(data, ev, sd, cx, cy);
     }
}

static void
_smart_cb_mouse_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Mouse_Up *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   int cx = 0, cy = 0;
   int shift, ctrl;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");

   _smart_xy_to_cursor(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);
   if (!shift && !ctrl && !sd->pty->selection.makesel)
      if (_rep_mouse_up(sd, ev, cx, cy))
        {
           if (sd->pty->selection.is_active)
             {
                _sel_set(sd, EINA_FALSE);
                _smart_update_queue(data, sd);
             }
           return;
        }
   if (sd->link.down.dnd) return;
   if (sd->pty->selection.makesel)
     {
        if (sd->mouse_selection_scroll_timer)
          {
             ecore_timer_del(sd->mouse_selection_scroll_timer);
             sd->mouse_selection_scroll_timer = NULL;
          }
        sd->pty->selection.makesel = EINA_FALSE;

        if (!sd->pty->selection.is_active)
          {
             /* Only change the end position */
             if (((sd->pty->selection.start.x == sd->pty->selection.end.x) &&
                  (sd->pty->selection.start.y == sd->pty->selection.end.y)))
               {
                  _sel_set(sd, EINA_FALSE);
                  sd->didclick = EINA_FALSE;
                  sd->pty->selection.last_click = time(NULL);
                  sd->pty->selection.by_line = EINA_FALSE;
                  sd->pty->selection.by_word = EINA_FALSE;
                  _smart_update_queue(data, sd);
                  return;
               }
          }
        else
          {
             if (sd->pty->selection.by_line)
               {
                  _sel_line_to(sd, cy - sd->scroll, shift);
               }
             else if (sd->pty->selection.by_word)
               {
                  _sel_word_to(sd, cx, cy - sd->scroll, shift);
               }
             else
               {
                  if (shift)
                    {
                       /* extend selection */
                       _sel_to(sd, cx, cy - sd->scroll, EINA_TRUE);
                    }
                  else
                    {
                       sd->didclick = EINA_TRUE;
                       _sel_to(sd, cx, cy - sd->scroll, EINA_FALSE);
                    }
               }
             _selection_dbl_fix(sd);
             _selection_newline_extend_fix(data);
             _smart_update_queue(data, sd);
             termio_take_selection(data, ELM_SEL_TYPE_PRIMARY);
             sd->pty->selection.makesel = EINA_FALSE;
          }
     }
}

static Eina_Bool
_mouse_selection_scroll(void *data)
{
   Evas_Object *obj = data;
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord oy, my;
   int cy;
   float fcy;

   if (!sd->pty->selection.makesel) return EINA_FALSE;

   evas_pointer_canvas_xy_get(evas_object_evas_get(obj), NULL, &my);
   evas_object_geometry_get(data, NULL, &oy, NULL, NULL);
   fcy = (my - oy) / (float)sd->font.chh;
   cy = fcy;
   if (fcy < 0.3)
     {
        if (cy == 0)
          cy = -1;
        sd->scroll -= cy;
        sd->pty->selection.end.y = -sd->scroll;
        _smart_update_queue(data, sd);
     }
   else if (fcy >= (sd->grid.h - 0.3))
     {
        if (cy <= sd->grid.h)
          cy = sd->grid.h + 1;
        sd->scroll -= cy - sd->grid.h;
        if (sd->scroll < 0) sd->scroll = 0;
        sd->pty->selection.end.y = sd->scroll + sd->grid.h - 1;
        _smart_update_queue(data, sd);
     }

   return EINA_TRUE;
}

static void
_smart_cb_mouse_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Mouse_Move *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   int cx, cy;
   float fcy;
   Evas_Coord ox, oy;
   Eina_Bool scroll = EINA_FALSE;
   int shift, ctrl;

   shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");

   EINA_SAFETY_ON_NULL_RETURN(sd);

   evas_object_geometry_get(data, &ox, &oy, NULL, NULL);
   cx = (ev->cur.canvas.x - ox) / sd->font.chw;
   fcy = (ev->cur.canvas.y - oy) / (float)sd->font.chh;
   cy = fcy;
   if (cx < 0) cx = 0;
   else if (cx >= sd->grid.w) cx = sd->grid.w - 1;
   if (fcy < 0.3)
     {
        cy = 0;
        if (sd->pty->selection.makesel)
             scroll = EINA_TRUE;
     }
   else if (fcy >= (sd->grid.h - 0.3))
     {
        cy = sd->grid.h - 1;
        if (sd->pty->selection.makesel)
             scroll = EINA_TRUE;
     }
   if (scroll == EINA_TRUE)
     {
        if (!sd->mouse_selection_scroll_timer)
          {
             sd->mouse_selection_scroll_timer
                = ecore_timer_add(0.05, _mouse_selection_scroll, data);
          }
        return;
     }
   else if (sd->mouse_selection_scroll_timer)
     {
        ecore_timer_del(sd->mouse_selection_scroll_timer);
        sd->mouse_selection_scroll_timer = NULL;
     }

   if ((sd->mouse.cx == cx) && (sd->mouse.cy == cy)) return;

   sd->mouse.cx = cx;
   sd->mouse.cy = cy;
   if (!shift && !ctrl)
     if (_rep_mouse_move(sd, ev, cx, cy)) return;
   if (sd->link.down.dnd)
     {
        sd->pty->selection.makesel = EINA_FALSE;
        _sel_set(sd, EINA_FALSE);
        _smart_update_queue(data, sd);
        return;
     }
   if (sd->pty->selection.makesel)
     {
        int start_x, start_y;

        /* Only change the end position */
        start_x = sd->pty->selection.start.x;
        start_y = sd->pty->selection.start.y;

        if (!sd->pty->selection.is_active)
          {
             if ((cx != start_x) ||
                 ((cy - sd->scroll) != start_y))
               _sel_set(sd, EINA_TRUE);
          }

        if (sd->pty->selection.by_line)
          {
             _sel_line_to(sd, cy - sd->scroll, EINA_FALSE);
          }
        else if (sd->pty->selection.by_word)
          {
             _sel_word_to(sd, cx, cy - sd->scroll, EINA_FALSE);
          }
        else
          {
             _sel_to(sd, cx, cy - sd->scroll, EINA_FALSE);
          }

        _selection_dbl_fix(sd);
        if (!sd->pty->selection.is_box)
          _selection_newline_extend_fix(data);
        _smart_update_queue(data, sd);
        sd->moved = EINA_TRUE;
     }
   /* TODO: make the following useless */
   if (sd->mouse_move_job) ecore_job_del(sd->mouse_move_job);
   sd->mouse_move_job = ecore_job_add(_smart_cb_mouse_move_job, data);
}

static void
_smart_cb_mouse_in(void *data, Evas *e EINA_UNUSED,
                   Evas_Object *obj EINA_UNUSED, void *event)
{
   int cx = 0, cy = 0;
   Evas_Event_Mouse_In *ev = event;
   Termio *sd = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN(sd);
   _smart_xy_to_cursor(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);
   sd->mouse.cx = cx;
   sd->mouse.cy = cy;
   termio_mouseover_suspend_pushpop(data, -1);
}

static void
_smart_cb_mouse_out(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
                    void *event)
{
   Termio *sd = evas_object_smart_data_get(data);
   Evas_Event_Mouse_Out *ev = event;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (sd->ctxpopup) return; /* ctxp triggers mouse out we should ignore */

   termio_mouseover_suspend_pushpop(data, 1);
   ty_dbus_link_hide();
   if ((ev->canvas.x == 0) || (ev->canvas.y == 0))
     {
        sd->mouse.cx = -1;
        sd->mouse.cy = -1;
        sd->link.suspend = EINA_FALSE;
     }
   else
     {
        int cx = 0, cy = 0;

        _smart_xy_to_cursor(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);
        sd->mouse.cx = cx;
        sd->mouse.cy = cy;
     }
   _remove_links(sd, obj);

   if (sd->mouseover_delay) ecore_timer_del(sd->mouseover_delay);
   sd->mouseover_delay = NULL;
}

static void
_smart_cb_mouse_wheel(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Mouse_Wheel *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   char buf[64];

   EINA_SAFETY_ON_NULL_RETURN(sd);

   /* do not handle horizontal scrolling */
   if (ev->direction) return;

   if (evas_key_modifier_is_set(ev->modifiers, "Control")) return;
   if (evas_key_modifier_is_set(ev->modifiers, "Alt")) return;
   if (evas_key_modifier_is_set(ev->modifiers, "Shift")) return;

   if (sd->pty->mouse_mode == MOUSE_OFF)
     {
        if (sd->pty->altbuf)
          {
             /* Emulate cursors */
             buf[0] = 0x1b;
             buf[1] = 'O';
             buf[2] = (ev->z < 0) ? 'A' : 'B';
             buf[3] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
          }
        else
          {
             sd->scroll -= (ev->z * 4);
             if (sd->scroll < 0)
               sd->scroll = 0;
             _smart_update_queue(data, sd);
             miniview_position_offset(term_miniview_get(sd->term),
                                      ev->z * 4, EINA_TRUE);

             _smart_cb_mouse_move_job(data);
          }
     }
   else
     {
       int cx = 0, cy = 0;

       _smart_xy_to_cursor(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);

       switch (sd->pty->mouse_ext)
         {
          case MOUSE_EXT_NONE:
            if ((cx < (0xff - ' ')) && (cy < (0xff - ' ')))
              {
                 int btn = (ev->z >= 0) ? 1 + 64 : 64;

                 buf[0] = 0x1b;
                 buf[1] = '[';
                 buf[2] = 'M';
                 buf[3] = btn + ' ';
                 buf[4] = cx + 1 + ' ';
                 buf[5] = cy + 1 + ' ';
                 buf[6] = 0;
                 termpty_write(sd->pty, buf, strlen(buf));
              }
            break;
          case MOUSE_EXT_UTF8: // ESC.[.M.BTN/FLGS.XUTF8.YUTF8
              {
                 int v, i;
                 int btn = (ev->z >= 0) ? 'a' : '`';

                 buf[0] = 0x1b;
                 buf[1] = '[';
                 buf[2] = 'M';
                 buf[3] = btn;
                 i = 4;
                 v = cx + 1 + ' ';
                 if (v <= 127) buf[i++] = v;
                 else
                   { // 14 bits for cx/cy - enough i think
                       buf[i++] = 0xc0 + (v >> 6);
                       buf[i++] = 0x80 + (v & 0x3f);
                   }
                 v = cy + 1 + ' ';
                 if (v <= 127) buf[i++] = v;
                 else
                   { // 14 bits for cx/cy - enough i think
                       buf[i++] = 0xc0 + (v >> 6);
                       buf[i++] = 0x80 + (v & 0x3f);
                   }
                 buf[i] = 0;
                 termpty_write(sd->pty, buf, strlen(buf));
              }
            break;
          case MOUSE_EXT_SGR: // ESC.[.<.NUM.;.NUM.;.NUM.M
              {
                 int btn = (ev->z >= 0) ? 1 + 64 : 64;
                 snprintf(buf, sizeof(buf), "%c[<%i;%i;%iM", 0x1b,
                          btn, cx + 1, cy + 1);
                 termpty_write(sd->pty, buf, strlen(buf));
              }
            break;
          case MOUSE_EXT_URXVT: // ESC.[.NUM.;.NUM.;.NUM.M
              {
                 int btn = (ev->z >= 0) ? 1 + 64 : 64;
                 snprintf(buf, sizeof(buf), "%c[%i;%i;%iM", 0x1b,
                          btn + ' ',
                          cx + 1, cy + 1);
                 termpty_write(sd->pty, buf, strlen(buf));
              }
            break;
          default:
            break;
         }
     }
}

/* }}} */
/* {{{ Gestures */

static Evas_Event_Flags
_smart_cb_gest_long_move(void *data, void *event EINA_UNUSED)
{
//   Elm_Gesture_Taps_Info *p = event;
   Termio *sd = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EVAS_EVENT_FLAG_ON_HOLD);
   evas_object_smart_callback_call(data, "options", NULL);
   sd->didclick = EINA_TRUE;
   return EVAS_EVENT_FLAG_ON_HOLD;
}

static Evas_Event_Flags
_smart_cb_gest_zoom_start(void *data, void *event)
{
   Elm_Gesture_Zoom_Info *p = event;
   Termio *sd = evas_object_smart_data_get(data);
   Config *config;

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EVAS_EVENT_FLAG_ON_HOLD);
   config = sd->config;
   if (config)
     {
        sd->gesture_zoom_start_size = (double)config->font.size;
        int sz = (double)config->font.size * p->zoom;
        sd->zoom_fontsize_start = config->font.size;
        if (sz != config->font.size)
          win_font_size_set(term_win_get(sd->term), sz);
     }
   sd->didclick = EINA_TRUE;
   return EVAS_EVENT_FLAG_ON_HOLD;
}

static Evas_Event_Flags
_smart_cb_gest_zoom_move(void *data, void *event)
{
   Elm_Gesture_Zoom_Info *p = event;
   Termio *sd = evas_object_smart_data_get(data);
   Config *config;

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EVAS_EVENT_FLAG_ON_HOLD);
   config = sd->config;
   if (config)
     {
        int sz = sd->gesture_zoom_start_size * p->zoom;
        if (sz != config->font.size)
          win_font_size_set(term_win_get(sd->term), sz);
     }
   sd->didclick = EINA_TRUE;
   return EVAS_EVENT_FLAG_ON_HOLD;
}

static Evas_Event_Flags
_smart_cb_gest_zoom_end(void *data, void *event)
{
   Elm_Gesture_Zoom_Info *p = event;
   Termio *sd = evas_object_smart_data_get(data);
   Config *config;

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EVAS_EVENT_FLAG_ON_HOLD);
   config = sd->config;
   if (config)
     {
        int sz = sd->gesture_zoom_start_size * p->zoom;
        sd->gesture_zoom_start_size = 0.0;
        if (sz != config->font.size)
          win_font_size_set(term_win_get(sd->term), sz);
     }
   sd->didclick = EINA_TRUE;
   return EVAS_EVENT_FLAG_ON_HOLD;
}

static Evas_Event_Flags
_smart_cb_gest_zoom_abort(void *data, void *event EINA_UNUSED)
{
   Termio *sd = evas_object_smart_data_get(data);
   Config *config;

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EVAS_EVENT_FLAG_ON_HOLD);
   config = sd->config;
   if (config)
     {
        if (sd->zoom_fontsize_start != config->font.size)
          win_font_size_set(term_win_get(sd->term), sd->zoom_fontsize_start);
     }
   sd->didclick = EINA_TRUE;
   return EVAS_EVENT_FLAG_ON_HOLD;
}

/* }}} */
/* {{{ Smart */

static void
_smart_apply(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh;
   Eina_List *l, *ln;
   Termblock *blk;
   int x, y, ch1 = 0, ch2 = 0, inv = 0, preedit_x = 0, preedit_y = 0;
   ssize_t w;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);

   EINA_LIST_FOREACH(sd->pty->block.active, l, blk)
     {
        blk->was_active = blk->active;
        blk->active = EINA_FALSE;
     }
   inv = sd->pty->termstate.reverse;
   termpty_backlog_lock();
   termpty_backscroll_adjust(sd->pty, &sd->scroll);
   for (y = 0; y < sd->grid.h; y++)
     {
        Termcell *cells;
        Evas_Textgrid_Cell *tc;

        w = 0;
        cells = termpty_cellrow_get(sd->pty, y - sd->scroll, &w);
        tc = evas_object_textgrid_cellrow_get(sd->grid.obj, y);
        if (!tc) continue;
        ch1 = -1;
        for (x = 0; x < sd->grid.w; x++)
          {
             if ((!cells) || (x >= w))
               {
                  if ((tc[x].codepoint != 0) ||
                      (tc[x].bg != COL_INVIS) ||
                      (tc[x].bg_extended))
                    {
                       if (ch1 < 0) ch1 = x;
                       ch2 = x;
                    }
                  tc[x].codepoint = 0;
                  if (inv) tc[x].bg = COL_INVERSEBG;
                  else tc[x].bg = COL_INVIS;
                  tc[x].bg_extended = 0;
                  tc[x].underline = 0;
                  tc[x].strikethrough = 0;
                  tc[x].bold = 0;
                  tc[x].italic = 0;
                  tc[x].double_width = 0;
               }
             else
               {
                  int bid, bx = 0, by = 0;

                  bid = termpty_block_id_get(&(cells[x]), &bx, &by);
                  if (bid >= 0)
                    {
                       if (ch1 < 0) ch1 = x;
                       ch2 = x;
                       tc[x].codepoint = 0;
                       tc[x].fg_extended = 0;
                       tc[x].bg_extended = 0;
                       tc[x].underline = 0;
                       tc[x].strikethrough = 0;
                       tc[x].bold = 0;
                       tc[x].italic = 0;
                       tc[x].double_width = 0;
                       tc[x].fg = COL_INVIS;
                       tc[x].bg = COL_INVIS;
                       blk = termpty_block_get(sd->pty, bid);
                       if (blk)
                         {
                            _block_activate(obj, blk);
                            blk->x = (x - bx);
                            blk->y = (y - by);
                            evas_object_move(blk->obj,
                                             ox + (blk->x * sd->font.chw),
                                             oy + (blk->y * sd->font.chh));
                            evas_object_resize(blk->obj,
                                               blk->w * sd->font.chw,
                                               blk->h * sd->font.chh);
                         }
                    }
                  else if (cells[x].att.invisible)
                    {
                       if ((tc[x].codepoint != 0) ||
                           (tc[x].bg != COL_INVIS) ||
                           (tc[x].bg_extended))
                         {
                            if (ch1 < 0) ch1 = x;
                            ch2 = x;
                         }
                       tc[x].codepoint = 0;
                       if (inv) tc[x].bg = COL_INVERSEBG;
                       else tc[x].bg = COL_INVIS;
                       tc[x].bg_extended = 0;
                       tc[x].underline = 0;
                       tc[x].strikethrough = 0;
                       tc[x].bold = 0;
                       tc[x].italic = 0;
                       tc[x].double_width = cells[x].att.dblwidth;
                       if ((tc[x].double_width) && (tc[x].codepoint == 0) &&
                           (ch2 == x - 1))
                         ch2 = x;
                    }
                  else
                    {
                       int fg, bg, fgext, bgext, codepoint;

                       // colors
                       fg = cells[x].att.fg;
                       bg = cells[x].att.bg;
                       fgext = cells[x].att.fg256;
                       bgext = cells[x].att.bg256;
                       codepoint = cells[x].codepoint;

                       if ((fg == COL_DEF) && (cells[x].att.inverse ^ inv))
                         fg = COL_INVERSEBG;
                       if (bg == COL_DEF)
                         {
                            if (cells[x].att.inverse ^ inv)
                              bg = COL_INVERSE;
                            else if (!bgext)
                              bg = COL_INVIS;
                         }
                       if ((cells[x].att.fgintense) && (!fgext)) fg += 48;
                       if ((cells[x].att.bgintense) && (!bgext)) bg += 48;
                       if (cells[x].att.inverse ^ inv)
                         {
                            int t;
                            t = fgext; fgext = bgext; bgext = t;
                            t = fg; fg = bg; bg = t;
                         }
                       if ((cells[x].att.bold) && (!fgext)) fg += 12;
                       if ((cells[x].att.faint) && (!fgext)) fg += 24;
                       if ((tc[x].codepoint != codepoint) ||
                           (tc[x].fg != fg) ||
                           (tc[x].bg != bg) ||
                           (tc[x].fg_extended != fgext) ||
                           (tc[x].bg_extended != bgext) ||
                           (tc[x].underline != cells[x].att.underline) ||
                           (tc[x].strikethrough != cells[x].att.strike))
                         {
                            if (ch1 < 0) ch1 = x;
                            ch2 = x;
                         }
                       tc[x].fg_extended = fgext;
                       tc[x].bg_extended = bgext;
                       tc[x].underline = cells[x].att.underline;
                       tc[x].strikethrough = cells[x].att.strike;
                       tc[x].bold = cells[x].att.bold;
                       tc[x].italic = cells[x].att.italic;
                       tc[x].double_width = cells[x].att.dblwidth;
                       tc[x].fg = fg;
                       tc[x].bg = bg;
                       tc[x].codepoint = codepoint;
                       if ((tc[x].double_width) && (tc[x].codepoint == 0) &&
                           (ch2 == x - 1))
                         ch2 = x;
                       // cells[x].att.blink
                       // cells[x].att.blink2
                    }
               }
          }
        evas_object_textgrid_cellrow_set(sd->grid.obj, y, tc);
        /* only bothering to keep 1 change span per row - not worth doing
         * more really */
        if (ch1 >= 0)
          evas_object_textgrid_update_add(sd->grid.obj, ch1, y,
                                          ch2 - ch1 + 1, 1);
     }
   if (sd->preedit_str && sd->preedit_str[0])
     {
        Eina_Unicode *uni, g;
        int len = 0, i, jump, xx, backx;
        Eina_Bool dbl;
        Evas_Textgrid_Cell *tc;
        x = sd->cursor.x, y = sd->cursor.y;

        uni = eina_unicode_utf8_to_unicode(sd->preedit_str, &len);
        if (uni)
          {
             for (i = 0; i < len; i++)
               {
                  jump = 1;
                  g = uni[i];
                  dbl = _termpty_is_dblwidth_get(sd->pty, g);
                  if (dbl) jump = 2;
                  backx = 0;
                  if ((x + jump) > sd->grid.w)
                    {
                       if (y < (sd->grid.h - 1))
                         {
                            x = jump;
                            backx = jump;
                            y++;
                         }
                    }
                  else
                    {
                       x += jump;
                       backx = jump;
                    }
                  tc = evas_object_textgrid_cellrow_get(sd->grid.obj, y);
                  xx = x - backx;
                  tc[xx].bold = 1;
                  tc[xx].bg = COL_BLACK;
                  tc[xx].fg = COL_WHITE;
                  tc[xx].fg_extended = 0;
                  tc[xx].bg_extended = 0;
                  tc[xx].underline = 1;
                  tc[xx].strikethrough = 0;
                  tc[xx].double_width = dbl;
                  tc[xx].codepoint = g;
                  if (dbl)
                    {
                       xx = x - backx + 1;
                       tc[xx].bold = 1;
                       tc[xx].bg = COL_BLACK;
                       tc[xx].fg = COL_WHITE;
                       tc[xx].fg_extended = 0;
                       tc[xx].bg_extended = 0;
                       tc[xx].underline = 1;
                       tc[xx].strikethrough = 0;
                       tc[xx].double_width = 0;
                       tc[xx].codepoint = 0;
                    }
                  evas_object_textgrid_cellrow_set(sd->grid.obj, y, tc);
                  if (x >= sd->grid.w)
                    {
                       if (y < (sd->grid.h - 1))
                         {
                            x = 0;
                            y++;
                         }
                    }
               }
             evas_object_textgrid_update_add(sd->grid.obj, 0, sd->cursor.y,
                                             sd->grid.w, y - sd->cursor.y + 1);
          }
        preedit_x = x - sd->cursor.x;
        preedit_y = y - sd->cursor.y;
     }
   termpty_backlog_unlock();

   EINA_LIST_FOREACH_SAFE(sd->pty->block.active, l, ln, blk)
     {
        if (!blk->active)
          {
             blk->was_active = EINA_FALSE;
             _block_obj_del(blk);
             sd->pty->block.active = eina_list_remove_list
               (sd->pty->block.active, l);
          }
     }
   if ((sd->scroll != 0) || (sd->pty->termstate.hide_cursor))
     evas_object_hide(sd->cursor.obj);
   else
     evas_object_show(sd->cursor.obj);
   sd->cursor.x = sd->pty->cursor_state.cx;
   sd->cursor.y = sd->pty->cursor_state.cy;
   evas_object_move(sd->cursor.obj,
                    ox + ((sd->cursor.x + preedit_x) * sd->font.chw),
                    oy + ((sd->cursor.y + preedit_y) * sd->font.chh));
   if (sd->pty->selection.is_active)
     {
        int start_x, start_y, end_x, end_y;
        int size_top, size_bottom;

        start_x = sd->pty->selection.start.x;
        start_y = sd->pty->selection.start.y;
        end_x   = sd->pty->selection.end.x;
        end_y   = sd->pty->selection.end.y;

        if (!sd->pty->selection.is_top_to_bottom)
          {
             INT_SWAP(start_y, end_y);
             INT_SWAP(start_x, end_x);
          }
        size_top = start_x * sd->font.chw;

        size_bottom = (sd->grid.w - end_x - 1) * sd->font.chw;

        evas_object_size_hint_min_set(sd->sel.top,
                                      size_top,
                                      sd->font.chh);
        evas_object_size_hint_max_set(sd->sel.top,
                                      size_top,
                                      sd->font.chh);
        evas_object_size_hint_min_set(sd->sel.bottom,
                                      size_bottom,
                                      sd->font.chh);
        evas_object_size_hint_max_set(sd->sel.bottom,
                                      size_bottom,
                                      sd->font.chh);
        evas_object_move(sd->sel.theme,
                         ox,
                         oy + ((start_y + sd->scroll) * sd->font.chh));
        evas_object_resize(sd->sel.theme,
                           sd->grid.w * sd->font.chw,
                           (end_y + 1 - start_y) * sd->font.chh);

        if (sd->pty->selection.is_box)
          {
             edje_object_signal_emit(sd->sel.theme,
                                  "mode,oneline", "terminology");
          }
        else
          {
             if ((start_y == end_y) ||
                 ((start_x == 0) && (end_x == (sd->grid.w - 1))))
               {
                  edje_object_signal_emit(sd->sel.theme,
                                          "mode,oneline", "terminology");
               }
             else if ((start_y == (end_y - 1)) &&
                      (start_x > end_x))
               {
                  edje_object_signal_emit(sd->sel.theme,
                                          "mode,disjoint", "terminology");
               }
             else if (start_x == 0)
               {
                  edje_object_signal_emit(sd->sel.theme,
                                          "mode,topfull", "terminology");
               }
             else if (end_x == (sd->grid.w - 1))
               {
                  edje_object_signal_emit(sd->sel.theme,
                                          "mode,bottomfull", "terminology");
               }
             else
               {
                  edje_object_signal_emit(sd->sel.theme,
                                          "mode,multiline", "terminology");
               }
          }
        evas_object_show(sd->sel.theme);
     }
   else
     evas_object_hide(sd->sel.theme);
   if (sd->mouseover_delay)
     {
       ecore_timer_reset(sd->mouseover_delay);
     }
   miniview_redraw(term_miniview_get(sd->term));
}

static void
_smart_size(Evas_Object *obj, int w, int h, Eina_Bool force)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (w < 1) w = 1;
   if (h < 1) h = 1;
   if (!force)
     {
        if ((w == sd->grid.w) && (h == sd->grid.h)) return;
     }
   evas_event_freeze(evas_object_evas_get(obj));
   evas_object_textgrid_size_set(sd->grid.obj, w, h);
   sd->grid.w = w;
   sd->grid.h = h;
   evas_object_resize(sd->cursor.obj, sd->font.chw, sd->font.chh);
   evas_object_size_hint_min_set(obj, sd->font.chw, sd->font.chh);
   if (!sd->noreqsize)
     evas_object_size_hint_request_set(obj,
                                       sd->font.chw * sd->grid.w,
                                       sd->font.chh * sd->grid.h);
   _sel_set(sd, EINA_FALSE);
   termpty_resize(sd->pty, w, h);

   _smart_calculate(obj);
   _smart_apply(obj);
   evas_event_thaw(evas_object_evas_get(obj));
}

static Eina_Bool
_smart_cb_delayed_size(void *data)
{
   Evas_Object *obj = data;
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord ow = 0, oh = 0;
   int w, h;

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EINA_FALSE);
   sd->delayed_size_timer = NULL;

   evas_object_geometry_get(obj, NULL, NULL, &ow, &oh);

   w = ow / sd->font.chw;
   h = oh / sd->font.chh;
   _smart_size(obj, w, h, EINA_FALSE);
   return EINA_FALSE;
}

static Eina_Bool
_smart_cb_change(void *data)
{
   Evas_Object *obj = data;
   Termio *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EINA_FALSE);
   sd->anim = NULL;
   _smart_apply(obj);
   evas_object_smart_callback_call(obj, "changed", NULL);
   return EINA_FALSE;
}

static void
_smart_update_queue(Evas_Object *obj, Termio *sd)
{
   if (sd->anim) return;
   sd->anim = ecore_animator_add(_smart_cb_change, obj);
}

static void
_cursor_cb_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   _imf_cursor_set(sd);
}

static void
_imf_event_commit_cb(void *data, Ecore_IMF_Context *ctx EINA_UNUSED, void *event)
{
   Termio *sd = data;
   char *str = event;
   DBG("IMF committed '%s'", str);
   if (!str) return;
   termpty_write(sd->pty, str, strlen(str));
   if (sd->preedit_str)
     {
        eina_stringshare_del(sd->preedit_str);
        sd->preedit_str = NULL;
     }
   _smart_update_queue(sd->self, sd);
}

static void
_imf_event_delete_surrounding_cb(void *data, Ecore_IMF_Context *ctx EINA_UNUSED, void *event)
{
   Termio *sd = data;
   Ecore_IMF_Event_Delete_Surrounding *ev = event;
   DBG("IMF del surrounding %p %i %i", sd, ev->offset, ev->n_chars);
}

static void
_imf_event_preedit_changed_cb(void *data, Ecore_IMF_Context *ctx, void *event EINA_UNUSED)
{
   Termio *sd = data;
   char *preedit_string;
   int cursor_pos;
   ecore_imf_context_preedit_string_get(ctx, &preedit_string, &cursor_pos);
   if (!preedit_string) return;
   DBG("IMF preedit str '%s'", preedit_string);
   if (sd->preedit_str) eina_stringshare_del(sd->preedit_str);
   sd->preedit_str = eina_stringshare_add(preedit_string);
   _smart_update_queue(sd->self, sd);
   free(preedit_string);
}


static void
_smart_add(Evas_Object *obj)
{
   Termio *sd;
   Evas_Object *o;

   sd = calloc(1, sizeof(Termio));
   EINA_SAFETY_ON_NULL_RETURN(sd);
   evas_object_smart_data_set(obj, sd);

   _parent_sc.add(obj);
   sd->self = obj;

   /* Terminal output widget */
   o = evas_object_textgrid_add(evas_object_evas_get(obj));
   evas_object_pass_events_set(o, EINA_TRUE);
   evas_object_propagate_events_set(o, EINA_FALSE);
   evas_object_smart_member_add(o, obj);
   evas_object_show(o);
   sd->grid.obj = o;

   /* Setup cursor */
   o = edje_object_add(evas_object_evas_get(obj));
   evas_object_pass_events_set(o, EINA_TRUE);
   evas_object_propagate_events_set(o, EINA_FALSE);
   evas_object_smart_member_add(o, obj);
   sd->cursor.obj = o;

   evas_object_event_callback_add(o, EVAS_CALLBACK_MOVE, _cursor_cb_move, obj);

   /* Setup the selection widget */
   o = evas_object_rectangle_add(evas_object_evas_get(obj));
   evas_object_pass_events_set(o, EINA_TRUE);
   evas_object_propagate_events_set(o, EINA_FALSE);
   sd->sel.top = o;
   o = evas_object_rectangle_add(evas_object_evas_get(obj));
   evas_object_pass_events_set(o, EINA_TRUE);
   evas_object_propagate_events_set(o, EINA_FALSE);
   sd->sel.bottom = o;
   o = edje_object_add(evas_object_evas_get(obj));
   evas_object_smart_member_add(o, obj);
   sd->sel.theme = o;
   edje_object_signal_callback_add(o, "mouse,in", "zone.bottom_right", _edje_cb_bottom_right_in, sd);
   edje_object_signal_callback_add(o, "mouse,in", "zone.top_left", _edje_cb_top_left_in, sd);
   edje_object_signal_callback_add(o, "mouse,out", "zone.bottom_right", _edje_cb_bottom_right_out, sd);
   edje_object_signal_callback_add(o, "mouse,out", "zone.top_left", _edje_cb_top_left_out, sd);

   /* Setup event catcher */
   o = evas_object_rectangle_add(evas_object_evas_get(obj));
   evas_object_repeat_events_set(o, EINA_TRUE);
   evas_object_smart_member_add(o, obj);
   sd->event = o;
   evas_object_color_set(o, 0, 0, 0, 0);
   evas_object_show(o);

   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _smart_cb_mouse_down, obj);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_UP,
                                  _smart_cb_mouse_up, obj);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_MOVE,
                                  _smart_cb_mouse_move, obj);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_IN,
                                  _smart_cb_mouse_in, obj);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_OUT,
                                  _smart_cb_mouse_out, obj);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_WHEEL,
                                  _smart_cb_mouse_wheel, obj);

   evas_object_event_callback_add(obj, EVAS_CALLBACK_KEY_DOWN,
                                  _smart_cb_key_down, obj);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_KEY_UP,
                                  _smart_cb_key_up, obj);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_FOCUS_IN,
                                  _smart_cb_focus_in, obj);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_FOCUS_OUT,
                                  _smart_cb_focus_out, obj);

   sd->link.suspend = 1;

   if (ecore_imf_init())
     {
        const char *imf_id = ecore_imf_context_default_id_get();
        Evas *e;

        if (!imf_id) sd->khdl.imf = NULL;
        else
          {
             const Ecore_IMF_Context_Info *imf_info;

             imf_info = ecore_imf_context_info_by_id_get(imf_id);
             if ((!imf_info->canvas_type) ||
                 (strcmp(imf_info->canvas_type, "evas") == 0))
               sd->khdl.imf = ecore_imf_context_add(imf_id);
             else
               {
                  imf_id = ecore_imf_context_default_id_by_canvas_type_get("evas");
                  if (imf_id) sd->khdl.imf = ecore_imf_context_add(imf_id);
               }
          }

        if (!sd->khdl.imf) goto imf_done;

        e = evas_object_evas_get(o);
        ecore_imf_context_client_window_set
          (sd->khdl.imf, (void *)ecore_evas_window_get(ecore_evas_ecore_evas_get(e)));
        ecore_imf_context_client_canvas_set(sd->khdl.imf, e);

        ecore_imf_context_event_callback_add
          (sd->khdl.imf, ECORE_IMF_CALLBACK_COMMIT, _imf_event_commit_cb, sd);
        ecore_imf_context_event_callback_add
          (sd->khdl.imf, ECORE_IMF_CALLBACK_DELETE_SURROUNDING, _imf_event_delete_surrounding_cb, sd);
        ecore_imf_context_event_callback_add
          (sd->khdl.imf, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, _imf_event_preedit_changed_cb, sd);
        /* make IMF usable by a terminal - no preedit, prediction... */
        ecore_imf_context_prediction_allow_set
          (sd->khdl.imf, EINA_FALSE);
        ecore_imf_context_autocapital_type_set
          (sd->khdl.imf, ECORE_IMF_AUTOCAPITAL_TYPE_NONE);
        ecore_imf_context_input_panel_layout_set
          (sd->khdl.imf, ECORE_IMF_INPUT_PANEL_LAYOUT_TERMINAL);
        ecore_imf_context_input_mode_set
          (sd->khdl.imf, ECORE_IMF_INPUT_MODE_FULL);
        ecore_imf_context_input_panel_language_set
          (sd->khdl.imf, ECORE_IMF_INPUT_PANEL_LANG_ALPHABET);
        ecore_imf_context_input_panel_return_key_type_set
          (sd->khdl.imf, ECORE_IMF_INPUT_PANEL_RETURN_KEY_TYPE_DEFAULT);
imf_done:
        if (sd->khdl.imf) DBG("Ecore IMF Setup");
        else WRN(_("Ecore IMF failed"));
     }
   terms = eina_list_append(terms, obj);
}

static void
_smart_del(Evas_Object *obj)
{
   Evas_Object *o;
   Termio *sd = evas_object_smart_data_get(obj);
   char *chid;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   terms = eina_list_remove(terms, obj);
   if (sd->khdl.imf)
     {
        ecore_imf_context_event_callback_del
          (sd->khdl.imf, ECORE_IMF_CALLBACK_COMMIT, _imf_event_commit_cb);
        ecore_imf_context_del(sd->khdl.imf);
     }
   if (sd->cursor.obj) evas_object_del(sd->cursor.obj);
   if (sd->event)
     {
        evas_object_event_callback_del(sd->event, EVAS_CALLBACK_MOUSE_DOWN,
                                       _smart_cb_mouse_down);
        evas_object_event_callback_del(sd->event, EVAS_CALLBACK_MOUSE_UP,
                                       _smart_cb_mouse_up);
        evas_object_event_callback_del(sd->event, EVAS_CALLBACK_MOUSE_MOVE,
                                       _smart_cb_mouse_move);
        evas_object_event_callback_del(sd->event, EVAS_CALLBACK_MOUSE_IN,
                                       _smart_cb_mouse_in);
        evas_object_event_callback_del(sd->event, EVAS_CALLBACK_MOUSE_OUT,
                                       _smart_cb_mouse_out);
        evas_object_event_callback_del(sd->event, EVAS_CALLBACK_MOUSE_WHEEL,
                                       _smart_cb_mouse_wheel);

        evas_object_del(sd->event);
        sd->event = NULL;
     }
   if (sd->self)
     {
        evas_object_event_callback_del(sd->self, EVAS_CALLBACK_KEY_DOWN,
                                       _smart_cb_key_down);
        evas_object_event_callback_del(sd->self, EVAS_CALLBACK_KEY_UP,
                                       _smart_cb_key_up);
        evas_object_event_callback_del(sd->self, EVAS_CALLBACK_FOCUS_IN,
                                       _smart_cb_focus_in);
        evas_object_event_callback_del(sd->self, EVAS_CALLBACK_FOCUS_OUT,
                                       _smart_cb_focus_out);
     }
   if (sd->sel.top) evas_object_del(sd->sel.top);
   if (sd->sel.bottom) evas_object_del(sd->sel.bottom);
   if (sd->sel.theme) evas_object_del(sd->sel.theme);
   if (sd->anim) ecore_animator_del(sd->anim);
   if (sd->delayed_size_timer) ecore_timer_del(sd->delayed_size_timer);
   if (sd->link_do_timer) ecore_timer_del(sd->link_do_timer);
   if (sd->mouse_move_job) ecore_job_del(sd->mouse_move_job);
   if (sd->mouseover_delay) ecore_timer_del(sd->mouseover_delay);
   if (sd->font.name) eina_stringshare_del(sd->font.name);
   if (sd->pty) termpty_free(sd->pty);
   if (sd->link.string) free(sd->link.string);
   if (sd->glayer) evas_object_del(sd->glayer);
   if (sd->win)
     evas_object_event_callback_del_full(sd->win, EVAS_CALLBACK_DEL,
                                         _win_obj_del, obj);
   EINA_LIST_FREE(sd->link.objs, o)
     {
        if (o == sd->link.down.dndobj) sd->link.down.dndobj = NULL;
        evas_object_del(o);
     }
   if (sd->link.down.dndobj) evas_object_del(sd->link.down.dndobj);
   keyin_compose_seq_reset(&sd->khdl);
   if (sd->sel_str) eina_stringshare_del(sd->sel_str);
   if (sd->preedit_str) eina_stringshare_del(sd->preedit_str);
   if (sd->sel_reset_job) ecore_job_del(sd->sel_reset_job);
   EINA_LIST_FREE(sd->cur_chids, chid) eina_stringshare_del(chid);
   sd->sel_str = NULL;
   sd->preedit_str = NULL;
   sd->sel_reset_job = NULL;
   sd->link.down.dndobj = NULL;
   sd->cursor.obj = NULL;
   sd->event = NULL;
   sd->sel.top = NULL;
   sd->sel.bottom = NULL;
   sd->sel.theme = NULL;
   sd->anim = NULL;
   sd->delayed_size_timer = NULL;
   sd->font.name = NULL;
   sd->pty = NULL;
   sd->khdl.imf = NULL;
   sd->win = NULL;
   sd->glayer = NULL;
   ecore_imf_shutdown();

   _parent_sc.del(obj);
}

static void
_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord ow, oh;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   evas_object_geometry_get(obj, NULL, NULL, &ow, &oh);
   if ((ow == w) && (oh == h)) return;
   evas_object_smart_changed(obj);
   if (!sd->delayed_size_timer)
     sd->delayed_size_timer = ecore_timer_add(0.0, _smart_cb_delayed_size, obj);
   else ecore_timer_reset(sd->delayed_size_timer);
   evas_object_resize(sd->event, ow, oh);
}

static void
_smart_calculate(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   evas_object_move(sd->grid.obj, ox, oy);
   evas_object_resize(sd->grid.obj,
                      sd->grid.w * sd->font.chw,
                      sd->grid.h * sd->font.chh);
   evas_object_move(sd->cursor.obj,
                    ox + (sd->cursor.x * sd->font.chw),
                    oy + (sd->cursor.y * sd->font.chh));

   evas_object_move(sd->event, ox, oy);
   evas_object_resize(sd->event, ow, oh);
}

static void
_smart_move(Evas_Object *obj, Evas_Coord x EINA_UNUSED, Evas_Coord y EINA_UNUSED)
{
   Termio *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN(sd);
   evas_object_smart_changed(obj);
}

static void
_smart_init(void)
{
   static Evas_Smart_Class sc;

   evas_object_smart_clipped_smart_set(&_parent_sc);
   sc           = _parent_sc;
   sc.name      = "termio";
   sc.version   = EVAS_SMART_CLASS_VERSION;
   sc.add       = _smart_add;
   sc.del       = _smart_del;
   sc.resize    = _smart_resize;
   sc.move      = _smart_move;
   sc.calculate = _smart_calculate;
   _smart = evas_smart_class_new(&sc);
}

static void
_smart_pty_change(void *data)
{
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN(sd);

// if scroll to bottom on updates
   if (sd->jump_on_change)  sd->scroll = 0;
   _smart_update_queue(data, sd);
}

static void
_smart_pty_title(void *data)
{
   Evas_Object *obj = data;
   Termio *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN(sd);
   if (!sd->win) return;
   evas_object_smart_callback_call(obj, "title,change", NULL);
//   elm_win_title_set(sd->win, sd->pty->prop.title);
}

static void
_smart_pty_icon(void *data)
{
   Evas_Object *obj = data;
   Termio *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN(sd);
   if (!sd->win) return;
   evas_object_smart_callback_call(obj, "icon,change", NULL);
//   elm_win_icon_name_set(sd->win, sd->pty->prop.icon);
}

static void
_smart_pty_cancel_sel(void *data)
{
   Evas_Object *obj = data;
   Termio *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN(sd);
   if (sd->pty->selection.is_active)
     {
        _sel_set(sd, EINA_FALSE);
        sd->pty->selection.makesel = EINA_FALSE;
        _smart_update_queue(data, sd);
     }
}

static void
_smart_pty_exited(void *data)
{
   Termio *sd = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (sd->event)
     {
        evas_object_event_callback_del(sd->event, EVAS_CALLBACK_MOUSE_DOWN,
                                       _smart_cb_mouse_down);
        evas_object_event_callback_del(sd->event, EVAS_CALLBACK_MOUSE_UP,
                                       _smart_cb_mouse_up);
        evas_object_event_callback_del(sd->event, EVAS_CALLBACK_MOUSE_MOVE,
                                       _smart_cb_mouse_move);
        evas_object_event_callback_del(sd->event, EVAS_CALLBACK_MOUSE_IN,
                                       _smart_cb_mouse_in);
        evas_object_event_callback_del(sd->event, EVAS_CALLBACK_MOUSE_OUT,
                                       _smart_cb_mouse_out);
        evas_object_event_callback_del(sd->event, EVAS_CALLBACK_MOUSE_WHEEL,
                                       _smart_cb_mouse_wheel);

        evas_object_del(sd->event);
        sd->event = NULL;
     }
   if (sd->self)
     {
        evas_object_event_callback_del(sd->self, EVAS_CALLBACK_KEY_DOWN,
                                       _smart_cb_key_down);
        evas_object_event_callback_del(sd->self, EVAS_CALLBACK_KEY_UP,
                                       _smart_cb_key_up);
        evas_object_event_callback_del(sd->self, EVAS_CALLBACK_FOCUS_IN,
                                       _smart_cb_focus_in);
        evas_object_event_callback_del(sd->self, EVAS_CALLBACK_FOCUS_OUT,
                                       _smart_cb_focus_out);
     }

   term_close(sd->win, sd->self, EINA_TRUE);
}

static void
_smart_pty_bell(void *data)
{
   Termio *sd = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN(sd);
   evas_object_smart_callback_call(data, "bell", NULL);
   edje_object_signal_emit(sd->cursor.obj, "bell", "terminology");
   if (sd->config->bell_rings)
     edje_object_signal_emit(sd->cursor.obj, "bell,ring", "terminology");
}

static void
_smart_pty_command(void *data)
{
   Evas_Object *obj = data;
   Termio *sd = evas_object_smart_data_get(obj);
   Config *config;
   Termpty *ty;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   config = sd->config;
   ty = sd->pty;
   if (!ty->cur_cmd)
     return;
   if (ty->cur_cmd[0] == 'q')
     {
        if (ty->cur_cmd[1] == 's')
          {
             char buf[256];

             snprintf(buf, sizeof(buf), "%i;%i;%i;%i\n",
                      sd->grid.w, sd->grid.h, sd->font.chw, sd->font.chh);
             termpty_write(ty, buf, strlen(buf));
             return;
          }
        else if (ty->cur_cmd[1] == 'j')
          {
             const char *chid = &(ty->cur_cmd[3]);

             if (ty->cur_cmd[2])
               {
                  if (ty->cur_cmd[2] == '+')
                    {
                       sd->cur_chids = eina_list_append
                          (sd->cur_chids, eina_stringshare_add(chid));
                    }
                  else if (ty->cur_cmd[2] == '-')
                    {
                       Eina_List *l;
                       char *chid2;

                       EINA_LIST_FOREACH(sd->cur_chids, l, chid2)
                         {
                            if (!(!strcmp(chid, chid2)))
                              {
                                 sd->cur_chids =
                                    eina_list_remove_list(sd->cur_chids, l);
                                 eina_stringshare_del(chid2);
                                 break;
                              }
                         }
                    }
               }
             else
               {
                  EINA_LIST_FREE(sd->cur_chids, chid)
                     eina_stringshare_del(chid);
               }
             return;
          }
        return;
     }
   if (!config->ty_escapes)
     return;
   if (ty->cur_cmd[0] == 'i')
     {
        if ((ty->cur_cmd[1] == 's') ||
            (ty->cur_cmd[1] == 'c') ||
            (ty->cur_cmd[1] == 'f') ||
            (ty->cur_cmd[1] == 't') ||
            (ty->cur_cmd[1] == 'j'))
          {
             const char *p, *p0, *p1, *path = NULL;
             char *pp;
             int ww = 0, hh = 0, repch;
             Eina_List *strs = NULL;

             // exact size in CHAR CELLS - WW (decimal) width CELLS,
             // HH (decimal) in CELLS.
             //
             // isCWW;HH;PATH
             //  OR
             // isCWW;HH;LINK\nPATH
             //  OR specific to 'j' (edje)
             // ijCWW;HH;PATH\nGROUP[commands]
             //  WHERE [commands] is an optional string set of:
             // \nCMD\nP1[\nP2][\nP3][[\nCMD2\nP21[\nP22]]...
             //  CMD is the command, P1, P2, P3 etc. are parameters (P2 and
             //  on are optional depending on CMD)
             repch = ty->cur_cmd[2];
             if (repch)
               {
                  char *link = NULL;

                  for (p0 = p = &(ty->cur_cmd[3]); *p; p++)
                    {
                       if (*p == ';')
                         {
                            ww = strtol(p0, NULL, 10);
                            p++;
                            break;
                         }
                    }
                  for (p0 = p; *p; p++)
                    {
                       if (*p == ';')
                         {
                            hh = strtol(p0, NULL, 10);
                            p++;
                            break;
                         }
                    }
                  if (ty->cur_cmd[1] == 'j')
                    {
                       // parse from p until end of string - one newline
                       // per list item in strs
                       p0 = p1 = p;
                       for (;;)
                         {
                            // end of str param
                            if ((*p1 == '\n') || (*p1 == '\r') || (!*p1))
                              {
                                 // if string is non-empty...
                                 if ((p1 - p0) >= 1)
                                   {
                                      // allocate, fill and add to list
                                      pp = malloc(p1 - p0 + 1);
                                      if (pp)
                                        {
                                           strncpy(pp, p0, p1 - p0);
                                           pp[p1 - p0] = 0;
                                           strs = eina_list_append(strs, pp);
                                        }
                                   }
                                 // end of string buffer
                                 if (!*p1) break;
                                 p1++; // skip \n or \r
                                 p0 = p1;
                              }
                            else
                              p1++;
                         }
                    }
                  else
                    {
                       path = p;
                       p = strchr(path, '\n');
                       if (p)
                         {
                            link = strdup(path);
                            path = p + 1;
                            if (isspace(path[0])) path++;
                            pp = strchr(link, '\n');
                            if (pp) *pp = 0;
                            pp = strchr(link, '\r');
                            if (pp) *pp = 0;
                         }
                    }
                  if ((ww < 512) && (hh < 512))
                    {
                       Termblock *blk = NULL;

                       if (strs)
                         {
                            const char *file, *group;
                            Eina_List *l;

                            file = eina_list_nth(strs, 0);
                            group = eina_list_nth(strs, 1);
                            l = eina_list_nth_list(strs, 2);
                            blk = termpty_block_new(ty, ww, hh, file, group);
                            for (;l; l = l->next)
                              {
                                 pp = l->data;
                                 if (pp)
                                   blk->cmds = eina_list_append(blk->cmds, pp);
                                 l->data = NULL;
                              }
                         }
                       else
                         blk = termpty_block_new(ty, ww, hh, path, link);
                       if (blk)
                         {
                            if (ty->cur_cmd[1] == 's')
                              blk->scale_stretch = EINA_TRUE;
                            else if (ty->cur_cmd[1] == 'c')
                              blk->scale_center = EINA_TRUE;
                            else if (ty->cur_cmd[1] == 'f')
                              blk->scale_fill = EINA_TRUE;
                            else if (ty->cur_cmd[1] == 't')
                              blk->thumb = EINA_TRUE;
                            else if (ty->cur_cmd[1] == 'j')
                              blk->edje = EINA_TRUE;
                            termpty_block_insert(ty, repch, blk);
                         }
                    }
                  free(link);
                  EINA_LIST_FREE(strs, pp) free(pp);
               }
             return;
          }
        else if (ty->cur_cmd[1] == 'C')
          {
             Termblock *blk = NULL;
             const char *p, *p0, *p1;
             char *pp;
             Eina_List *strs = NULL;

             p = &(ty->cur_cmd[2]);
             // parse from p until end of string - one newline
             // per list item in strs
             p0 = p1 = p;
             for (;;)
               {
                  // end of str param
                  if ((*p1 == '\n') || (*p1 == '\r') || (!*p1))
                    {
                       // if string is non-empty...
                       if ((p1 - p0) >= 1)
                         {
                            // allocate, fill and add to list
                            pp = malloc(p1 - p0 + 1);
                            if (pp)
                              {
                                 strncpy(pp, p0, p1 - p0);
                                 pp[p1 - p0] = 0;
                                 strs = eina_list_append(strs, pp);
                              }
                         }
                       // end of string buffer
                       if (!*p1) break;
                       p1++; // skip \n or \r
                       p0 = p1;
                    }
                  else
                    p1++;
               }
             if (strs)
               {
                  char *chid = strs->data;
                  blk = termpty_block_chid_get(ty, chid);
                  if (blk)
                    {
                       _block_edje_cmds(ty, blk, strs->next, EINA_FALSE);
                    }
               }
             EINA_LIST_FREE(strs, pp) free(pp);
          }
        else if (ty->cur_cmd[1] == 'b')
          {
             ty->block.on = EINA_TRUE;
          }
        else if (ty->cur_cmd[1] == 'e')
          {
             ty->block.on = EINA_FALSE;
          }
     }
   else if (ty->cur_cmd[0] == 'q')
     {
        if (ty->cur_cmd[1] == 's')
          {
             char buf[256];

             snprintf(buf, sizeof(buf), "%i;%i;%i;%i\n",
                      sd->grid.w, sd->grid.h, sd->font.chw, sd->font.chh);
             termpty_write(ty, buf, strlen(buf));
             return;
          }
        else if (ty->cur_cmd[1] == 'j')
          {
             const char *chid = &(ty->cur_cmd[3]);

             if (ty->cur_cmd[2])
               {
                  if (ty->cur_cmd[2] == '+')
                    {
                       sd->cur_chids = eina_list_append
                         (sd->cur_chids, eina_stringshare_add(chid));
                    }
                  else if (ty->cur_cmd[2] == '-')
                    {
                       Eina_List *l;
                       char *chid2;

                       EINA_LIST_FOREACH(sd->cur_chids, l, chid2)
                         {
                            if (!(!strcmp(chid, chid2)))
                              {
                                 sd->cur_chids =
                                   eina_list_remove_list(sd->cur_chids, l);
                                 eina_stringshare_del(chid2);
                                 break;
                              }
                         }
                    }
               }
             else
               {
                  EINA_LIST_FREE(sd->cur_chids, chid)
                    eina_stringshare_del(chid);
               }
             return;
          }
     }
   evas_object_smart_callback_call(obj, "command", (void *)ty->cur_cmd);
}

#if !((ELM_VERSION_MAJOR == 1) && (ELM_VERSION_MINOR < 8))
static void
_smart_cb_drag_enter(void *data EINA_UNUSED, Evas_Object *o EINA_UNUSED)
{
   DBG("dnd enter");
}

static void
_smart_cb_drag_leave(void *data EINA_UNUSED, Evas_Object *o EINA_UNUSED)
{
   DBG("dnd leave");
}

static void
_smart_cb_drag_pos(void *data EINA_UNUSED, Evas_Object *o EINA_UNUSED, Evas_Coord x, Evas_Coord y, Elm_Xdnd_Action action)
{
   DBG("dnd at %i %i act:%i", x, y, action);
}

static Eina_Bool
_smart_cb_drop(void *data, Evas_Object *o EINA_UNUSED, Elm_Selection_Data *ev)
{
   Evas_Object *obj = data;
   Termio *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EINA_TRUE);
   if (ev->action == ELM_XDND_ACTION_COPY)
     {
        if (strchr(ev->data, '\n'))
          {
             char *p, *p2, *p3, *tb;

             tb = malloc(strlen(ev->data) + 1);
             if (tb)
               {
                  for (p = ev->data; p;)
                    {
                       p2 = strchr(p, '\n');
                       p3 = strchr(p, '\r');
                       if (p2 && p3)
                         {
                            if (p3 < p2) p2 = p3;
                         }
                       else if (!p2) p2 = p3;
                       if (p2)
                         {
                            strncpy(tb, p, p2 - p);
                            tb[p2 - p] = 0;
                            p = p2;
                            while ((*p) && (isspace(*p))) p++;
                            if (strlen(tb) > 0)
                              evas_object_smart_callback_call
                              (obj, "popup,queue", tb);
                         }
                       else
                         {
                            strcpy(tb, p);
                            if (strlen(tb) > 0)
                              evas_object_smart_callback_call
                              (obj, "popup,queue", tb);
                            break;
                         }
                    }
                  free(tb);
               }
          }
        else
          evas_object_smart_callback_call(obj, "popup", ev->data);
     }
   else
     termpty_write(sd->pty, ev->data, ev->len);
   return EINA_TRUE;
}
#endif

/* }}} */


Evas_Object *
termio_add(Evas_Object *win, Config *config,
           const char *cmd, Eina_Bool login_shell, const char *cd,
           int w, int h, Term *term)
{
   Evas *e;
   Evas_Object *obj, *g;
   Termio *sd;
   char *modules[] =
     {
        NULL,
        "gstreamer",
        "xine",
        "vlc",
        "gstreamer1"
     };
   char *mod = NULL;

   EINA_SAFETY_ON_NULL_RETURN_VAL(win, NULL);
   e = evas_object_evas_get(win);
   if (!e) return NULL;

   if (!_smart) _smart_init();
   obj = evas_object_smart_add(e, _smart);
   sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, obj);

   if ((config->vidmod >= 0) &&
       (config->vidmod < (int)EINA_C_ARRAY_LENGTH(modules)))
     mod = modules[config->vidmod];

   termio_config_set(obj, config);
   sd->term = term;
   sd->win = win;

   sd->glayer = g = elm_gesture_layer_add(win);
   elm_gesture_layer_attach(g, sd->event);

   elm_gesture_layer_cb_set(g, ELM_GESTURE_N_LONG_TAPS,
                            ELM_GESTURE_STATE_MOVE, _smart_cb_gest_long_move,
                            obj);

   elm_gesture_layer_cb_set(g, ELM_GESTURE_ZOOM,
                            ELM_GESTURE_STATE_START, _smart_cb_gest_zoom_start,
                            obj);
   elm_gesture_layer_cb_set(g, ELM_GESTURE_ZOOM,
                            ELM_GESTURE_STATE_MOVE, _smart_cb_gest_zoom_move,
                            obj);
   elm_gesture_layer_cb_set(g, ELM_GESTURE_ZOOM,
                            ELM_GESTURE_STATE_END, _smart_cb_gest_zoom_end,
                            obj);
   elm_gesture_layer_cb_set(g, ELM_GESTURE_ZOOM,
                            ELM_GESTURE_STATE_ABORT, _smart_cb_gest_zoom_abort,
                            obj);

#if !((ELM_VERSION_MAJOR == 1) && (ELM_VERSION_MINOR < 8))
   elm_drop_target_add(sd->event,
                       ELM_SEL_FORMAT_TEXT | ELM_SEL_FORMAT_IMAGE,
                       _smart_cb_drag_enter, obj,
                       _smart_cb_drag_leave, obj,
                       _smart_cb_drag_pos, obj,
                       _smart_cb_drop, obj);
#endif

   sd->pty = termpty_new(cmd, login_shell, cd, w, h, config->scrollback,
                         config->xterm_256color, config->erase_is_del, mod);
   if (!sd->pty)
     {
        ERR(_("Could not allocate termpty"));
        evas_object_del(obj);
        return NULL;
     }
   sd->pty->obj = obj;
   sd->pty->cb.change.func = _smart_pty_change;
   sd->pty->cb.change.data = obj;
   sd->pty->cb.set_title.func = _smart_pty_title;
   sd->pty->cb.set_title.data = obj;
   sd->pty->cb.set_icon.func = _smart_pty_icon;
   sd->pty->cb.set_icon.data = obj;
   sd->pty->cb.cancel_sel.func = _smart_pty_cancel_sel;
   sd->pty->cb.cancel_sel.data = obj;
   sd->pty->cb.exited.func = _smart_pty_exited;
   sd->pty->cb.exited.data = obj;
   sd->pty->cb.bell.func = _smart_pty_bell;
   sd->pty->cb.bell.data = obj;
   sd->pty->cb.command.func = _smart_pty_command;
   sd->pty->cb.command.data = obj;
   _smart_size(obj, w, h, EINA_FALSE);
   return obj;
}
