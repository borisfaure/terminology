#include "private.h"

#include <Elementary.h>
#include <Ecore_Input.h>

#include "termio.h"
#include "termiolink.h"
#include "termpty.h"
#include "backlog.h"
#include "termptyops.h"
#include "termcmd.h"
#include "termptydbl.h"
#include "utf8.h"
#include "col.h"
#include "keyin.h"
#include "config.h"
#include "utils.h"
#include "media.h"
#include "miniview.h"
#include "gravatar.h"
#include "sb.h"

#if defined (__MacOSX__) || (defined (__MACH__) && defined (__APPLE__))
# include <sys/proc_info.h>
# include <libproc.h>
#endif


static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

static Eina_List *terms = NULL;

static void _smart_apply(Evas_Object *obj);
static void _smart_size(Evas_Object *obj, int w, int h, Eina_Bool force);
static void _smart_calculate(Evas_Object *obj);
static Eina_Bool _mouse_in_selection(Termio *sd, int cx, int cy);


/* {{{ Helpers */

Termio *
termio_get_from_obj(Evas_Object *obj)
{
   return evas_object_smart_data_get(obj);
}

void
termio_object_geometry_get(Termio *sd,
                           Evas_Coord *x, Evas_Coord *y,
                           Evas_Coord *w, Evas_Coord *h)
{
   evas_object_geometry_get(sd->self, x, y, w, h);
}

static void
_win_obj_del(void *data,
             Evas *_e EINA_UNUSED,
             Evas_Object *obj,
             void *_event EINA_UNUSED)
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
   termio_smart_update_queue(sd);
}

void
termio_size_get(const Evas_Object *obj, int *w, int *h)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   if (w) *w = sd->grid.w;
   if (h) *h = sd->grid.h;
}

int
termio_scroll_get(const Evas_Object *obj)
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
   termio_smart_update_queue(sd);
   miniview_position_offset(term_miniview_get(sd->term), -delta, EINA_TRUE);
}

void
termio_scroll_set(Evas_Object *obj, int scroll)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   sd->scroll = scroll;
   termio_remove_links(sd);
   _smart_apply(obj);
}

void
termio_scroll_top_backlog(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   sd->scroll = INT32_MAX;
   termio_remove_links(sd);
   _smart_apply(obj);
}

const char *
termio_title_get(const Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);
   if (sd->pty->prop.user_title)
      return sd->pty->prop.user_title;
   return sd->pty->prop.title;
}

const char *
termio_user_title_get(const Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);
   return sd->pty->prop.user_title;
}

void
termio_user_title_set(Evas_Object *obj, const char *title)
{
    Termio *sd = evas_object_smart_data_get(obj);
    size_t len = 0;
    EINA_SAFETY_ON_NULL_RETURN(sd);

    eina_stringshare_del(sd->pty->prop.user_title);
    sd->pty->prop.user_title = NULL;

    if (title)
      {
         len = strlen(title);
      }
    if (len)
      sd->pty->prop.user_title = eina_stringshare_add_length(title, len);
    if (sd->pty->cb.set_title.func)
      sd->pty->cb.set_title.func(sd->pty->cb.set_title.data);
}

const char *
termio_icon_name_get(const Evas_Object *obj)
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
termio_pty_get(const Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);

   return sd->pty;
}

Evas_Object *
termio_miniview_get(const Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);

   return term_miniview_get(sd->term);
}

Term*
termio_term_get(const Evas_Object *obj)
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
termio_font_update(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Config *config;
   EINA_SAFETY_ON_NULL_RETURN(sd);

   config = sd->config;

   if (config)
     {
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
   siz = readlink(procpath, buf, size);
   if ((siz == -1) || (siz >= (ssize_t)size))
     {
        ERR(_("Could not load working directory %s: %s"),
            procpath, strerror(errno));
        return EINA_FALSE;
     }
   buf[siz] = '\0';

#endif

   buf[size -1] = '\0';

   return EINA_TRUE;
}

Evas_Object *
termio_textgrid_get(const Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);

   return sd->grid.obj;
}

Evas_Object *
termio_win_get(const Evas_Object *obj)
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

   EINA_SAFETY_ON_NULL_RETURN(sd);

   eina_stringshare_del(sd->font.name);
   sd->font.name = NULL;

   if (sd->config->font.bitmap)
     {
        char buf[4096];

        snprintf(buf, sizeof(buf), "%s/fonts/%s",
                 elm_app_data_dir_get(), sd->config->font.name);
        sd->font.name = eina_stringshare_add(buf);
     }
   else
     sd->font.name = eina_stringshare_add(sd->config->font.name);
   sd->font.size = sd->config->font.size;

   sd->jump_on_change = sd->config->jump_on_change;
   sd->jump_on_keypress = sd->config->jump_on_keypress;

   termpty_config_update(sd->pty, sd->config);
   sd->scroll = 0;

   colors_term_init(sd->grid.obj, sd->theme, sd->config);

   evas_object_scale_set(sd->grid.obj, elm_config_scale_get());
   evas_object_textgrid_font_set(sd->grid.obj, sd->font.name, sd->font.size);
   evas_object_textgrid_cell_size_get(sd->grid.obj, &w, &h);

   if (w < 1) w = 1;
   if (h < 1) h = 1;
   sd->font.chw = w;
   sd->font.chh = h;

   evas_object_geometry_get(obj, NULL, NULL, &ow, &oh);
   termio_set_cursor_shape(obj, sd->config->cursor_shape);
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

   termio_set_cursor_shape(obj, sd->cursor.shape);

   theme_apply(sd->sel.theme, config, "terminology/selection");
   theme_auto_reload_enable(sd->sel.theme);
   edje_object_part_swallow(sd->sel.theme, "terminology.top_left", sd->sel.top);
   edje_object_part_swallow(sd->sel.theme, "terminology.bottom_right", sd->sel.bottom);
}

static const char *
_cursor_shape_to_group_name(Cursor_Shape shape)
{
   switch (shape)
     {
      case CURSOR_SHAPE_BLOCK: return "terminology/cursor";
      case CURSOR_SHAPE_BAR: return "terminology/cursor_bar";
      case CURSOR_SHAPE_UNDERLINE: return "terminology/cursor_underline";
     }
   return NULL;
}

void
termio_set_cursor_shape(Evas_Object *obj, Cursor_Shape shape)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Config *config;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   config = sd->config;
   theme_apply(sd->cursor.obj, config, _cursor_shape_to_group_name(shape));
   theme_auto_reload_enable(sd->cursor.obj);
   evas_object_resize(sd->cursor.obj, sd->font.chw, sd->font.chh);
   evas_object_show(sd->cursor.obj);
   sd->cursor.shape = shape;

   if (term_is_focused(sd->term))
     {
        edje_object_signal_emit(sd->cursor.obj, "focus,out", "terminology");
        if (sd->config->disable_cursor_blink)
          edje_object_signal_emit(sd->cursor.obj, "focus,in,noblink", "terminology");
        else
          edje_object_signal_emit(sd->cursor.obj, "focus,in", "terminology");
     }
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

/*
 * Returned string needs to be freed.
 * Does not handle colors */
