#include <assert.h>
#include <Elementary.h>
#include "win.h"
#include "termcmd.h"
#include "config.h"
#include "main.h"
#include "miniview.h"
#include "gravatar.h"
#include "app_server.h"
#include "media.h"
#include "termio.h"
#include "utils.h"
#include "private.h"
#include "dbus.h"
#include "sel.h"
#include "controls.h"

#if (ELM_VERSION_MAJOR == 1) && (ELM_VERSION_MINOR < 8)
  #define PANES_TOP "left"
  #define PANES_BOTTOM "right"
#else
  #define PANES_TOP "top"
  #define PANES_BOTTOM "bottom"
#endif

/* {{{ Structs */

typedef struct _Term_Container Term_Container;
typedef struct _Split Split;

struct _Term
{
   Win         *wn;
   Config      *config;

   Term_Container *container;
   Evas_Object *bg;
   Evas_Object *base;
   Evas_Object *termio;
   Evas_Object *media;
   Evas_Object *popmedia;
   Evas_Object *miniview;
   //Evas_Object *sel;
   Evas_Object *tabcount_spacer;
   Eina_List   *popmedia_queue;
   Media_Type   poptype, mediatype;
   int          step_x, step_y, min_w, min_h, req_w, req_h;
   struct {
      int       x, y;
   } down;
   unsigned char focused : 1;
   unsigned char hold : 1;
   unsigned char unswallowed : 1;
   unsigned char missed_bell : 1;
   unsigned char miniview_shown : 1;
   unsigned char popmedia_deleted : 1;
};

typedef struct _Sizeinfo Sizeinfo;

struct _Sizeinfo
{
   int min_w;
   int min_h;
   int step_x;
   int step_y;
   int req_w;
   int req_h;
   int bg_min_w;
   int bg_min_h;
   int req;
};

typedef enum _Term_Container_Type
{
   TERM_CONTAINER_TYPE_UNKNOWN,
   TERM_CONTAINER_TYPE_SOLO,
   TERM_CONTAINER_TYPE_SPLIT,
   TERM_CONTAINER_TYPE_TAB,
   TERM_CONTAINER_TYPE_WIN
} Term_Container_Type;

typedef struct _Term_Container Term_Container;
typedef struct _Solo Solo;
typedef struct _Tab Tab;

struct _Term_Container {
     Term_Container_Type type;
     Term_Container *parent;
     Win *wn;

     Term *(*term_next)(Term_Container *tc, Term_Container *child);
     Term *(*term_prev)(Term_Container *tc, Term_Container *child);
     Term *(*term_first)(Term_Container *tc);
     Term *(*term_last)(Term_Container *tc);
     Evas_Object* (*get_evas_object)(Term_Container *container);
     Term *(*find_term_at_coords)(Term_Container *container,
                                  Evas_Coord mx, Evas_Coord my);
     void (*size_eval)(Term_Container *container, Sizeinfo *info);
     void (*split)(Term_Container *container, const char *cmd,
                   Eina_Bool is_horizontal);
     void (*swallow)(Term_Container *container, Term_Container *orig,
                     Term_Container *new_child);
     void (*close)(Term_Container *container, Term_Container *child,
                   Eina_Bool refocus);
};

struct _Solo {
     Term_Container tc;
     Term *term;
};

struct _Tab {
     Term_Container tc;
     /* TODO */
};

struct _Split
{
   Term_Container tc;
   Term_Container *tc1, *tc2; // left/right or top/bottom child splits, null if leaf
   Evas_Object *panes; // null if a leaf node
   unsigned char is_horizontal : 1;
};


struct _Win
{
   Term_Container tc;

   Term_Container *child;
   Evas_Object *win;
   Evas_Object *conform;
   Evas_Object *backbg;
   Evas_Object *base;
   Config      *config;
   Eina_List   *terms;
   Ecore_Job   *size_job;
   Evas_Object *cmdbox;
   Ecore_Timer *cmdbox_del_timer;
   Ecore_Timer *cmdbox_focus_timer;
   unsigned char focused : 1;
   unsigned char cmdbox_up : 1;
};

/* }}} */
static Eina_List   *wins = NULL;


static Term_Container *_solo_new(Term *term, Win *wn);
static Term_Container *_split_new(Term_Container *tc1, Term_Container *tc2, Eina_Bool is_horizontal);
//static void _term_resize_track_start(Term *term);
static void _split_tabcount_update(Split *sp, Term *tm);
static Term * _win_focused_term_get(Win *wn);
static Split * _split_find(Evas_Object *win, Evas_Object *term, Term **ptm);
static void _term_focus(Term *term);
static void term_free(Term *term);
//static void _split_free(Split *sp);
//static void _sel_restore(Split *sp);
static void _sel_go(Split *sp, Term *term);
//static void _term_resize_track_stop(Term *term);
//static void _split_merge(Split *spp, Split *sp, const char *slot);
//static void _term_focus_show(Split *sp, Term *term);
//static void _main_term_bg_redo(Term *term);
static void _term_media_update(Term *term, const Config *config);
static void _term_miniview_check(Term *term);
static void _popmedia_queue_process(Term *term);
static void _cb_size_hint(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event EINA_UNUSED);

#if 0
static Term *
_find_term_under_mouse(Win *wn)
{
   /* TODO */
   Evas_Coord mx, my;
   Split *sp;

   evas_pointer_canvas_xy_get(evas_object_evas_get(wn->win), &mx, &my);

   sp = wn->split;
   while (sp)
     {
        if (sp->term)
          {
            return sp->term;
          }
        else
          {
             Evas_Coord ox, oy, ow, oh;
             Evas_Object *o1 = sp->s1->panes ? sp->s1->panes : sp->s1->term->base;

             evas_object_geometry_get(o1, &ox, &oy, &ow, &oh);
             if (ELM_RECTS_INTERSECT(ox, oy, ow, oh, mx, my, 1, 1))
               {
                  sp = sp->s1;
               }
             else
               {
                  sp = sp->s2;
               }
          }
     }
   return NULL;
}
#endif

static void
_cb_win_focus_in(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Win *wn = data;
   Term *term;

   if (!wn->focused) elm_win_urgent_set(wn->win, EINA_FALSE);
   wn->focused = EINA_TRUE;
   if ((wn->cmdbox_up) && (wn->cmdbox))
     elm_object_focus_set(wn->cmdbox, EINA_TRUE);

   term = _win_focused_term_get(wn);

   if ( wn->config->mouse_over_focus )
     {
        Term *term_mouse;
        Term_Container *tc_win;
        Evas_Coord mx, my;

        tc_win = (Term_Container*) wn;
        evas_pointer_canvas_xy_get(evas_object_evas_get(wn->win), &mx, &my);
        term_mouse = tc_win->find_term_at_coords(tc_win, mx, my);
        if ((term_mouse) && (term_mouse != term))
          {
             if (term)
               {
                  edje_object_signal_emit(term->bg, "focus,out", "terminology");
                  edje_object_signal_emit(term->base, "focus,out", "terminology");
                  if (!wn->cmdbox_up)
                    elm_object_focus_set(term->termio, EINA_FALSE);
               }
             term = term_mouse;
          }
     }

   if (!term) return;

   edje_object_signal_emit(term->bg, "focus,in", "terminology");
   edje_object_signal_emit(term->base, "focus,in", "terminology");
   if (!wn->cmdbox_up)
     elm_object_focus_set(term->termio, EINA_TRUE);
   /* TODO */
#if 0
   sp = _split_find(wn->win, term->term, NULL);
   if (sp->sel)
     {
        if (!wn->cmdbox_up) elm_object_focus_set(sp->sel, EINA_TRUE);
     }
   else
     {
     }
#endif
}

static void
_cb_win_focus_out(void *data, Evas_Object *obj EINA_UNUSED,
                  void *event EINA_UNUSED)
{
   Win *wn = data;
   Term *term;

   wn->focused = EINA_FALSE;
   if ((wn->cmdbox_up) && (wn->cmdbox))
     elm_object_focus_set(wn->cmdbox, EINA_FALSE);
   term = _win_focused_term_get(wn);
   if (!term)
     return;
   edje_object_signal_emit(term->bg, "focus,out", "terminology");
   edje_object_signal_emit(term->base, "focus,out", "terminology");
   if (!wn->cmdbox_up)
     elm_object_focus_set(term->termio, EINA_FALSE);
}

static void
_cb_term_mouse_in(void *data, Evas *e EINA_UNUSED,
                  Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;
   Config *config;

   if ((!term) || (!term->termio))
     return;

   config = termio_config_get(term->termio);
   if ((!config) || (!config->mouse_over_focus))
     return;
   if ((!term->wn) || (!term->wn->focused))
     return;

   term->focused = EINA_TRUE;

   _term_focus(term);
}

static void
_cb_term_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event )
{
   Evas_Event_Mouse_Down *ev = event;
   Term *term = data;
   Term *term2;

   term2 = _win_focused_term_get(term->wn);
   if (term == term2) return;
   term->down.x = ev->canvas.x;
   term->down.y = ev->canvas.y;
   _term_focus(term);
}


/* {{{ Solo */

static Evas_Object *
_solo_get_evas_object(Term_Container *container)
{
   Solo *solo;
   assert (container->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)container;

   return solo->term->bg;
}

static Term *
_solo_find_term_at_coords(Term_Container *container,
                          Evas_Coord mx EINA_UNUSED,
                          Evas_Coord my EINA_UNUSED)
{
   Solo *solo;
   assert (container->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)container;

   return solo->term;
}

static void
_solo_size_eval(Term_Container *container, Sizeinfo *info)
{
   Term *term;
   int mw = 0, mh = 0;
   Solo *solo;
   assert (container->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)container;

   term = solo->term;

   info->min_w = term->min_w;
   info->min_h = term->min_h;
   info->step_x = term->step_x;
   info->step_y = term->step_y;
   info->req_w = term->req_w;
   info->req_h = term->req_h;
   if (!evas_object_data_get(term->termio, "sizedone"))
     {
        evas_object_data_set(term->termio, "sizedone", term->termio);
        info->req = 1;
     }
   evas_object_size_hint_min_get(term->bg, &mw, &mh);
   info->bg_min_w = mw;
   info->bg_min_h = mh;
}

static void
_solo_close(Term_Container *tc, Term_Container *child EINA_UNUSED,
            Eina_Bool refocus)
{
   tc->parent->close(tc->parent, tc, refocus);

   free(tc);
}

static void
_solo_split(Term_Container *tc, const char *cmd, Eina_Bool is_horizontal)
{
   Solo *solo;
   Term *tm_new, *tm;
   Term_Container *tc_split, *tc_new, *tc_parent;
   Win *wn;
   Evas_Object *obj_split;
   char buf[PATH_MAX], *wdir = NULL;

   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;
   tm = solo->term;
   wn = tc->wn;

   tc_parent = tc->parent;

   if (termio_cwd_get(tm->termio, buf, sizeof(buf)))
     wdir = buf;
   tm_new = term_new(wn, wn->config,
                     cmd, wn->config->login_shell, wdir,
                     80, 24, EINA_FALSE);
   tc_new = _solo_new(tm_new, wn);
   evas_object_data_set(tm_new->termio, "sizedone", tm_new->termio);

   /* TODO: focus */

   tc_split = _split_new(tc, tc_new, is_horizontal);

   obj_split = tc_split->get_evas_object(tc_split);

   tc_parent->swallow(tc_parent, tc, tc_split);

   evas_object_show(obj_split);
}

static Term *
_solo_term_next(Term_Container *tc, Term_Container *child EINA_UNUSED)
{
   return tc->parent->term_next(tc->parent, tc);
}

static Term *
_solo_term_prev(Term_Container *tc, Term_Container *child EINA_UNUSED)
{
   return tc->parent->term_prev(tc->parent, tc);
}

