#include <assert.h>
#include <Elementary.h>
#include "win.h"
#include "termcmd.h"
#include "config.h"
#include "main.h"
#include "miniview.h"
#include "gravatar.h"
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
   Evas_Object *tabcount_spacer;
   Eina_List   *popmedia_queue;
   Media_Type   poptype, mediatype;
   int          step_x, step_y, min_w, min_h, req_w, req_h;
   struct {
      int       x, y;
   } down;
   unsigned char hold : 1;
   unsigned char unswallowed : 1;
   unsigned char missed_bell : 1;
   unsigned char miniview_shown : 1;
   unsigned char popmedia_deleted : 1;
};

typedef struct _Solo Solo;
typedef struct _Tabs Tabs;


struct _Solo {
     Term_Container tc;
     Term *term;
};

typedef struct _Tab_Item Tab_Item;
struct _Tab_Item {
     Term_Container *tc;
     Evas_Object *obj;
     Elm_Object_Item *elm_item;
     void *selector_entry;
};

struct _Tabs {
     Term_Container tc;
     Evas_Object *base;
     Evas_Object *selector;
     Evas_Object *selector_bg;
     Evas_Object *box;
     Evas_Object *bg;
     Evas_Object *btn_add;
     Evas_Object *btn_hide;
     Evas_Object *tabbar;
     Evas_Object *tabbar_spacer;
     Evas_Object *selector_spacer;
     Eina_List *tabs;
     Tab_Item *current;
     unsigned char tabbar_shown : 1;
};

struct _Split
{
   Term_Container tc;
   Term_Container *tc1, *tc2; // left/right or top/bottom child splits, null if leaf
   Evas_Object *panes; // null if a leaf node
   Term_Container *last_focus;

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
   unsigned char cmdbox_up : 1;
};

/* }}} */

static Eina_List   *wins = NULL;


static Eina_Bool _win_is_focused(Win *wn);
static Eina_Bool _term_is_focused(Term *term);
static Term_Container *_solo_new(Term *term, Win *wn);
static Term_Container *_split_new(Term_Container *tc1, Term_Container *tc2, Eina_Bool is_horizontal);
static Term_Container *_tabs_new(Term_Container *child, Term_Container *parent);
static Term * _win_focused_term_get(Win *wn);
static void _term_focus(Term *term, Eina_Bool force);
static void _term_free(Term *term);
static void _term_media_update(Term *term, const Config *config);
static void _term_miniview_check(Term *term);
static void _popmedia_queue_process(Term *term);
static void _cb_size_hint(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event EINA_UNUSED);


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

   eina_stringshare_del(tc->title);

   free(tc);
}

static void
_solo_split(Term_Container *tc, const char *cmd, Eina_Bool is_horizontal)
{
   Solo *solo;
   Split *split;
   Term *tm_new, *tm;
   Term_Container *tc_split, *tc_solo_new, *tc_parent;
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
   tc_solo_new = _solo_new(tm_new, wn);
   evas_object_data_set(tm_new->termio, "sizedone", tm_new->termio);

   tc_split = _split_new(tc, tc_solo_new, is_horizontal);

   obj_split = tc_split->get_evas_object(tc_split);

   tc_parent->swallow(tc_parent, tc, tc_split);

   evas_object_show(obj_split);

   split = (Split*) tc_split;

   tc_split->swallow(tc_split, split->tc1, _tabs_new(split->tc1, tc_split));
   tc_split->swallow(tc_split, split->tc2, _tabs_new(split->tc2, tc_split));

   DBG("split");
   _term_focus(tm_new, EINA_FALSE);
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

static void
_solo_set_title(Term_Container *tc, Term_Container *child EINA_UNUSED,
                const char *title)
{
   eina_stringshare_del(tc->title);
   tc->title = eina_stringshare_add(title);
   DBG("set title: '%s'", title);
   tc->parent->set_title(tc->parent, tc, title);
}

static void
_solo_bell(Term_Container *tc EINA_UNUSED, Term_Container *child EINA_UNUSED)
{
   DBG("bell is_focused:%d", tc->is_focused);
   if (tc->is_focused)
     return;
   tc->missed_bell = EINA_TRUE;
   tc->parent->bell(tc->parent, tc);
}

static void
_solo_unfocus(Term_Container *tc, Term_Container *relative EINA_UNUSED)
{
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);

   DBG("unfocus %d", tc->is_focused);

   if (!tc->is_focused)
     return;

   if (tc->parent != relative)
     {
        DBG("unfocus from child");
        tc->parent->unfocus(tc->parent, tc);
     }

   tc->is_focused = EINA_FALSE;
}

static void
_solo_focus(Term_Container *tc, Term_Container *relative EINA_UNUSED)
{
   Solo *solo;
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;

   DBG("focus %d", tc->is_focused);

   if (tc->is_focused)
     return;

   tc->missed_bell = EINA_FALSE;

   if (tc->parent == relative)
     {
        DBG("focus from parent");
        _term_focus(solo->term, EINA_FALSE);
     }
   else
     {
        DBG("focus from child");
        tc->parent->focus(tc->parent, tc);
     }

   tc->is_focused = EINA_TRUE;
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
   tc->swallow = NULL;
   tc->focus = _solo_focus;
   tc->unfocus = _solo_unfocus;
   tc->set_title = _solo_set_title;
   tc->bell = _solo_bell;
   tc->close= _solo_close;
   tc->title = eina_stringshare_add("Terminology");
   tc->type = TERM_CONTAINER_TYPE_SOLO;

   DBG("tc:%p", tc);

   tc->parent = NULL;
   tc->wn = wn;

   solo->term = term;

   term->container = tc;

   return tc;
}

/* }}} */
/* {{{ Win */

static void
_cb_win_focus_in(void *data,
                 Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Win *wn = data;
   Term_Container *tc = (Term_Container*) wn;
   Term *term;

   DBG("win focus in");

   if (!tc->is_focused) elm_win_urgent_set(wn->win, EINA_FALSE);
   tc->is_focused = EINA_TRUE;
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

   if (term)
     _term_focus(term, EINA_TRUE);
}