const char *
termio_link_get(const Evas_Object *obj,
                Eina_Bool *from_escape_code)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);
   const char *link;

   if (!sd->link.string && !sd->link.id)
     return NULL;

   if (from_escape_code)
        *from_escape_code = EINA_FALSE;
   link = sd->link.string;
   if (sd->link.id)
     {
        Term_Link *hl = &sd->pty->hl.links[sd->link.id];
        if (!hl->url)
          return NULL;
        link = hl->url;
        if (from_escape_code)
          *from_escape_code = EINA_TRUE;
     }
   if (link_is_url(link))
     {
        if (casestartswith(link, "file://"))
          {
             // TODO: decode string: %XX -> char
             link = link + sizeof("file://") - 1;
             /* Handle cases where / is ommitted: file://HOSTNAME/home/ */
             if (link[0] != '/')
               {
                  link = strchr(link, '/');
                  if (!link)
                    return NULL;
               }
          }
     }
   return strdup(link);
}

static void
_activate_link(Evas_Object *obj, Eina_Bool may_inline)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Config *config;
   char buf[PATH_MAX], *s, *escaped;
   const char *path = NULL, *cmd = NULL;
   const char *link = NULL;
   Eina_Bool from_escape_code = EINA_FALSE;
   Eina_Bool url = EINA_FALSE, email = EINA_FALSE, handled = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   config = sd->config;
   if (!config)
     return;

   link = termio_link_get(obj, &from_escape_code);
   if (!link)
     return;

   if (from_escape_code && !config->active_links_escape)
     goto end;

   if (link_is_url(link))
     {
        if (casestartswith(link, "mailto:"))
          {
             email = EINA_TRUE;
             if (!config->active_links_email)
               goto end;
          }
        else
          {
             url = EINA_TRUE;
             if (!config->active_links_url)
               goto end;
          }
     }
   else if (link[0] == '/')
     {
        path = link;
        if (!config->active_links_file)
          goto end;
     }
   else if (link_is_email(link))
     {
        email = EINA_TRUE;
        if (!config->active_links_email)
          goto end;
     }

   s = eina_str_escape(link);
   if (!s)
     goto end;
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

        escaped = ecore_file_escape_name(path);
        if (escaped)
          {
             Media_Type type = media_src_type_get(path);
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
             Media_Type type = media_src_type_get(link);
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
        goto end;
     }
   free(s);
   if (!handled)
     ecore_exe_run(buf, NULL);

end:
   free((char*)link);
}

static void
_cb_ctxp_del(void *data,
             Evas *_e EINA_UNUSED,
             Evas_Object *_obj EINA_UNUSED,
             void *_event EINA_UNUSED)
{
   Termio *sd = data;
   EINA_SAFETY_ON_NULL_RETURN(sd);
   sd->ctxpopup = NULL;

   /* Force refocus */
   term_unfocus(sd->term);
   term_focus(sd->term);
}

static void
_cb_ctxp_dismissed(void *data,
                   Evas_Object *obj,
                   void *_event EINA_UNUSED)
{
   Termio *sd = data;
   EINA_SAFETY_ON_NULL_RETURN(sd);
   sd->ctxpopup = NULL;
   evas_object_del(obj);
}

static void
_cb_ctxp_link_preview(void *data,
                      Evas_Object *obj,
                      void *_event EINA_UNUSED)
{
   Evas_Object *term = data;
   Termio *sd = evas_object_smart_data_get(term);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   _activate_link(term, EINA_TRUE);

   sd->ctxpopup = NULL;
   evas_object_del(obj);
}

static void
_cb_ctxp_link_open(void *data,
                   Evas_Object *obj,
                   void *_event EINA_UNUSED)
{
   Evas_Object *term = data;
   Termio *sd = evas_object_smart_data_get(term);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   _activate_link(term, EINA_FALSE);

   sd->ctxpopup = NULL;
   evas_object_del(obj);
}

static void _lost_selection(void *data, Elm_Sel_Type selection);

static void
_lost_selection_reset_job(void *data)
{
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   sd->sel_reset_job = NULL;
   if (sd->sel_str)
     {
        elm_cnp_selection_set(sd->win, sd->sel_type,
                              ELM_SEL_FORMAT_TEXT,
                              sd->sel_str, strlen(sd->sel_str));
        elm_cnp_selection_loss_callback_set(sd->win, sd->sel_type,
                                            _lost_selection, data);
     }
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
             eina_stringshare_del(sd->sel_str);
             sd->sel_str = NULL;
             termio_sel_set(sd, EINA_FALSE);
             elm_object_cnp_selection_clear(sd->win, selection);
             termio_smart_update_queue(sd);
             sd->have_sel = EINA_FALSE;
          }
     }
}



void
termio_take_selection_text(Termio *sd, Elm_Sel_Type type, const char *text)
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
   eina_stringshare_del(sd->sel_str);
   sd->sel_str = eina_stringshare_add(text);
}


Eina_Bool
termio_take_selection(Evas_Object *obj, Elm_Sel_Type type)
{
   Termio *sd = termio_get_from_obj(obj);
   const char *s = NULL;
   size_t len = 0;

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EINA_FALSE);

   s = termio_internal_get_selection(sd, &len);
   if (s)
     {
        if ((sd->win) && (len > 0))
          termio_take_selection_text(sd, type, s);
        eina_stringshare_del(s);
        return EINA_TRUE;
     }
   return EINA_FALSE;
}

static void
_cb_ctxp_link_content_copy(void *data,
                           Evas_Object *obj,
                           void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Termio *sd = evas_object_smart_data_get(term);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (sd->link.id)
     {
        Term_Link *hl = &sd->pty->hl.links[sd->link.id];

        if (!hl->url)
          return;
        termio_take_selection_text(sd, ELM_SEL_TYPE_CLIPBOARD, hl->url);
     }
   else
     {
        struct ty_sb sb = {.buf = NULL, .len = 0, .alloc = 0};
        char *raw_link;

        termio_selection_get(sd,
                             sd->link.x1, sd->link.y1,
                             sd->link.x2, sd->link.y2,
                             &sb, EINA_FALSE);
        raw_link = ty_sb_steal_buf(&sb);
        termio_take_selection_text(sd, ELM_SEL_TYPE_CLIPBOARD, raw_link);
        free(raw_link);
     }

   sd->ctxpopup = NULL;
   evas_object_del(obj);
}

static void
_cb_ctxp_link_copy(void *data,
                   Evas_Object *obj,
                   void *_event EINA_UNUSED)
{
   Evas_Object *term = data;
   Termio *sd = evas_object_smart_data_get(term);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   EINA_SAFETY_ON_NULL_RETURN(sd->link.string);
   termio_take_selection_text(sd, ELM_SEL_TYPE_CLIPBOARD, sd->link.string);

   sd->ctxpopup = NULL;
   evas_object_del(obj);
}

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