static Term *
_solo_term_first(Term_Container *tc)
{
   Solo *solo;
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;
   return solo->term;
}

static Term *
_solo_term_last(Term_Container *tc)
{
   Solo *solo;
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;
   return solo->term;
}

static Term_Container *
_solo_new(Term *term, Win *wn)
{
   Term_Container *tc = NULL;
   Solo *solo = NULL;
   solo = calloc(1, sizeof(Solo));
   if (!solo)
     {
        free(solo);
        return NULL;
     }

   tc = (Term_Container*)solo;
   tc->term_next = _solo_term_next;
   tc->term_prev = _solo_term_prev;
   tc->term_first = _solo_term_first;
   tc->term_last = _solo_term_last;
   tc->get_evas_object = _solo_get_evas_object;
   tc->find_term_at_coords = _solo_find_term_at_coords;
   tc->size_eval = _solo_size_eval;
   tc->split = _solo_split;
   tc->swallow = NULL;
   tc->close= _solo_close;

   tc->parent = NULL;
   tc->wn = wn;

   solo->term = term;
   tc->type = TERM_CONTAINER_TYPE_SOLO;

   term->container = tc;

   return tc;
}

/* }}} */
/* {{{ Win */


int win_term_set(Win *wn, Term *term)
{
   Term_Container *tc_win, *tc_child;
   Evas_Object *base = win_base_get(wn);
   Evas *evas = evas_object_evas_get(base);

   /* TODO: boris tab */

   tc_child = _solo_new(term, wn);
   if (!tc_child) return -1;

   tc_win = (Term_Container*) wn;

   /* TODO: resize track */
   //_term_resize_track_start(sp);

   tc_win->swallow(tc_win, NULL, tc_child);

   _cb_size_hint(term, evas, term->termio, NULL);

   return 0;
}


Evas_Object *
win_base_get(Win *wn)
{
   return wn->base;
}

Config *win_config_get(Win *wn)
{
   return wn->config;
}

Eina_List * win_terms_get(Win *wn)
{
   return wn->terms;
}

Evas_Object *
win_evas_object_get(Win *wn)
{
   return wn->win;
}

static void
_win_trans(Win *wn, Term *term, Eina_Bool trans)
{
   Edje_Message_Int msg;

   if (term->config->translucent)
     msg.val = term->config->opacity;
   else
     msg.val = 100;
   edje_object_message_send(term->bg, EDJE_MESSAGE_INT, 1, &msg);
   edje_object_message_send(term->base, EDJE_MESSAGE_INT, 1, &msg);

   if (trans)
     {
        elm_win_alpha_set(wn->win, EINA_TRUE);
        evas_object_hide(wn->backbg);
     }
   else
     {
        elm_win_alpha_set(wn->win, EINA_FALSE);
        evas_object_show(wn->backbg);
     }
}

void
main_trans_update(const Config *config)
{
   Win *wn;
   Term *term, *term2;
   Eina_List *l, *ll;

   EINA_LIST_FOREACH(wins, l, wn)
     {
        EINA_LIST_FOREACH(wn->terms, ll, term)
          {
             if (term->config == config)
               {
                  if (config->translucent)
                    _win_trans(wn, term, EINA_TRUE);
                  else
                    {
                       Eina_Bool trans_exists = EINA_FALSE;

                       EINA_LIST_FOREACH(wn->terms, ll, term2)
                         {
                            if (term2->config->translucent)
                              {
                                 trans_exists = EINA_TRUE;
                                 break;
                              }
                         }
                       _win_trans(wn, term, trans_exists);
                    }
                  return;
               }
          }
     }
}


static void
_cb_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Win *wn = data;

   // already obj here is deleted - dont do it again
   wn->win = NULL;
   win_free(wn);
}

void
win_free(Win *wn)
{
   Term *term;

   wins = eina_list_remove(wins, wn);
   EINA_LIST_FREE(wn->terms, term)
     {
        term_free(term);
     }
   if (wn->cmdbox_del_timer)
     {
        ecore_timer_del(wn->cmdbox_del_timer);
        wn->cmdbox_del_timer = NULL;
     }
   if (wn->cmdbox_focus_timer)
     {
        ecore_timer_del(wn->cmdbox_focus_timer);
        wn->cmdbox_focus_timer = NULL;
     }
   if (wn->cmdbox)
     {
        evas_object_del(wn->cmdbox);
        wn->cmdbox = NULL;
     }
   if (wn->win)
     {
        evas_object_event_callback_del_full(wn->win, EVAS_CALLBACK_DEL, _cb_del, wn);
        evas_object_del(wn->win);
     }
   if (wn->size_job) ecore_job_del(wn->size_job);
   if (wn->config) config_del(wn->config);
   free(wn);
}


static Win *
_win_find(Evas_Object *win)
{
   Win *wn;
   Eina_List *l;

   EINA_LIST_FOREACH(wins, l, wn)
     {
        if (wn->win == win) return wn;
     }
   return NULL;
}

Eina_List *
terms_from_win_object(Evas_Object *win)
{
   Win *wn;

   wn = _win_find(win);
   if (!wn) return NULL;

   return wn->terms;
}


static Evas_Object *
tg_win_add(const char *name, const char *role, const char *title, const char *icon_name)
{
   Evas_Object *win, *o;
   char buf[4096];

   if (!name) name = "main";
   if (!title) title = "Terminology";
   if (!icon_name) icon_name = "Terminology";

   win = elm_win_add(NULL, name, ELM_WIN_BASIC);
   elm_win_title_set(win, title);
   elm_win_icon_name_set(win, icon_name);
   if (role) elm_win_role_set(win, role);

   evas_object_smart_callback_add(win, "delete,request",
                                  app_server_win_del_request_cb, win);

   elm_win_autodel_set(win, EINA_TRUE);

   o = evas_object_image_add(evas_object_evas_get(win));
   snprintf(buf, sizeof(buf), "%s/images/terminology.png",
            elm_app_data_dir_get());
   evas_object_image_file_set(o, buf, NULL);
   elm_win_icon_object_set(win, o);

   return win;
}

static Evas_Object *
_win_get_evas_object(Term_Container *tc)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;

   return wn->win;
}

static Term *
_win_term_next(Term_Container *tc, Term_Container *child)
{
   return child->term_first(child);
}

static Term *
_win_term_prev(Term_Container *tc, Term_Container *child)
{
   return child->term_last(child);
}

static Term *
_win_term_first(Term_Container *tc)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;
   return wn->child->term_first(wn->child);
}

static Term *
_win_term_last(Term_Container *tc)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;
   return wn->child->term_last(wn->child);
}

static Term *
_win_find_term_at_coords(Term_Container *tc,
                         Evas_Coord mx, Evas_Coord my)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;

   return wn->child->find_term_at_coords(wn->child, mx, my);
}

static void
_win_size_eval(Term_Container *tc, Sizeinfo *info)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;

   wn->child->size_eval(wn->child, info);
}

static void
_win_swallow(Term_Container *tc, Term_Container *orig,
             Term_Container *child)
{
   Win *wn;
   Evas_Object *base;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;
   base = win_base_get(wn);

   if (orig)
     {
        edje_object_part_unswallow(base,
                                   orig->get_evas_object(orig));
     }
   edje_object_part_swallow(base, "terminology.content",
                            child->get_evas_object(child));
   child->parent = tc;
   wn->child = child;
}

static void
_win_close(Term_Container *tc, Term_Container *child EINA_UNUSED,
           Eina_Bool refocus EINA_UNUSED)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;

   win_free(wn);
}

Win *
win_new(const char *name, const char *role, const char *title,
        const char *icon_name, Config *config,
        Eina_Bool fullscreen, Eina_Bool iconic,
        Eina_Bool borderless, Eina_Bool override,
        Eina_Bool maximized)
{
   Win *wn;
   Evas_Object *o;
   Term_Container *tc;

   wn = calloc(1, sizeof(Win));
   if (!wn) return NULL;

   wn->win = tg_win_add(name, role, title, icon_name);
   if (!wn->win)
     {
        free(wn);
        return NULL;
     }

   tc = (Term_Container*) wn;
   tc->term_next = _win_term_next;
   tc->term_prev = _win_term_prev;
   tc->term_first = _win_term_first;
   tc->term_last = _win_term_last;
   tc->get_evas_object = _win_get_evas_object;
   tc->find_term_at_coords = _win_find_term_at_coords;
   tc->size_eval = _win_size_eval;
   tc->split = NULL;
   tc->swallow = _win_swallow;
   tc->close = _win_close;
   tc->type = TERM_CONTAINER_TYPE_WIN;

   config_default_font_set(config, evas_object_evas_get(wn->win));

   wn->config = config_fork(config);

   evas_object_event_callback_add(wn->win, EVAS_CALLBACK_DEL, _cb_del, wn);

   if (fullscreen) elm_win_fullscreen_set(wn->win, EINA_TRUE);
   if (iconic) elm_win_iconified_set(wn->win, EINA_TRUE);
   if (borderless) elm_win_borderless_set(wn->win, EINA_TRUE);
   if (override) elm_win_override_set(wn->win, EINA_TRUE);
   if (maximized) elm_win_maximized_set(wn->win, EINA_TRUE);

   wn->backbg = o = evas_object_rectangle_add(evas_object_evas_get(wn->win));
   evas_object_color_set(o, 0, 0, 0, 255);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_win_resize_object_add(wn->win, o);
   evas_object_show(o);

   wn->conform = o = elm_conformant_add(wn->win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_win_resize_object_add(wn->win, o);
   evas_object_show(o);

   wn->base = o = edje_object_add(evas_object_evas_get(wn->win));
   theme_apply(o, config, "terminology/base");
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(wn->conform, o);
   evas_object_show(o);

   evas_object_smart_callback_add(wn->win, "focus,in", _cb_win_focus_in, wn);
   evas_object_smart_callback_add(wn->win, "focus,out", _cb_win_focus_out, wn);

   wins = eina_list_append(wins, wn);
   return wn;
}

void
main_close(Evas_Object *win, Evas_Object *term)
{
   Term *tm;
   Term_Container *tc;
   Win *wn = _win_find(win);

   if (!wn) return;

   app_server_term_del(term);

   tm = evas_object_data_get(term, "term");
   if (!tm) return;

   wn->terms = eina_list_remove(wn->terms, tm);
   tc = tm->container;
   tc->close(tc, tc, tm->focused);

   term_free(tm);

#if 0
   Split *sp = _split_find(win, term, &tm);
   Split *spp, *spkeep = NULL;
   Eina_List *l;
   const char *slot = PANES_TOP;

   if (!sp) return;
   if (!sp->term) return;
   if (sp->sel) _sel_restore(sp);
   spp = sp->parent;


   if (spp)
     {
        if (eina_list_count(sp->terms) <= 1)
          {
             if (sp == spp->s2)
               {
                  spkeep = spp->s1;
                  spp->s2 = NULL;
               }
             else
               {
                  spkeep = spp->s2;
                  spp->s1 = NULL;
               }
          }
        l = eina_list_data_find_list(sp->terms, tm);
        _term_resize_track_stop(sp);
        term_free(tm);
        if (l)
          {
             if (tm == sp->term)
               {
                  if (l->next) sp->term = l->next->data;
                  else if (l->prev) sp->term = l->prev->data;
                  else sp->term = NULL;
               }
             sp->terms = eina_list_remove_list(sp->terms, l);
          }
        else
          {
             sp->term = NULL;
          }
        if (!sp->term)
          {
             _split_free(sp);
             sp = NULL;
             if ((spp->parent) && (spp->parent->s2 == spp))
               slot = PANES_BOTTOM;
             _split_merge(spp, spkeep, slot);

             if (term_was_focused)
               {
                  tm = spp->term;
                  sp = spp;
                  while (tm == NULL)
                    {
                       tm = spp->term;
                       sp = spp;
                       spp = spp->s1;
                    }
                  _term_focus(tm);
                  _term_focus_show(sp, tm);
               }
          }
        else
          {
             _term_resize_track_start(sp);
             if ((sp->parent) && (sp->parent->s2 == sp)) slot = PANES_BOTTOM;
             elm_object_part_content_set(sp->parent->panes, slot,
                                         sp->term->bg);
             evas_object_show(sp->term->bg);
             if (term_was_focused)
               {
                  _term_focus(sp->term);
                  _term_focus_show(sp, sp->term);
               }
          }
        if (sp) _split_tabcount_update(sp, sp->term);
     }
   else
     {
        _term_resize_track_stop(sp);
        edje_object_part_unswallow(sp->wn->base, sp->term->bg);
        l = eina_list_data_find_list(sp->terms, tm);
        term_free(tm);
        if (l)
          {
             if (tm == sp->term)
               {
                  if (l->next) sp->term = l->next->data;
                  else if (l->prev) sp->term = l->prev->data;
                  else sp->term = NULL;
               }
             sp->terms = eina_list_remove_list(sp->terms, l);
          }
        else
          {
             sp->term = NULL;
          }
        if (sp->term)
          {
             _term_resize_track_start(sp);
             edje_object_part_swallow(sp->wn->base, "terminology.content",
                                      sp->term->bg);
             evas_object_show(sp->term->bg);
             _term_focus(sp->term);
             _term_focus_show(sp, sp->term);
          }
        if (!sp->wn->terms) evas_object_del(sp->wn->win);
        else _split_tabcount_update(sp, sp->term);
     }
#endif
}

static Term *
_win_focused_term_get(Win *wn)
{
   Term *term;
   Eina_List *l;
   /* TODO: have that property in Win */

   EINA_LIST_FOREACH(wn->terms, l, term)
     {
        if (term->focused) return term;
     }
   return NULL;
}


/* }}} */
/* {{{ Splits */

static Term *
_split_term_next(Term_Container *tc, Term_Container *child)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   if (child == split->tc1)
     return split->tc2->term_first(split->tc2);
   else
     return tc->parent->term_next(tc->parent, tc);
}