static void
_cb_win_focus_out(void *data, Evas_Object *obj EINA_UNUSED,
                  void *event EINA_UNUSED)
{
   Win *wn = data;
   Term *term;
   Term_Container *tc = (Term_Container*) wn;

   DBG("win focus out");

   tc->is_focused = EINA_FALSE;
   if ((wn->cmdbox_up) && (wn->cmdbox))
     elm_object_focus_set(wn->cmdbox, EINA_FALSE);
   term = _win_focused_term_get(wn);
   DBG("term:%p", term);
   if (!term)
     return;
   tc = term->container;
   tc->unfocus(tc, tc);
   edje_object_signal_emit(term->bg, "focus,out", "terminology");
   edje_object_signal_emit(term->base, "focus,out", "terminology");
   if (!wn->cmdbox_up)
     elm_object_focus_set(term->termio, EINA_FALSE);
}

static Eina_Bool
_win_is_focused(Win *wn)
{
   Term_Container *tc;
   if (!wn)
     return EINA_FALSE;

   tc = (Term_Container*) wn;

   return tc->is_focused;
}


int win_term_set(Win *wn, Term *term)
{
   Term_Container *tc_win = NULL, *tc_tabs = NULL, *tc_child = NULL;
   Evas_Object *base = win_base_get(wn);
   Evas *evas = evas_object_evas_get(base);

   tc_child = _solo_new(term, wn);
   if (!tc_child)
     goto bad;

   tc_win = (Term_Container*) wn;

   tc_tabs = _tabs_new(tc_child, tc_win);
   if (!tc_tabs)
     goto bad;

   /* TODO: resize track */
   //_term_resize_track_start(sp);

   tc_win->swallow(tc_win, NULL, tc_tabs);

   _cb_size_hint(term, evas, term->termio, NULL);

   return 0;

bad:
   free(tc_child);
   free(tc_tabs);
   return -1;
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
_cb_del(void *data, Evas *e EINA_UNUSED,
        Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
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
        _term_free(term);
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
_win_term_next(Term_Container *tc EINA_UNUSED, Term_Container *child)
{
   return child->term_first(child);
}

static Term *
_win_term_prev(Term_Container *tc EINA_UNUSED, Term_Container *child)
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
   Evas_Object *o;
   Eina_Bool refocus = EINA_FALSE;

   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;
   base = win_base_get(wn);

   if (orig)
     {
        o = edje_object_part_swallow_get(base, "terminology.content");
        edje_object_part_unswallow(base, o);
        evas_object_hide(o);
        refocus = tc->is_focused;
     }
   o = child->get_evas_object(child);
   edje_object_part_swallow(base, "terminology.content", o);
   evas_object_show(o);
   child->parent = tc;
   wn->child = child;
   if (refocus)
     child->focus(child, tc);
}

static void
_win_close(Term_Container *tc, Term_Container *child EINA_UNUSED,
           Eina_Bool refocus EINA_UNUSED)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;

   eina_stringshare_del(tc->title);

   win_free(wn);
}

static void
_win_focus(Term_Container *tc, Term_Container *child EINA_UNUSED)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;
   DBG("focus from child");

   if (!tc->is_focused) elm_win_urgent_set(wn->win, EINA_FALSE);
   tc->is_focused = EINA_TRUE;
}

static void
_win_unfocus(Term_Container *tc, Term_Container *child EINA_UNUSED)
{
   tc->is_focused = EINA_FALSE;
}

static void
_win_bell(Term_Container *tc, Term_Container *child EINA_UNUSED)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;

   if (tc->is_focused) return;

   if (wn->config->urg_bell)
     {
        elm_win_urgent_set(wn->win, EINA_TRUE);
     }
}

static void
_win_set_title(Term_Container *tc, Term_Container *child EINA_UNUSED,
               const char *title)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;

   eina_stringshare_del(tc->title);
   tc->title =  eina_stringshare_ref(title);

   DBG("set title: '%s'", title);

   elm_win_title_set(wn->win, title);
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
   tc->swallow = _win_swallow;
   tc->focus = _win_focus;
   tc->unfocus = _win_unfocus;
   tc->set_title = _win_set_title;
   tc->bell = _win_bell;
   tc->close = _win_close;
   tc->title = eina_stringshare_add("Terminology");
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

   tm = evas_object_data_get(term, "term");
   if (!tm) return;

   wn->terms = eina_list_remove(wn->terms, tm);
   tc = tm->container;
   tc->close(tc, tc, _term_is_focused(tm));

   _term_free(tm);
}