static Eina_Bool
_getsel_cb(void *data,
           Evas_Object *_obj EINA_UNUSED,
           Elm_Selection_Data *ev)
{
   Termio *sd = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EINA_FALSE);

   if (ev->format == ELM_SEL_FORMAT_TEXT)
     {
        char *buf;

        if (ev->len <= 0) return EINA_TRUE;

        buf = calloc(2, ev->len); /* twice in case the paste is only \n */
        if (buf)
          {
             const char *s = ev->data;
             int i, j, pos = 0;

             /* apparently we have to convert \n into \r in terminal land. */
             for (i = 0; i < (int)ev->len && s[i];)
               {
                  Eina_Unicode g = 0;
                  int prev_i = i;
                  g = eina_unicode_utf8_next_get(s, &i);
                  /* Skip escape codes as a security measure */
                  if (! ((g == '\t') || (g == '\n') || (g >= ' ')))
                    {
                       continue;
                    }
                  if (g == '\n')
                    buf[pos++] = '\r';
                  else
                      for (j = prev_i; j < i; j++)
                          buf[pos++] = s[j];
               }
             if (pos)
               {
                  if (sd->pty->bracketed_paste)
                    termpty_write(sd->pty, "\x1b[200~",
                                  sizeof("\x1b[200~") - 1);

                  termpty_write(sd->pty, buf, pos);

                  if (sd->pty->bracketed_paste)
                    termpty_write(sd->pty, "\x1b[201~",
                                  sizeof("\x1b[201~") - 1);
               }
             free(buf);
          }
     }
   else
     {
        const char *fmt = "UNKNOWN";
        switch (ev->format)
          {
           case ELM_SEL_FORMAT_TARGETS: fmt = "TARGETS"; break;
           case ELM_SEL_FORMAT_NONE: fmt = "NONE"; break;
           case ELM_SEL_FORMAT_TEXT: fmt = "TEXT"; break; /* shouldn't happen */
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
   if (!sd->win)
       return;
   elm_cnp_selection_get(sd->win, type, ELM_SEL_FORMAT_TEXT,
                         _getsel_cb, obj);
}

static const char *
_color_to_txt(const Termio *sd)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);

   if (sd->link.color.a == 255)
     return eina_stringshare_printf("#%.2x%.2x%.2x",
                                    sd->link.color.r,
                                    sd->link.color.g,
                                    sd->link.color.b);
   else
     return eina_stringshare_printf("#%.2x%.2x%.2x%.2x",
                                    sd->link.color.r,
                                    sd->link.color.g,
                                    sd->link.color.b,
                                    sd->link.color.a);
}

static void
_cb_ctxp_color_copy(void *data,
                    Evas_Object *obj,
                    void *_event EINA_UNUSED)
{
   Termio *sd = data;
   const char *txt;
   EINA_SAFETY_ON_NULL_RETURN(sd);

   txt = _color_to_txt(sd);
   if (!sd) return;

   termio_take_selection_text(sd, ELM_SEL_TYPE_CLIPBOARD, txt);

   eina_stringshare_del(txt);
   sd->ctxpopup = NULL;
   evas_object_del(obj);
}

static void
_cb_link_down(void *data,
              Evas *_e EINA_UNUSED,
              Evas_Object *_obj EINA_UNUSED,
              void *event)
{
   Evas_Event_Mouse_Down *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   Term_Link *hl = NULL;


   if (!sd->link.string && sd->link.id)
     {
        hl = &sd->pty->hl.links[sd->link.id];
        if (!hl->url)
          return;
        sd->link.string = eina_stringshare_add(hl->url);
     }

   if (ev->button == 1)
     {
        sd->link.down.down = EINA_TRUE;
        sd->link.down.x = ev->canvas.x;
        sd->link.down.y = ev->canvas.y;
     }
   else if (ev->button == 3) /* right click */
     {
        Evas_Object *ctxp;

        ctxp = elm_ctxpopup_add(sd->win);
        sd->ctxpopup = ctxp;

        if (sd->link.is_color)
          {
             const char *fmt, *txt;
             if (sd->link.color.a == 255)
               {
                  fmt = eina_stringshare_printf(_("Copy '%s'"),
                                                "#%.2x%.2x%.2x");
                  txt = eina_stringshare_printf(fmt,
                                                sd->link.color.r,
                                                sd->link.color.g,
                                                sd->link.color.b);
               }
               else
               {
                  fmt = eina_stringshare_printf(_("Copy '%s'"),
                                                "#%.2x%.2x%.2x%.2x");
                  txt = eina_stringshare_printf(fmt,
                                                sd->link.color.r,
                                                sd->link.color.g,
                                                sd->link.color.b,
                                                sd->link.color.a);
               }
             elm_ctxpopup_item_append(ctxp, txt, NULL,
                                      _cb_ctxp_color_copy, sd);
             eina_stringshare_del(txt);
             eina_stringshare_del(fmt);
          }
        else
          {
             Eina_Bool absolut = EINA_FALSE;
             const char *raw_link;
             size_t len;

             if (sd->pty->selection.is_active)
               {
                  int cx = 0, cy = 0;

                  termio_cursor_to_xy(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);
                  if (_mouse_in_selection(sd, cx, cy))
                    return;
               }


             if (hl)
               {
                  raw_link = hl->url;
                  len = strlen(hl->url);
               }
             else
               {
                  struct ty_sb sb = {.buf = NULL, .len = 0, .alloc = 0};
                  termio_selection_get(sd,
                                       sd->link.x1,
                                       sd->link.y1,
                                       sd->link.x2,
                                       sd->link.y2,
                                       &sb,
                                       EINA_FALSE);
                  len = sb.len;
                  raw_link = ty_sb_steal_buf(&sb);
               }

             if (len > 0 && raw_link[0] == '/')
               absolut = EINA_TRUE;

             if (sd->config->helper.inline_please)
               {
                  Media_Type type = media_src_type_get(raw_link);

                  if ((type == MEDIA_TYPE_IMG) ||
                      (type == MEDIA_TYPE_SCALE) ||
                      (type == MEDIA_TYPE_EDJE) ||
                      (type == MEDIA_TYPE_MOV))
                    elm_ctxpopup_item_append(ctxp, _("Preview"), NULL,
                                             _cb_ctxp_link_preview, sd->self);
               }
             elm_ctxpopup_item_append(ctxp, _("Open"), NULL, _cb_ctxp_link_open,
                                      sd->self);

             if (!absolut &&
                 !link_is_url(raw_link) &&
                 !link_is_email(raw_link))
               {
                  elm_ctxpopup_item_append(ctxp, _("Copy relative path"), NULL, _cb_ctxp_link_content_copy,
                                           sd->self);
                  elm_ctxpopup_item_append(ctxp, _("Copy full path"), NULL, _cb_ctxp_link_copy,
                                           sd->self);
               }
             else
               {
                  elm_ctxpopup_item_append(ctxp, _("Copy"), NULL, _cb_ctxp_link_copy, sd->self);
               }
             if (!hl)
               free((void*)raw_link);
          }

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
_cb_link_up(void *data,
            Evas *_e EINA_UNUSED,
            Evas_Object *_obj EINA_UNUSED,
            void *event)
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
_cb_link_drag_accept(void *data,
                     Evas_Object *_obj EINA_UNUSED,
                     Eina_Bool doaccept)
{
   Termio *sd = evas_object_smart_data_get(data);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   DBG("dnd accept: %i", doaccept);
}

static void
_cb_link_drag_done(void *data, Evas_Object *_obj EINA_UNUSED)
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

static void
_cb_link_move(void *data,
              Evas *_e EINA_UNUSED,
              Evas_Object *obj,
              void *event)
{
   Evas_Event_Mouse_Move *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   Evas_Coord dx, dy;
   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (!sd->link.down.down)
     return;
   dx = abs(ev->cur.canvas.x - sd->link.down.x);
   dy = abs(ev->cur.canvas.y - sd->link.down.y);
   if ((sd->config->drag_links) &&
       (sd->link.string) &&
       ((dx > elm_config_finger_size_get()) ||
           (dy > elm_config_finger_size_get())))
     {
        sd->link.down.down = EINA_FALSE;
        sd->link.down.dnd = EINA_TRUE;
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
     }
}

static Evas_Object *
_color_tooltip_content(void *data,
                       Evas_Object *obj,
                       Evas_Object *_tt EINA_UNUSED)
{
   Termio *sd = data;
   Evas_Object *o;
   Evas *canvas = evas_object_evas_get(obj);
   const char *txt;

   o = edje_object_add(canvas);
   theme_apply(o, sd->config, "terminology/color_preview");
   evas_object_size_hint_min_set(o, 80, 80);
   edje_object_color_class_set(o, "color_preview",
       sd->link.color.r, sd->link.color.g, sd->link.color.b, sd->link.color.a,
       sd->link.color.r, sd->link.color.g, sd->link.color.b, sd->link.color.a,
       sd->link.color.r, sd->link.color.g, sd->link.color.b, sd->link.color.a);

   txt = _color_to_txt(sd);
   edje_object_part_text_set(o, "name", txt);
   eina_stringshare_del(txt);
   return o;
}

static void
_color_tooltip(Evas_Object *obj,
               Termio *sd)
{
   elm_object_tooltip_content_cb_set(obj, _color_tooltip_content, sd, NULL);
}

static void
_update_link(Termio *sd, Eina_Bool same_geom)
{
   Evas_Coord ox, oy, ow, oh;
   Evas_Object *o;
   Evas_Object *obj;
   Eina_Bool popup_exists;
   int y;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   obj = sd->self;

   if (sd->link.id || sd->link.is_color)
     {
        same_geom = EINA_FALSE;
     }

   if (same_geom)
     return;

   sd->link.id = 0;

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
   if (!sd->link.string && !sd->link.is_color)
     return;

   popup_exists = main_term_popup_exists(sd->term);
   for (y = sd->link.y1; y <= sd->link.y2; y++)
     {
        o = elm_layout_add(sd->win);
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
        if (!popup_exists)
          {
             if (link_is_email(sd->link.string))
               {
                  gravatar_tooltip(o, sd->config, sd->link.string);
               }
             if (sd->link.is_color)
               {
                  _color_tooltip(o, sd);
               }
          }
     }
}

void
termio_remove_links(Termio *sd)
{
   Eina_Bool same_geom = EINA_FALSE;

   eina_stringshare_del(sd->link.string);
   sd->link.string = NULL;

   sd->link.x1 = -1;
   sd->link.y1 = -1;
   sd->link.x2 = -1;
   sd->link.y2 = -1;
   sd->link.suspend = 0;
   sd->link.id = 0;
   sd->link.is_color = EINA_FALSE;
   sd->link.color.r = 0;
   sd->link.color.g = 0;
   sd->link.color.b = 0;
   sd->link.color.a = 0;
   _update_link(sd, same_geom);
}

static void
_hyperlink_end(Termio *sd,
               Term_Link *hl,
               Evas_Object *o,
               Eina_Bool add_tooltip)
{
   Eina_Bool popup_exists;

   sd->link.objs = eina_list_append(sd->link.objs, o);
   elm_object_cursor_set(o, "hand2");
   evas_object_show(o);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _cb_link_down, sd->self);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_UP,
                                  _cb_link_up, sd->self);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_MOVE,
                                  _cb_link_move, sd->self);
   popup_exists = main_term_popup_exists(sd->term);
   if (!popup_exists && add_tooltip)
     {
        /* display tooltip */
        elm_object_tooltip_text_set(o, hl->url);

        if (link_is_email(hl->url))
          {
             gravatar_tooltip(o, sd->config, hl->url);
          }
     }
}