static Term *
_split_term_prev(Term_Container *tc, Term_Container *child)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   if (child == split->tc2)
     return split->tc1->term_last(split->tc1);
   else
     return tc->parent->term_prev(tc->parent, tc);
}

static Term *
_split_term_first(Term_Container *tc)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   return split->tc1->term_first(split->tc1);
}

static Term *
_split_term_last(Term_Container *tc)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   return split->tc2->term_last(split->tc2);
}


static Evas_Object *
_split_get_evas_object(Term_Container *tc)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   return split->panes;
}

static void
_split_size_eval(Term_Container *tc, Sizeinfo *info)
{
   Evas_Coord mw = 0, mh = 0;
   Term_Container *tc1, *tc2;
   Sizeinfo inforet = {0, 0, 0, 0, 0, 0, 0, 0, 0};
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   tc1 = split->tc1;
   tc2 = split->tc2;

   info->min_w = 0;
   info->min_h = 0;
   info->req_w = 0;
   info->req_h = 0;

   evas_object_size_hint_min_get(split->panes, &mw, &mh);
   info->bg_min_w = mw;
   info->bg_min_h = mh;

   if (split->is_horizontal)
     {
        tc1->size_eval(tc1, &inforet);
        info->req |= inforet.req;
        mh -= inforet.min_h;
        if (info->req)
          {
             info->req_h += inforet.req_h;
             info->req_w = inforet.req_w;
          }

        tc2->size_eval(tc2, &inforet);
        info->req |= inforet.req;
        mh -= inforet.min_h;
        if (info->req)
          {
             info->req_h += inforet.req_h;
             info->req_w = inforet.req_w;
          }
        info->req_h += mh;
        if (info->req)
          info->req_w += mw - inforet.min_w - inforet.step_x;
     }
   else
     {
        tc1->size_eval(tc1, &inforet);
        info->req |= inforet.req;
        mw -= inforet.min_w;
        if (info->req)
          {
             info->req_w += inforet.req_w;
             info->req_h = inforet.req_h;
          }

        tc2->size_eval(tc2, &inforet);
        info->req |= inforet.req;
        mw -= inforet.min_w;
        if (info->req)
          {
             info->req_w += inforet.req_w;
             info->req_h = inforet.req_h;
          }
        info->req_w += mw;
        if (info->req)
          info->req_h += mh - inforet.min_h - inforet.step_y;
     }
   info->step_x = inforet.step_x;
   info->step_y = inforet.step_y;
}

static void
_split_swallow(Term_Container *tc, Term_Container *orig,
               Term_Container *new_child)
{
   Split *split;
   Evas_Object *o;
   Evas_Coord x, y, w, h;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   /* TODO: shouldn't have to do that… */
   o = orig->get_evas_object(orig);
   evas_object_geometry_get(o, &x, &y, &w, &h);

   o = new_child->get_evas_object(new_child);
   if (split->tc1 == orig)
     {
        elm_object_part_content_unset(split->panes, PANES_TOP);
        elm_object_part_content_set(split->panes, PANES_TOP, o);
        split->tc1 = new_child;
     }
   else
     {
        elm_object_part_content_unset(split->panes, PANES_BOTTOM);
        elm_object_part_content_set(split->panes, PANES_BOTTOM, o);
        split->tc2 = new_child;
     }
   new_child->parent = tc;
   evas_object_geometry_set(o, x, y, w, h);
   evas_object_show(o);
   evas_object_show(split->panes);
}

static Term *
_split_find_term_at_coords(Term_Container *tc,
                           Evas_Coord mx, Evas_Coord my)
{
   Split *split;
   Evas_Coord ox, oy, ow, oh;
   Evas_Object *o;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   o = split->tc1->get_evas_object(split->tc1);

   evas_object_geometry_get(o, &ox, &oy, &ow, &oh);
   if (ELM_RECTS_INTERSECT(ox, oy, ow, oh, mx, my, 1, 1))
     {
        tc = split->tc1;
     }
   else
     {
        tc = split->tc2;
     }

   return tc->find_term_at_coords(tc, mx, my);
}

static void
_split_close(Term_Container *tc, Term_Container *child,
             Eina_Bool refocus EINA_UNUSED)
{
   Split *split;
   Term_Container *parent;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   /* TODO: refocus */
   elm_object_part_content_unset(split->panes, PANES_TOP);
   elm_object_part_content_unset(split->panes, PANES_BOTTOM);

   parent = tc->parent;
   if (child == split->tc1)
     {
        parent->swallow(parent, tc, split->tc2);
     }
   else
     {
        parent->swallow(parent, tc, split->tc1);
     }

   evas_object_del(split->panes);

   free(tc);
}

static Term_Container *
_split_new(Term_Container *tc1, Term_Container *tc2, Eina_Bool is_horizontal)
{
   Evas_Object *o;
   Term_Container *tc = NULL;
   Split *split = NULL;
   split = calloc(1, sizeof(Split));
   if (!split)
     {
        free(split);
        return NULL;
     }

   tc = (Term_Container*)split;
   tc->term_next = _split_term_next;
   tc->term_prev = _split_term_prev;
   tc->term_first = _split_term_first;
   tc->term_last = _split_term_last;
   tc->get_evas_object = _split_get_evas_object;
   tc->find_term_at_coords = _split_find_term_at_coords;
   tc->size_eval = _split_size_eval;
   tc->split = NULL;
   tc->swallow = _split_swallow;
   tc->close = _split_close;

   tc->parent = NULL;
   tc->wn = tc1->wn;

   tc->type = TERM_CONTAINER_TYPE_SPLIT;

   tc1->parent = tc2->parent = tc;

   split->tc1 = tc1;
   split->tc2 = tc2;

   o = split->panes = elm_panes_add(tc1->wn->win);
   elm_object_style_set(o, "flush");
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

   split->is_horizontal = is_horizontal;
   elm_panes_horizontal_set(o, split->is_horizontal);

   elm_object_part_content_set(o, PANES_TOP,
                               tc1->get_evas_object(tc1));
   elm_object_part_content_set(o, PANES_BOTTOM,
                               tc2->get_evas_object(tc2));

   return tc;
}

#if 0
static void
_split_size_walk(Split *sp, Sizeinfo *info)
{
   Sizeinfo inforet = { 0, 0, 0, 0, 0, 0, 0 };

   if (sp->term)
     {
        info->min_w = sp->term->min_w;
        info->min_h = sp->term->min_h;
        info->step_x = sp->term->step_x;
        info->step_y = sp->term->step_y;
        info->req_w = sp->term->req_w;
        info->req_h = sp->term->req_h;
        // XXXX sp->terms sizedone too?
        if (!evas_object_data_get(sp->term->term, "sizedone"))
          {
             evas_object_data_set(sp->term->term, "sizedone", sp->term->term);
             info->req = 1;
          }
     }
   else
     {
        Evas_Coord mw = 0, mh = 0;

        info->min_w = 0;
        info->min_h = 0;
        info->req_w = 0;
        info->req_h = 0;
        evas_object_size_hint_min_get(sp->panes, &mw, &mh);
        if (!sp->horizontal)
          {
             _split_size_walk(sp->s1, &inforet);
             info->req |= inforet.req;
             mw -= inforet.min_w;
             if (info->req)
               {
                  info->req_w += inforet.req_w;
                  info->req_h = inforet.req_h;
               }

             _split_size_walk(sp->s2, &inforet);
             info->req |= inforet.req;
             mw -= inforet.min_w;
             if (info->req)
               {
                  info->req_w += inforet.req_w;
                  info->req_h = inforet.req_h;
               }
             info->req_w += mw;
             if (info->req) info->req_h += mh - inforet.min_h - inforet.step_y;
          }
        else
          {
             _split_size_walk(sp->s1, &inforet);
             info->req |= inforet.req;
             mh -= inforet.min_h;
             if (info->req)
               {
                  info->req_h += inforet.req_h;
                  info->req_w = inforet.req_w;
               }

             _split_size_walk(sp->s2, &inforet);
             info->req |= inforet.req;
             mh -= inforet.min_h;
             if (info->req)
               {
                  info->req_h += inforet.req_h;
                  info->req_w = inforet.req_w;
               }
             info->req_h += mh;
             if (info->req) info->req_w += mw - inforet.min_w - inforet.step_x;
         }
        info->step_x = inforet.step_x;
        info->step_y = inforet.step_y;
     }
}
#endif

static void
_size_job(void *data)
{
   Win *wn = data;
   Sizeinfo info = {0, 0, 0, 0, 0, 0, 0, 0, 0};
   Term_Container *tc = (Term_Container*) wn;

   wn->size_job = NULL;
   tc->size_eval(tc, &info);

   elm_win_size_base_set(wn->win,
                         info.bg_min_w - info.step_x,
                         info.bg_min_h - info.step_y);
   elm_win_size_step_set(wn->win, info.step_x, info.step_y);
   evas_object_size_hint_min_set(wn->backbg,
                                 info.bg_min_w,
                                 info.bg_min_h);
   if (info.req) evas_object_resize(wn->win, info.req_w, info.req_h);
}

void
win_sizing_handle(Win *wn)
{
   if (wn->size_job) ecore_job_del(wn->size_job);
   _size_job(wn);
}