static Term *
_win_focused_term_get(Win *wn)
{
   Term *term;
   Eina_List *l;
   /* TODO: have that property in Win */

   EINA_LIST_FOREACH(wn->terms, l, term)
     {
        DBG("term:%p", term);
        if (_term_is_focused(term)) return term;
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
   Eina_Bool refocus;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   /* TODO: shouldn't have to do that… */
   o = orig->get_evas_object(orig);
   evas_object_geometry_get(o, &x, &y, &w, &h);

   if (orig == split->last_focus)
     split->last_focus = new_child;

   refocus = orig->is_focused;

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

   tc->missed_bell = EINA_FALSE;
   if (split->tc1->missed_bell || split->tc2->missed_bell)
     tc->missed_bell = EINA_TRUE;

   if (refocus)
     new_child->focus(new_child, tc);
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
             Eina_Bool refocus)
{
   Split *split;
   Term_Container *parent, *other_child;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   elm_object_part_content_unset(split->panes, PANES_TOP);
   elm_object_part_content_unset(split->panes, PANES_BOTTOM);

   parent = tc->parent;
   other_child = (child == split->tc1) ? split->tc2 : split->tc1;
   parent->swallow(parent, tc, other_child);
   if (refocus)
     {
        other_child->focus(other_child, tc->parent);
     }

   evas_object_del(split->panes);

   eina_stringshare_del(tc->title);
   free(tc);
}

static void
_split_focus(Term_Container *tc, Term_Container *relative)
{
   Split *split;
   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   if (tc->is_focused)
     return;

   if (tc->parent == relative)
     {
        DBG("focus from parent");
        split->last_focus->focus(split->last_focus, tc);
     }
   else
     {
        DBG("focus from child");
        split->last_focus = relative;
        tc->parent->focus(tc->parent, tc);
     }

   tc->missed_bell = EINA_FALSE;
   if (split->tc1->missed_bell || split->tc2->missed_bell)
     tc->missed_bell = EINA_TRUE;
}

static void
_split_unfocus(Term_Container *tc, Term_Container *relative EINA_UNUSED)
{
   if (!tc->is_focused)
     return;

   tc->is_focused = EINA_TRUE;

   tc->parent->unfocus(tc->parent, tc);
}

static void
_split_set_title(Term_Container *tc, Term_Container *child,
                 const char *title)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   DBG("set title: '%s' child:%p split->last_focus:%p",
       title, child, split->last_focus);

   if (child == split->last_focus)
     {
        eina_stringshare_del(tc->title);
        tc->title =  eina_stringshare_ref(title);
        tc->parent->set_title(tc->parent, tc, title);
     }
}

static void
_split_bell(Term_Container *tc, Term_Container *child)
{
   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);

   DBG("bell: self:%p child:%p", tc, child);

   if (tc->is_focused)
     return;

   tc->missed_bell = EINA_TRUE;

   tc->parent->bell(tc->parent, tc);
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
   tc->swallow = _split_swallow;
   tc->focus = _split_focus;
   tc->unfocus = _split_unfocus;
   tc->set_title = _split_set_title;
   tc->bell = _split_bell;
   tc->close = _split_close;
   tc->title = eina_stringshare_add("Terminology");
   tc->type = TERM_CONTAINER_TYPE_SPLIT;


   tc->parent = NULL;
   tc->wn = tc1->wn;

   tc1->parent = tc2->parent = tc;

   split->tc1 = tc1;
   split->tc2 = tc2;
   split->last_focus = tc2;

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
_cb_size_hint(void *data,
              Evas *e EINA_UNUSED, Evas_Object *obj, void *event EINA_UNUSED)
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

void
split_horizontally(Evas_Object *win EINA_UNUSED, Evas_Object *term,
                   const char *cmd)
{
   Term *tm;
   Term_Container *tc;

   tm = evas_object_data_get(term, "term");
   if (!tm) return;

   tc = tm->container;
   _solo_split(tc, cmd, EINA_TRUE);
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
   _solo_split(tc, cmd, EINA_FALSE);
}

/* }}} */
/* {{{ Tabs */

static void
_tab_go(Term *term, int tnum)
{
   Term_Container *tc = term->container;

   while (tc)
     {
        Tabs *tabs;
        Tab_Item *tab_item;

        if (tc->type != TERM_CONTAINER_TYPE_TABS)
          {
             tc = tc->parent;
             continue;
          }
        tabs = (Tabs*) tc;
        tab_item = eina_list_nth(tabs->tabs, tnum);
        if (!tab_item)
          {
             tc = tc->parent;
             continue;
          }
        if (tab_item == tabs->current)
          return;
        elm_toolbar_item_selected_set(tabs->current->elm_item, EINA_FALSE);
        elm_toolbar_item_selected_set(tab_item->elm_item, EINA_TRUE);
        return;
     }
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

static void
_tabs_selector_cb_selected(void *data,
                           Evas_Object *obj EINA_UNUSED,
                           void *info);
static void
_tabs_selector_cb_exit(void *data,
                       Evas_Object *obj EINA_UNUSED,
                       void *info EINA_UNUSED);

static void
_tabs_restore(Tabs *tabs)
{
   Eina_List *l;
   Tab_Item *tab_item;
   Evas_Object *o;
   Term_Container *tc = (Term_Container*)tabs;

   DBG("exit: %p", tabs);
   if (!tabs->selector)
     return;

   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        tab_item->selector_entry = NULL;
     }

   o = tabs->current->tc->get_evas_object(tabs->current->tc);
   edje_object_part_swallow(tabs->base, "content", o);
   evas_object_show(o);
   evas_object_show(tabs->base);
   evas_object_smart_callback_del_full(tabs->selector, "selected",
                                  _tabs_selector_cb_selected, tabs);
   evas_object_smart_callback_del_full(tabs->selector, "exit",
                                  _tabs_selector_cb_exit, tabs);
   evas_object_del(tabs->selector);
   evas_object_del(tabs->selector_bg);
   tabs->selector = NULL;
   tabs->selector_bg = NULL;

   /* XXX: reswallow in parent */
   tc->parent->swallow(tc->parent, tc, tc);

   tc->focus(tc, tc->parent);

   elm_toolbar_item_selected_set(tabs->current->elm_item, EINA_TRUE);
}

static void
_tabs_selector_cb_selected(void *data,
                           Evas_Object *obj EINA_UNUSED,
                           void *info)
{
   Tabs *tabs = data;
   Eina_List *l;
   Tab_Item *tab_item;

   DBG("selected: %p info:%p", tabs, info);

   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        if (tab_item->tc->selector_img == info)
          {
             tabs->current = tab_item;
             _tabs_restore(tabs);
             return;
          }
     }

   ERR("Can't find selected tab item");
}

static void
_tabs_selector_cb_exit(void *data,
                       Evas_Object *obj EINA_UNUSED,
                       void *info EINA_UNUSED)
{
   Tabs *tabs = data;

   DBG("exit: %p", tabs);
   _tabs_restore(tabs);
}