static void
_hyperlink_mouseover(Termio *sd,
                     uint16_t link_id)
{
   Evas_Coord ox, oy, ow, oh;
   int x, y;
   Term_Link *hl;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   /* If it's the same link, consider we already have the correct links
    * displayed */
   if (sd->link.id == link_id)
     return;

   if (sd->link.suspend)
     return;

   termio_remove_links(sd);
   sd->link.id = link_id;

   hl = &sd->pty->hl.links[link_id];
   if (!hl->url)
     return;

   /* Scan the whole screen and display links as needed */
   termio_object_geometry_get(sd, &ox, &oy, &ow, &oh);
   termpty_backlog_lock();
   termpty_backscroll_adjust(sd->pty, &sd->scroll);
   for (y = 0; y < sd->grid.h; y++)
     {
        Termcell *cells;
        Evas_Object *o;
        ssize_t w = 0;
        int start_x = -1;
        Eina_Bool add_tooltip = EINA_FALSE;

        o = NULL;

        cells = termpty_cellrow_get(sd->pty, y - sd->scroll, &w);
        if (!cells)
          continue;
        for (x = 0; x < w; x++)
          {
             Termcell *c = cells + x;
             if (term_link_eq(sd->pty, hl, c->att.link_id))
               {
                  if (!o)
                    {
                       o = elm_layout_add(sd->win);
                       evas_object_smart_member_add(o, sd->self);
                       theme_apply(elm_layout_edje_get(o), sd->config,
                                   "terminology/link");
                       evas_object_move(o,
                                        ox + (x * sd->font.chw),
                                        oy + (y * sd->font.chh));
                       start_x = x;
                    }
               }
             else
               {
                  if (o)
                    {
                       evas_object_resize(o,
                                          (x - start_x) * sd->font.chw,
                                          sd->font.chh);
                       add_tooltip = ((y == sd->mouse.cy) &&
                                      ((start_x <= sd->mouse.cx) &&
                                       (sd->mouse.cx <= x)));
                       _hyperlink_end(sd, hl, o, add_tooltip);
                       if (add_tooltip)
                         {
                            sd->link.y1 = sd->link.y2 = y;
                            sd->link.x1 = start_x;
                            sd->link.x2 = x;
                         }
                       o = NULL;
                    }
               }
          }
        if (o)
          {
             evas_object_resize(o,
                                (x - start_x + 1) * sd->font.chw,
                                sd->font.chh);
             add_tooltip = ((y == sd->mouse.cy) &&
                            ((start_x <= sd->mouse.cx) &&
                             (sd->mouse.cx <= x)));
             _hyperlink_end(sd, hl, o, add_tooltip);
             if (add_tooltip)
               {
                  sd->link.y1 = sd->link.y2 = y;
                  sd->link.x1 = start_x;
                  sd->link.x2 = x;
               }
          }
     }
   termpty_backlog_unlock();
}


/* }}} */
/* {{{ Blocks */