static void
_cb_size_hint(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event EINA_UNUSED)
{
   Term *term = data;
   Evas_Coord mw, mh, rw, rh, w = 0, h = 0;

   evas_object_size_hint_min_get(obj, &mw, &mh);
   evas_object_size_hint_request_get(obj, &rw, &rh);
   edje_object_size_min_calc(term->base, &w, &h);
   evas_object_size_hint_min_set(term->base, w, h);
   edje_object_size_min_calc(term->bg, &w, &h);
   evas_object_size_hint_min_set(term->bg, w, h);
   term->step_x = mw;
   term->step_y = mh;
   term->min_w = w - mw;
   term->min_h = h - mh;
   term->req_w = w - mw + rw;
   term->req_h = h - mh + rh;

   if (term->wn->size_job) ecore_job_del(term->wn->size_job);
   term->wn->size_job = ecore_job_add(_size_job, term->wn);
}

#if 0
static Split *
_split_split_find(Split *sp, Evas_Object *term, Term **ptm)
{
   Split *sp2;
   Eina_List *l;
   Term *tm;

   if (sp->term)
     {
        if (sp->term->term == term)
          {
             if (ptm) *ptm = sp->term;
             return sp;
          }
        EINA_LIST_FOREACH(sp->terms, l, tm)
          {
             if (tm->term == term)
               {
                  if (ptm) *ptm = tm;
                  return sp;
               }
          }
     }
   if (sp->s1)
     {
        sp2 = _split_split_find(sp->s1, term, ptm);
        if (sp2) return sp2;
     }
   if (sp->s2)
     {
        sp2 = _split_split_find(sp->s2, term, ptm);
        if (sp2) return sp2;
     }
   return NULL;
}
#endif

static Split *
_split_find(Evas_Object *win EINA_UNUSED, Evas_Object *term EINA_UNUSED, Term **ptm EINA_UNUSED)
{
   /* TODO */
#if 0
   Win *wn;
   Eina_List *l;

   EINA_LIST_FOREACH(wins, l, wn)
     {
        if (wn->win == win) return _split_split_find(wn->split, term, ptm);
     }
#endif
   return NULL;
}

#if 0
static void
_split_free(Split *sp)
{
   if (sp->s1) _split_free(sp->s1);
   if (sp->s2) _split_free(sp->s2);
   if (sp->panes) evas_object_del(sp->panes);
   free(sp);
}
#endif

static void
_split_tabcount_update(Split *sp EINA_UNUSED, Term *tm EINA_UNUSED)
{
   /* TODO */
#if 0
   char buf[32], bufm[32];
   int n = eina_list_count(sp->terms);
   int missed = 0;
   int cnt = 0, term_cnt = 0;
   Eina_List *l;
   Term *term;

   EINA_LIST_FOREACH(sp->terms, l, term)
     {
        if (term->missed_bell) missed++;

        cnt++;
        if (tm == term) term_cnt = cnt;
     }
   snprintf(buf, sizeof(buf), "%i/%i", term_cnt, n);
   if (missed > 0) snprintf(bufm, sizeof(bufm), "%i", missed);
   else bufm[0] = 0;
   EINA_LIST_FOREACH(sp->terms, l, term)
     {
        Evas_Coord w = 0, h = 0;

        if (!term->tabcount_spacer)
          {
             term->tabcount_spacer = evas_object_rectangle_add(evas_object_evas_get(term->bg));
             evas_object_color_set(term->tabcount_spacer, 0, 0, 0, 0);
          }
        elm_coords_finger_size_adjust(1, &w, 1, &h);
        evas_object_size_hint_min_set(term->tabcount_spacer, w, h);
        edje_object_part_swallow(term->bg, "terminology.tabcount.control", term->tabcount_spacer);
        if (n > 1)
          {
             edje_object_part_text_set(term->bg, "terminology.tabcount.label", buf);
             edje_object_part_text_set(term->bg, "terminology.tabmissed.label", bufm);
             edje_object_signal_emit(term->bg, "tabcount,on", "terminology");
          }
        else
          edje_object_signal_emit(term->bg, "tabcount,off", "terminology");
        if (missed > 0)
          edje_object_signal_emit(term->bg, "tabmissed,on", "terminology");
        else
          edje_object_signal_emit(term->bg, "tabmissed,off", "terminology");
     }
#endif
}

#if 0
static void
_split_split(Split *sp, Eina_Bool horizontal, char *cmd)
{
   /* TODO: boris */
   Split *sp2, *sp1;
   Evas_Object *o;
   Config *config;
   char buf[PATH_MAX], *wdir = NULL;

   if (!sp->term) return;

   o = sp->panes = elm_panes_add(sp->wn->win);
   elm_object_style_set(o, "flush");
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   sp->horizontal = horizontal;
   elm_panes_horizontal_set(o, sp->horizontal);

   _term_resize_track_stop(sp->term);
   sp1 = sp->s1 = calloc(1, sizeof(Split));
   sp1->parent = sp;
   sp1->wn = sp->wn;
   sp1->term = sp->term;
   sp1->terms = sp->terms;
   _term_resize_track_start(sp1->term);

   sp->terms = NULL;

   if (!sp->parent) edje_object_part_unswallow(sp->wn->base, sp->term->bg);
   _main_term_bg_redo(sp1->term);
   _split_tabcount_update(sp1, sp1->term);

   sp2 = sp->s2 = calloc(1, sizeof(Split));
   sp2->parent = sp;
   sp2->wn = sp->wn;
   config = config_fork(sp->term->config);
   if (termio_cwd_get(sp->term->term, buf, sizeof(buf))) wdir = buf;
   sp2->term = term_new(sp->wn, config,
                        cmd, config->login_shell, wdir,
                        80, 24, EINA_FALSE);
   sp2->terms = eina_list_append(sp2->terms, sp2->term);
   _term_resize_track_start(sp2->term);
   _term_focus(sp2->term);
   _term_media_update(sp2->term, config);
   _split_tabcount_update(sp2, sp2->term);
   evas_object_data_set(sp2->term->term, "sizedone", sp2->term->term);
   elm_object_part_content_set(sp->panes, PANES_TOP, sp1->term->bg);
   elm_object_part_content_set(sp->panes, PANES_BOTTOM, sp2->term->bg);

   if (!sp->parent)
     edje_object_part_swallow(sp->wn->base, "terminology.content", sp->panes);
   else
     {
        if (sp == sp->parent->s1)
          {
             elm_object_part_content_unset(sp->parent->panes, PANES_TOP);
             elm_object_part_content_set(sp->parent->panes, PANES_TOP, sp->panes);
          }
        else
          {
             elm_object_part_content_unset(sp->parent->panes, PANES_BOTTOM);
             elm_object_part_content_set(sp->parent->panes, PANES_BOTTOM, sp->panes);
          }
     }
   evas_object_show(sp->panes);
   sp->term = NULL;
}
#endif

#if 0
static void
_term_focus_show(Split *sp, Term *term)
{
   if (term != sp->term)
     {
        _term_resize_track_stop(sp);
        evas_object_hide(sp->term->bg);
        sp->term = term;
        _term_resize_track_start(sp);
     }
   if (!sp->parent)
     edje_object_part_swallow(sp->wn->base, "terminology.content",
                              sp->term->bg);
   else
     {
        if (sp == sp->parent->s1)
          {
             elm_object_part_content_unset(sp->parent->panes, PANES_TOP);
             elm_object_part_content_set(sp->parent->panes, PANES_TOP,
                                         sp->term->bg);
          }
        else
          {
             elm_object_part_content_unset(sp->parent->panes, PANES_BOTTOM);
             elm_object_part_content_set(sp->parent->panes, PANES_BOTTOM,
                                         sp->term->bg);
          }
     }
   evas_object_show(sp->term->bg);
}
#endif

void
main_new_with_dir(Evas_Object *win EINA_UNUSED,
                  Evas_Object *term EINA_UNUSED,
                  const char *wdir EINA_UNUSED)
{
   /* TODO */
#if 0
   Split *sp = _split_find(win, term, NULL);
   Config *config;
   int w, h;

   if (!sp) return;
   _term_resize_track_stop(sp->term);
   evas_object_hide(sp->term->bg);
   config = config_fork(sp->term->config);
   termio_size_get(sp->term->term, &w, &h);
   sp->term = term_new(sp->wn, config,
                            NULL, config->login_shell, wdir,
                            w, h, EINA_FALSE);
   sp->terms = eina_list_append(sp->terms, sp->term);
   _term_resize_track_start(sp->term);
   _term_focus(sp->term);
   _term_media_update(sp->term, config);
   evas_object_data_set(sp->term->term, "sizedone", sp->term->term);
   //_term_focus_show(sp, sp->term);
   _split_tabcount_update(sp, sp->term);
#endif
}

void
main_new(Evas_Object *win EINA_UNUSED, Evas_Object *term EINA_UNUSED)
{
   DBG("TODO");
   /* TODO → tabs */
#if 0
   Split *sp = _split_find(win, term, NULL);
   char buf[PATH_MAX], *wdir = NULL;

   if (termio_cwd_get(sp->term->term, buf, sizeof(buf))) wdir = buf;
   main_new_with_dir(win, term, wdir);
#endif
}

void
split_horizontally(Evas_Object *win EINA_UNUSED, Evas_Object *term,
                   const char *cmd)
{
   Term *tm;
   Term_Container *tc;

   tm = evas_object_data_get(term, "term");
   if (!tm) return;

   tc = tm->container;
   tc->split(tc, cmd, EINA_TRUE);
}

void
split_vertically(Evas_Object *win EINA_UNUSED, Evas_Object *term,
                 const char *cmd)
{
   Term *tm;
   Term_Container *tc;

   tm = evas_object_data_get(term, "term");
   if (!tm) return;

   tc = tm->container;
   tc->split(tc, cmd, EINA_FALSE);
}

#if 0
static void
_split_append(Split *sp, Eina_List **flat)
{
   if (sp->term)
     *flat = eina_list_append(*flat, sp);
   else
     {
        _split_append(sp->s1, flat);
        _split_append(sp->s2, flat);
     }
}
#endif

#if 0
static Eina_List *
_split_flatten(Split *sp)
{
   Eina_List *flat = NULL;

   _split_append(sp, &flat);
   return flat;
}
#endif

static Term *
_term_next_get(Term *termin)
{
   /* TODO */
#if 0
   Split *sp;
   Eina_List *flat, *l;

   sp = _split_find(termin->wn->win, termin->term, NULL);
   l = eina_list_data_find_list(sp->terms, termin);
   if ((l) && (l->next)) return l->next->data;
   if (!sp->parent) return sp->terms->data;
   flat = _split_flatten(termin->wn->split);
   if (!flat) return NULL;
   l = eina_list_data_find_list(flat, sp);
   if (!l)
     {
        eina_list_free(flat);
        return NULL;
     }
   if (l->next)
     {
        sp = l->next->data;
        eina_list_free(flat);
        if (sp->terms) return sp->terms->data;
        return sp->term;
     }
   sp = flat->data;
   eina_list_free(flat);
   if (sp->terms) return sp->terms->data;
   return sp->term;
#endif
   return termin;
}

static Term *
_term_prev_get(Term *termin)
{
   /* TODO */
#if 0
   Split *sp;
   Eina_List *flat, *l;

   sp = _split_find(termin->wn->win, termin->term, NULL);
   l = eina_list_data_find_list(sp->terms, termin);
   if ((l) && (l->prev)) return l->prev->data;
   if (!sp->parent) return eina_list_data_get(eina_list_last(sp->terms));
   flat = _split_flatten(termin->wn->split);
   if (!flat) return NULL;
   l = eina_list_data_find_list(flat, sp);
   if (!l)
     {
        eina_list_free(flat);
        return NULL;
     }
   if (l->prev)
     {
        sp = l->prev->data;
        eina_list_free(flat);
        l = eina_list_last(sp->terms);
        if (l) return l->data;
        return sp->term;
     }
#if (EINA_VERSION_MAJOR > 1) || (EINA_VERSION_MINOR >= 8)
   sp = eina_list_last_data_get(flat);
#else
   sp = eina_list_data_get(eina_list_last(flat));
#endif
   eina_list_free(flat);
   l = eina_list_last(sp->terms);
   if (l) return l->data;
   return sp->term;
#endif
   return termin;
}