static void
_cb_tab_selector_show(void *data,
                      Evas_Object *obj EINA_UNUSED,
                      const char *sig EINA_UNUSED,
                      const char *src EINA_UNUSED)
{
   Tabs *tabs = data;
   Term_Container *tc = (Term_Container *)tabs;
   Eina_List *l;
   int count;
   double z;
   Edje_Message_Int msg;
   Win *wn = tc->wn;
   Tab_Item *tab_item;
   Evas_Object *o;

   DBG("show tabs selector: %p", tabs->selector_bg);

   if (tabs->selector_bg)
     return;

   tabs->selector_bg = edje_object_add(evas_object_evas_get(tabs->base));
   theme_apply(tabs->selector_bg, wn->config, "terminology/sel/base");
   if (wn->config->translucent)
     msg.val = wn->config->opacity;
   else
     msg.val = 100;

   edje_object_message_send(tabs->selector_bg, EDJE_MESSAGE_INT, 1, &msg);
   edje_object_signal_emit(tabs->selector_bg, "begin", "terminology");

   tab_item = tabs->current;
   o = edje_object_part_swallow_get(tabs->base, "content");
   edje_object_part_unswallow(tabs->base, o);
   evas_object_hide(o);

   tabs->selector = sel_add(wn->win);
   DBG("tabs->selector: %p", tabs->selector);
   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        Evas_Object *img;
        Evas_Coord w, h;
        Eina_Bool is_selected, missed_bell;

        img = evas_object_image_filled_add(evas_object_evas_get(wn->win));
        o = tab_item->tc->get_evas_object(tab_item->tc);
        evas_object_lower(o);
        evas_object_move(o, -9999, -9999);
        evas_object_show(o);
        evas_object_clip_unset(o);
        evas_object_image_source_set(img, o);
        evas_object_geometry_get(o, NULL, NULL, &w, &h);
        evas_object_resize(img, w, h);
        evas_object_data_set(img, "tc", tab_item->tc);
        tab_item->tc->selector_img = img;

        is_selected = (tab_item == tabs->current);
        missed_bell = EINA_FALSE;
        tab_item->selector_entry = sel_entry_add(tabs->selector, img,
                                                 is_selected,
                                                 missed_bell, wn->config);
        DBG("adding entry %p selected:%d img:%p selector_entry:%p",
            tab_item, is_selected, img, tab_item->selector_entry);
     }
   edje_object_part_swallow(tabs->selector_bg, "terminology.content",
                            tabs->selector);
   evas_object_show(tabs->selector);

   /* XXX: reswallow in parent */
   tc->parent->swallow(tc->parent, tc, tc);
   evas_object_show(tabs->selector_bg);

   evas_object_smart_callback_add(tabs->selector, "selected",
                                  _tabs_selector_cb_selected, tabs);
   evas_object_smart_callback_add(tabs->selector, "exit",
                                  _tabs_selector_cb_exit, tabs);
   z = 1.0;
   sel_go(tabs->selector);
   count = eina_list_count(tabs->tabs);
   if (count >= 1)
     z = 1.0 / (sqrt(count) * 0.8);
   if (z > 1.0) z = 1.0;
   sel_orig_zoom_set(tabs->selector, z);
   sel_zoom(tabs->selector, z);
   elm_object_focus_set(tabs->selector, EINA_TRUE);
}

static void
_cb_select(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc = term->container;

   while (tc)
     {
        Tabs *tabs;

        if (tc->type != TERM_CONTAINER_TYPE_TABS)
          {
             tc = tc->parent;
             continue;
          }
        tabs = (Tabs*) tc;
        if (eina_list_count(tabs->tabs) < 2)
          {
             tc = tc->parent;
             continue;
          }

        _cb_tab_selector_show(tabs, NULL, NULL, NULL);
        return;
     }
}



static Evas_Object *
_tabs_get_evas_object(Term_Container *container)
{
   Tabs *tabs;
   assert (container->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)container;

   DBG("evas_object: tabs->selector_bg:%p", tabs->selector_bg);
   if (tabs->selector_bg)
     return tabs->selector_bg;

   return tabs->base;
}

static Term *
_tabs_find_term_at_coords(Term_Container *container,
                          Evas_Coord mx,
                          Evas_Coord my)
{
   Tabs *tabs;
   Term_Container *tc;

   assert (container->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)container;

   tc = tabs->current->tc;

   return tc->find_term_at_coords(tc, mx, my);
}

static void
_tabs_size_eval(Term_Container *container, Sizeinfo *info)
{
   int mw = 0, mh = 0;
   Tabs *tabs;
   Term_Container *tc;
   Sizeinfo inforet = {0, 0, 0, 0, 0, 0, 0, 0, 0};

   assert (container->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)container;

   info->min_w = 0;
   info->min_h = 0;
   info->req_w = 0;
   info->req_h = 0;

   evas_object_size_hint_min_get(tabs->base, &mw, &mh);
   info->bg_min_w = mw;
   info->bg_min_h = mh;
   DBG("obj: mw:%d, mh:%d", mw, mh);

   tc = tabs->current->tc;
   tc->size_eval(tc, &inforet);
   DBG("min_w:%d min_h:%d step_x:%d step_y:%d req_w:%d req_h:%d bg_min_w:%d bg_min_h:%d req:%d",
       inforet.min_w, inforet.min_h, inforet.step_x, inforet.step_y,
       inforet.req_w, inforet.req_h, inforet.bg_min_w, inforet.bg_min_h, inforet.req);

   info->min_w += inforet.min_w + mw;
   info->min_h += inforet.min_h + mh;

   info->step_x = inforet.step_x;
   info->step_y = inforet.step_y;

   info->req |= inforet.req;
   if (info->req)
     {
        info->req_h = inforet.req_h;
        info->req_w = inforet.req_w;
     }
   info->bg_min_w = inforet.bg_min_w + mw;
   info->bg_min_h = inforet.bg_min_h + mh;
}

static Eina_List *
_tab_item_find(Tabs *tabs, Term_Container *child)
{
   Eina_List *l;
   Tab_Item *tab_item;

   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        if (tab_item->tc == child)
          return l;
     }
   return NULL;
}