static void
_smart_media_clicked(void *data, Evas_Object *obj, void *_info EINA_UNUSED)
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
             Media_Type type = media_src_type_get(blk->link);
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
                            char *escaped;

                            escaped = ecore_file_escape_name(file);
                            if (escaped)
                              {
                                 char buf[PATH_MAX];

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
_smart_media_del(void *data,
                 Evas *_e EINA_UNUSED,
                 Evas_Object *obj,
                 void *_info EINA_UNUSED)
{
   Termblock *blk = data;

   if (blk->obj == obj) blk->obj = NULL;
}

static void
_smart_media_play(void *data,
                  Evas_Object *obj,
                  void *_info EINA_UNUSED)
{
   Termblock *blk = data;

   if (blk->obj != obj) return;

   blk->mov_state = MOVIE_STATE_PLAY;
}

static void
_smart_media_pause(void *data,
                   Evas_Object *obj,
                   void *_info EINA_UNUSED)
{
   Termblock *blk = data;

   if (blk->obj != obj) return;

   blk->mov_state = MOVIE_STATE_PAUSE;
}

static void
_smart_media_stop(void *data,
                  Evas_Object *obj,
                  void *_info EINA_UNUSED)
{
   Termblock *blk = data;

   if (blk->obj != obj) return;

   blk->mov_state = MOVIE_STATE_STOP;
}

static void
_block_edje_signal_cb(void *data,
                      Evas_Object *_obj EINA_UNUSED,
                      const char *sig,
                      const char *src)
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
_block_edje_message_cb(void *data,
                       Evas_Object *_obj EINA_UNUSED,
                       Edje_Message_Type type, int id, void *msg)
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

#define ISCMD(cmd) !strcmp(s, cmd)
#define GETS(var) l = l->next; if (!l) return; var = l->data
#define GETI(var) l = l->next; if (!l) return; var = atoi(l->data)
#define GETF(var) l = l->next; if (!l) return; var = (double)atoi(l->data) / 1000.0
   l = cmds;
   while (l)
     {
        char *s = l->data;

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
             if ((size_t)snprintf(path, sizeof(path),
                                  "%s/.terminology/objlib/%s",
                                  home, blk->path) >= sizeof(path))
               {
                  ERR("Not enough space in buffer for path to edje lib file");
                  goto skip;
               }
             ok = edje_object_file_set(blk->obj, path, blk->link);
          }
        if (!ok)
          {
             if ((size_t)snprintf(path, sizeof(path), "%s/objlib/%s",
                                  elm_app_data_dir_get(), blk->path)
                 >= sizeof(path))
               {
                  ERR("Not enough space in buffer for path to edje lib file");
                  goto skip;
               }
             ok = edje_object_file_set(blk->obj, path, blk->link);
          }
     }
skip:
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

void
termio_block_activate(Evas_Object *obj, Termblock *blk)
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

void
termio_imf_cursor_set(Evas_Object *obj, Ecore_IMF_Context *imf)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord cx, cy, cw, ch;

   if (!imf)
     return;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   evas_object_geometry_get(sd->cursor.obj, &cx, &cy, &cw, &ch);
   ecore_imf_context_cursor_location_set(imf, cx, cy, cw, ch);
   ecore_imf_context_cursor_position_set(imf, (sd->cursor.y * sd->grid.w) + sd->cursor.x);
}

void
termio_focus_in(Evas_Object *termio)
{
   Termio *sd = evas_object_smart_data_get(termio);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (sd->config->disable_cursor_blink)
     edje_object_signal_emit(sd->cursor.obj, "focus,in,noblink", "terminology");
   else
     edje_object_signal_emit(sd->cursor.obj, "focus,in", "terminology");
   if (!sd->win) return;
}

void
termio_focus_out(Evas_Object *termio)
{
   Termio *sd = evas_object_smart_data_get(termio);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (!sd->config->disable_focus_visuals)
     edje_object_signal_emit(sd->cursor.obj, "focus,out", "terminology");
   if (!sd->win) return;
   sd->pty->selection.last_click = 0;
   if (!sd->ctxpopup)
     termio_remove_links(sd);
   term_unfocus(sd->term);
}

static void
_smart_mouseover_apply(Termio *sd)
{
   char *s;
   int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
   Eina_Bool same_geom = EINA_FALSE;
   Config *config;
   Termcell *cell = NULL;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   config = sd->config;

   if ((sd->mouse.cx < 0) || (sd->mouse.cy < 0) ||
       (sd->link.suspend) || (!term_is_focused(sd->term)))
     {
        termio_remove_links(sd);
        return;
     }
   cell = termpty_cell_get(sd->pty, sd->mouse.cy - sd->scroll, sd->mouse.cx);
   if (!cell)
     {
        termio_remove_links(sd);
        return;
     }

   if (cell->att.link_id)
     {
        if (config->active_links_escape)
          _hyperlink_mouseover(sd, cell->att.link_id);
        return;
     }

   s = termio_link_find(sd->self, sd->mouse.cx, sd->mouse.cy,
                        &x1, &y1, &x2, &y2);
   if (!s)
     {
        uint8_t r = 0, g = 0, b = 0, a = 0;
        /* TODO: boris: check config */
        if (termio_color_find(sd->self, sd->mouse.cx, sd->mouse.cy,
                              &x1, &y1, &x2, &y2, &r, &g, &b, &a))
          {
             sd->link.is_color = EINA_TRUE;
             sd->link.color.r = r;
             sd->link.color.g = g;
             sd->link.color.b = b;
             sd->link.color.a = a;
             goto found;
          }
        termio_remove_links(sd);
        return;
     }

   if (link_is_url(s))
     {
        if (casestartswith(s, "mailto:"))
          {
             if (!config->active_links_email)
               goto end;
          }
        else
          {
             if (!config->active_links_url)
               goto end;
          }
     }
   else if (s[0] == '/')
     {
        if (!config->active_links_file)
          goto end;
     }
   else if (link_is_email(s))
     {
        if (!config->active_links_email)
          goto end;
     }

   eina_stringshare_del(sd->link.string);
   sd->link.string = eina_stringshare_add(s);

found:
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
   _update_link(sd, same_geom);

end:
   free(s);
}


static Eina_Bool
_smart_mouseover_delay(void *data)
{
   Termio *sd = data;

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EINA_FALSE);
   sd->mouseover_delay = NULL;
   _smart_mouseover_apply(sd);
   return EINA_FALSE;
}


void
termio_smart_cb_mouse_move_job(void *data)
{
   Termio *sd = data;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   sd->mouse_move_job = NULL;
   if (sd->mouseover_delay)
     ecore_timer_reset(sd->mouseover_delay);
   else
     sd->mouseover_delay = ecore_timer_add(0.05, _smart_mouseover_delay, sd);
}


static void
_edje_cb_bottom_right_in(void *data,
                         Evas_Object *_obj EINA_UNUSED,
                         const char *_emission EINA_UNUSED,
                         const char *_source EINA_UNUSED)
{
   Termio *sd = data;

   sd->bottom_right = EINA_TRUE;
}

static void
_edje_cb_top_left_in(void *data,
                     Evas_Object *_obj EINA_UNUSED,
                     const char *_emission EINA_UNUSED,
                     const char *_source EINA_UNUSED)
{
   Termio *sd = data;

   sd->top_left = EINA_TRUE;
}

static void
_edje_cb_bottom_right_out(void *data,
                          Evas_Object *_obj EINA_UNUSED,
                          const char *_emission EINA_UNUSED,
                          const char *_source EINA_UNUSED)
{
   Termio *sd = data;

   sd->bottom_right = EINA_FALSE;
}

static void
_edje_cb_top_left_out(void *data,
                      Evas_Object *_obj EINA_UNUSED,
                      const char *_emission EINA_UNUSED,
                      const char *_source EINA_UNUSED)
{
   Termio *sd = data;

   sd->top_left = EINA_FALSE;
}