#if 0
static void
_split_merge(Split *spp, Split *sp, const char *slot)
{
   Evas_Object *o = NULL;
   if (!sp) return;

   if (sp->term)
     {
        _main_term_bg_redo(sp->term);
        _term_resize_track_stop(sp->term);
        spp->term = sp->term;
        spp->terms = sp->terms;
        sp->term = NULL;
        sp->terms = NULL;
        _term_resize_track_start(sp->term);
        o = spp->term->bg;
        spp->s1 = NULL;
        spp->s2 = NULL;
        evas_object_del(spp->panes);
        spp->panes = NULL;
        if (spp->parent)
          {
             elm_object_part_content_unset(spp->parent->panes, slot);
             elm_object_part_content_set(spp->parent->panes, slot, o);
          }
        else
          edje_object_part_swallow(spp->wn->base, "terminology.content", o);
        _split_tabcount_update(sp, sp->term);
     }
   else
     {
        spp->s1 = sp->s1;
        spp->s2 = sp->s2;
        spp->s1->parent = spp;
        spp->s2->parent = spp;
        spp->horizontal = sp->horizontal;
        o = sp->panes;
        elm_object_part_content_unset(sp->parent->panes, slot);
        elm_object_part_content_unset(sp->parent->panes,
                                      (!strcmp(slot, PANES_TOP)) ?
                                      PANES_BOTTOM : PANES_TOP);
        if (spp->parent)
          {
             elm_object_part_content_unset(spp->parent->panes, slot);
             elm_object_part_content_set(spp->parent->panes, slot, o);
          }
        else
          edje_object_part_swallow(spp->wn->base, "terminology.content", o);
        evas_object_del(spp->panes);
        spp->panes = o;
        sp->s1 = NULL;
        sp->s2 = NULL;
        sp->panes = NULL;
     }
   _split_free(sp);
}
#endif

/* }}} */
/* {{{ Term */

#if 0
static void
_cb_size_track(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event EINA_UNUSED)
{
   Term *term = data;
   Evas_Coord w = 0, h = 0;

   evas_object_geometry_get(obj, NULL, NULL, &w, &h);
   /* TODO resize other terms */
   if (term->bg != obj) evas_object_resize(term->bg, w, h);
}

static void
_term_resize_track_start(Term *term)
{
   if ((!term) || (!term->bg)) return;
   evas_object_event_callback_del_full(term->bg, EVAS_CALLBACK_RESIZE,
                                       _cb_size_track, term);
   evas_object_event_callback_add(term->bg, EVAS_CALLBACK_RESIZE,
                                  _cb_size_track, term);
}

static void
_term_resize_track_stop(Term *term)
{
   if ((!term) || (!term->bg)) return;
   evas_object_event_callback_del_full(term->bg, EVAS_CALLBACK_RESIZE,
                                       _cb_size_track, term);
}
#endif


void change_theme(Evas_Object *win, Config *config)
{
   const Eina_List *terms, *l;
   Term *term;

   terms = terms_from_win_object(win);
   if (!terms) return;

   EINA_LIST_FOREACH(terms, l, term)
     {
        Evas_Object *edje = termio_theme_get(term->termio);

        if (!theme_apply(edje, config, "terminology/background"))
          ERR("Couldn't find terminology theme!");
        colors_term_init(termio_textgrid_get(term->termio), edje, config);
        termio_config_set(term->termio, config);
     }

   l = elm_theme_overlay_list_get(NULL);
   if (l) l = eina_list_last(l);
   if (l) elm_theme_overlay_del(NULL, l->data);
   elm_theme_overlay_add(NULL, config_theme_path_get(config));
   main_trans_update(config);
}

static void
_term_focus(Term *term)
{
   Eina_List *l;
   Term *term2;

   EINA_LIST_FOREACH(term->wn->terms, l, term2)
     {
        if (term2 != term)
          {
             if (term2->focused)
               {
                  term2->focused = EINA_FALSE;
                  edje_object_signal_emit(term2->bg, "focus,out", "terminology");
                  edje_object_signal_emit(term2->base, "focus,out", "terminology");
                  elm_object_focus_set(term2->termio, EINA_FALSE);
               }
          }
     }
   term->focused = EINA_TRUE;
   edje_object_signal_emit(term->bg, "focus,in", "terminology");
   edje_object_signal_emit(term->base, "focus,in", "terminology");
   if (term->wn->cmdbox) elm_object_focus_set(term->wn->cmdbox, EINA_FALSE);
   elm_object_focus_set(term->termio, EINA_TRUE);
   elm_win_title_set(term->wn->win, termio_title_get(term->termio));
   if (term->missed_bell)
     term->missed_bell = EINA_FALSE;

   /* TODO: tabs ? */
/* TODO: why ? */
#if 0
   Split *sp = NULL;
   sp = _split_find(term->wn->win, term->term, NULL);
   if (sp) _split_tabcount_update(sp, term);
#endif
}

void term_prev(Term *term)
{
   Term *new_term, *focused_term;
   Win *wn = term->wn;
   Term_Container *tc;

   focused_term = _win_focused_term_get(wn);
   if (!focused_term)
     focused_term = term;
   tc = focused_term->container;
   new_term = tc->term_prev(tc, tc);
   if (new_term && new_term != focused_term)
     _term_focus(new_term);

#if 0
   if (term->focused) term2 = _term_prev_get(term);
   if ((term2 != NULL) && (term2 != term))
     {
        Config *config = termio_config_get(term->term);
        Split *sp, *sp0;

        sp0 = _split_find(term->wn->win, term->term, NULL);
        sp = _split_find(term2->wn->win, term2->term, NULL);
        if (sp == sp0 && config->tab_zoom >= 0.01)
          _sel_go(sp, term2);
        else
          {
             _term_focus(term2);
             if (sp) _term_focus_show(sp, term2);
          }
     }
#endif
   /* TODO: get rid of it? */
   _term_miniview_check(term);
}

void term_next(Term *term)
{
   Term *new_term, *focused_term;
   Win *wn = term->wn;
   Term_Container *tc;

   focused_term = _win_focused_term_get(wn);
   if (!focused_term)
     focused_term = term;
   tc = focused_term->container;
   new_term = tc->term_next(tc, tc);
   if (new_term && new_term != focused_term)
     _term_focus(new_term);

#if 0
   if (term->focused) term2 = _term_next_get(term);
   if ((term2 != NULL) && (term2 != term))
     {
        Config *config = termio_config_get(term->term);
        Split *sp, *sp0;

        sp0 = _split_find(term->wn->win, term->term, NULL);
        sp = _split_find(term2->wn->win, term2->term, NULL);
        if (sp == sp0 && config->tab_zoom >= 0.01)
          _sel_go(sp, term2);
        else
          {
             _term_focus(term2);
             if (sp) _term_focus_show(sp, term2);
          }
     }
#endif
   /* TODO: get rid of it? */
   _term_miniview_check(term);
}

static void
_cb_popmedia_del(void *data, Evas *e EINA_UNUSED, Evas_Object *o EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Term *term = data;
   
   term->popmedia = NULL;
   term->popmedia_deleted = EINA_TRUE;
   edje_object_signal_emit(term->bg, "popmedia,off", "terminology");
}

static void
_cb_popmedia_done(void *data, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   Term *term = data;
   
   if (term->popmedia || term->popmedia_deleted)
     {
        if (term->popmedia)
          {
             evas_object_event_callback_del(term->popmedia, EVAS_CALLBACK_DEL,
                                            _cb_popmedia_del);
             evas_object_del(term->popmedia);
             term->popmedia = NULL;
          }
        term->popmedia_deleted = EINA_FALSE;
        termio_mouseover_suspend_pushpop(term->termio, -1);
        _popmedia_queue_process(term);
     }
}

static void
_cb_media_loop(void *data, Evas_Object *obj EINA_UNUSED, void *info EINA_UNUSED)
{
   Term *term = data;
   
   if (term->popmedia_queue)
     {
        if (term->popmedia) media_play_set(term->popmedia, EINA_FALSE);
        edje_object_signal_emit(term->bg, "popmedia,off", "terminology");
     }
}

static void
_popmedia_show(Term *term, const char *src)
{
   Evas_Object *o;
   Config *config = termio_config_get(term->termio);
   Media_Type type;

   if (!config) return;
   ty_dbus_link_hide();
   if (term->popmedia)
     {
        const char *s;
        
        EINA_LIST_FREE(term->popmedia_queue, s)
          {
             eina_stringshare_del(s);
          }
        term->popmedia_queue = eina_list_append(term->popmedia_queue,
                                                eina_stringshare_add(src));
        edje_object_signal_emit(term->bg, "popmedia,off", "terminology");
        return;
     }
   termio_mouseover_suspend_pushpop(term->termio, 1);
   type = media_src_type_get(src);
   term->popmedia = o = media_add(win_evas_object_get(term->wn),
                                  src, config, MEDIA_POP, type);
   term->popmedia_deleted = EINA_FALSE;
   evas_object_smart_callback_add(o, "loop", _cb_media_loop, term);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _cb_popmedia_del, term);
   edje_object_part_swallow(term->bg, "terminology.popmedia", o);
   evas_object_show(o);
   term->poptype = type;
   switch (type)
     {
      case MEDIA_TYPE_IMG:
         edje_object_signal_emit(term->bg, "popmedia,image", "terminology");
         break;
      case MEDIA_TYPE_SCALE:
         edje_object_signal_emit(term->bg, "popmedia,scale", "terminology");
         break;
      case MEDIA_TYPE_EDJE:
         edje_object_signal_emit(term->bg, "popmedia,edje", "terminology");
         break;
      case MEDIA_TYPE_MOV:
         edje_object_signal_emit(term->bg, "popmedia,movie", "terminology");
         break;
      case MEDIA_TYPE_UNKNOWN:
      default:
         break;
     }
}

static void
_term_miniview_check(Term *term)
{
   Eina_List *l, *wn_list;
   DBG("TODO");

   EINA_SAFETY_ON_NULL_RETURN(term);
   EINA_SAFETY_ON_NULL_RETURN(term->miniview);

   wn_list = win_terms_get(term_win_get(term));

   EINA_LIST_FOREACH(wn_list, l, term)
     {
        //Split *sp = _split_find(term->wn->win, term->term, NULL);
        if (term->miniview_shown)
          {
             if (term->focused)
               edje_object_signal_emit(term->bg, "miniview,on", "terminology");
#if 0
             else if (sp->term != term)
               edje_object_signal_emit(term->bg, "miniview,off", "terminology");
#endif
          }
        //sp = NULL;
     }
}

void
term_miniview_hide(Term *term)
{
   EINA_SAFETY_ON_NULL_RETURN(term);
   EINA_SAFETY_ON_NULL_RETURN(term->miniview);

   if (term->miniview_shown)
     {
        edje_object_signal_emit(term->bg, "miniview,off", "terminology");
        term->miniview_shown = EINA_FALSE;
     }
}

void
term_miniview_toggle(Term *term)
{
   EINA_SAFETY_ON_NULL_RETURN(term);
   EINA_SAFETY_ON_NULL_RETURN(term->miniview);

   if (term->miniview_shown)
     {
        edje_object_signal_emit(term->bg, "miniview,off", "terminology");
        term->miniview_shown = EINA_FALSE;
     }
   else
     {
        edje_object_signal_emit(term->bg, "miniview,on", "terminology");
        term->miniview_shown = EINA_TRUE;
     }
}