static void
_tabs_bells_check(Tabs *tabs)
{
   Term_Container *tc = (Term_Container*)tabs;
   Eina_List *l;
   Tab_Item *tab_item;

   tc->missed_bell = EINA_FALSE;

   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        if (tab_item->tc->missed_bell)
          {
             tc->missed_bell = EINA_TRUE;
             goto end;
          }
     }
 end:
   edje_object_part_text_set(tabs->base, "terminology.tabmissed.label",
                             tc->missed_bell ? "!" : "");
}

static void
_tabs_close(Term_Container *tc, Term_Container *child,
            Eina_Bool refocus)
{
   int count;
   Tabs *tabs;
   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)tc;

   count = eina_list_count(tabs->tabs);
   if (count == 1)
     {
        if (tabs->selector)
          _tabs_restore(tabs);

        tc->parent->close(tc->parent, tc, refocus);
        evas_object_del(tabs->base);
        evas_object_del(tabs->box);
        evas_object_del(tabs->btn_add);
        evas_object_del(tabs->btn_hide);
        evas_object_del(tabs->tabbar);
        evas_object_del(tabs->bg);
        evas_object_del(tabs->tabbar_spacer);
        evas_object_del(tabs->selector_spacer);
        eina_stringshare_del(tc->title);
        free(tc);
     }
   else
     {
        Eina_List *l;
        Tab_Item *tab_item;
        char buf[32];

        l = _tab_item_find(tabs, child);
        tab_item = l->data;

        if (tab_item == tabs->current)
          {
             Tab_Item *next_tab_item;
             Eina_List *next;

             next = eina_list_next(l);
             if (!next)
               next = tabs->tabs;
             next_tab_item = next->data;

             elm_toolbar_item_selected_set(next_tab_item->elm_item, EINA_TRUE);
             if (refocus)
               {
                  tabs->current->tc->focus(tabs->current->tc, tc);
               }
          }

        if (tab_item->tc->selector_img)
          {
             Evas_Object *o;
             o = tab_item->tc->selector_img;
             tab_item->tc->selector_img = NULL;
             evas_object_del(o);
          }

        elm_object_item_del(tab_item->elm_item);
        tabs->tabs = eina_list_remove_list(tabs->tabs, l);
        free(tab_item);

        _tabs_bells_check(tabs);
        count--;
        snprintf(buf, sizeof(buf), "%i", count);
        edje_object_part_text_set(tabs->base, "terminology.tabcount.label", buf);

        if (eina_list_count(tabs->tabs) == 1 && !tabs->tabbar_shown)
          edje_object_signal_emit(tabs->base, "tabcount,off", "terminology");
     }
}

static Term *
_tabs_term_next(Term_Container *tc, Term_Container *child)
{
   Tabs *tabs;
   Tab_Item *tab_item;
   Eina_List *l;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)tc;
   l = _tab_item_find(tabs, child);
   l = eina_list_next(l);
   if (l)
     {
        tab_item = l->data;
        tc = tab_item->tc;
        return tc->term_first(tc);
     }
   else
     {
        return tc->parent->term_next(tc->parent, tc);
     }
}

static Term *
_tabs_term_prev(Term_Container *tc, Term_Container *child)
{
   Tabs *tabs;
   Tab_Item *tab_item;
   Eina_List *l;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)tc;
   l = _tab_item_find(tabs, child);
   l = eina_list_prev(l);
   if (l)
     {
        tab_item = l->data;
        tc = tab_item->tc;
        return tc->term_last(tc);
     }
   else
     {
        return tc->parent->term_prev(tc->parent, tc);
     }
}

static Term *
_tabs_term_first(Term_Container *tc)
{
   Tabs *tabs;
   Tab_Item *tab_item;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)tc;

   tab_item = tabs->tabs->data;
   tc = tab_item->tc;

   return tc->term_first(tc);
}

static Term *
_tabs_term_last(Term_Container *tc)
{
   Tabs *tabs;
   Tab_Item *tab_item;
   Eina_List *l;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)tc;

   l = eina_list_last(tabs->tabs);
   tab_item = l->data;
   tc = tab_item->tc;

   return tc->term_last(tc);
}

static void
_tabs_swallow(Term_Container *tc, Term_Container *orig,
              Term_Container *new_child)
{
   Tabs *tabs;
   Tab_Item *tab_item;
   Eina_List *l;
   Eina_Bool refocus;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*) tc;

   l = _tab_item_find(tabs, orig);
   tab_item = l->data;
   tab_item->tc = new_child;

   new_child->parent = tc;

   refocus = orig->is_focused;

   if (tabs->selector)
     {
        Evas_Object *img = tab_item->tc->selector_img;
        evas_object_image_source_set(img,
                                     new_child->get_evas_object(new_child));
        evas_object_data_set(img, "tc", new_child);
        sel_entry_update(tab_item->selector_entry);
     }
   else if (tab_item == tabs->current)
     {
        Evas_Object *o;

        o = edje_object_part_swallow_get(tabs->base, "content");
        edje_object_part_unswallow(tabs->base, o);
        evas_object_hide(o);
        edje_object_part_swallow(tabs->base, "content",
                                 new_child->get_evas_object(new_child));
     }

   _tabs_bells_check(tabs);

   if (refocus)
     new_child->focus(new_child, tc);
}


static void
_tab_selected(void *data,
              Evas_Object *obj EINA_UNUSED,
              void *event_info EINA_UNUSED)
{
   Term_Container *tc, *child;
   Tabs *tabs;
   Tab_Item *tab_item = data;

   DBG("selected %p", tab_item->tc);
   tc = tab_item->tc->parent;
   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*) tc;

   if (tabs->current && tab_item == tabs->current)
     return;
   if (tabs->current)
     {
        Evas_Object *o;
        child = tabs->current->tc;
        o = edje_object_part_swallow_get(tabs->base, "content");
        edje_object_part_unswallow(tabs->base, o);
        evas_object_hide(o);
        child = tab_item->tc;
        o = child->get_evas_object(child);
        edje_object_part_swallow(tabs->base, "content", o);
        evas_object_show(o);

        elm_toolbar_item_selected_set(tabs->current->elm_item, EINA_FALSE);
     }
   tabs->current = tab_item;
   tab_item->tc->focus(tab_item->tc, tc);
}