static void
_cb_ctxp_sel_copy(void *data,
                  Evas_Object *obj,
                  void *_event EINA_UNUSED)
{
   Evas_Object *term = data;
   Termio *sd = evas_object_smart_data_get(term);

   EINA_SAFETY_ON_NULL_RETURN(sd);

   termio_take_selection(data, ELM_SEL_TYPE_CLIPBOARD);

   sd->ctxpopup = NULL;
   evas_object_del(obj);
}

static void
_cb_ctxp_sel_open_as_url(void *data,
                         Evas_Object *obj,
                         void *_event EINA_UNUSED)
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


void
termio_handle_right_click(Evas_Event_Mouse_Down *ev, Termio *sd,
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
   if (!sd->link.string && !sd->link.is_color)
     evas_object_smart_callback_call(sd->self, "options", NULL);
}

static void
_smart_cb_mouse_down(void *data,
                     Evas *_e EINA_UNUSED,
                     Evas_Object *_obj EINA_UNUSED,
                     void *event)
{
   Evas_Event_Mouse_Down *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   Termio_Modifiers modifiers = {};

   EINA_SAFETY_ON_NULL_RETURN(sd);

   modifiers.alt = evas_key_modifier_is_set(ev->modifiers, "Alt");
   modifiers.shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   modifiers.ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   modifiers.super = evas_key_modifier_is_set(ev->modifiers, "Super");
   modifiers.meta = evas_key_modifier_is_set(ev->modifiers, "Meta");
   modifiers.hyper = evas_key_modifier_is_set(ev->modifiers, "Hyper");
   modifiers.iso_level3_shift = evas_key_modifier_is_set(ev->modifiers, "ISO_Level3_Shift");
   modifiers.altgr= evas_key_modifier_is_set(ev->modifiers, "AltGr");

   termio_internal_mouse_down(sd, ev, modifiers);
}

static void
_smart_cb_mouse_up(void *data,
                   Evas *_e EINA_UNUSED,
                   Evas_Object *_obj EINA_UNUSED,
                   void *event)
{
   Evas_Event_Mouse_Up *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   Termio_Modifiers modifiers = {};

   EINA_SAFETY_ON_NULL_RETURN(sd);

   modifiers.alt = evas_key_modifier_is_set(ev->modifiers, "Alt");
   modifiers.shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   modifiers.ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   modifiers.super = evas_key_modifier_is_set(ev->modifiers, "Super");
   modifiers.meta = evas_key_modifier_is_set(ev->modifiers, "Meta");
   modifiers.hyper = evas_key_modifier_is_set(ev->modifiers, "Hyper");
   modifiers.iso_level3_shift = evas_key_modifier_is_set(ev->modifiers, "ISO_Level3_Shift");
   modifiers.altgr= evas_key_modifier_is_set(ev->modifiers, "AltGr");

   termio_internal_mouse_up(sd, ev, modifiers);
}

static void
_smart_cb_mouse_move(void *data,
                     Evas *_e EINA_UNUSED,
                     Evas_Object *_obj EINA_UNUSED,
                     void *event)
{
   Evas_Event_Mouse_Move *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   Termio_Modifiers modifiers = {};

   EINA_SAFETY_ON_NULL_RETURN(sd);

   modifiers.alt = evas_key_modifier_is_set(ev->modifiers, "Alt");
   modifiers.shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   modifiers.ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   modifiers.super = evas_key_modifier_is_set(ev->modifiers, "Super");
   modifiers.meta = evas_key_modifier_is_set(ev->modifiers, "Meta");
   modifiers.hyper = evas_key_modifier_is_set(ev->modifiers, "Hyper");
   modifiers.iso_level3_shift = evas_key_modifier_is_set(ev->modifiers, "ISO_Level3_Shift");
   modifiers.altgr= evas_key_modifier_is_set(ev->modifiers, "AltGr");

   termio_internal_mouse_move(sd, ev, modifiers);
}

static void
_smart_cb_mouse_in(void *data,
                   Evas *_e EINA_UNUSED,
                   Evas_Object *_obj EINA_UNUSED,
                   void *event)
{
   int cx = 0, cy = 0;
   Evas_Event_Mouse_In *ev = event;
   Termio *sd = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN(sd);
   termio_cursor_to_xy(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);
   sd->mouse.cx = cx;
   sd->mouse.cy = cy;
   termio_mouseover_suspend_pushpop(data, -1);
}

static void
_smart_cb_mouse_out(void *data,
                    Evas *_e EINA_UNUSED,
                    Evas_Object *obj EINA_UNUSED,
                    void *event)
{
   Termio *sd = evas_object_smart_data_get(data);
   Evas_Event_Mouse_Out *ev = event;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   if (sd->ctxpopup) return; /* ctxp triggers mouse out we should ignore */

   termio_mouseover_suspend_pushpop(data, 1);
   if ((ev->canvas.x == 0) || (ev->canvas.y == 0))
     {
        sd->mouse.cx = -1;
        sd->mouse.cy = -1;
        sd->link.suspend = 0;
     }
   else
     {
        int cx = 0, cy = 0;

        termio_cursor_to_xy(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);
        sd->mouse.cx = cx;
        sd->mouse.cy = cy;
     }
   termio_remove_links(sd);

   if (sd->mouseover_delay) ecore_timer_del(sd->mouseover_delay);
   sd->mouseover_delay = NULL;
}

static void
_smart_cb_mouse_wheel(void *data,
                      Evas *_e EINA_UNUSED,
                      Evas_Object *_obj EINA_UNUSED,
                      void *event)
{
   Evas_Event_Mouse_Wheel *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   Termio_Modifiers modifiers = {};

   EINA_SAFETY_ON_NULL_RETURN(sd);

   modifiers.alt = evas_key_modifier_is_set(ev->modifiers, "Alt");
   modifiers.shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   modifiers.ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   modifiers.super = evas_key_modifier_is_set(ev->modifiers, "Super");
   modifiers.meta = evas_key_modifier_is_set(ev->modifiers, "Meta");
   modifiers.hyper = evas_key_modifier_is_set(ev->modifiers, "Hyper");
   modifiers.iso_level3_shift = evas_key_modifier_is_set(ev->modifiers, "ISO_Level3_Shift");
   modifiers.altgr= evas_key_modifier_is_set(ev->modifiers, "AltGr");

   termio_internal_mouse_wheel(sd, ev, modifiers);
}

/* }}} */
/* {{{ Gestures */

static Evas_Event_Flags
_smart_cb_gest_long_move(void *data, void *_event EINA_UNUSED)
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
_smart_cb_gest_zoom_abort(void *data, void *_event EINA_UNUSED)
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

Eina_Bool
termio_file_send_ok(const Evas_Object *obj, const char *file)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Termpty *ty;

   if (!sd) return EINA_FALSE;
   if (!file) return EINA_FALSE;
   ty = sd->pty;
   sd->sendfile.f = fopen(file, "w");
   if (sd->sendfile.f)
     {
        eina_stringshare_del(sd->sendfile.file);
        sd->sendfile.file = eina_stringshare_add(file);
        sd->sendfile.active = EINA_TRUE;
        termpty_write(ty, "k\n", 2);
        return EINA_TRUE;
     }
   eina_stringshare_del(sd->sendfile.file);
   sd->sendfile.file = NULL;
   sd->sendfile.active = EINA_FALSE;
   termpty_write(ty, "n\n", 2);
   return EINA_FALSE;
}