static void
_popmedia_queue_process(Term *term)
{
   const char *src;
   
   if (!term->popmedia_queue) return;
   src = term->popmedia_queue->data;
   term->popmedia_queue = eina_list_remove_list(term->popmedia_queue, 
                                                term->popmedia_queue);
   if (!src) return;
   _popmedia_show(term, src);
   eina_stringshare_del(src);
}

static void
_popmedia_queue_add(Term *term, const char *src)
{
   term->popmedia_queue = eina_list_append(term->popmedia_queue,
                                           eina_stringshare_add(src));
   if (!term->popmedia) _popmedia_queue_process(term);
}

static void
_cb_popup(void *data, Evas_Object *obj EINA_UNUSED, void *event)
{
   Term *term = data;
   const char *src = event;
   if (!src) src = termio_link_get(term->termio);
   if (!src) return;
   _popmedia_show(term, src);
}

static void
_cb_popup_queue(void *data, Evas_Object *obj EINA_UNUSED, void *event)
{
   Term *term = data;
   const char *src = event;
   if (!src) src = termio_link_get(term->termio);
   if (!src) return;
   _popmedia_queue_add(term, src);
}

static void
_set_alpha(Config *config, const char *val, Eina_Bool save)
{
   int opacity;

   if (!config || !val) return;

   config->temporary = !save;

   if (isdigit(*val))
     {
        opacity = atoi(val);
        if (opacity >= 100)
          {
             config->translucent = EINA_FALSE;
             config->opacity = 100;
          }
        else if (opacity >= 0)
          {
             config->translucent = EINA_TRUE;
             config->opacity = opacity;
          }
     }
   else if ((!strcasecmp(val, "on")) ||
            (!strcasecmp(val, "true")) ||
            (!strcasecmp(val, "yes")))
     config->translucent = EINA_TRUE;
   else
     config->translucent = EINA_FALSE;
   main_trans_update(config);

   if (save) config_save(config, NULL);
}

static void
_cb_command(void *data, Evas_Object *obj EINA_UNUSED, void *event)
{
   Term *term = data;
   const char *cmd = event;

   if (cmd[0] == 'p') // popmedia
     {
        if (cmd[1] == 'n') // now
          {
             _popmedia_show(term, cmd + 2);
          }
        else if (cmd[1] == 'q') // queue it to display after current one
          {
              _popmedia_queue_add(term, cmd + 2);
          }
     }
   else if (cmd[0] == 'b') // set background
     {
        if (cmd[1] == 't') // temporary
          {
             Config *config = termio_config_get(term->termio);

             if (config)
               {
                  config->temporary = EINA_TRUE;
                  if (cmd[2])
                    eina_stringshare_replace(&(config->background), cmd + 2);
                  else
                    eina_stringshare_replace(&(config->background), NULL);
                  main_media_update(config);
               }
          }
        else if (cmd[1] == 'p') // permanent
          {
             Config *config = termio_config_get(term->termio);

             if (config)
               {
                  config->temporary = EINA_FALSE;
                  if (cmd[2])
                    eina_stringshare_replace(&(config->background), cmd + 2);
                  else
                    eina_stringshare_replace(&(config->background), NULL);
                  main_media_update(config);
                  config_save(config, NULL);
               }
          }
     }
   else if (cmd[0] == 'a') // set alpha
     {
        if (cmd[1] == 't') // temporary
          _set_alpha(termio_config_get(term->termio), cmd + 2, EINA_FALSE);
        else if (cmd[1] == 'p') // permanent
          _set_alpha(termio_config_get(term->termio), cmd + 2, EINA_TRUE);
     }
}

static void
_cb_tabcount_go(void *data, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   Term *term = data;
   Split *sp;

   sp = _split_find(term->wn->win, term->termio, NULL);
   _sel_go(sp, term);
}

static void
_cb_prev(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;

   term_prev(term);
}

static void
_cb_next(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;

   term_next(term);
}

static void
_cb_new(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;

   main_new(term->wn->win, term->termio);
   _term_miniview_check(term);
}

void
main_term_focus(Term *term EINA_UNUSED)
{
DBG("TODO");
#if 0
   Split *sp;

   sp = _split_find(term->wn->win, term->term, NULL);
   if (sp->terms->next != NULL)
     _sel_go(sp, term);
#endif
}

static void
_cb_select(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;
   main_term_focus(term);
}

static void
_cb_split_h(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc = term->container;

   assert(tc->type == TERM_CONTAINER_TYPE_SOLO);
   tc->split(tc, NULL, EINA_TRUE);
}

static void
_cb_split_v(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc = term->container;

   assert(tc->type == TERM_CONTAINER_TYPE_SOLO);
   tc->split(tc, NULL, EINA_FALSE);
}

static void
_cb_title(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;
   if (term->focused)
     elm_win_title_set(term->wn->win, termio_title_get(term->termio));
}

static void
_cb_icon(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;
   if (term->focused)
     elm_win_icon_name_set(term->wn->win, termio_icon_name_get(term->termio));
}

static void
_tab_go(Term *term EINA_UNUSED, int tnum EINA_UNUSED)
{
#if 0
   Term *term2;
   Split *sp = _split_find(term->wn->win, term->term, NULL);
   if (!sp) return;

   term2 = eina_list_nth(sp->terms, tnum);
   if ((!term2) || (term2 == term)) return;
   _sel_go(sp, term2);
#endif
}

#define CB_TAB(TAB) \
static void                                             \
_cb_tab_##TAB(void *data, Evas_Object *obj EINA_UNUSED, \
             void *event EINA_UNUSED)                   \
{                                                       \
   _tab_go(data, TAB - 1);                              \
}

CB_TAB(1)
CB_TAB(2)
CB_TAB(3)
CB_TAB(4)
CB_TAB(5)
CB_TAB(6)
CB_TAB(7)
CB_TAB(8)
CB_TAB(9)
CB_TAB(10)
#undef CB_TAB

static Eina_Bool
_cb_cmd_focus(void *data)
{
   Win *wn = data;
   Term *term;
   
   wn->cmdbox_focus_timer = NULL;
   term = _win_focused_term_get(wn);
   if (term)
     {
        elm_object_focus_set(term->termio, EINA_FALSE);
        if (term->wn->cmdbox) elm_object_focus_set(wn->cmdbox, EINA_TRUE);
     }
   return EINA_FALSE;
}

static Eina_Bool
_cb_cmd_del(void *data)
{
   Win *wn = data;
   
   wn->cmdbox_del_timer = NULL;
   if (wn->cmdbox)
     {
        evas_object_del(wn->cmdbox);
        wn->cmdbox = NULL;
     }
   return EINA_FALSE;
}

static void
_cb_cmd_activated(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Win *wn = data;
   char *cmd = NULL;
   Term *term;
   
   if (wn->cmdbox) elm_object_focus_set(wn->cmdbox, EINA_FALSE);
   edje_object_signal_emit(wn->base, "cmdbox,hide", "terminology");
   term = _win_focused_term_get(wn);
   if (term) elm_object_focus_set(term->termio, EINA_TRUE);
   if (wn->cmdbox) cmd = (char *)elm_entry_entry_get(wn->cmdbox);
   if (cmd)
     {
        cmd = elm_entry_markup_to_utf8(cmd);
        if (cmd)
          {
             if (term) termcmd_do(term->termio, term->wn->win, term->bg, cmd);
             free(cmd);
          }
     }
   if (wn->cmdbox_focus_timer)
     {
        ecore_timer_del(wn->cmdbox_focus_timer);
        wn->cmdbox_focus_timer = NULL;
     }
   wn->cmdbox_up = EINA_FALSE;
   if (wn->cmdbox_del_timer) ecore_timer_del(wn->cmdbox_del_timer);
   wn->cmdbox_del_timer = ecore_timer_add(5.0, _cb_cmd_del, wn);
}

static void
_cb_cmd_aborted(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Win *wn = data;
   Term *term;
   
   if (wn->cmdbox) elm_object_focus_set(wn->cmdbox, EINA_FALSE);
   edje_object_signal_emit(wn->base, "cmdbox,hide", "terminology");
   term = _win_focused_term_get(wn);
   if (term) elm_object_focus_set(term->termio, EINA_TRUE);
   if (wn->cmdbox_focus_timer)
     {
        ecore_timer_del(wn->cmdbox_focus_timer);
        wn->cmdbox_focus_timer = NULL;
     }
   wn->cmdbox_up = EINA_FALSE;
   if (wn->cmdbox_del_timer) ecore_timer_del(wn->cmdbox_del_timer);
   wn->cmdbox_del_timer = ecore_timer_add(5.0, _cb_cmd_del, wn);
}

static void
_cb_cmd_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Win *wn = data;
   char *cmd = NULL;
   Term *term;
   
   term = _win_focused_term_get(wn);
   if (!term) return;
   if (wn->cmdbox) cmd = (char *)elm_entry_entry_get(wn->cmdbox);
   if (cmd)
     {
        cmd = elm_entry_markup_to_utf8(cmd);
        if (cmd)
          {
             termcmd_watch(term->termio, term->wn->win, term->bg, cmd);
             free(cmd);
          }
     }
}

static void
_cb_cmd_hints_changed(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Win *wn = data;
   
   if (wn->cmdbox)
     {
        evas_object_show(wn->cmdbox);
        edje_object_part_swallow(wn->base, "terminology.cmdbox", wn->cmdbox);
     }
}

static void
_cb_cmdbox(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;
   
   term->wn->cmdbox_up = EINA_TRUE;
   if (!term->wn->cmdbox)
     {
        Evas_Object *o;
        Win *wn = term->wn;
        
        wn->cmdbox = o = elm_entry_add(wn->win);
        elm_entry_single_line_set(o, EINA_TRUE);
        elm_entry_scrollable_set(o, EINA_FALSE);
        elm_scroller_policy_set(o, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
        elm_entry_input_panel_layout_set(o, ELM_INPUT_PANEL_LAYOUT_TERMINAL);
        elm_entry_autocapital_type_set(o, ELM_AUTOCAPITAL_TYPE_NONE);
        elm_entry_input_panel_enabled_set(o, EINA_TRUE);
        elm_entry_input_panel_language_set(o, ELM_INPUT_PANEL_LANG_ALPHABET);
        elm_entry_input_panel_return_key_type_set(o, ELM_INPUT_PANEL_RETURN_KEY_TYPE_GO);
        elm_entry_prediction_allow_set(o, EINA_FALSE);
        evas_object_show(o);
        evas_object_smart_callback_add(o, "activated", _cb_cmd_activated, wn);
        evas_object_smart_callback_add(o, "aborted", _cb_cmd_aborted, wn);
        evas_object_smart_callback_add(o, "changed,user", _cb_cmd_changed, wn);
        evas_object_event_callback_add(o, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                       _cb_cmd_hints_changed, wn);
        edje_object_part_swallow(wn->base, "terminology.cmdbox", o);
     }
   edje_object_signal_emit(term->wn->base, "cmdbox,show", "terminology");
   elm_object_focus_set(term->termio, EINA_FALSE);
   elm_entry_entry_set(term->wn->cmdbox, "");
   evas_object_show(term->wn->cmdbox);
   if (term->wn->cmdbox_focus_timer)
     ecore_timer_del(term->wn->cmdbox_focus_timer);
   term->wn->cmdbox_focus_timer =
     ecore_timer_add(0.2, _cb_cmd_focus, term->wn);
   if (term->wn->cmdbox_del_timer)
     {
        ecore_timer_del(term->wn->cmdbox_del_timer);
        term->wn->cmdbox_del_timer = NULL;
     }
}


static void
_cb_media_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Term *term = data;
   Config *config = NULL;

   if (term->termio)
     config = termio_config_get(term->termio);
   term->media = NULL;
   if (term->bg)
     {
        edje_object_signal_emit(term->bg, "media,off", "terminology");
        edje_object_signal_emit(term->base, "media,off", "terminology");
     }
   if (!config) return;
   if (config->temporary)
     eina_stringshare_replace(&(config->background), NULL);
}