static Tab_Item*
tab_item_new(Tabs *tabs, Term_Container *child)
{
   Elm_Object_Item *toolbar_item;
   Tab_Item *tab_item;
   char buf[32];
   int n;

   tab_item = calloc(1, sizeof(Tab_Item));
   tab_item->tc = child;
   assert(child != NULL);

   toolbar_item = elm_toolbar_item_append(tabs->tabbar,
                                          NULL, "Terminology",
                                          _tab_selected, tab_item);

   elm_toolbar_item_priority_set(toolbar_item, 1);
   tab_item->elm_item = toolbar_item;

   tabs->tabs = eina_list_append(tabs->tabs, tab_item);

   elm_toolbar_item_selected_set(toolbar_item, EINA_TRUE);

   tabs->current = tab_item;

   n = eina_list_count(tabs->tabs);
   snprintf(buf, sizeof(buf), "%i", n);
   edje_object_part_text_set(tabs->base, "terminology.tabcount.label", buf);

   return tab_item;
}

static void
_tab_new_cb(void *data,
            Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Tabs *tabs = data;
   Term_Container *tc_parent = (Term_Container*) tabs,
                  *tc_new;
   Term *tm_new;
   Win *wn = tc_parent->wn;
   DBG("new");

   tm_new = term_new(wn, wn->config,
                     NULL, wn->config->login_shell, NULL,
                     80, 24, EINA_FALSE);
   tc_new = _solo_new(tm_new, wn);
   evas_object_data_set(tm_new->termio, "sizedone", tm_new->termio);

   tc_new->parent = tc_parent;

   tab_item_new(tabs, tc_new);

   tc_new->focus(tc_new, tc_parent);

   if (!tabs->tabbar_shown)
     edje_object_signal_emit(tabs->base, "tabcount,on", "terminology");
}

static void
_cb_new(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc = term->container;

   while (tc)
     {
        Tabs *tabs;

        if (tc->type != TERM_CONTAINER_TYPE_TABS)
          {
             tc = tc->parent;
             continue;
          }
        tabs = (Tabs*) tc;
        if ((eina_list_count(tabs->tabs) < 2) &&
            (tc->parent != (Term_Container*)tc->wn))
          {
             tc = tc->parent;
             continue;
          }
        _tab_new_cb(tabs, NULL, NULL);
        return;
     }
}

void
main_new(Evas_Object *win EINA_UNUSED, Evas_Object *term)
{
   Term *tm;

   tm = evas_object_data_get(term, "term");
   if (!tm) return;

   _cb_new(tm, term, NULL);
}

static void
_cb_tabbar_show(void *data, Evas_Object *obj EINA_UNUSED,
                const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   Tabs *tabs = data;

   DBG("show");
   tabs->tabbar_shown = EINA_TRUE;

   edje_object_signal_emit(tabs->base, "tabbar,on", "terminology");
   edje_object_signal_emit(tabs->base, "tabcontrols,off", "terminology");
   edje_object_signal_emit(tabs->base, "tabcount,off", "terminology");
   edje_object_part_swallow(tabs->base, "terminology.tabbar", tabs->box);
   evas_object_show(tabs->box);
}

static void
_tabs_hide_cb(void *data,
              Evas_Object *obj EINA_UNUSED,
              void *event_info EINA_UNUSED)
{
   Evas_Coord w = 0, h = 0;
   Tabs *tabs = data;
   Term_Container *tc;
   char buf[32];
   int n;

   DBG("hide");

   tc = (Term_Container*) tabs;

   elm_coords_finger_size_adjust(1, &w, 1, &h);

   if (!tabs->tabbar_spacer)
     {
        tabs->tabbar_spacer =
           evas_object_rectangle_add(evas_object_evas_get(tabs->base));
        evas_object_color_set(tabs->tabbar_spacer, 0, 0, 0, 0);
        evas_object_size_hint_min_set(tabs->tabbar_spacer, w, h);
        evas_object_show(tabs->tabbar_spacer);
        edje_object_part_swallow(tabs->base, "terminology.tabbar.spacer",
                                 tabs->tabbar_spacer);
     }

   if (!tabs->selector_spacer)
     {
        tabs->selector_spacer =
           evas_object_rectangle_add(evas_object_evas_get(tabs->base));
        evas_object_color_set(tabs->selector_spacer, 0, 0, 0, 0);
        evas_object_size_hint_min_set(tabs->selector_spacer, w, h);
        evas_object_show(tabs->selector_spacer);
        edje_object_part_swallow(tabs->base, "terminology.tabselector.spacer",
                                 tabs->selector_spacer);
     }

   n = eina_list_count(tabs->tabs);
   snprintf(buf, sizeof(buf), "%i", n);
   edje_object_part_text_set(tabs->base, "terminology.tabcount.label", buf);
   edje_object_part_text_set(tabs->base, "terminology.tabmissed.label",
                             tc->missed_bell ? "!" : "");

   edje_object_signal_emit(tabs->base, "tabbar,off", "terminology");
   edje_object_signal_emit(tabs->base, "tabcontrols,on", "terminology");
   if (eina_list_count(tabs->tabs) > 1)
     edje_object_signal_emit(tabs->base, "tabcount,on", "terminology");
   else
     edje_object_signal_emit(tabs->base, "tabcount,off", "terminology");
   edje_object_part_unswallow(tabs->base, tabs->box);
   evas_object_hide(tabs->box);

   tabs->tabbar_shown = EINA_FALSE;
}