void
termio_file_send_cancel(const Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Termpty *ty;

   if (!sd) return;
   ty = sd->pty;
   if (!sd->sendfile.active) goto done;
   sd->sendfile.progress = 0.0;
   sd->sendfile.total = 0;
   sd->sendfile.size = 0;
   if (sd->sendfile.file)
     {
        ecore_file_unlink(sd->sendfile.file);
        eina_stringshare_del(sd->sendfile.file);
        sd->sendfile.file = NULL;
     }
   if (sd->sendfile.f)
     {
        fclose(sd->sendfile.f);
        sd->sendfile.f = NULL;
     }
   sd->sendfile.active = EINA_FALSE;
done:
   termpty_write(ty, "n\n", 2);
}

double
termio_file_send_progress_get(const Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);

   if (!sd) return 0.0;
   if (!sd->sendfile.active) return 0.0;
   return sd->sendfile.progress;
}

/* {{{ Smart */

static void
_smart_apply(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh;
   int preedit_x = 0, preedit_y = 0;
   Termblock *blk;
   Eina_List *l, *ln;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);

   termio_internal_render(sd,
                          ox, oy,
                          &preedit_x, &preedit_y);

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
   Evas_Coord mw = 0, mh = 0;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   if ((w <= 1) || (h <= 1))
     {
        w = 80;
        h = 24;
     }

   evas_object_size_hint_min_get(obj, &mw, &mh);
   if ((mw != sd->font.chw) || (mh != sd->font.chh))
     evas_object_size_hint_min_set(obj, sd->font.chw, sd->font.chh);

   if (!force)
     {
        if ((w == sd->grid.w) && (h == sd->grid.h)) return;
     }
   sd->grid.w = w;
   sd->grid.h = h;
   evas_event_freeze(evas_object_evas_get(obj));
   evas_object_textgrid_size_set(sd->grid.obj, w, h);
   evas_object_resize(sd->cursor.obj, sd->font.chw, sd->font.chh);
   if (!sd->noreqsize)
     evas_object_size_hint_request_set(obj,
                                       sd->font.chw * sd->grid.w,
                                       sd->font.chh * sd->grid.h);
   termio_sel_set(sd, EINA_FALSE);
   termpty_resize(sd->pty, w, h);

   _smart_calculate(obj);
   _smart_apply(obj);
   evas_event_thaw(evas_object_evas_get(obj));
   evas_event_thaw_eval(evas_object_evas_get(obj));
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

void
termio_smart_update_queue(Termio *sd)
{
   if (sd->anim)
       return;
   sd->anim = ecore_animator_add(_smart_cb_change, sd->self);
}

void
termio_sel_set(Termio *sd, Eina_Bool enable)
{
   if (sd->pty->selection.is_active == enable)
     return;
   sd->pty->selection.is_active = enable;
   if (enable)
     evas_object_smart_callback_call(sd->win, "selection,on", NULL);
   else
     {
        evas_object_smart_callback_call(sd->win, "selection,off", NULL);
        sd->pty->selection.by_word = EINA_FALSE;
        sd->pty->selection.by_line = EINA_FALSE;
        free(sd->pty->selection.codepoints);
        sd->pty->selection.codepoints = NULL;
     }
}


static void
_cursor_cb_move(void *data,
                Evas *_e EINA_UNUSED,
                Evas_Object *_obj EINA_UNUSED,
                void *_event EINA_UNUSED)
{
   Termio *sd = evas_object_smart_data_get(data);
   Ecore_IMF_Context *imf;
   EINA_SAFETY_ON_NULL_RETURN(sd);

   imf = term_imf_context_get(sd->term);
   if (imf)
     termio_imf_cursor_set(sd->self, imf);
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

   sd->link.suspend = 1;

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
   if (sd->sel.top) evas_object_del(sd->sel.top);
   if (sd->sel.bottom) evas_object_del(sd->sel.bottom);
   if (sd->sel.theme) evas_object_del(sd->sel.theme);
   if (sd->anim) ecore_animator_del(sd->anim);
   if (sd->delayed_size_timer) ecore_timer_del(sd->delayed_size_timer);
   if (sd->link_do_timer) ecore_timer_del(sd->link_do_timer);
   if (sd->mouse_move_job) ecore_job_del(sd->mouse_move_job);
   if (sd->mouseover_delay) ecore_timer_del(sd->mouseover_delay);
   eina_stringshare_del(sd->font.name);
   if (sd->pty) termpty_free(sd->pty);
   eina_stringshare_del(sd->link.string);
   if (sd->glayer) evas_object_del(sd->glayer);
   if (sd->win)
     evas_object_event_callback_del_full(sd->win, EVAS_CALLBACK_DEL,
                                         _win_obj_del, obj);
   EINA_LIST_FREE(sd->link.objs, o)
     {
        if (o == sd->link.down.dndobj)
          sd->link.down.dndobj = NULL;
        evas_object_del(o);
     }
   if (sd->link.down.dndobj) evas_object_del(sd->link.down.dndobj);
   if (sd->sendfile.active)
     {
        if (sd->sendfile.file)
          {
             ecore_file_unlink(sd->sendfile.file);
             eina_stringshare_del(sd->sendfile.file);
             sd->sendfile.file = NULL;
          }
        if (sd->sendfile.f)
          {
             fclose(sd->sendfile.f);
             sd->sendfile.f = NULL;
          }
        sd->sendfile.active = EINA_FALSE;
     }
   eina_stringshare_del(sd->sel_str);
   if (sd->sel_reset_job) ecore_job_del(sd->sel_reset_job);
   EINA_LIST_FREE(sd->cur_chids, chid) eina_stringshare_del(chid);
   sd->sel_str = NULL;
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
   sd->win = NULL;
   sd->glayer = NULL;

   _parent_sc.del(obj);
}

static void
_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord ow, oh;

   EINA_SAFETY_ON_NULL_RETURN(sd);
   evas_object_geometry_get(obj, NULL, NULL, &ow, &oh);
   /* Do not resize if same size */
   if ((ow == w) && (oh == h))
     {
        return;
     }
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
_smart_move(Evas_Object *obj,
            Evas_Coord _x EINA_UNUSED,
            Evas_Coord _y EINA_UNUSED)
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
   if (sd->jump_on_change)
     sd->scroll = 0;
   termio_smart_update_queue(sd);
}