static void
_term_media_update(Term *term, const Config *config)
{
   if ((config->background) && (config->background[0]))
     {
        Evas_Object *o;
        Media_Type type;

        if (term->media)
          {
             evas_object_event_callback_del(term->media,
                                            EVAS_CALLBACK_DEL,
                                            _cb_media_del);
             evas_object_del(term->media);
          }
        type = media_src_type_get(config->background);
        term->media = o = media_add(term->wn->win,
                                    config->background, config,
                                    MEDIA_BG, type);
        evas_object_event_callback_add(o, EVAS_CALLBACK_DEL,
                                       _cb_media_del, term);
        edje_object_part_swallow(term->base, "terminology.background", o);
        evas_object_show(o);
        term->mediatype = type;
        switch (type)
          {
           case MEDIA_TYPE_IMG:
              edje_object_signal_emit(term->bg, "media,image", "terminology");
              edje_object_signal_emit(term->base, "media,image", "terminology");
              break;
           case MEDIA_TYPE_SCALE:
              edje_object_signal_emit(term->bg, "media,scale", "terminology");
              edje_object_signal_emit(term->base, "media,scale", "terminology");
              break;
           case MEDIA_TYPE_EDJE:
              edje_object_signal_emit(term->bg, "media,edje", "terminology");
              edje_object_signal_emit(term->base, "media,edje", "terminology");
              break;
           case MEDIA_TYPE_MOV:
              edje_object_signal_emit(term->bg, "media,movie", "terminology");
              edje_object_signal_emit(term->base, "media,movie", "terminology");
              break;
           case MEDIA_TYPE_UNKNOWN:
           default:
              break;
          }
     }
   else
     {
        if (term->media)
          {
             evas_object_event_callback_del(term->media,
                                            EVAS_CALLBACK_DEL,
                                            _cb_media_del);
             edje_object_signal_emit(term->bg, "media,off", "terminology");
             edje_object_signal_emit(term->base, "media,off", "terminology");
             evas_object_del(term->media);
             term->media = NULL;
          }
     }
}

void
main_media_update(const Config *config)
{
   Win *wn;
   Term *term;
   Eina_List *l, *ll;

   EINA_LIST_FOREACH(wins, l, wn)
     {
        EINA_LIST_FOREACH(wn->terms, ll, term)
          {
             if (term->config != config) continue;
             if (!config) continue;
             _term_media_update(term, config);
          }
     }
}

void
main_media_mute_update(const Config *config)
{
   Win *wn;
   Term *term;
   Eina_List *l, *ll;

   EINA_LIST_FOREACH(wins, l, wn)
     {
        EINA_LIST_FOREACH(wn->terms, ll, term)
          {
             if (term->media) media_mute_set(term->media, config->mute);
             termio_media_mute_set(term->termio, config->mute);
          }
     }
}

void
main_media_visualize_update(const Config *config)
{
   Win *wn;
   Term *term;
   Eina_List *l, *ll;

   EINA_LIST_FOREACH(wins, l, wn)
     {
        EINA_LIST_FOREACH(wn->terms, ll, term)
          {
             if (term->media) media_visualize_set(term->media, config->visualize);
             termio_media_visualize_set(term->termio, config->visualize);
          }
     }
}

void
main_config_sync(const Config *config)
{
   Win *wn;
   Term *term;
   Eina_List *l, *ll;
   Config *main_config = main_config_get();

   if (config != main_config) config_sync(config, main_config);
   EINA_LIST_FOREACH(wins, l, wn)
     {
        if (wn->config != config) config_sync(config, wn->config);
        EINA_LIST_FOREACH(wn->terms, ll, term)
          {
             if (term->config != config)
               {
                  Evas_Coord mw = 1, mh = 1, w, h, tsize_w = 0, tsize_h = 0;

                  config_sync(config, term->config);
                  evas_object_geometry_get(term->termio, NULL, NULL,
                                           &tsize_w, &tsize_h);
                  evas_object_data_del(term->termio, "sizedone");
                  termio_config_update(term->termio);
                  evas_object_size_hint_min_get(term->termio, &mw, &mh);
                  if (mw < 1) mw = 1;
                  if (mh < 1) mh = 1;
                  w = tsize_w / mw;
                  h = tsize_h / mh;
                  evas_object_data_del(term->termio, "sizedone");
                  evas_object_size_hint_request_set(term->termio,
                                                    w * mw, h * mh);
               }
          }
     }
}

static void
term_free(Term *term)
{
   const char *s;
   
   EINA_LIST_FREE(term->popmedia_queue, s)
     {
        eina_stringshare_del(s);
     }
   if (term->media)
     {
        evas_object_event_callback_del(term->media,
                                       EVAS_CALLBACK_DEL,
                                       _cb_media_del);
        evas_object_del(term->media);
     }
   term->media = NULL;
   if (term->popmedia) evas_object_del(term->popmedia);
   if (term->miniview)
     {
        evas_object_del(term->miniview);
        term->miniview = NULL;
     }
   term->popmedia = NULL;
   term->popmedia_deleted = EINA_FALSE;
   evas_object_del(term->termio);
   term->termio = NULL;
   evas_object_del(term->base);
   term->base = NULL;
   evas_object_del(term->bg);
   term->bg = NULL;
   if (term->tabcount_spacer)
     {
        evas_object_del(term->tabcount_spacer);
        term->tabcount_spacer = NULL;
     }
   free(term);
}

static void
_cb_tabcount_prev(void *data, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   _cb_prev(data, NULL, NULL);
}

static void
_cb_tabcount_next(void *data, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   _cb_next(data, NULL, NULL);
}

static void
main_term_bg_config(Term *term)
{
   Edje_Message_Int msg;

   if (term->config->translucent)
     msg.val = term->config->opacity;
   else
     msg.val = 100;

   edje_object_message_send(term->bg, EDJE_MESSAGE_INT, 1, &msg);
   edje_object_message_send(term->base, EDJE_MESSAGE_INT, 1, &msg);

   termio_theme_set(term->termio, term->bg);
   edje_object_signal_callback_add(term->bg, "popmedia,done", "terminology",
                                   _cb_popmedia_done, term); 
   edje_object_signal_callback_add(term->bg, "tabcount,go", "terminology",
                                   _cb_tabcount_go, term);
   edje_object_signal_callback_add(term->bg, "tabcount,prev", "terminology",
                                   _cb_tabcount_prev, term);
   edje_object_signal_callback_add(term->bg, "tabcount,next", "terminology",
                                   _cb_tabcount_next, term);
   edje_object_part_swallow(term->base, "terminology.content", term->termio);
   edje_object_part_swallow(term->bg, "terminology.content", term->base);
   edje_object_part_swallow(term->bg, "terminology.miniview", term->miniview);
   if (term->popmedia)
     {
        edje_object_part_swallow(term->bg, "terminology.popmedia", term->popmedia);
        switch (term->poptype)
          {
           case MEDIA_TYPE_IMG:
              edje_object_signal_emit(term->bg, "popmedia,image", "terminology");
              break;
           case MEDIA_TYPE_SCALE:
              edje_object_signal_emit(term->bg, "popmedia,scale", "terminology");
              break;
           case MEDIA_TYPE_EDJE:
              edje_object_signal_emit(term->bg, "popmedia,edje", "terminology");
              break;
           case MEDIA_TYPE_MOV:
              edje_object_signal_emit(term->bg, "popmedia,movie", "terminology");
              break;
           default:
              break;
          }
     }
   if (term->media)
     {
        edje_object_part_swallow(term->base, "terminology.background", term->media);
        switch (term->mediatype)
          {
           case MEDIA_TYPE_IMG:
              edje_object_signal_emit(term->bg, "media,image", "terminology");
              edje_object_signal_emit(term->base, "media,image", "terminology");
              break;
           case MEDIA_TYPE_SCALE:
              edje_object_signal_emit(term->bg, "media,scale", "terminology");
              edje_object_signal_emit(term->base, "media,scale", "terminology");
              break;
           case MEDIA_TYPE_EDJE:
             edje_object_signal_emit(term->bg, "media,edje", "terminology");
             edje_object_signal_emit(term->base, "media,edje", "terminology");
             break;
           case MEDIA_TYPE_MOV:
             edje_object_signal_emit(term->bg, "media,movie", "terminology");
             edje_object_signal_emit(term->base, "media,movie", "terminology");
             break;
           case MEDIA_TYPE_UNKNOWN:
           default:
             break;
          }
     }

   if ((term->focused) && (term->wn->focused))
     {
        edje_object_signal_emit(term->bg, "focus,in", "terminology");
        edje_object_signal_emit(term->base, "focus,in", "terminology");
        if (term->wn->cmdbox)
          elm_object_focus_set(term->wn->cmdbox, EINA_FALSE);
        elm_object_focus_set(term->termio, EINA_TRUE);
     }
   if (term->miniview_shown)
        edje_object_signal_emit(term->bg, "miniview,on", "terminology");
}

#if 0
static void
_main_term_bg_redo(Term *term)
{
   Evas_Object *o;

   if (term->tabcount_spacer)
     {
        evas_object_del(term->tabcount_spacer);
        term->tabcount_spacer = NULL;
     }
   if (term->miniview)
     {
        edje_object_part_unswallow(term->bg, term->miniview);
        evas_object_del(term->miniview);
        term->miniview = NULL;
     }
   evas_object_del(term->base);
   evas_object_del(term->bg);
   
   term->base = o = edje_object_add(evas_object_evas_get(term->wn->win));
   theme_apply(o, term->config, "terminology/core");

   theme_auto_reload_enable(o);
   evas_object_data_set(o, "theme_reload_func", main_term_bg_config);
   evas_object_data_set(o, "theme_reload_func_data", term);
   evas_object_show(o);

   term->bg = o = edje_object_add(evas_object_evas_get(term->wn->win));
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   theme_apply(o, term->config, "terminology/background");

   term->miniview = o = miniview_add(term->wn->win, term->term);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

   o = term->bg;

   theme_auto_reload_enable(o);
   evas_object_data_set(o, "theme_reload_func", main_term_bg_config);
   evas_object_data_set(o, "theme_reload_func_data", term);
   evas_object_show(o);
   main_term_bg_config(term);
   if (term->miniview_shown)
        edje_object_signal_emit(term->bg, "miniview,on", "terminology");
}
#endif

Eina_Bool
main_term_popup_exists(const Term *term)
{
   return term->popmedia || term->popmedia_queue;
}

Win *
term_win_get(Term *term)
{
   return term->wn;
}


Evas_Object *
main_term_evas_object_get(Term *term)
{
   return term->termio;
}

Evas_Object *
term_miniview_get(Term *term)
{
   if (term)
     return term->miniview;
   return NULL;
}


static void
_cb_bell(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;
   Config *config = termio_config_get(term->termio);

   if (!config) return;
   if (!config->disable_visual_bell)
     {
#if 0
        Split *sp;

        edje_object_signal_emit(term->bg, "bell", "terminology");
        edje_object_signal_emit(term->base, "bell", "terminology");
        if (config->bell_rings)
          {
             edje_object_signal_emit(term->bg, "bell,ring", "terminology");
             edje_object_signal_emit(term->base, "bell,ring", "terminology");
          }
        sp = _split_find(term->wn->win, term->term, NULL);
        if (sp)
          {
             if (sp->term != term)
               {
                  term->missed_bell = EINA_TRUE;
                  _split_tabcount_update(sp, sp->term);
               }
          }
#endif
     }
   if (config->urg_bell)
     {
        if (!term->wn->focused) elm_win_urgent_set(term->wn->win, EINA_TRUE);
     }
}