static void
_tab_item_redo_title(Tab_Item *tab_item)
{
   Term_Container *tc = tab_item->tc;

   if (tc->missed_bell)
     {
        const char *custom_title;
        custom_title = eina_stringshare_printf("! %s", tc->title);
        elm_object_item_text_set(tab_item->elm_item, custom_title);
        eina_stringshare_del(custom_title);
     }
   else
     {
        elm_object_item_text_set(tab_item->elm_item, tc->title);
     }
}


static void
_tabs_focus(Term_Container *tc, Term_Container *relative)
{
   Tabs *tabs;

   if (tc->is_focused)
     return;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*) tc;

   if (tc->parent == relative)
     {
        DBG("focus from parent");
        tabs->current->tc->focus(tabs->current->tc, tc);
        _tab_item_redo_title(tabs->current);
     }
   else
     {
        Eina_List *l;
        Tab_Item *tab_item;
        DBG("focus from child");

        l = _tab_item_find(tabs, relative);
        assert(l);

        tab_item = l->data;
        elm_toolbar_item_selected_set(tabs->current->elm_item, EINA_FALSE);
        elm_toolbar_item_selected_set(tab_item->elm_item, EINA_TRUE);
        tc->parent->focus(tc->parent, tc);

        _tab_item_redo_title(tab_item);
     }
   _tabs_bells_check(tabs);
}

static void
_tabs_unfocus(Term_Container *tc, Term_Container *relative EINA_UNUSED)
{
   if (!tc->is_focused)
     return;

   tc->is_focused = EINA_FALSE;

   tc->parent->unfocus(tc->parent, tc);
}

static void
_tabs_bell(Term_Container *tc, Term_Container *child)
{
   Tabs *tabs;
   Tab_Item *tab_item;
   Eina_List *l;

   DBG("bell");

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*) tc;

   _tabs_bells_check(tabs);

   if (tc->is_focused)
     return;

   l = _tab_item_find(tabs, child);
   assert(l);
   tab_item = l->data;

   _tab_item_redo_title(tab_item);

   tc->parent->bell(tc->parent, tc);
}

static void
_tabs_set_title(Term_Container *tc, Term_Container *child,
                const char *title)
{
   Tabs *tabs;
   Tab_Item *tab_item;
   Eina_List *l;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*) tc;

   l = _tab_item_find(tabs, child);
   assert(l);
   tab_item = l->data;

   _tab_item_redo_title(tab_item);

   DBG("set title: '%s' child:%p current->tc:%p",
       title, child, tabs->current->tc);

   if (tabs->selector)
     {
        sel_entry_title_set(tab_item->selector_entry, title);
     }

   if (tab_item == tabs->current)
     {
        eina_stringshare_del(tc->title);
        tc->title =  eina_stringshare_ref(title);
        tc->parent->set_title(tc->parent, tc, title);
     }
}

static Term_Container *
_tabs_new(Term_Container *child, Term_Container *parent)
{
   Win *wn;
   Term_Container *tc;
   Tabs *tabs;
   Evas_Object *o, *ic;

   tabs = calloc(1, sizeof(Tabs));
   if (!tabs)
     {
        free(tabs);
        return NULL;
     }

   wn = child->wn;

   tc = (Term_Container*)tabs;
   tc->term_next = _tabs_term_next;
   tc->term_prev = _tabs_term_prev;
   tc->term_first = _tabs_term_first;
   tc->term_last = _tabs_term_last;
   tc->get_evas_object = _tabs_get_evas_object;
   tc->find_term_at_coords = _tabs_find_term_at_coords;
   tc->size_eval = _tabs_size_eval;
   tc->swallow = _tabs_swallow;
   tc->focus = _tabs_focus;
   tc->unfocus = _tabs_unfocus;
   tc->set_title = _tabs_set_title;
   tc->bell = _tabs_bell;
   tc->close= _tabs_close;
   tc->title = eina_stringshare_add("Terminology");
   tc->type = TERM_CONTAINER_TYPE_TABS;

   tc->parent = parent;
   tc->wn = wn;

   tabs->base = o = edje_object_add(evas_object_evas_get(wn->win));
   theme_apply(o, wn->config, "terminology/tabs");
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(o);

   tabs->box = o = elm_box_add(wn->win);
   elm_box_horizontal_set(o, EINA_TRUE);

   tabs->tabbar = o = elm_toolbar_add(wn->win);
   elm_toolbar_homogeneous_set(o, EINA_FALSE);
   elm_toolbar_shrink_mode_set(o, ELM_TOOLBAR_SHRINK_EXPAND);
   elm_toolbar_transverse_expanded_set(o, EINA_TRUE);
   elm_toolbar_standard_priority_set(o, 0);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(tabs->box, o);
   evas_object_show(o);

   tabs->btn_add = o = elm_button_add(wn->win);
   elm_object_text_set(o, "+");
   elm_box_pack_end(tabs->box, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "clicked", _tab_new_cb, tabs);

   ic = elm_icon_add(wn->win);
   elm_icon_standard_set(ic, "menu/arrow_up");
   tabs->btn_hide = o = elm_button_add(wn->win);
   elm_object_part_content_set(o, "icon", ic);
   elm_box_pack_end(tabs->box, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "clicked", _tabs_hide_cb, tabs);

   child->parent = tc;
   tab_item_new(tabs, child);

   tabs->bg = o = elm_bg_add(wn->win);
   edje_object_part_swallow(tabs->base, "terminology.bg", tabs->bg);
   evas_object_show(o);

   edje_object_part_swallow(tabs->base, "terminology.tabbar", tabs->box);
   o = child->get_evas_object(child);
   DBG("swallow:%p", o);
   edje_object_part_swallow(tabs->base, "content", o);

   edje_object_signal_callback_add(tabs->base, "tabbar,show", "terminology",
                                   _cb_tabbar_show, tabs);
   edje_object_signal_callback_add(tabs->base, "tabselector,show", "terminology",
                                   _cb_tab_selector_show, tabs);

   tabs->tabbar_shown = EINA_TRUE;

   if (((parent->type == TERM_CONTAINER_TYPE_WIN) &&
        wn->config->hide_top_tabbar) ||
       (parent->type != TERM_CONTAINER_TYPE_WIN))
     {
        _tabs_hide_cb(tabs, NULL, NULL);
     }

   return tc;
}