static void
_smart_pty_title(void *data)
{
   Evas_Object *obj = data;
   Termio *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN(sd);
   if (!sd->win) return;
   evas_object_smart_callback_call(obj, "title,change", NULL);
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
        termio_sel_set(sd, EINA_FALSE);
        sd->pty->selection.makesel = EINA_FALSE;
        termio_smart_update_queue(sd);
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
_handle_query_esc(Termio *sd)
{
   Termpty *ty = sd->pty;
   if (ty->cur_cmd[1] == 's')
     {
        char buf[256];

        snprintf(buf, sizeof(buf), "%i;%i;%i;%i\n",
                 sd->grid.w, sd->grid.h, sd->font.chw, sd->font.chh);
        termpty_write(ty, buf, strlen(buf));
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
     }
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
        _handle_query_esc(sd);
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
             char *link = NULL;

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
             if (!repch)
               return;

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
   else if (ty->cur_cmd[0] == 'f') // file...
     {
        if (ty->cur_cmd[1] == 'r') // receive
          {
             sd->sendfile.progress = 0.0;
             sd->sendfile.total = 0;
             sd->sendfile.size = 0;
          }
        else if (ty->cur_cmd[1] == 's') // file size
          {
             sd->sendfile.total = 0;
             sd->sendfile.size = atoll(&(ty->cur_cmd[2]));
          }
        else if (ty->cur_cmd[1] == 'd') // data packet
          {
             char *p = strchr(ty->cur_cmd, ' ');
             Eina_Bool valid = EINA_TRUE;

             if (p)
               {
                  Eina_Binbuf *bb = eina_binbuf_new();

                  if (bb)
                    {
                       unsigned char localbuf[128];
                       unsigned char localbufpos = 0;
                       int pksum = atoi(&(ty->cur_cmd[2]));
                       int sum;

                       eina_binbuf_expand(bb, 32 * 1024);
                       for (sum = 0, p++; *p; p++)
                         {
                            // high nibble
                            unsigned char v = (unsigned char)(*p);
                            sum += v;
                            v = ((v - '@') & 0xf) << 4;
                            // low nibble
                            p++;
                            sum += *p;
                            v |= ((*p - '@') & 0xf);
                            localbuf[localbufpos++] = v;
                            if (localbufpos >= sizeof(localbuf))
                              {
                                 eina_binbuf_append_length(bb, localbuf, localbufpos);
                                 localbufpos = 0;
                              }
                            if (!*p) valid = EINA_FALSE;
                         }
                       if (localbufpos > 0)
                         eina_binbuf_append_length(bb, localbuf, localbufpos);

                       if ((valid) && (sum == pksum) && (sd->sendfile.active))
                         {
                            // write "ok" (k) to term
                            size_t size = eina_binbuf_length_get(bb);

                            sd->sendfile.total += size;
                            if (sd->sendfile.size > 0.0)
                              {
                                 sd->sendfile.progress =
                                   (double)sd->sendfile.total /
                                   (double)sd->sendfile.size;
                                 evas_object_smart_callback_call
                                   (obj, "send,progress", NULL);
                              }
                            fwrite(eina_binbuf_string_get(bb), size, 1,
                                   sd->sendfile.f);
                            termpty_write(ty, "k\n", 2);
                         }
                       else
                         {
                            // write "not valid" (n) to term
                            if (sd->sendfile.file)
                              {
                                 ecore_file_unlink(sd->sendfile.file);
                                 eina_stringshare_del(sd->sendfile.file);
                                 sd->sendfile.file = NULL;
                              }
                            if (sd->sendfile.f)
                              {
                                 fclose(sd->sendfile.f);
                                 sd->sendfile.f = NULL;
                              }
                            sd->sendfile.active = EINA_FALSE;
                            termpty_write(ty, "n\n", 2);
                            evas_object_smart_callback_call
                              (obj, "send,end", NULL);
                         }
                       eina_binbuf_free(bb);
                    }
               }
          }
        else if (ty->cur_cmd[1] == 'x') // exit data stream
          {
             if (sd->sendfile.active)
               {
                  sd->sendfile.progress = 0.0;
                  sd->sendfile.size = 0;
                  eina_stringshare_del(sd->sendfile.file);
                  sd->sendfile.file = NULL;
                  if (sd->sendfile.f)
                    {
                       fclose(sd->sendfile.f);
                       sd->sendfile.f = NULL;
                    }
                  sd->sendfile.active = EINA_FALSE;
                  evas_object_smart_callback_call
                    (obj, "send,end", NULL);
               }
          }
     }
   evas_object_smart_callback_call(obj, "command", (void *)ty->cur_cmd);
}

static void
_smart_cb_drag_enter(void *_data EINA_UNUSED, Evas_Object *_o EINA_UNUSED)
{
   DBG("dnd enter");
}

static void
_smart_cb_drag_leave(void *_data EINA_UNUSED, Evas_Object *_o EINA_UNUSED)
{
   DBG("dnd leave");
}

static void
_smart_cb_drag_pos(void *_data EINA_UNUSED,
                   Evas_Object *_o EINA_UNUSED,
                   Evas_Coord x, Evas_Coord y, Elm_Xdnd_Action action)
{
   DBG("dnd at %i %i act:%i", x, y, action);
}

static Eina_Bool
_smart_cb_drop(void *data,
               Evas_Object *_o EINA_UNUSED,
               Elm_Selection_Data *ev)
{
   Evas_Object *obj = data;
   Termio *sd = evas_object_smart_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EINA_TRUE);
   if (ev->action == ELM_XDND_ACTION_COPY)
     {
        if (strchr(ev->data, '\n'))
          {
             char *tb = malloc(strlen(ev->data) + 1);
             if (tb)
               {
                  char *p;
                  for (p = ev->data; p;)
                    {
                       char *p2, *p3;
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

// As we do not control the lifecycle of the Evas_Object *win,
// the death of the gesture layer can happen before the termio
// is destroyed. So by NULLing the structure field on gesture
// death, we make sure to not manipulate dead pointer
static void
_gesture_layer_death(void *data,
                     Evas *e EINA_UNUSED,
                     Evas_Object *obj EINA_UNUSED,
                     void *event EINA_UNUSED)
{
   Termio *sd = data;

   sd->glayer = NULL;
}

/* }}} */


Evas_Object *
termio_add(Evas_Object *win, Config *config,
           const char *cmd, Eina_Bool login_shell, const char *cd,
           int w, int h, Term *term, const char *title)
{
   Evas *e;
   Evas_Object *obj, *g;
   Termio *sd;
   Ecore_Window window_id;

   EINA_SAFETY_ON_NULL_RETURN_VAL(win, NULL);
   e = evas_object_evas_get(win);
   if (!e) return NULL;

   if (!_smart) _smart_init();
   obj = evas_object_smart_add(e, _smart);
   sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, obj);

   sd->cursor.shape = config->cursor_shape;
   termio_config_set(obj, config);
   sd->term = term;
   sd->win = win;

   sd->glayer = g = elm_gesture_layer_add(win);
   evas_object_event_callback_add(g, EVAS_CALLBACK_FREE,
                                  _gesture_layer_death, sd);
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

   elm_drop_target_add(sd->event,
                       ELM_SEL_FORMAT_TEXT | ELM_SEL_FORMAT_IMAGE,
                       _smart_cb_drag_enter, obj,
                       _smart_cb_drag_leave, obj,
                       _smart_cb_drag_pos, obj,
                       _smart_cb_drop, obj);

   window_id = elm_win_window_id_get(win);
   sd->pty = termpty_new(cmd, login_shell, cd, w, h, config, title,
                         window_id);
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
   _smart_size(obj, w, h, EINA_TRUE);
   return obj;
}

void
termio_key_down(Evas_Object *termio,
                Evas_Event_Key_Down *ev,
                Eina_Bool action_handled)
{
   Termio *sd = evas_object_smart_data_get(termio);

   EINA_SAFETY_ON_NULL_RETURN(sd);
   if (sd->jump_on_keypress && !action_handled)
     {
        if (!key_is_modifier(ev->key))
          {
             sd->scroll = 0;
             termio_smart_update_queue(sd);
          }
     }
   if (sd->config->flicker_on_key)
     edje_object_signal_emit(sd->cursor.obj, "key,down", "terminology");
   ev->event_flags = EVAS_EVENT_FLAG_ON_HOLD;
}