static void
_cb_options_done(void *data EINA_UNUSED)
{
   Win *wn = data;
   Eina_List *l;
   Term *term;
   if (!wn->focused) return;
   EINA_LIST_FOREACH(wn->terms, l, term)
     {
        if (term->focused)
          {
             elm_object_focus_set(term->termio, EINA_TRUE);
             termio_event_feed_mouse_in(term->termio);
          }
     }
}

static void
_cb_options(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event EINA_UNUSED)
{
   Term *term = data;

   controls_toggle(term->wn->win, term->wn->base, term->termio,
                   _cb_options_done, term->wn);
}

static void
_cb_exited(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;

   DBG("exit term:%p hold:%d", term, term->hold);
   if (!term->hold)
     {
        Evas_Object *win = win_evas_object_get(term->wn);
        main_close(win, term->termio);
     }
}


Term *
term_new(Win *wn, Config *config, const char *cmd,
         Eina_Bool login_shell, const char *cd,
         int size_w, int size_h, Eina_Bool hold)
{
   Term *term;
   Evas_Object *o;
   Evas *canvas = evas_object_evas_get(wn->win);
   Edje_Message_Int msg;
   
   term = calloc(1, sizeof(Term));
   if (!term) return NULL;

   if (!config) abort();

   /* TODO: clean up that */
   termpty_init();
   miniview_init();
   gravatar_init();

   term->wn = wn;
   term->hold = hold;
   term->config = config;
   
   term->base = o = edje_object_add(canvas);
   theme_apply(o, term->config, "terminology/core");

   theme_auto_reload_enable(o);
   evas_object_data_set(o, "theme_reload_func", main_term_bg_config);
   evas_object_data_set(o, "theme_reload_func_data", term);
   evas_object_show(o);

   term->bg = o = edje_object_add(canvas);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   if (!theme_apply(o, config, "terminology/background"))
     {
        CRITICAL(_("Couldn't find terminology theme! Forgot 'make install'?"));
        evas_object_del(term->bg);
        free(term);
        return NULL;
     }

   theme_auto_reload_enable(o);
   evas_object_data_set(o, "theme_reload_func", main_term_bg_config);
   evas_object_data_set(o, "theme_reload_func_data", term);
   evas_object_show(o);

   if (term->config->translucent)
     msg.val = term->config->opacity;
   else
     msg.val = 100;

   edje_object_message_send(term->bg, EDJE_MESSAGE_INT, 1, &msg);
   edje_object_message_send(term->base, EDJE_MESSAGE_INT, 1, &msg);

   term->termio = o = termio_add(wn->win, config, cmd, login_shell, cd,
                                 size_w, size_h, term);
   evas_object_data_set(o, "term", term);
   colors_term_init(termio_textgrid_get(term->termio), term->bg, config);

   termio_win_set(o, wn->win);
   termio_theme_set(o, term->bg);

   term->miniview = o = miniview_add(wn->win, term->termio);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

   o = term->termio;

   edje_object_signal_callback_add(term->bg, "popmedia,done", "terminology",
                                   _cb_popmedia_done, term);
   edje_object_signal_callback_add(term->bg, "tabcount,go", "terminology",
                                   _cb_tabcount_go, term);
   edje_object_signal_callback_add(term->bg, "tabcount,prev", "terminology",
                                   _cb_tabcount_prev, term);
   edje_object_signal_callback_add(term->bg, "tabcount,next", "terminology",
                                   _cb_tabcount_next, term);

   evas_object_size_hint_weight_set(o, 0, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, 0, EVAS_HINT_FILL);
   evas_object_event_callback_add(o, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                  _cb_size_hint, term);
   edje_object_part_swallow(term->base, "terminology.content", o);
   edje_object_part_swallow(term->bg, "terminology.content", term->base);
   edje_object_part_swallow(term->bg, "terminology.miniview", term->miniview);
   evas_object_smart_callback_add(o, "options", _cb_options, term);
   evas_object_smart_callback_add(o, "exited", _cb_exited, term);
   evas_object_smart_callback_add(o, "bell", _cb_bell, term);
   evas_object_smart_callback_add(o, "popup", _cb_popup, term);
   evas_object_smart_callback_add(o, "popup,queue", _cb_popup_queue, term);
   evas_object_smart_callback_add(o, "cmdbox", _cb_cmdbox, term);
   evas_object_smart_callback_add(o, "command", _cb_command, term);
   evas_object_smart_callback_add(o, "prev", _cb_prev, term);
   evas_object_smart_callback_add(o, "next", _cb_next, term);
   evas_object_smart_callback_add(o, "new", _cb_new, term);
   evas_object_smart_callback_add(o, "select", _cb_select, term);
   evas_object_smart_callback_add(o, "split,h", _cb_split_h, term);
   evas_object_smart_callback_add(o, "split,v", _cb_split_v, term);
   evas_object_smart_callback_add(o, "title,change", _cb_title, term);
   evas_object_smart_callback_add(o, "icon,change", _cb_icon, term);
   evas_object_smart_callback_add(o, "tab,1", _cb_tab_1, term);
   evas_object_smart_callback_add(o, "tab,2", _cb_tab_2, term);
   evas_object_smart_callback_add(o, "tab,3", _cb_tab_3, term);
   evas_object_smart_callback_add(o, "tab,4", _cb_tab_4, term);
   evas_object_smart_callback_add(o, "tab,5", _cb_tab_5, term);
   evas_object_smart_callback_add(o, "tab,6", _cb_tab_6, term);
   evas_object_smart_callback_add(o, "tab,7", _cb_tab_7, term);
   evas_object_smart_callback_add(o, "tab,8", _cb_tab_8, term);
   evas_object_smart_callback_add(o, "tab,9", _cb_tab_9, term);
   evas_object_smart_callback_add(o, "tab,0", _cb_tab_10, term);
   evas_object_show(o);

   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _cb_term_mouse_down, term);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_IN,
                                  _cb_term_mouse_in, term);

   if (!wn->terms) term->focused = EINA_TRUE;

   wn->terms = eina_list_append(wn->terms, term);
   app_server_term_add(term);

   return term;
}


/* }}} */
/* {{{ Sel */

#if 0
static void
_sel_restore(Split *sp EINA_UNUSED)
{
   Eina_List *l;
   Term *tm;

   EINA_LIST_FOREACH(sp->terms, l, tm)
     {
        if (tm->unswallowed)
          {
#if (EVAS_VERSION_MAJOR > 1) || (EVAS_VERSION_MINOR >= 8)
             evas_object_image_source_visible_set(tm->sel, EINA_TRUE);
#endif
             edje_object_part_swallow(tm->bg, "terminology.content", tm->base);
             tm->unswallowed = EINA_FALSE;
             evas_object_show(tm->base);
             tm->sel = NULL;
          }
     }
   evas_object_del(sp->sel);
   evas_object_del(sp->sel_bg);
   sp->sel = NULL;
   sp->sel_bg = NULL;
}

static void
_sel_cb_selected(void *data EINA_UNUSED,
                 Evas_Object *obj EINA_UNUSED,
                 void *info EINA_UNUSED)
{
   /* TODO */
   DBG("selected");
   Split *sp = data;
   Eina_List *l;
   Term *tm;

   EINA_LIST_FOREACH(sp->terms, l, tm)
     {
        if (tm->sel == info)
          {
             _term_focus(tm);
             _term_focus_show(sp, tm);
             _sel_restore(sp);
             _term_miniview_check(tm);
             return;
          }
     }
   _sel_restore(sp);
   _term_focus(sp->term);
   _term_focus_show(sp, sp->term);
   _term_miniview_check(tm);
}

static void
_sel_cb_exit(void *data,
             Evas_Object *obj EINA_UNUSED,
             void *info EINA_UNUSED)
{
   Split *sp = data;
   _sel_restore(sp);
   _term_focus(sp->term);
   //_term_focus_show(sp, sp->term);
}

static void
_sel_cb_ending(void *data, Evas_Object *obj EINA_UNUSED, void *info EINA_UNUSED)
{
   Split *sp = data;
   edje_object_signal_emit(sp->sel_bg, "end", "terminology");
}
#endif

static void
_sel_go(Split *sp EINA_UNUSED, Term *term EINA_UNUSED)
{
#if 0
   Eina_List *l;
   Term *tm;
   double z;
   Edje_Message_Int msg;

   evas_object_hide(sp->term->bg);
   sp->sel_bg = edje_object_add(evas_object_evas_get(sp->wn->win));
   theme_apply(sp->sel_bg, term->config, "terminology/sel/base");
   if (sp->term->config->translucent)
     msg.val = term->config->opacity;
   else
     msg.val = 100;
   edje_object_message_send(sp->sel_bg, EDJE_MESSAGE_INT, 1, &msg);
   edje_object_signal_emit(sp->sel_bg, "begin", "terminology");
   sp->sel = sel_add(sp->wn->win);
   EINA_LIST_FOREACH(sp->terms, l, tm)
     {
        Evas_Object *img;
        Evas_Coord w, h;
        
        edje_object_part_unswallow(tm->bg, tm->base);
        evas_object_lower(tm->base);
        evas_object_move(tm->base, -9999, -9999);
        evas_object_show(tm->base);
        evas_object_clip_unset(tm->base);
#if (EVAS_VERSION_MAJOR > 1) || (EVAS_VERSION_MINOR >= 8)
        evas_object_image_source_visible_set(tm->sel, EINA_FALSE);
#endif
        tm->unswallowed = EINA_TRUE;

        img = evas_object_image_filled_add(evas_object_evas_get(sp->wn->win));
        evas_object_image_source_set(img, tm->base);
        evas_object_geometry_get(tm->base, NULL, NULL, &w, &h);
        evas_object_resize(img, w, h);
        evas_object_data_set(img, "termio", tm->term);
        tm->sel = img;
        
        sel_entry_add(sp->sel, tm->sel, (tm == sp->term),
                      tm->missed_bell, tm->config);
     }
   edje_object_part_swallow(sp->sel_bg, "terminology.content", sp->sel);
   evas_object_show(sp->sel);
   if (!sp->parent)
     edje_object_part_swallow(sp->wn->base, "terminology.content", sp->sel_bg);
   else
     {
        if (sp == sp->parent->s1)
          {
             elm_object_part_content_unset(sp->parent->panes, PANES_TOP);
             elm_object_part_content_set(sp->parent->panes, PANES_TOP,
                                         sp->sel_bg);
          }
        else
          {
             elm_object_part_content_unset(sp->parent->panes, PANES_BOTTOM);
             elm_object_part_content_set(sp->parent->panes, PANES_BOTTOM,
                                         sp->sel_bg);
          }
     }
   evas_object_show(sp->sel_bg);
   evas_object_smart_callback_add(sp->sel, "selected", _sel_cb_selected, sp);
   evas_object_smart_callback_add(sp->sel, "exit", _sel_cb_exit, sp);
   evas_object_smart_callback_add(sp->sel, "ending", _sel_cb_ending, sp);
   z = 1.0;
   sel_go(sp->sel);
   if (eina_list_count(sp->terms) >= 1)
     z = 1.0 / (sqrt(eina_list_count(sp->terms)) * 0.8);
   if (z > 1.0) z = 1.0;
   sel_orig_zoom_set(sp->sel, z);
   sel_zoom(sp->sel, z);
   if (term != sp->term)
     {
        sel_entry_selected_set(sp->sel, term->sel, EINA_TRUE);
        sel_exit(sp->sel);
     }
   elm_object_focus_set(sp->sel, EINA_TRUE);
#endif
}
/* }}} */

void
windows_free(void)
{
   Win *wn;

   while (wins)
     {
        wn = eina_list_data_get(wins);
        win_free(wn);
     }
}