/* }}} */
/* {{{ Term */

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
   if (!_win_is_focused(term->wn))
     return;

   DBG("term mouse in");
   _term_focus(term, EINA_TRUE);
}

static void
_cb_term_mouse_down(void *data, Evas *e EINA_UNUSED,
                    Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Mouse_Down *ev = event;
   Term *term = data;
   Term *term2;

   term2 = _win_focused_term_get(term->wn);
   if (term == term2) return;
   term->down.x = ev->canvas.x;
   term->down.y = ev->canvas.y;
   DBG("term mouse down");
   _term_focus(term, EINA_TRUE);
}

static Eina_Bool
_term_is_focused(Term *term)
{
   Term_Container *tc;

   if (!term)
     return EINA_FALSE;

   tc = term->container;
   return tc->is_focused;
}

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
_term_focus(Term *term, Eina_Bool force)
{
   Term_Container *tc;
   Eina_List *l;
   Term *term2;
   const char *title;

   DBG("term focus: %p %d", term, _term_is_focused(term));
   if (!force && (_term_is_focused(term) || !_win_is_focused(term->wn)))
     return;

   EINA_LIST_FOREACH(term->wn->terms, l, term2)
     {
        if (term2 != term)
          {
             if (_term_is_focused(term2))
               {
                  tc = term2->container;
                  tc->unfocus(tc, tc);
                  edje_object_signal_emit(term2->bg, "focus,out", "terminology");
                  edje_object_signal_emit(term2->base, "focus,out", "terminology");
                  elm_object_focus_set(term2->termio, EINA_FALSE);
               }
          }
     }

   tc = term->container;
   tc->focus(tc, tc);

   edje_object_signal_emit(term->bg, "focus,in", "terminology");
   edje_object_signal_emit(term->base, "focus,in", "terminology");
   if (term->wn->cmdbox) elm_object_focus_set(term->wn->cmdbox, EINA_FALSE);
   elm_object_focus_set(term->termio, EINA_TRUE);

   title = termio_title_get(term->termio);
   if (title)
      tc->set_title(tc, tc, title);

   if (term->missed_bell)
     term->missed_bell = EINA_FALSE;
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
     _term_focus(new_term, EINA_FALSE);

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
     _term_focus(new_term, EINA_FALSE);

   /* TODO: get rid of it? */
   _term_miniview_check(term);
}

static void
_cb_popmedia_del(void *data, Evas *e EINA_UNUSED,
                 Evas_Object *o EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Term *term = data;

   term->popmedia = NULL;
   term->popmedia_deleted = EINA_TRUE;
   edje_object_signal_emit(term->bg, "popmedia,off", "terminology");
}

static void
_cb_popmedia_done(void *data, Evas_Object *obj EINA_UNUSED,
                  const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
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
_cb_media_loop(void *data,
               Evas_Object *obj EINA_UNUSED, void *info EINA_UNUSED)
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
        if (term->miniview_shown)
          {
             if (_term_is_focused(term))
               edje_object_signal_emit(term->bg, "miniview,on", "terminology");
          }
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
_cb_split_h(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc = term->container;

   assert(tc->type == TERM_CONTAINER_TYPE_SOLO);
   _solo_split(tc, NULL, EINA_TRUE);
}

static void
_cb_split_v(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc = term->container;

   assert(tc->type == TERM_CONTAINER_TYPE_SOLO);
   _solo_split(tc, NULL, EINA_FALSE);
}

static void
_cb_title(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc = term->container;
   const char *title = termio_title_get(term->termio);

   if (title)
     tc->set_title(tc, tc, title);
}

static void
_cb_icon(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Term *term = data;
   if (_term_is_focused(term))
     elm_win_icon_name_set(term->wn->win, termio_icon_name_get(term->termio));
}

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
_cb_cmd_aborted(void *data,
                Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
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
_cb_cmd_changed(void *data,
                Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
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
_cb_cmd_hints_changed(void *data, Evas *e EINA_UNUSED,
                      Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Win *wn = data;

   if (wn->cmdbox)
     {
        evas_object_show(wn->cmdbox);
        edje_object_part_swallow(wn->base, "terminology.cmdbox", wn->cmdbox);
     }
}

static void
_cb_cmdbox(void *data,
           Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
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
_cb_media_del(void *data, Evas *e EINA_UNUSED,
              Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
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
_term_free(Term *term)
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
_cb_tabcount_prev(void *data, Evas_Object *obj EINA_UNUSED,
                  const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   _cb_prev(data, NULL, NULL);
}

static void
_cb_tabcount_next(void *data, Evas_Object *obj EINA_UNUSED,
                  const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
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

   if (_term_is_focused(term) && (_win_is_focused(term->wn)))
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
   Term_Container *tc;

   tc = term->container;

   tc->bell(tc, tc);

   if (!tc->wn->config->disable_visual_bell)
     {
        edje_object_signal_emit(term->bg, "bell", "terminology");
        edje_object_signal_emit(term->base, "bell", "terminology");
        if (tc->wn->config->bell_rings)
          {
             edje_object_signal_emit(term->bg, "bell,ring", "terminology");
             edje_object_signal_emit(term->base, "bell,ring", "terminology");
          }
     }
}


static void
_cb_options_done(void *data)
{
   Win *wn = data;
   Eina_List *l;
   Term *term;

   if (!_win_is_focused(wn)) return;
   EINA_LIST_FOREACH(wn->terms, l, term)
     {
        if (_term_is_focused(term))
          {
             elm_object_focus_set(term->termio, EINA_TRUE);
             termio_event_feed_mouse_in(term->termio);
          }
     }
}

static void
_cb_options(void *data, Evas_Object *obj EINA_UNUSED,
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
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

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

   wn->terms = eina_list_append(wn->terms, term);

   return term;
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

