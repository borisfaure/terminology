#include <assert.h>
#include <Elementary.h>
#include <Ecore_Input.h>
#include <Ecore_IMF.h>
#include <Ecore_IMF_Evas.h>
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
#include "sel.h"
#include "controls.h"
#include "keyin.h"
#include "term_container.h"


/**
 * Design:
 * A terminal widget is Term. It hosts various Evas_Object, like a `termio`
 * handling the textgrid.
 * It is hosted in a Term_Container of type Solo.
 * On Term_Container:
 *   It is a generic structure with a set of function pointers. It is a simple
 *   way to objectify and have genericity between a Window, a Split or Tabs.
 * Solo, Win, Split, Tabs have a Term_Container as their first field and thus
 * can be casted to Term_Container to have access to those APIs.
 *
 * Solo is the simplest container, hosting just a Term.
 * Win is a window and has only one container child.
 * Split is a widget to separate an area of the screen in 2 and thus has 2
 * children that can be either Solo or Tabs.
 * Tabs is a Term_Container containing many containers (at the moment, only
 * Solo ones) and have a system of tabs.
 *
 * All the windows are in the `wins` list.
 */




/* specific log domain to help debug code in that file */
int _win_log_dom = -1;

#undef CRITICAL
#undef ERR
#undef WRN
#undef INF
#undef DBG

#define CRITICAL(...) EINA_LOG_DOM_CRIT(_win_log_dom, __VA_ARGS__)
#define ERR(...)      EINA_LOG_DOM_ERR(_win_log_dom, __VA_ARGS__)
#define WRN(...)      EINA_LOG_DOM_WARN(_win_log_dom, __VA_ARGS__)
#define INF(...)      EINA_LOG_DOM_INFO(_win_log_dom, __VA_ARGS__)
#define DBG(...)      EINA_LOG_DOM_DBG(_win_log_dom, __VA_ARGS__)

#define PANES_TOP "top"
#define PANES_BOTTOM "bottom"

/* {{{ Structs */

typedef struct _Split Split;
typedef struct _Tabbar Tabbar;

struct _Tabbar
{
   struct {
      Evas_Object *box;
      Eina_List *tabs;
   } l, r;
};

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
   Evas_Object *sel;
   Evas_Object *sendfile_request;
   Evas_Object *sendfile_progress;
   Evas_Object *sendfile_progress_bar;
   Evas_Object *tabcount_spacer;
   Evas_Object *tab_spacer;
   Evas_Object *tab_region_base;
   Evas_Object *tab_region_bg;
   Eina_List   *popmedia_queue;
   Ecore_Timer *sendfile_request_hide_timer;
   Ecore_Timer *sendfile_progress_hide_timer;
   const char  *sendfile_dir;
   Media_Type   poptype, mediatype;
   Tabbar       tabbar;
   int          step_x, step_y, min_w, min_h, req_w, req_h;
   struct {
      int       x, y;
   } down;
   int refcnt;
   unsigned char hold : 1;
   unsigned char unswallowed : 1;
   unsigned char missed_bell : 1;
   unsigned char miniview_shown : 1;
   unsigned char popmedia_deleted : 1;

   Eina_Bool sendfile_request_enabled : 1;
   Eina_Bool sendfile_progress_enabled : 1;
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
     void *selector_entry;
};

struct _Tabs {
     Term_Container tc;
     Evas_Object *selector;
     Evas_Object *selector_bg;
     Eina_List *tabs; // Tab_Item
     Tab_Item *current;
};

struct _Split
{
   Term_Container tc;
   Term_Container *tc1, *tc2; // left/right or top/bottom child splits, null if leaf
   Evas_Object *panes;
   Term_Container *last_focus;

   unsigned char is_horizontal : 1;
};




struct _Win
{
   Term_Container tc; /* has to be first field */

   Keys_Handler khdl;
   const char *preedit_str;
   Term_Container *child;
   Evas_Object *win;
   Evas_Object *conform;
   Evas_Object *backbg;
   Evas_Object *base;
   Config      *config;
   Eina_List   *terms;
   Split       *split;
   Ecore_Job   *size_job;
   Evas_Object *cmdbox;
   Ecore_Timer *cmdbox_del_timer;
   Ecore_Timer *cmdbox_focus_timer;
   unsigned char focused : 1;
   unsigned char cmdbox_up : 1;
   unsigned char group_input : 1;
   unsigned char group_only_visible : 1;
   unsigned char group_once_handled : 1;

   unsigned int  on_popover;
};

/* }}} */
static Eina_List   *wins = NULL;

static Eina_Bool _win_is_focused(Win *wn);
static Eina_Bool _term_is_focused(Term *term);
static Term_Container *_solo_new(Term *term, Win *wn);
static Term_Container *_split_new(Term_Container *tc1, Term_Container *tc2, Eina_Bool is_horizontal);
static Term_Container *_tabs_new(Term_Container *child, Term_Container *parent);
static void _term_free(Term *term);
static void _term_media_update(Term *term, const Config *config);
static void _term_miniview_check(Term *term);
static void _popmedia_queue_process(Term *term);
static void _cb_size_hint(void *data, Evas *_e EINA_UNUSED, Evas_Object *obj, void *_event EINA_UNUSED);
static void _tab_new_cb(void *data, Evas_Object *_obj EINA_UNUSED, void *_event_info EINA_UNUSED);
static Tab_Item* tab_item_new(Tabs *tabs, Term_Container *child);
static void _tabs_refresh(Tabs *tabs);
static void _term_tabregion_free(Term *term);
static void _set_trans(Config *config, Evas_Object *bg, Evas_Object *base);
static void _imf_event_commit_cb(void *data, Ecore_IMF_Context *_ctx EINA_UNUSED, void *event);


/* {{{ Solo */

static Evas_Object *
_solo_get_evas_object(const Term_Container *tc)
{
   Solo *solo;
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;

   return solo->term->bg;
}

static Term *
_solo_focused_term_get(const Term_Container *container)
{
   Solo *solo;
   assert (container->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)container;

   return container->is_focused ? solo->term : NULL;
}

static Term *
_solo_find_term_at_coords(const Term_Container *tc,
                          Evas_Coord _mx EINA_UNUSED,
                          Evas_Coord _my EINA_UNUSED)
{
   Solo *solo;
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;

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
_solo_close(Term_Container *tc,
            Term_Container *_child EINA_UNUSED)
{
   Solo *solo;
   Term *term;

   DBG("close");
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;
   tc->parent->close(tc->parent, tc);

   eina_stringshare_del(tc->title);

   term = solo->term;
   term->container = NULL;

   free(tc);
}

static void
_solo_tabs_new(Term_Container *tc)
{
   if (tc->parent->type != TERM_CONTAINER_TYPE_TABS)
     _tabs_new(tc, tc->parent);
   _tab_new_cb(tc->parent, NULL, NULL);
}

static void
_solo_split(Term_Container *tc,
            Term_Container *_child EINA_UNUSED,
            Term *from,
            const char *cmd,
            Eina_Bool is_horizontal)
{
   tc->parent->split(tc->parent, tc, from, cmd, is_horizontal);
}

static Term *
_solo_term_next(const Term_Container *tc,
                const Term_Container *_child EINA_UNUSED)
{
   return tc->parent->term_next(tc->parent, tc);
}

static Term *
_solo_term_prev(const Term_Container *tc,
                const Term_Container *_child EINA_UNUSED)
{
   return tc->parent->term_prev(tc->parent, tc);
}

static Term *
_solo_term_up(const Term_Container *tc,
              const Term_Container *_child EINA_UNUSED)
{
   return tc->parent->term_up(tc->parent, tc);
}

static Term *
_solo_term_down(const Term_Container *tc,
                const Term_Container *_child EINA_UNUSED)
{
   return tc->parent->term_down(tc->parent, tc);
}

static Term *
_solo_term_left(const Term_Container *tc,
                const Term_Container *_child EINA_UNUSED)
{
   return tc->parent->term_left(tc->parent, tc);
}

static Term *
_solo_term_right(const Term_Container *tc,
                 const Term_Container *_child EINA_UNUSED)
{
   return tc->parent->term_right(tc->parent, tc);
}

static Term *
_solo_term_first(const Term_Container *tc)
{
   Solo *solo;
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;
   return solo->term;
}

static Term *
_solo_term_last(const Term_Container *tc)
{
   Solo *solo;
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;
   return solo->term;
}

static void
_solo_set_title(Term_Container *tc,
                Term_Container *_child EINA_UNUSED,
                const char *title)
{
   eina_stringshare_del(tc->title);
   tc->title = eina_stringshare_add(title);
   tc->parent->set_title(tc->parent, tc, title);
}

static void
_solo_bell(Term_Container *tc,
           Term_Container *_child EINA_UNUSED)
{
   Solo *solo;
   Term *term;

   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;
   term = solo->term;

   term->missed_bell = EINA_TRUE;

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
   tc->parent->bell(tc->parent, tc);
}

static void
_solo_unfocus(Term_Container *tc, Term_Container *relative)
{
   Solo *solo;
   Term *term;

   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;
   term = solo->term;

   DBG("tc:%p tc->is_focused:%d from_parent:%d",
       tc, tc->is_focused, tc->parent == relative);
   if (!tc->is_focused)
     return;

   tc->is_focused = EINA_FALSE;
   termio_focus_out(term->termio);

   if (tc->parent != relative)
     tc->parent->unfocus(tc->parent, tc);

   if (!term->config->disable_focus_visuals)
     {
        edje_object_signal_emit(term->bg, "focus,out", "terminology");
        edje_object_signal_emit(term->base, "focus,out", "terminology");
     }
}

static void
_solo_focus(Term_Container *tc, Term_Container *relative)
{
   Solo *solo;
   Term *term;
   const char *title;

   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;
   term = solo->term;

   if (!tc->parent)
     return;

   DBG("tc:%p tc->is_focused:%d from_parent:%d",
       tc, tc->is_focused, tc->parent == relative);
   if (tc->is_focused)
     return;

   term->missed_bell = EINA_FALSE;

   if (tc->parent != relative)
     {
        DBG("focus tc:%p", tc);
        tc->parent->focus(tc->parent, tc);
     }

   tc->is_focused = EINA_TRUE;
   if (term->config->disable_focus_visuals)
     {
        edje_object_signal_emit(term->bg, "focused,set", "terminology");
        edje_object_signal_emit(term->base, "focused,set", "terminology");
     }
   else
     {
        edje_object_signal_emit(term->bg, "focus,in", "terminology");
        edje_object_signal_emit(term->base, "focus,in", "terminology");
     }
   if (term->wn->cmdbox)
     elm_object_focus_set(term->wn->cmdbox, EINA_FALSE);

   elm_object_focus_set(term->termio, EINA_TRUE);
   termio_event_feed_mouse_in(term->termio);
   termio_focus_in(term->termio);

   title = termio_title_get(term->termio);
   if (title)
      tc->set_title(tc, tc, title);

   if (term->missed_bell)
     term->missed_bell = EINA_FALSE;
}

static void
_solo_update(Term_Container *tc)
{
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
}

static Eina_Bool
_solo_is_visible(Term_Container *tc, Term_Container *_child EINA_UNUSED)
{
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   return tc->parent->is_visible(tc->parent, tc);
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
   tc->term_up = _solo_term_up;
   tc->term_down = _solo_term_down;
   tc->term_left = _solo_term_left;
   tc->term_right = _solo_term_right;
   tc->term_first = _solo_term_first;
   tc->term_last = _solo_term_last;
   tc->focused_term_get = _solo_focused_term_get;
   tc->get_evas_object = _solo_get_evas_object;
   tc->split = _solo_split;
   tc->find_term_at_coords = _solo_find_term_at_coords;
   tc->size_eval = _solo_size_eval;
   tc->swallow = NULL;
   tc->focus = _solo_focus;
   tc->unfocus = _solo_unfocus;
   tc->set_title = _solo_set_title;
   tc->bell = _solo_bell;
   tc->close = _solo_close;
   tc->update = _solo_update;
   tc->title = eina_stringshare_add("Terminology");
   tc->is_visible = _solo_is_visible;
   tc->type = TERM_CONTAINER_TYPE_SOLO;

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
                 Evas_Object *_obj EINA_UNUSED,
                 void *_event EINA_UNUSED)
{
   Win *wn = data;
   Term_Container *tc = (Term_Container*) wn;
   Term *term;

   DBG("FOCUS_IN tc:%p tc->is_focused:%d",
       tc, tc->is_focused);
   if (!tc->is_focused)
     elm_win_urgent_set(wn->win, EINA_FALSE);
   tc->is_focused = EINA_TRUE;
   if ((wn->cmdbox_up) && (wn->cmdbox))
     elm_object_focus_set(wn->cmdbox, EINA_TRUE);
   if (wn->on_popover)
       return;

   term = tc->focused_term_get(tc);

   if ( wn->config->mouse_over_focus )
     {
        Term *term_mouse;
        Evas_Coord mx, my;

        evas_pointer_canvas_xy_get(evas_object_evas_get(wn->win), &mx, &my);
        term_mouse = tc->find_term_at_coords(tc, mx, my);
        if ((term_mouse) && (term_mouse != term))
          {
             if (term)
               {
                  if (!term->config->disable_focus_visuals)
                    {
                       edje_object_signal_emit(term->bg, "focus,out", "terminology");
                       edje_object_signal_emit(term->base, "focus,out", "terminology");
                    }
               }
             term = term_mouse;
          }
     }

   if (term)
     {
        term_focus(term);
     }
   else
     {
        DBG("focus tc:%p", tc);
        tc->focus(tc, tc);
     }
}

static void
_cb_win_focus_out(void *data,
                  Evas_Object *_obj EINA_UNUSED,
                  void *_event EINA_UNUSED)
{
   Win *wn = data;
   Term_Container *tc = (Term_Container*) wn;

   DBG("FOCUS OUT tc:%p tc->is_focused:%d",
       tc, tc->is_focused);
   tc->unfocus(tc, NULL);
}

static Eina_Bool
_win_is_focused(Win *wn)
{
   Term_Container *tc;

   if (!wn)
     return EINA_FALSE;

   tc = (Term_Container*) wn;

   DBG("tc:%p tc->is_focused:%d",
       tc, tc->is_focused);
   return tc->is_focused;
}


int win_term_set(Win *wn, Term *term)
{
   Term_Container *tc_win = NULL, *tc_child = NULL;
   Evas_Object *base = win_base_get(wn);
   Evas *evas = evas_object_evas_get(base);

   tc_child = _solo_new(term, wn);
   if (!tc_child)
     goto bad;

   tc_win = (Term_Container*) wn;

   tc_win->swallow(tc_win, NULL, tc_child);

   _cb_size_hint(term, evas, term->termio, NULL);

   return 0;

bad:
   free(tc_child);
   return -1;
}


Evas_Object *
win_base_get(const Win *wn)
{
   return wn->base;
}

Config *
win_config_get(const Win *wn)
{
   return wn->config;
}

Eina_List *
win_terms_get(const Win *wn)
{
   return wn->terms;
}

Evas_Object *
win_evas_object_get(const Win *wn)
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
_cb_del(void *data,
        Evas *_e EINA_UNUSED,
        Evas_Object *_obj EINA_UNUSED,
        void *_event EINA_UNUSED)
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
        term_unref(term);
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
        evas_object_smart_callback_del_full(wn->win, "focus,in", _cb_win_focus_in, wn);
        evas_object_smart_callback_del_full(wn->win, "focus,out", _cb_win_focus_out, wn);
        evas_object_event_callback_del_full(wn->win, EVAS_CALLBACK_DEL, _cb_del, wn);
        evas_object_del(wn->win);
     }
   if (wn->size_job)
     ecore_job_del(wn->size_job);
   if (wn->config)
     config_del(wn->config);
   if (wn->preedit_str)
     eina_stringshare_del(wn->preedit_str);
   keyin_compose_seq_reset(&wn->khdl);
   if (wn->khdl.imf)
     {
        ecore_imf_context_event_callback_del
          (wn->khdl.imf, ECORE_IMF_CALLBACK_COMMIT, _imf_event_commit_cb);
        ecore_imf_context_del(wn->khdl.imf);
     }
   ecore_imf_shutdown();
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
win_add(const char *name, const char *role,
        const char *title, const char *icon_name)
{
   Evas_Object *win;

   if (!name) name = "main";
   if (!title) title = "Terminology";
   if (!icon_name) icon_name = "terminology";

   win = elm_win_add(NULL, name, ELM_WIN_BASIC);
   elm_win_title_set(win, title);
   elm_win_icon_name_set(win, icon_name);
   if (role) elm_win_role_set(win, role);

   elm_win_autodel_set(win, EINA_TRUE);

   return win;
}

static Evas_Object *
_win_get_evas_object(const Term_Container *tc)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;

   return wn->win;
}

static Term *
_win_term_next(const Term_Container *_tc EINA_UNUSED,
               const Term_Container *child)
{
   return child->term_first(child);
}

static Term *
_win_term_prev(const Term_Container *_tc EINA_UNUSED,
               const Term_Container *child)
{
   return child->term_last(child);
}

static Term *
_win_term_up(const Term_Container *_tc EINA_UNUSED,
             const Term_Container *child EINA_UNUSED)
{
   return NULL;
}

static Term *
_win_term_down(const Term_Container *_tc EINA_UNUSED,
               const Term_Container *child EINA_UNUSED)
{
   return NULL;
}

static Term *
_win_term_left(const Term_Container *_tc EINA_UNUSED,
               const Term_Container *child EINA_UNUSED)
{
   return NULL;
}

static Term *
_win_term_right(const Term_Container *_tc EINA_UNUSED,
                const Term_Container *child EINA_UNUSED)
{
   return NULL;
}

static Term *
_win_term_first(const Term_Container *tc)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;
   return wn->child->term_first(wn->child);
}

static Term *
_win_term_last(const Term_Container *tc)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;
   return wn->child->term_last(wn->child);
}

static Term *
_win_focused_term_get(const Term_Container *tc)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;

   return tc->is_focused ? wn->child->focused_term_get(wn->child) : NULL;
}

static Term *
_win_find_term_at_coords(const Term_Container *tc,
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
             Term_Container *new_child)
{
   Win *wn;
   Evas_Object *base;
   Evas_Object *o;

   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;
   base = win_base_get(wn);

   DBG("orig:%p", orig);

   if (orig)
     {
        o = orig->get_evas_object(orig);
        edje_object_part_unswallow(base, o);
     }

   o = new_child->get_evas_object(new_child);
   edje_object_part_swallow(base, "terminology.content", o);

   evas_object_show(o);
   new_child->parent = tc;
   wn->child = new_child;
}

static void
_win_close(Term_Container *tc,
           Term_Container *_child EINA_UNUSED)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;
   eina_stringshare_del(tc->title);
   win_free(wn);
}

static void
_win_focus(Term_Container *tc, Term_Container *relative)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;
   DBG("tc:%p tc->is_focused:%d from_child:%d",
       tc, tc->is_focused, wn->child == relative);
   if (relative != wn->child)
     {
        wn->child->focus(wn->child, tc);
        elm_win_keyboard_mode_set(wn->win, ELM_WIN_KEYBOARD_TERMINAL);
        if (wn->khdl.imf)
          {
             Term *focused;

             ecore_imf_context_input_panel_show(wn->khdl.imf);
             ecore_imf_context_reset(wn->khdl.imf);
             ecore_imf_context_focus_in(wn->khdl.imf);

             focused = tc->focused_term_get(tc);
             if (focused)
               termio_imf_cursor_set(focused->termio, wn->khdl.imf);
          }
     }

   if (!tc->is_focused)
     elm_win_urgent_set(wn->win, EINA_FALSE);
   tc->is_focused = EINA_TRUE;
}

static void
_win_unfocus(Term_Container *tc, Term_Container *relative)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;

   DBG("tc:%p tc->is_focused:%d from_child:%d",
       tc, tc->is_focused, wn->child == relative);
   elm_win_keyboard_mode_set(wn->win, ELM_WIN_KEYBOARD_OFF);
   if (relative != wn->child && wn->child)
     {
        if (wn->khdl.imf)
          {
             Term *focused;

             ecore_imf_context_reset(wn->khdl.imf);
             focused = tc->focused_term_get(tc);
             if (focused)
               termio_imf_cursor_set(focused->termio, wn->khdl.imf);
             ecore_imf_context_focus_out(wn->khdl.imf);
             ecore_imf_context_input_panel_hide(wn->khdl.imf);
          }
        tc->is_focused = EINA_FALSE;
        wn->child->unfocus(wn->child, tc);

        if ((wn->cmdbox_up) && (wn->cmdbox))
          elm_object_focus_set(wn->cmdbox, EINA_FALSE);
     }
}

static void
_win_bell(Term_Container *tc,
          Term_Container *_child EINA_UNUSED)
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
_win_set_title(Term_Container *tc,
               Term_Container *_child EINA_UNUSED,
               const char *title)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;

   eina_stringshare_del(tc->title);
   tc->title =  eina_stringshare_ref(title);

   elm_win_title_set(wn->win, title);
}

Eina_Bool
_term_container_is_splittable(Term_Container *tc, Eina_Bool is_horizontal)
{
   int w = 0, h = 0, c_w = 0, c_h = 0;
   Term *tm;

   if (terminology_starting_up)
     return EINA_TRUE;

   tm = tc->term_first(tc);
   evas_object_geometry_get(tm->bg, NULL, NULL, &w, &h);
   evas_object_textgrid_cell_size_get(termio_textgrid_get(tm->termio),
                                      &c_w, &c_h);
   if (is_horizontal)
     {
        if (c_h * 2 > h)
           return EINA_FALSE;
     }
   else
     {
        if (c_w * 2 > w)
           return EINA_FALSE;
     }
   return EINA_TRUE;
}

static void
_win_split(Term_Container *tc, Term_Container *child,
           Term *from, const char *cmd, Eina_Bool is_horizontal)
{
   Win *wn;

   assert (tc->type == TERM_CONTAINER_TYPE_WIN);
   wn = (Win*) tc;

   if (_term_container_is_splittable(tc, is_horizontal))
     {
        Term *tm_new, *tm;
        Term_Container *tc_split, *tc_solo_new;
        char *wdir = NULL;
        char buf[PATH_MAX];
        Evas_Object *base;
        Evas_Object *o;

        // copy the current path to wdir if we should change the directory,
        // passing wdir NULL otherwise:
        if (wn->config->changedir_to_current)
          {
             if (from)
               tm = from;
             else
               tm = tc->focused_term_get(tc);
             if (tm && termio_cwd_get(tm->termio, buf, sizeof(buf)))
               wdir = buf;
          }
        tm_new = term_new(wn, wn->config,
                          cmd, wn->config->login_shell, wdir,
                          80, 24, EINA_FALSE, NULL);
        tc_solo_new = _solo_new(tm_new, wn);
        evas_object_data_set(tm_new->termio, "sizedone", tm_new->termio);

        base = win_base_get(wn);
        o = child->get_evas_object(child);
        edje_object_part_unswallow(base, o);

        tc_split = _split_new(child, tc_solo_new, is_horizontal);

        tc_split->is_focused = tc->is_focused;
        tc->swallow(tc, NULL, tc_split);
     }
   else
     {
        DBG("term is not splittable");
     }
}

static void
_win_update(Term_Container *tc)
{
   Win *wn;

   assert (tc->type == TERM_CONTAINER_TYPE_WIN);
   wn = (Win*) tc;

   wn->child->update(wn->child);
}

static void
_cb_win_key_up(void *data,
               Evas *_e EINA_UNUSED,
               Evas_Object *_obj EINA_UNUSED,
               void *event)
{
   Win *wn = data;
   Evas_Event_Key_Up *ev = event;

   DBG("GROUP key up (%p) (ctrl:%d)",
       wn, evas_key_modifier_is_set(ev->modifiers, "Control"));
   keyin_handle_up(&wn->khdl, ev);
}

#define GROUPED_INPUT_TERM_FOREACH(_wn, _list, _term) \
   EINA_LIST_FOREACH(_wn->terms, _list, _term) \
     if (!_wn->group_only_visible || term_is_visible(_term))


const char *
term_preedit_str_get(Term *term)
{
   Win *wn = term->wn;
   Term_Container *tc = (Term_Container*) wn;

   if (wn->on_popover)
     return NULL;
   tc = (Term_Container*) wn;
   term = tc->focused_term_get(tc);
   if (term)
     {
        return wn->preedit_str;
     }
   return NULL;
}

Ecore_IMF_Context *
term_imf_context_get(Term *term)
{
   Win *wn = term->wn;
   Term_Container *tc = (Term_Container*) wn;
   Term *focused;

   tc = (Term_Container*) wn;
   focused = tc->focused_term_get(tc);
   if (term == focused)
     return wn->khdl.imf;
   return NULL;
}

static void
_imf_event_commit_cb(void *data,
                     Ecore_IMF_Context *_ctx EINA_UNUSED,
                     void *event)
{
   Eina_List *l;
   Term *term;
   Win *wn = data;
   Termpty *ty;
   char *str = event;
   int len;
   DBG("IMF committed '%s'", str);
   if (!str)
     return;
   len = strlen(str);
   if (wn->group_input)
     {
        GROUPED_INPUT_TERM_FOREACH(wn, l, term)
          {
             ty = termio_pty_get(term->termio);
             if (ty)
               termpty_write(ty, str, len);
          }
     }
   else
     {
        Term_Container *tc = (Term_Container*) wn;

        term = tc->focused_term_get(tc);
        if (term)
          {
             ty = termio_pty_get(term->termio);
             if (ty)
               termpty_write(ty, str, len);
          }
     }
   if (wn->preedit_str)
     {
        eina_stringshare_del(wn->preedit_str);
        wn->preedit_str = NULL;
     }
}



static void
_imf_event_delete_surrounding_cb(void *data,
                                 Ecore_IMF_Context *_ctx EINA_UNUSED,
                                 void *event)
{
   Win *wn = data;
   Ecore_IMF_Event_Delete_Surrounding *ev = event;
   DBG("IMF del surrounding %p %i %i", wn, ev->offset, ev->n_chars);
}

static void
_imf_event_preedit_changed_cb(void *data,
                              Ecore_IMF_Context *ctx,
                              void *_event EINA_UNUSED)
{
   Win *wn = data;
   char *preedit_string;
   int cursor_pos;

   ecore_imf_context_preedit_string_get(ctx, &preedit_string, &cursor_pos);
   if (!preedit_string)
     return;
   DBG("IMF preedit str '%s'", preedit_string);
   if (wn->preedit_str)
     eina_stringshare_del(wn->preedit_str);
   wn->preedit_str = eina_stringshare_add(preedit_string);
   free(preedit_string);
}


static void
_cb_win_key_down(void *data,
                 Evas *_e EINA_UNUSED,
                 Evas_Object *_obj EINA_UNUSED,
                 void *event_info)
{
   Win *wn = data;
   Eina_List *l = NULL;
   Term *term = NULL;
   Termpty *ty = NULL;
   Evas_Event_Key_Down *ev = event_info;
   Eina_Bool done = EINA_FALSE;
   int ctrl, alt, shift, win, meta, hyper;

   DBG("GROUP key down (%p) (ctrl:%d)",
       wn, evas_key_modifier_is_set(ev->modifiers, "Control"));

   if (wn->on_popover)
       return;

   ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   alt = evas_key_modifier_is_set(ev->modifiers, "Alt");
   shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   win = evas_key_modifier_is_set(ev->modifiers, "Super");
   meta = evas_key_modifier_is_set(ev->modifiers, "Meta") ||
      evas_key_modifier_is_set(ev->modifiers, "AltGr") ||
      evas_key_modifier_is_set(ev->modifiers, "ISO_Level3_Shift");
   hyper = evas_key_modifier_is_set(ev->modifiers, "Hyper");
   DBG("ctrl:%d alt:%d shift:%d win:%d meta:%d hyper:%d",
       ctrl, alt, shift, win, meta, hyper);

   /* 1st/ Miniview */
   if (wn->group_input)
     {
        GROUPED_INPUT_TERM_FOREACH(wn, l, term)
          {
             done = miniview_handle_key(term_miniview_get(term), ev);
             if (!wn->group_input)
               return;
          }
     }
   else
     {
        Term_Container *tc = (Term_Container*) wn;

        term = tc->focused_term_get(tc);
        if (!term)
          return;
        done = miniview_handle_key(term_miniview_get(term), ev);
     }
   if (done)
     {
        keyin_compose_seq_reset(&wn->khdl);
        goto end;
     }


   /* 2nd/ PopMedia */
   done = EINA_FALSE;
   if (wn->group_input)
     {
        GROUPED_INPUT_TERM_FOREACH(wn, l, term)
          {
             if (term_has_popmedia(term) && !strcmp(ev->key, "Escape"))
               {
                  term_popmedia_close(term);
                  done = EINA_TRUE;
               }
          }
     }
   else
     {
        Term_Container *tc = (Term_Container*) wn;

        term = tc->focused_term_get(tc);
        if (!term)
          return;
        if (term_has_popmedia(term) && !strcmp(ev->key, "Escape"))
          {
             term_popmedia_close(term);
             done = EINA_TRUE;
          }
     }
   if (done)
     {
        keyin_compose_seq_reset(&wn->khdl);
        goto end;
     }

   /* 3rd/ Handle key bindings */
   done = EINA_FALSE;
   if (wn->group_input)
     {
        wn->group_once_handled = EINA_FALSE;
        GROUPED_INPUT_TERM_FOREACH(wn, l, term)
          {
             done = keyin_handle_key_binding(term->termio, ev, ctrl, alt,
                                             shift, win, meta, hyper);
             if (!wn->group_input)
               return;
          }
     }
   else
     {
        Term_Container *tc = (Term_Container*) wn;

        term = tc->focused_term_get(tc);
        if (!term)
          return;
        done = keyin_handle_key_binding(term->termio, ev, ctrl, alt,
                                        shift, win, meta, hyper);
     }
   if (done)
     {
        keyin_compose_seq_reset(&wn->khdl);
        goto end;
     }
   done = EINA_FALSE;

   /* 4th/ Composing */
   /* composing */
   if (wn->khdl.imf)
     {
        // EXCEPTION. Don't filter modifiers alt+shift -> breaks emacs
        // and jed (alt+shift+5 for search/replace for example)
        // Don't filter modifiers alt, is used by shells
        if ((!alt) && (!ctrl))
          {
             Ecore_IMF_Event_Key_Down imf_ev;

             ecore_imf_evas_event_key_down_wrap(ev, &imf_ev);
             if (!wn->khdl.composing)
               {
                  if (ecore_imf_context_filter_event(wn->khdl.imf,
                                                     ECORE_IMF_EVENT_KEY_DOWN,
                                                     (Ecore_IMF_Event *)&imf_ev))
                    goto end;
               }
          }
     }
   if (!wn->khdl.composing)
     {
        Ecore_Compose_State state;
        char *compres = NULL;

        keyin_compose_seq_reset(&wn->khdl);
        wn->khdl.seq = eina_list_append(wn->khdl.seq,
                                        eina_stringshare_add(ev->key));
        state = ecore_compose_get(wn->khdl.seq, &compres);
        if (state == ECORE_COMPOSE_MIDDLE)
          wn->khdl.composing = EINA_TRUE;
        else
          wn->khdl.composing = EINA_FALSE;
        if (!wn->khdl.composing)
          keyin_compose_seq_reset(&wn->khdl);
        else
          goto end;
     }
   else
     {
        Ecore_Compose_State state;
        char *compres = NULL;

        if (key_is_modifier(ev->key))
          goto end;
        wn->khdl.seq = eina_list_append(wn->khdl.seq,
                                        eina_stringshare_add(ev->key));
        state = ecore_compose_get(wn->khdl.seq, &compres);
        if (state == ECORE_COMPOSE_NONE)
          keyin_compose_seq_reset(&wn->khdl);
        else if (state == ECORE_COMPOSE_DONE)
          {
             keyin_compose_seq_reset(&wn->khdl);
             if (compres)
               {
                  int len = strlen(compres);
                  if (wn->group_input)
                    {
                       GROUPED_INPUT_TERM_FOREACH(wn, l, term)
                         {
                            ty = termio_pty_get(term->termio);
                            if (ty && termpty_can_handle_key(ty, &wn->khdl, ev))
                              termpty_write(ty, compres, len);
                         }
                    }
                 else
                    {
                       ty = termio_pty_get(term->termio);
                       if (ty && termpty_can_handle_key(ty, &wn->khdl, ev))
                         termpty_write(ty, compres, len);
                    }
                  free(compres);
                  compres = NULL;
               }
             goto end;
          }
        else
          goto end;
     }

   /* 5th/ send key to pty */
   if (wn->group_input)
     {
        GROUPED_INPUT_TERM_FOREACH(wn, l, term)
          {
             ty = termio_pty_get(term->termio);
             if (ty && termpty_can_handle_key(ty, &wn->khdl, ev))
               keyin_handle_key_to_pty(ty, ev, alt, shift, ctrl);
          }
     }
   else
     {
        ty = termio_pty_get(term->termio);
        if (ty && termpty_can_handle_key(ty, &wn->khdl, ev))
          keyin_handle_key_to_pty(ty, ev, alt, shift, ctrl);
     }

   /* 6th: specifics: jump on keypress / flicker on key */
end:
   if (wn->group_input)
     {
        GROUPED_INPUT_TERM_FOREACH(wn, l, term)
          {
             if (term)
               termio_key_down(term->termio, ev, done);
          }
     }
   else
     {
        if (term)
          termio_key_down(term->termio, ev, done);
     }
}

static void
_cb_win_mouse_down(void *data,
                   Evas *_e EINA_UNUSED,
                   Evas_Object *_obj EINA_UNUSED,
                   void *event)
{
   Win *wn = data;
   Evas_Event_Mouse_Down *ev = event;
   Term *term, *term_mouse;
   Term_Container *tc = (Term_Container*) wn;
   Term_Container *tc_child;

   if (wn->on_popover || wn->group_input)
     return;

   term_mouse = tc->find_term_at_coords(tc, ev->canvas.x, ev->canvas.y);
   term = tc->focused_term_get(tc);
   if (term_mouse == term)
     return;

   if (term)
     {
        tc_child = term->container;
        tc_child->unfocus(tc_child, tc);
     }

   tc_child = term_mouse->container;
   tc_child->focus(tc_child, tc);
}

static void
_cb_win_mouse_move(void *data,
                   Evas *_e EINA_UNUSED,
                   Evas_Object *obj EINA_UNUSED,
                   void *event)
{
   Win *wn = data;
   Evas_Event_Mouse_Move *ev = event;
   Term *term, *term_mouse;
   Term_Container *tc = (Term_Container*) wn;
   Term_Container *tc_child = NULL;

   if (wn->on_popover || wn->group_input)
     return;

   if (!wn->config->mouse_over_focus)
     return;

   term_mouse = tc->find_term_at_coords(tc,
                                        ev->cur.canvas.x, ev->cur.canvas.y);
   term = tc->focused_term_get(tc);
   if (term_mouse == term)
     return;

   DBG("mouse move");
   if (term)
     {
        tc_child = term->container;
        tc_child->unfocus(tc_child, tc);
     }

   tc_child = term_mouse->container;
   DBG("need to focus");
   tc_child->focus(tc_child, tc);
}

static Eina_Bool
_win_is_visible(Term_Container *tc, Term_Container *_child EINA_UNUSED)
{
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);
   return EINA_TRUE;
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

   wn->win = win_add(name, role, title, icon_name);
   if (!wn->win)
     {
        free(wn);
        return NULL;
     }

   tc = (Term_Container*) wn;
   tc->term_next = _win_term_next;
   tc->term_prev = _win_term_prev;
   tc->term_up = _win_term_up;
   tc->term_down = _win_term_down;
   tc->term_left = _win_term_left;
   tc->term_right = _win_term_right;
   tc->term_first = _win_term_first;
   tc->term_last = _win_term_last;
   tc->focused_term_get = _win_focused_term_get;
   tc->get_evas_object = _win_get_evas_object;
   tc->split = _win_split;
   tc->find_term_at_coords = _win_find_term_at_coords;
   tc->size_eval = _win_size_eval;
   tc->swallow = _win_swallow;
   tc->focus = _win_focus;
   tc->unfocus = _win_unfocus;
   tc->set_title = _win_set_title;
   tc->bell = _win_bell;
   tc->close = _win_close;
   tc->update = _win_update;
   tc->is_visible = _win_is_visible;
   tc->title = eina_stringshare_add(title? title : "Terminology");
   tc->type = TERM_CONTAINER_TYPE_WIN;
   tc->wn = wn;

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

   evas_object_event_callback_add(wn->base,
                                  EVAS_CALLBACK_KEY_DOWN,
                                  _cb_win_key_down,
                                  wn);
   evas_object_event_callback_add(wn->base,
                                  EVAS_CALLBACK_KEY_UP,
                                  _cb_win_key_up,
                                  wn);
   evas_object_event_callback_add(wn->base,
                                  EVAS_CALLBACK_MOUSE_DOWN,
                                  _cb_win_mouse_down,
                                  wn);
   evas_object_event_callback_add(wn->base,
                                  EVAS_CALLBACK_MOUSE_MOVE,
                                  _cb_win_mouse_move,
                                  wn);

   if (ecore_imf_init())
     {
        const char *imf_id = ecore_imf_context_default_id_get();
        Evas *e;

        if (!imf_id)
          wn->khdl.imf = NULL;
        else
          {
             const Ecore_IMF_Context_Info *imf_info;

             imf_info = ecore_imf_context_info_by_id_get(imf_id);
             if ((!imf_info->canvas_type) ||
                 (strcmp(imf_info->canvas_type, "evas") == 0))
               wn->khdl.imf = ecore_imf_context_add(imf_id);
             else
               {
                  imf_id = ecore_imf_context_default_id_by_canvas_type_get("evas");
                  if (imf_id)
                    wn->khdl.imf = ecore_imf_context_add(imf_id);
               }
          }

        if (!wn->khdl.imf)
          goto imf_done;

        e = evas_object_evas_get(o);
        ecore_imf_context_client_window_set
          (wn->khdl.imf, (void *)ecore_evas_window_get(ecore_evas_ecore_evas_get(e)));
        ecore_imf_context_client_canvas_set(wn->khdl.imf, e);

        ecore_imf_context_event_callback_add
          (wn->khdl.imf, ECORE_IMF_CALLBACK_COMMIT, _imf_event_commit_cb, wn);
        ecore_imf_context_event_callback_add
          (wn->khdl.imf, ECORE_IMF_CALLBACK_DELETE_SURROUNDING, _imf_event_delete_surrounding_cb, wn);
        ecore_imf_context_event_callback_add
          (wn->khdl.imf, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, _imf_event_preedit_changed_cb, wn);
        /* make IMF usable by a terminal - no preedit, prediction... */
        ecore_imf_context_prediction_allow_set
          (wn->khdl.imf, EINA_FALSE);
        ecore_imf_context_autocapital_type_set
          (wn->khdl.imf, ECORE_IMF_AUTOCAPITAL_TYPE_NONE);
        ecore_imf_context_input_panel_layout_set
          (wn->khdl.imf, ECORE_IMF_INPUT_PANEL_LAYOUT_TERMINAL);
        ecore_imf_context_input_mode_set
          (wn->khdl.imf, ECORE_IMF_INPUT_MODE_FULL);
        ecore_imf_context_input_panel_language_set
          (wn->khdl.imf, ECORE_IMF_INPUT_PANEL_LANG_ALPHABET);
        ecore_imf_context_input_panel_return_key_type_set
          (wn->khdl.imf, ECORE_IMF_INPUT_PANEL_RETURN_KEY_TYPE_DEFAULT);
imf_done:
        if (wn->khdl.imf)
          DBG("Ecore IMF Setup");
        else
          WRN(_("Ecore IMF failed"));

     }

   wins = eina_list_append(wins, wn);
   return wn;
}

void
term_close(Evas_Object *win, Evas_Object *term, Eina_Bool hold_if_requested)
{
   Term *tm;
   Term_Container *tc;
   Win *wn = _win_find(win);

   if (!wn)
     return;

   tm = evas_object_data_get(term, "term");
   if (!tm)
     return;

   if (tm->hold && hold_if_requested)
     return;

   wn->terms = eina_list_remove(wn->terms, tm);
   tc = tm->container;

   tc->close(tc, tc);

   term_unref(tm);
}

/* Returns True if action is permitted */
Eina_Bool
win_is_group_action_handled(Win *wn)
{
   DBG("wn->group_input:%d wn->group_once_handled:%d wn:%p",
       wn->group_input, wn->group_once_handled, wn);
   if (!wn->group_input)
     return EINA_FALSE;
   if (wn->group_once_handled)
     return EINA_TRUE;
   wn->group_once_handled = EINA_TRUE;
   return EINA_FALSE;
}

Eina_Bool
win_is_group_input(Win *wn)
{
   return wn->group_input;
}



static void
_win_toggle_group(Win *wn)
{
   Eina_List *l;
   Term *term;

   DBG("WIN TOGGLE");
   if (!wn->group_input)
     {
        GROUPED_INPUT_TERM_FOREACH(wn, l, term)
          {
             edje_object_signal_emit(term->bg, "focus,in", "terminology");
             termio_event_feed_mouse_in(term->termio);
             termio_focus_in(term->termio);
          }
        wn->group_input = EINA_TRUE;
        DBG("GROUP INPUT is now TRUE");
     }
   else
     {
        wn->group_input = EINA_FALSE;
        DBG("GROUP INPUT is now FALSE");
        GROUPED_INPUT_TERM_FOREACH(wn, l, term)
          {
             edje_object_signal_emit(term->bg, "focus,out", "terminology");
             termio_focus_out(term->termio);
          }
        term = wn->child->term_first(wn->child);
        wn->child->focus(wn->child, &wn->tc);
     }
}


void
win_toggle_all_group(Win *wn)
{
   wn->group_only_visible = EINA_FALSE;
   _win_toggle_group(wn);
}
void
win_toggle_visible_group(Win *wn)
{
   wn->group_only_visible = EINA_TRUE;
   _win_toggle_group(wn);
}

/* }}} */
/* {{{ Splits */

static Term *
_split_term_next(const Term_Container *tc, const Term_Container *child)
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
_split_term_prev(const Term_Container *tc, const Term_Container *child)
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
_split_term_up(const Term_Container *tc,
               const Term_Container *child)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   if (child == split->tc2 && split->is_horizontal)
     return split->tc1->term_last(split->tc1);
   else
     return tc->parent->term_up(tc->parent, tc);
}

static Term *
_split_term_down(const Term_Container *tc,
                 const Term_Container *child)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   if (child == split->tc1 && split->is_horizontal)
     return split->tc2->term_first(split->tc2);
   else
     return tc->parent->term_down(tc->parent, tc);
}

static Term *
_split_term_left(const Term_Container *tc,
                 const Term_Container *child)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   if (child == split->tc2 && !split->is_horizontal)
     return split->tc1->term_last(split->tc1);
   else
     return tc->parent->term_left(tc->parent, tc);
}

static Term *
_split_term_right(const Term_Container *tc,
                  const Term_Container *child)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   if (child == split->tc1 && !split->is_horizontal)
     return split->tc2->term_first(split->tc2);
   else
     return tc->parent->term_right(tc->parent, tc);
}

static Term *
_split_term_first(const Term_Container *tc)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   return split->tc1->term_first(split->tc1);
}

static Term *
_split_term_last(const Term_Container *tc)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   return split->tc2->term_last(split->tc2);
}

static Evas_Object *
_split_get_evas_object(const Term_Container *tc)
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

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   assert (orig && (orig == split->tc1 || orig == split->tc2));

   if (split->last_focus == orig)
     split->last_focus = new_child;

   o = orig->get_evas_object(orig);
   evas_object_hide(o);

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
   evas_object_show(o);
   evas_object_show(split->panes);
}

static Term *
_split_focused_term_get(const Term_Container *tc)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   return tc->is_focused ?
      split->last_focus->focused_term_get(split->last_focus)
      : NULL;
}

static Term *
_split_find_term_at_coords(const Term_Container *tc,
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
_split_close(Term_Container *tc, Term_Container *child)
{
   Split *split;
   Term_Container *parent, *other_child;
   Evas_Object *top, *bottom;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   DBG("close");

   top = elm_object_part_content_unset(split->panes, PANES_TOP);
   bottom = elm_object_part_content_unset(split->panes, PANES_BOTTOM);
   evas_object_hide(top);
   evas_object_hide(bottom);

   parent = tc->parent;
   other_child = (child == split->tc1) ? split->tc2 : split->tc1;
   parent->swallow(parent, tc, other_child);

   if (tc->is_focused)
     {
        other_child->focus(other_child, parent);
     }

   evas_object_del(split->panes);

   eina_stringshare_del(tc->title);
   free(tc);
}

static void
_split_update(Term_Container *tc)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   split->tc1->update(split->tc1);
   split->tc2->update(split->tc2);
}

static void
_split_focus(Term_Container *tc, Term_Container *relative)
{
   Split *split;
   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   DBG("tc:%p tc->is_focused:%d from_parent:%d",
       tc, tc->is_focused, tc->parent == relative);

   if (!tc->parent)
     return;

   if (tc->parent == relative)
     {
        tc->is_focused = EINA_TRUE;
        split->last_focus->focus(split->last_focus, tc);
     }
   else
     {
        if (split->last_focus != relative)
          split->last_focus->unfocus(split->last_focus, tc);
        split->last_focus = relative;
        if (!tc->is_focused)
          tc->parent->focus(tc->parent, tc);
        tc->is_focused = EINA_TRUE;
     }
}

static void
_split_unfocus(Term_Container *tc, Term_Container *relative)
{
   Split *split;
   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   DBG("tc:%p tc->is_focused:%d from_parent:%d",
       tc, tc->is_focused, tc->parent == relative);
   if (!tc->is_focused)
     return;

   tc->is_focused = EINA_FALSE;
   if (tc->parent == relative)
     split->last_focus->unfocus(split->last_focus, tc);
   else
     tc->parent->unfocus(tc->parent, tc);
}

static void
_split_set_title(Term_Container *tc, Term_Container *child,
                 const char *title)
{
   Split *split;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   if (child == split->last_focus)
     {
        eina_stringshare_del(tc->title);
        tc->title =  eina_stringshare_ref(title);
        tc->parent->set_title(tc->parent, tc, title);
     }
}

static void
_split_bell(Term_Container *tc,
            Term_Container *_child EINA_UNUSED)
{
   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);

   if (tc->is_focused)
     return;

   tc->parent->bell(tc->parent, tc);
}

static void
_split_split(Term_Container *tc, Term_Container *child,
             Term *from,
             const char *cmd, Eina_Bool is_horizontal)
{
   Split *split;
   Win *wn;
   Term *tm_new, *tm;
   char *wdir = NULL;
   char buf[PATH_MAX];
   Term_Container *tc_split, *tc_solo_new;
   Evas_Object *obj_split;

   DBG(" ");
   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split *)tc;
   wn = tc->wn;

   if (!_term_container_is_splittable(tc, is_horizontal))
     return;

   // copy the current path to wdir if we should change the directory,
   // passing wdir NULL otherwise:
   if (wn->config->changedir_to_current)
     {
        if (from)
          tm = from;
        else
          tm = child->focused_term_get(child);
        if (tm && termio_cwd_get(tm->termio, buf, sizeof(buf)))
          wdir = buf;
     }
   tm_new = term_new(wn, wn->config,
                     cmd, wn->config->login_shell, wdir,
                     80, 24, EINA_FALSE, NULL);
   tc_solo_new = _solo_new(tm_new, wn);
   evas_object_data_set(tm_new->termio, "sizedone", tm_new->termio);

   if (child == split->tc1)
     elm_object_part_content_unset(split->panes, PANES_TOP);
   else
     elm_object_part_content_unset(split->panes, PANES_BOTTOM);

   tc_split = _split_new(child, tc_solo_new, is_horizontal);

   obj_split = tc_split->get_evas_object(tc_split);

   tc_split->is_focused = tc->is_focused;
   tc->swallow(tc, child, tc_split);

   evas_object_show(obj_split);
}

static Eina_Bool
_split_is_visible(Term_Container *tc, Term_Container *_child EINA_UNUSED)
{
   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   /* Could return True with the current design because splits are at a higher
    * level than tabs */
   return tc->parent->is_visible(tc->parent, tc);
}

static Term_Container *
_split_new(Term_Container *tc1, Term_Container *tc2,
           Eina_Bool is_horizontal)
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
   tc->term_up = _split_term_up;
   tc->term_down = _split_term_down;
   tc->term_left = _split_term_left;
   tc->term_right = _split_term_right;
   tc->term_first = _split_term_first;
   tc->term_last = _split_term_last;
   tc->focused_term_get = _split_focused_term_get;
   tc->get_evas_object = _split_get_evas_object;
   tc->split = _split_split;
   tc->find_term_at_coords = _split_find_term_at_coords;
   tc->size_eval = _split_size_eval;
   tc->swallow = _split_swallow;
   tc->focus = _split_focus;
   tc->unfocus = _split_unfocus;
   tc->set_title = _split_set_title;
   tc->bell = _split_bell;
   tc->close = _split_close;
   tc->update = _split_update;
   tc->is_visible = _split_is_visible;
   tc->title = eina_stringshare_add("Terminology");
   tc->type = TERM_CONTAINER_TYPE_SPLIT;


   tc->parent = NULL;
   tc->wn = tc1->wn;

   tc1->parent = tc2->parent = tc;

   split->tc1 = tc1;
   split->tc2 = tc2;
   if (tc1->is_focused)
     {
        tc1->unfocus(tc1, tc);
        tc2->focus(tc2, tc);
        split->last_focus = tc2;
     }
   else
     split->last_focus = tc1;

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
              Evas *_e EINA_UNUSED,
              Evas_Object *obj,
              void *_event EINA_UNUSED)
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
split_horizontally(Evas_Object *_win EINA_UNUSED,
                   Evas_Object *term,
                   const char *cmd)
{
   Term *tm;
   Term_Container *tc;

   tm = evas_object_data_get(term, "term");
   if (!tm) return;

   tc = tm->container;
   tc->split(tc, tc, tm, cmd, EINA_TRUE);
}

void
split_vertically(Evas_Object *_win EINA_UNUSED,
                 Evas_Object *term,
                 const char *cmd)
{
   Term *tm;
   Term_Container *tc;

   tm = evas_object_data_get(term, "term");
   if (!tm) return;

   tc = tm->container;
   tc->split(tc, tc, tm, cmd, EINA_FALSE);
}

/* }}} */
/* {{{ Tabs */

static void
_tabbar_clear(Term *tm)
{
   Evas_Object *o;

   if (tm->tabbar.l.box)
     {
        EINA_LIST_FREE(tm->tabbar.l.tabs, o) evas_object_del(o);
        evas_object_del(tm->tabbar.l.box);
        tm->tabbar.l.box = NULL;
     }
   if (tm->tabbar.r.box)
     {
        EINA_LIST_FREE(tm->tabbar.r.tabs, o) evas_object_del(o);
        evas_object_del(tm->tabbar.r.box);
        tm->tabbar.r.box = NULL;
     }
   if (tm->tab_spacer)
     {
        edje_object_signal_emit(tm->bg, "tabbar,off", "terminology");
        edje_object_message_signal_process(tm->bg);
        edje_object_part_unswallow(tm->bg, tm->tab_spacer);
        evas_object_del(tm->tab_spacer);
        tm->tab_spacer = NULL;
     }
}

static void
_cb_tab_activate(void *data,
                 Evas_Object *_obj EINA_UNUSED,
                 const char *_sig EINA_UNUSED,
                 const char *_src EINA_UNUSED)
{
   Tab_Item *tab_item = data;
   Solo *solo;
   Term *term;

   assert (tab_item->tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)tab_item->tc;
   term = solo->term;
   term_focus(term);
}

static void
_cb_tab_close(void *data,
              Evas_Object *_obj EINA_UNUSED,
              const char *_sig EINA_UNUSED,
              const char *_src EINA_UNUSED)
{
   Term *term = data;
   Win *wn = term->wn;
   Evas_Object *win = win_evas_object_get(wn);

   term_close(win, term->termio, EINA_FALSE);
}

static void
_cb_tab_title(void *data,
              Evas_Object *_obj EINA_UNUSED,
              const char *_sig EINA_UNUSED,
              const char *_src EINA_UNUSED)
{
   Term *term = data;

   term_set_title(term);
}

static void
_tabbar_fill(Tabs *tabs)
{
   Eina_List *l;
   Term *term;
   Solo *solo;
   Evas_Object *o;
   int n = eina_list_count(tabs->tabs);
   int i = 0, j = 0;
   Tab_Item *tab_item;

   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        if (tab_item == tabs->current) break;
        i++;
     }

   tab_item = tabs->current;
   assert (tab_item->tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)tab_item->tc;
   term = solo->term;

   assert(term->tabbar.l.box == NULL);
   assert(term->tabbar.r.box == NULL);
   assert(term->tab_spacer != NULL);

   if (i > 0)
     {
        term->tabbar.l.box = o = elm_box_add(tabs->tc.wn->win);
        elm_box_horizontal_set(o, EINA_TRUE);
        elm_box_homogeneous_set(o, EINA_TRUE);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        edje_object_part_swallow(term->bg, "terminology.tabl.content", o);
        evas_object_show(o);
     }
   if (i < (n - 1))
     {
        term->tabbar.r.box = o = elm_box_add(tabs->tc.wn->win);
        elm_box_horizontal_set(o, EINA_TRUE);
        elm_box_homogeneous_set(o, EINA_TRUE);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        edje_object_part_swallow(term->bg, "terminology.tabr.content", o);
        evas_object_show(o);
     }
   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        if (tab_item != tabs->current)
          {
             Evas_Coord w, h;

             solo = (Solo*)tab_item->tc;
             _tabbar_clear(solo->term);

             o = edje_object_add(evas_object_evas_get(tabs->tc.wn->win));
             theme_apply(o, term->config, "terminology/tabbar_back");
             evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
             edje_object_part_text_set(o, "terminology.title",
                                       tab_item->tc->title);
             edje_object_size_min_calc(o, &w, &h);
             evas_object_size_hint_min_set(o, w, h);
             assert(i != j);
             if (j < i)
               {
                  term->tabbar.l.tabs = eina_list_append(term->tabbar.l.tabs, o);
                  elm_box_pack_end(term->tabbar.l.box, o);
               }
             else if (j > i)
               {
                  term->tabbar.r.tabs = eina_list_append(term->tabbar.r.tabs, o);
                  elm_box_pack_end(term->tabbar.r.box, o);
               }
             evas_object_data_set(o, "term", term);
             evas_object_show(o);
             edje_object_signal_callback_add(o, "tab,activate", "terminology",
                                             _cb_tab_activate, tab_item);
             edje_object_signal_callback_add(o, "tab,close", "terminology",
                                             _cb_tab_close, term);
             edje_object_signal_callback_add(o, "tab,title", "terminology",
                                             _cb_tab_title, term);
          }
        j++;
     }
}

Eina_Bool
term_tab_go(Term *term, int tnum)
{
   Term_Container *tc = term->container,
                  *child = tc;

   while (tc)
     {
        Tabs *tabs;
        Tab_Item *tab_item;

        if (tc->type != TERM_CONTAINER_TYPE_TABS)
          {
             child = tc;
             tc = tc->parent;
             continue;
          }
        tabs = (Tabs*) tc;
        tab_item = eina_list_nth(tabs->tabs, tnum);
        if (!tab_item)
          {
             child = tc;
             tc = tc->parent;
             continue;
          }
        if (tab_item != tabs->current)
           tab_item->tc->focus(tab_item->tc, child);
        return EINA_TRUE;
     }
   return EINA_FALSE;
}

static void
_tabs_selector_cb_selected(void *data,
                           Evas_Object *_obj EINA_UNUSED,
                           void *info);
static void
_tabs_selector_cb_exit(void *data,
                       Evas_Object *_obj EINA_UNUSED,
                       void *_info EINA_UNUSED);

static void
_tabs_selector_cb_ending(void *data,
                         Evas_Object *_obj EINA_UNUSED,
                         void *_info EINA_UNUSED);

static void
_tabs_restore(Tabs *tabs)
{
   Eina_List *l;
   Tab_Item *tab_item;
   Term_Container *tc = (Term_Container*)tabs;
   Term *term;
   Solo *solo;

   if (!tabs->selector)
     return;

   EINA_LIST_FOREACH(tc->wn->terms, l, term)
     {
        if (term->unswallowed)
          {
             evas_object_image_source_visible_set(term->sel, EINA_TRUE);
             edje_object_part_swallow(term->bg, "terminology.content", term->base);
             term->unswallowed = EINA_FALSE;
             evas_object_show(term->base);
          }
     }

   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        tab_item->selector_entry = NULL;
        if (tab_item->tc->is_focused)
          tab_item->tc->unfocus(tab_item->tc, tc);
     }

   evas_object_smart_callback_del_full(tabs->selector, "selected",
                                  _tabs_selector_cb_selected, tabs);
   evas_object_smart_callback_del_full(tabs->selector, "exit",
                                  _tabs_selector_cb_exit, tabs);
   evas_object_smart_callback_del_full(tabs->selector, "ending",
                                  _tabs_selector_cb_ending, tabs);
   evas_object_del(tabs->selector);
   evas_object_del(tabs->selector_bg);
   tabs->selector = NULL;
   tabs->selector_bg = NULL;

   /* XXX: reswallow in parent */
   tc->parent->swallow(tc->parent, tc, tc);
   solo = (Solo*)tabs->current->tc;
   term = solo->term;
   _tabbar_clear(term);

   _tabs_refresh(tabs);
   tabs->current->tc->unfocus(tabs->current->tc, tabs->current->tc);
   tabs->current->tc->focus(tabs->current->tc, tabs->current->tc);
}

static void
_tabs_selector_cb_ending(void *data,
                         Evas_Object *_obj EINA_UNUSED,
                         void *_info EINA_UNUSED)
{
   Tabs *tabs = data;

   edje_object_signal_emit(tabs->selector_bg, "end", "terminology");
}

static void
_tabs_selector_cb_selected(void *data,
                           Evas_Object *_obj EINA_UNUSED,
                           void *info)
{
   Tabs *tabs = data;
   Eina_List *l;
   Tab_Item *tab_item;

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
                       Evas_Object *_obj EINA_UNUSED,
                       void *_info EINA_UNUSED)
{
   Tabs *tabs = data;

   _tabs_restore(tabs);
}

static void
_cb_tab_selector_show(Tabs *tabs, Tab_Item *to_item)
{
   Term_Container *tc = (Term_Container *)tabs;
   Eina_List *l;
   int count;
   double z;
   Win *wn = tc->wn;
   Tab_Item *tab_item;
   Evas_Object *o;
   Evas_Coord x, y, w, h;

   if (tabs->selector_bg)
     return;

   o = tc->get_evas_object(tc);
   evas_object_geometry_get(o, &x, &y, &w, &h);

   tabs->selector_bg = edje_object_add(evas_object_evas_get(tc->wn->win));
   theme_apply(tabs->selector_bg, wn->config, "terminology/sel/base");

   evas_object_geometry_set(tabs->selector_bg, x, y, w, h);
   evas_object_hide(o);

   _set_trans(wn->config, tabs->selector_bg, NULL);
   background_set_shine(wn->config, tabs->selector_bg);
   edje_object_signal_emit(tabs->selector_bg, "begin", "terminology");

   tabs->selector = sel_add(wn->win);
   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        Evas_Object *img;
        Eina_Bool is_selected, missed_bell;
        Solo *solo;
        Term *term;

        solo = (Solo*)tab_item->tc;
        term = solo->term;
        _tabbar_clear(term);

        edje_object_part_unswallow(term->bg, term->base);
        term->unswallowed = EINA_TRUE;
        img = evas_object_image_filled_add(evas_object_evas_get(wn->win));
        o = term->base;
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
        missed_bell = term->missed_bell;
        tab_item->selector_entry = NULL;
        tab_item->selector_entry = sel_entry_add(tabs->selector, img,
                                                 is_selected,
                                                 missed_bell, wn->config);
     }
   edje_object_part_swallow(tabs->selector_bg, "terminology.content",
                            tabs->selector);

   evas_object_show(tabs->selector);

   /* XXX: refresh */
   tc->parent->swallow(tc->parent, tc, tc);

   evas_object_show(tabs->selector_bg);
   evas_object_smart_callback_add(tabs->selector, "selected",
                                  _tabs_selector_cb_selected, tabs);
   evas_object_smart_callback_add(tabs->selector, "exit",
                                  _tabs_selector_cb_exit, tabs);
   evas_object_smart_callback_add(tabs->selector, "ending",
                                  _tabs_selector_cb_ending, tabs);
   z = 1.0;
   sel_go(tabs->selector);
   count = eina_list_count(tabs->tabs);
   if (count >= 1)
     z = 1.0 / (sqrt(count) * 0.8);
   if (z > 1.0) z = 1.0;
   sel_orig_zoom_set(tabs->selector, z);
   sel_zoom(tabs->selector, z);
   if (to_item)
     {
        sel_entry_selected_set(tabs->selector, to_item->tc->selector_img,
                               EINA_TRUE);
        sel_exit(tabs->selector);
     }
   elm_object_focus_set(tabs->selector, EINA_TRUE);
}

static void
_cb_select(void *data,
           Evas_Object *_obj EINA_UNUSED,
           void *_event EINA_UNUSED)
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

        _cb_tab_selector_show(tabs, NULL);
        return;
     }
}



static Evas_Object *
_tabs_get_evas_object(const Term_Container *container)
{
   Tabs *tabs;
   Term_Container *tc;

   assert (container->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)container;

   if (tabs->selector_bg)
     return tabs->selector_bg;

   assert(tabs->current != NULL);
   tc = tabs->current->tc;
   return tc->get_evas_object(tc);
}

static Term *
_tabs_focused_term_get(const Term_Container *tc)
{
   Tabs *tabs;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)tc;


   return tc->is_focused ?
      tabs->current->tc->focused_term_get(tabs->current->tc)
      : NULL;
}

static Term *
_tabs_find_term_at_coords(const Term_Container *container,
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
   Tabs *tabs;
   Term_Container *tc;
   Config *config;

   assert (container->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)container;

   tc = tabs->current->tc;
   config = tc->wn->config;
   tc->size_eval(tc, info);
   /* Current sizing code does not take the tab area correctly into account */
   if (!config->notabs)
     {
        info->step_x = 1;
        info->step_y = 1;
     }
}

static Eina_List *
_tab_item_find(const Tabs *tabs, const Term_Container *child)
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
_tabs_close(Term_Container *tc, Term_Container *child)
{
   int count;
   Tabs *tabs;
   Eina_List *l;
   Tab_Item *item, *next_item;
   Eina_List *next;
   Term_Container *next_child, *tc_parent;
   Term *term;
   Solo *solo;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)tc;

   tc_parent = tc->parent;

   l = _tab_item_find(tabs, child);
   item = l->data;

   next = eina_list_next(l);
   if (!next)
     next = tabs->tabs;
   next_item = next->data;
   next_child = next_item->tc;
   assert (next_child->type == TERM_CONTAINER_TYPE_SOLO);
   tabs->tabs = eina_list_remove_list(tabs->tabs, l);

   assert (child->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)child;
   term = solo->term;
   child->unfocus(child, tc);
   _tabbar_clear(term);

   edje_object_signal_emit(term->bg, "tabcount,off", "terminology");

   count = eina_list_count(tabs->tabs);
   if (count == 1)
     {
        Term *next_term;
        Solo *next_solo;

        assert (next_child->type == TERM_CONTAINER_TYPE_SOLO);
        next_solo = (Solo*)next_child;
        next_term = next_solo->term;

        edje_object_signal_emit(next_term->bg, "tabcount,off", "terminology");
        if (next_term->tabcount_spacer)
          {
             evas_object_del(next_term->tabcount_spacer);
             next_term->tabcount_spacer = NULL;
          }

        if (tabs->selector)
          _tabs_restore(tabs);
        eina_stringshare_del(tc->title);

        tc_parent->swallow(tc_parent, tc, next_child);
        if (tc->is_focused)
          next_child->focus(next_child, tc);

        free(next_item);
        free(tc);
        free(item);
     }
   else
     {
        if (item == tabs->current)
          {
             tabs->current = next_item;
             /* XXX: refresh */
             tc_parent->swallow(tc_parent, tc, tc);
             tc->swallow(tc, child, next_child);
          }
        else
          {
             next_item = tabs->current;
             next_child = next_item->tc;
             if (tc->is_focused)
               next_child->focus(next_child, tc);
          }

        if (item->tc->selector_img)
          {
             Evas_Object *o;
             o = item->tc->selector_img;
             item->tc->selector_img = NULL;
             evas_object_del(o);
          }

        free(item);
        count--;
        _tabs_refresh(tabs);
        if (tc->is_focused)
          {
             next_child->focus(next_child, tc);
          }
     }
}

static void
_tabs_update(Term_Container *tc)
{
   Tabs *tabs;
   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)tc;

   _tabs_refresh(tabs);
}

static Term *
_tabs_term_next(const Term_Container *tc, const Term_Container *child)
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
_tabs_term_prev(const Term_Container *tc, const Term_Container *child)
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
_tabs_term_up(const Term_Container *tc,
              const Term_Container *_child EINA_UNUSED)
{
   return tc->parent->term_up(tc->parent, tc);
}

static Term *
_tabs_term_down(const Term_Container *tc,
                const Term_Container *_child EINA_UNUSED)
{
   return tc->parent->term_down(tc->parent, tc);
}

static Term *
_tabs_term_left(const Term_Container *tc,
                const Term_Container *_child EINA_UNUSED)
{
   return tc->parent->term_left(tc->parent, tc);
}

static Term *
_tabs_term_right(const Term_Container *tc,
                 const Term_Container *_child EINA_UNUSED)
{
   return tc->parent->term_right(tc->parent, tc);
}

static Term *
_tabs_term_first(const Term_Container *tc)
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
_tabs_term_last(const Term_Container *tc)
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
   Evas_Object *o;
   Evas_Coord x, y, w, h;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*) tc;

   l = _tab_item_find(tabs, new_child);
   tab_item = l->data;

   if (tabs->selector)
     {
        Evas_Object *img = tab_item->tc->selector_img;
        evas_object_image_source_set(img,
                                     new_child->get_evas_object(new_child));
        evas_object_data_set(img, "tc", new_child);
        if (tab_item->selector_entry)
          sel_entry_update(tab_item->selector_entry);
     }
   else if (tab_item != tabs->current)
     {
        Term *term;
        Solo *solo;
        Term_Container *tc_parent = tc->parent;
        if (tc->is_focused)
          tabs->current->tc->unfocus(tabs->current->tc, tc);
        tabs->current = tab_item;

        assert (orig->type == TERM_CONTAINER_TYPE_SOLO);
        solo = (Solo*)orig;
        term = solo->term;
        edje_object_signal_emit(term->bg, "tabcount,off", "terminology");
        if (term->tabcount_spacer)
          {
             evas_object_del(term->tabcount_spacer);
             term->tabcount_spacer = NULL;
          }

        o = orig->get_evas_object(orig);
        evas_object_geometry_get(o, &x, &y, &w, &h);
        evas_object_hide(o);

        o = new_child->get_evas_object(new_child);
        evas_object_geometry_set(o, x, y, w, h);
        evas_object_show(o);

        /* XXX: need to refresh */
        tc_parent->swallow(tc_parent, tc, tc);
        /* TODO: redo title */
     }

   _tabs_refresh(tabs);
}


static void
_tab_new_cb(void *data,
            Evas_Object *_obj EINA_UNUSED,
            void *_event_info EINA_UNUSED)
{
   Tabs *tabs = data;
   Tab_Item *tab_item;
   Evas_Object *o;
   Evas_Coord x, y, w, h;
   Term_Container *tc = (Term_Container*) tabs,
                  *tc_new, *tc_parent, *tc_old;
   Term *tm_new;
   Win *wn = tc->wn;
   char *wdir = NULL;
   char buf[PATH_MAX];

   // copy the current path to wdir if we should change the directory,
   // passing wdir NULL otherwise:
   if (wn->config->changedir_to_current)
     {
        Term *tm;

        tc_old = tabs->current->tc;
        tm = tc_old->term_first(tc_old);

        if (tm && termio_cwd_get(tm->termio, buf, sizeof(buf)))
          wdir = buf;
     }

   tm_new = term_new(wn, wn->config,
                     NULL, wn->config->login_shell, wdir,
                     80, 24, EINA_FALSE, NULL);
   tc_new = _solo_new(tm_new, wn);
   evas_object_data_set(tm_new->termio, "sizedone", tm_new->termio);

   tc_new->parent = tc;

   tab_item = tab_item_new(tabs, tc_new);
   tc_parent = tc->parent;
   tc_old = tabs->current->tc;
   tc_old->unfocus(tc_old, tc);
   o = tc_old->get_evas_object(tc_old);
   evas_object_geometry_get(o, &x, &y, &w, &h);
   evas_object_hide(o);
   o = tc_new->get_evas_object(tc_new);
   evas_object_geometry_set(o, x, y, w, h);
   evas_object_show(o);
   tabs->current = tab_item;
   /* XXX: need to refresh */
   tc_parent->swallow(tc_parent, tc, tc);

   if (tc->is_focused)
     tc_new->focus(tc_new, tc);

   _tabs_refresh(tabs);
}

static void
_cb_new(void *data,
        Evas_Object *_obj EINA_UNUSED,
        void *_event EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc = term->container;

   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);

   _solo_tabs_new(tc);
}

static void
_cb_close(void *data,
          Evas_Object *_obj EINA_UNUSED,
          void *_event EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc = term->container;

   term_close(tc->wn->win, term->termio, EINA_FALSE);
}

void
main_new(Evas_Object *_win EINA_UNUSED,
         Evas_Object *term)
{
   Term *tm;

   tm = evas_object_data_get(term, "term");
   if (!tm) return;

   _cb_new(tm, term, NULL);
}

static void
_tabs_focus(Term_Container *tc, Term_Container *relative)
{
   Tabs *tabs;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*) tc;

   if (!tc->parent)
     return;

   DBG("tc:%p tc->is_focused:%d from_parent:%d",
       tc, tc->is_focused, tc->parent == relative);
   if (tc->parent == relative)
     {
        if (!tc->is_focused)
          {
             tc->is_focused = EINA_TRUE;
             tabs->current->tc->focus(tabs->current->tc, tc);
          }
     }
   else
     {
        Eina_List *l;
        Tab_Item *tab_item;

        l = _tab_item_find(tabs, relative);
        if (!l)
          return;

        tc->is_focused = EINA_TRUE;

        tab_item = l->data;
        if (tab_item != tabs->current)
          {
             Config *config = tc->wn->config;
             tabs->current->tc->unfocus(tabs->current->tc, tc);

             if (config->tab_zoom >= 0.01 && config->notabs)
               {
                  _cb_tab_selector_show(tabs, tab_item);
                  return;
               }

             tc->swallow(tc, tabs->current->tc, relative);
          }
        tc->parent->focus(tc->parent, tc);
     }
}

static void
_tabs_unfocus(Term_Container *tc, Term_Container *relative)
{
   Tabs *tabs;

   DBG("tc:%p tc->is_focused:%d from_parent:%d",
       tc, tc->is_focused, tc->parent == relative);
   if (!tc->is_focused)
     return;
   if (!tc->parent)
     return;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*) tc;

   if (tc->parent == relative)
     {
        tabs->current->tc->unfocus(tabs->current->tc, tc);
        tc->is_focused = EINA_FALSE;
     }
   else
     {
        Tab_Item *tab_item;
        Eina_List *l;

        EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
          {
             if (relative == tab_item->tc) {
                  tc->parent->unfocus(tc->parent, tc);
                  tc->is_focused = EINA_FALSE;
                  return;
             }
          }
     }
}

static void
_tabs_bell(Term_Container *tc,
           Term_Container *_child EINA_UNUSED)
{
   Tabs *tabs;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*) tc;

   _tabs_refresh(tabs);

   if (tc->is_focused)
     return;

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
   if (!l)
     return;
   tab_item = l->data;

   if (tabs->selector && tab_item->selector_entry)
     {
        sel_entry_title_set(tab_item->selector_entry, title);
     }

   if (tab_item == tabs->current)
     {
        Solo *solo;
        Term *term;

        eina_stringshare_del(tc->title);
        tc->title =  eina_stringshare_ref(title);
        tc->parent->set_title(tc->parent, tc, title);

        assert (tab_item->tc->type == TERM_CONTAINER_TYPE_SOLO);
        solo = (Solo*)tab_item->tc;
        term = solo->term;

        if (!term->config->notabs)
          {
             edje_object_part_text_set(term->bg, "terminology.tab.title",
                                       title);
          }
     }
   else
     {
        _tabs_refresh(tabs);
     }
}

static void
_tabs_refresh(Tabs *tabs)
{
   Eina_List *l;
   Solo *solo;
   Term *term;
   char buf[32], bufmissed[32];
   Tab_Item *tab_item;
   Evas_Coord w = 0, h = 0;
   int missed = 0;
   int i = 1;
   int n = eina_list_count(tabs->tabs);

   if (n <= 0)
     return;

   buf[0] = '\0';
   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        assert (tab_item->tc->type == TERM_CONTAINER_TYPE_SOLO);
        solo = (Solo*) tab_item->tc;
        term = solo->term;

        if (term->missed_bell)
          missed++;
     }
   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        if (tabs->current == tab_item)
          {
             snprintf(buf, sizeof(buf), "%i/%i", i, n);
             break;
          }
        i++;
     }
   if (missed > 0) snprintf(bufmissed, sizeof(bufmissed), "%i", missed);
   else bufmissed[0] = '\0';

   tab_item = tabs->current;
   assert (tab_item->tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)tab_item->tc;
   term = solo->term;

   _tabbar_clear(term);

   if (!term->tabcount_spacer)
     {
        term->tabcount_spacer = evas_object_rectangle_add(evas_object_evas_get(term->bg));
        evas_object_color_set(term->tabcount_spacer, 0, 0, 0, 0);
     }
   elm_coords_finger_size_adjust(1, &w, 1, &h);
   evas_object_size_hint_min_set(term->tabcount_spacer, w, h);
   edje_object_part_swallow(term->bg, "terminology.tabcount.control",
                            term->tabcount_spacer);
   edje_object_part_text_set(term->bg, "terminology.tabcount.label", buf);
   edje_object_part_text_set(term->bg, "terminology.tabmissed.label", bufmissed);
   edje_object_signal_emit(term->bg, "tabcount,on", "terminology");
   // this is all below just for tab bar at the top
   if (!term->config->notabs)
     {
        double v1, v2;

        v1 = (double)(i-1) / (double)n;
        v2 = (double)i / (double)n;
        if (!term->tab_spacer)
          {
             term->tab_spacer = evas_object_rectangle_add(
                evas_object_evas_get(term->bg));
             evas_object_color_set(term->tab_spacer, 0, 0, 0, 0);
             elm_coords_finger_size_adjust(1, &w, 1, &h);
             evas_object_size_hint_min_set(term->tab_spacer, w, h);
             edje_object_part_swallow(term->bg, "terminology.tab", term->tab_spacer);
             edje_object_part_drag_value_set(term->bg, "terminology.tabl", v1, 0.0);
             edje_object_part_drag_value_set(term->bg, "terminology.tabr", v2, 0.0);
             edje_object_part_text_set(term->bg, "terminology.tab.title",
                                       solo->tc.title);
             edje_object_signal_emit(term->bg, "tabbar,on", "terminology");
             edje_object_message_signal_process(term->bg);
          }
        else
          {
             edje_object_part_drag_value_set(term->bg, "terminology.tabl", v1, 0.0);
             edje_object_part_drag_value_set(term->bg, "terminology.tabr", v2, 0.0);
             edje_object_message_signal_process(term->bg);
          }
        _tabbar_fill(tabs);
     }
   else
     {
        _tabbar_clear(term);
     }
   if (missed > 0)
     edje_object_signal_emit(term->bg, "tabmissed,on", "terminology");
   else
     edje_object_signal_emit(term->bg, "tabmissed,off", "terminology");
}

static Tab_Item*
tab_item_new(Tabs *tabs, Term_Container *child)
{
   Tab_Item *tab_item;

   tab_item = calloc(1, sizeof(Tab_Item));
   if (!tab_item) return NULL;
   tab_item->tc = child;
   assert(child != NULL);
   assert(child->type == TERM_CONTAINER_TYPE_SOLO);

   tabs->tabs = eina_list_append(tabs->tabs, tab_item);

   return tab_item;
}

static void
_tabs_split(Term_Container *tc,
            Term_Container *_child EINA_UNUSED,
            Term *from,
            const char *cmd,
            Eina_Bool is_horizontal)
{
   tc->parent->split(tc->parent, tc, from, cmd, is_horizontal);
}

static Eina_Bool
_tabs_is_visible(Term_Container *tc, Term_Container *child)
{
   Tabs *tabs;
   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*) tc;
   return child == tabs->current->tc;
}


static Term_Container *
_tabs_new(Term_Container *child, Term_Container *parent)
{
   Win *wn;
   Term_Container *tc;
   Tabs *tabs;
   Tab_Item *tab_item;

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
   tc->term_up = _tabs_term_up;
   tc->term_down = _tabs_term_down;
   tc->term_left = _tabs_term_left;
   tc->term_right = _tabs_term_right;
   tc->term_first = _tabs_term_first;
   tc->term_last = _tabs_term_last;
   tc->focused_term_get = _tabs_focused_term_get;
   tc->get_evas_object = _tabs_get_evas_object;
   tc->split = _tabs_split;
   tc->find_term_at_coords = _tabs_find_term_at_coords;
   tc->size_eval = _tabs_size_eval;
   tc->swallow = _tabs_swallow;
   tc->focus = _tabs_focus;
   tc->unfocus = _tabs_unfocus;
   tc->set_title = _tabs_set_title;
   tc->bell = _tabs_bell;
   tc->close = _tabs_close;
   tc->update = _tabs_update;
   tc->is_visible = _tabs_is_visible;
   tc->title = eina_stringshare_add("Terminology");
   tc->type = TERM_CONTAINER_TYPE_TABS;

   tc->parent = parent;
   tc->wn = wn;

   child->parent = tc;
   tab_item = tab_item_new(tabs, child);
   tabs->current = tab_item;

   /* XXX: need to refresh */
   parent->swallow(parent, child, tc);

   tc->is_focused = child->is_focused;

   return tc;
}


/* }}} */
/* {{{ Term */

Eina_Bool
term_is_visible(Term *term)
{
   /* TODO: boris */
   Term_Container *tc;

   if (!term)
     return EINA_FALSE;

   tc = term->container;
   if (!tc)
     return EINA_FALSE;

   return tc->is_visible(tc, tc);
}

void
background_set_shine(Config *config, Evas_Object *bg)
{
   Edje_Message_Int msg;

   if (config)
     msg.val = config->shine;
   else
     msg.val = 255;

   if (bg)
       edje_object_message_send(bg, EDJE_MESSAGE_INT, 2, &msg);
}

void
term_apply_shine(Term *term, int shine)
{
   Config *config = term->config;

   if (config->shine != shine)
     {
        config->shine = shine;
        background_set_shine(config, term->bg);
        config_save(config, NULL);
     }
}


static void
_set_trans(Config *config, Evas_Object *bg, Evas_Object *base)
{
   Edje_Message_Int msg;

   if (config && config->translucent)
     msg.val = config->opacity;
   else
     msg.val = 100;

   if (bg)
       edje_object_message_send(bg, EDJE_MESSAGE_INT, 1, &msg);
   if (base)
       edje_object_message_send(base, EDJE_MESSAGE_INT, 1, &msg);
}

static void
_term_config_set(Term *term, Config *config)
{
   Config *old_config = term->config;

   term->config = config;
   termio_config_set(term->termio, config);
   _term_media_update(term, term->config);
   if (old_config != term->wn->config)
     config_del(old_config);
}

Eina_Bool
term_has_popmedia(const Term *term)
{
   return !!term->popmedia;
}

void
term_popmedia_close(Term *term)
{
   if (term->popmedia)
     edje_object_signal_emit(term->bg, "popmedia,off", "terminology");
}


static Eina_Bool
_term_is_focused(Term *term)
{
   Term_Container *tc;

   if (!term)
     return EINA_FALSE;

   tc = term->container;
   if (!tc)
     return EINA_FALSE;

   DBG("tc:%p tc->is_focused:%d", tc, tc->is_focused);
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
        Evas_Object *edje = term->bg;

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

void
term_focus(Term *term)
{
   Term_Container *tc;

   DBG("is focused? tc:%p", term->container);
   if (_term_is_focused(term))
     return;

   tc = term->container;
   DBG("tc:%p", tc);
   tc->focus(tc, tc);
}

void
term_unfocus(Term *term)
{
   Term_Container *tc;

   DBG("is focused? tc:%p", term->container);
   if (!_term_is_focused(term))
     return;

   tc = term->container;
   DBG("tc:%p", tc);
   tc->unfocus(tc, tc);
}

enum term_to_direction {
     TERM_TO_PREV,
     TERM_TO_NEXT,
     TERM_TO_UP,
     TERM_TO_DOWN,
     TERM_TO_LEFT,
     TERM_TO_RIGHT,
};

static void
term_go_to(Term *from, enum term_to_direction dir)
{
   Term *new_term, *focused_term;
   Win *wn = from->wn;
   Term_Container *tc;

   tc = (Term_Container *) wn;

   focused_term = tc->focused_term_get(tc);
   if (!focused_term)
     focused_term = from;
   tc = focused_term->container;

   switch (dir)
     {
      case TERM_TO_PREV:
         new_term = tc->term_prev(tc, tc);
         break;
      case TERM_TO_NEXT:
         new_term = tc->term_next(tc, tc);
         break;
      case TERM_TO_UP:
         new_term = tc->term_up(tc, tc);
         break;
      case TERM_TO_DOWN:
         new_term = tc->term_down(tc, tc);
         break;
      case TERM_TO_LEFT:
         new_term = tc->term_left(tc, tc);
         break;
      case TERM_TO_RIGHT:
         new_term = tc->term_right(tc, tc);
         break;
     }

   if (new_term && new_term != focused_term)
     term_focus(new_term);

   /* TODO: get rid of it? */
   _term_miniview_check(from);
}

void
term_prev(Term *term)
{
   term_go_to(term, TERM_TO_PREV);
}

void
term_next(Term *term)
{
   term_go_to(term, TERM_TO_NEXT);
}

void
term_up(Term *term)
{
   term_go_to(term, TERM_TO_UP);
}

void
term_down(Term *term)
{
   term_go_to(term, TERM_TO_DOWN);
}

void
term_left(Term *term)
{
   term_go_to(term, TERM_TO_LEFT);
}

void
term_right(Term *term)
{
   term_go_to(term, TERM_TO_RIGHT);
}

Term *
term_prev_get(const Term *term)
{
   Term_Container *tc = term->container;

   return tc->term_prev(tc, tc);
}

Term *
term_next_get(const Term *term)
{
   Term_Container *tc = term->container;

   return tc->term_next(tc, tc);
}


static void
_cb_popmedia_del(void *data,
                 Evas *_e EINA_UNUSED,
                 Evas_Object *_o EINA_UNUSED,
                 void *_event_info EINA_UNUSED)
{
   Term *term = data;

   term->popmedia = NULL;
   term->popmedia_deleted = EINA_TRUE;
   edje_object_signal_emit(term->bg, "popmedia,off", "terminology");
}

static void
_cb_popmedia_done(void *data,
                  Evas_Object *_obj EINA_UNUSED,
                  const char *_sig EINA_UNUSED,
                  const char *_src EINA_UNUSED)
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
               Evas_Object *_obj EINA_UNUSED,
               void *_info EINA_UNUSED)
{
   Term *term = data;

   if (term->popmedia_queue)
     {
        if (term->popmedia) media_play_set(term->popmedia, EINA_FALSE);
        edje_object_signal_emit(term->bg, "popmedia,off", "terminology");
     }
}

static void
_popmedia_show(Term *term, const char *src, Media_Type type)
{
   Evas_Object *o;
   Config *config = termio_config_get(term->termio);

   EINA_SAFETY_ON_NULL_RETURN(config);
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

#ifdef HAVE_ECORE_CON_URL_HEAD
typedef struct _Ty_Http_Head {
     const char *handler;
     const char *src;
     Ecore_Con_Url *url;
     Ecore_Event_Handler *url_complete;
     Ecore_Timer *timeout;
     Term *term;
} Ty_Http_Head;

static void
_ty_http_head_delete(Ty_Http_Head *ty_head)
{
   eina_stringshare_del(ty_head->handler);
   eina_stringshare_del(ty_head->src);
   ecore_con_url_free(ty_head->url);
   ecore_event_handler_del(ty_head->url_complete);
   if (ty_head->term)
     {
        edje_object_signal_emit(ty_head->term->bg, "done", "terminology");
        term_unref(ty_head->term);
     }
   ecore_timer_del(ty_head->timeout);

   free(ty_head);
}


static Eina_Bool
_media_http_head_timeout(void *data)
{
   Ty_Http_Head *ty_head = data;
   media_unknown_handle(ty_head->handler, ty_head->src);
   ty_head->timeout = NULL;
   _ty_http_head_delete(ty_head);
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_media_http_head_complete(void *data,
                          int _kind EINA_UNUSED,
                          void *event_info)
{
   Ecore_Con_Event_Url_Complete *ev = event_info;
   Ty_Http_Head *ty_head = data;
   const Eina_List *headers, *l;
   Media_Type type = MEDIA_TYPE_UNKNOWN;
   char *str;

   if (ev->status != 200)
     goto error;
   headers = ecore_con_url_response_headers_get(ev->url_con);
   EINA_LIST_FOREACH(headers, l, str)
     {
#define  _CONTENT_TYPE_HDR "Content-Type: "
#define  _LOCATION_HDR "Location: "
        if (!strncmp(str, _LOCATION_HDR, strlen(_LOCATION_HDR)))
          {
             unsigned int len;

             str += strlen(_LOCATION_HDR);
             if (*str != '/')
               {
                  eina_stringshare_del(ty_head->src);

                  /* skip the crlf */
                  len = strlen(str);
                  if (len <= 2)
                    goto error;

                  ty_head->src = eina_stringshare_add_length(str, len - 2);
                  if (!ty_head->src)
                    goto error;
               }
          }
        else if (!strncmp(str, _CONTENT_TYPE_HDR, strlen(_CONTENT_TYPE_HDR)))
          {
             str += strlen(_CONTENT_TYPE_HDR);
             if (!strncmp(str, "image/", strlen("image/")))
               {
                  type = MEDIA_TYPE_IMG;
               }
             else if (!strncmp(str, "video/", strlen("video/")))
               {
                  type = MEDIA_TYPE_MOV;
               }
             if (type != MEDIA_TYPE_UNKNOWN)
               {
                  _popmedia_show(ty_head->term, ty_head->src, type);
                  break;
               }
          }
#undef _CONTENT_TYPE_HDR
#undef _LOCATION_HDR
     }
   if (type == MEDIA_TYPE_UNKNOWN)
     goto error;

   _ty_http_head_delete(ty_head);
   return EINA_TRUE;
error:
   media_unknown_handle(ty_head->handler, ty_head->src);
   _ty_http_head_delete(ty_head);
   return EINA_TRUE;
}
#endif

static void
_popmedia(Term *term, const char *src)
{
   Media_Type type;
   Config *config = termio_config_get(term->termio);

   type = media_src_type_get(src);
   if (type == MEDIA_TYPE_UNKNOWN)
     {
#ifdef HAVE_ECORE_CON_URL_HEAD
        Ty_Http_Head *ty_head = calloc(1, sizeof(Ty_Http_Head));
        if (!ty_head) return;

        if (config->helper.local.general && config->helper.local.general[0])
          {
             ty_head->handler = eina_stringshare_add(config->helper.local.general);
             if (!ty_head->handler) goto error;
          }
        ty_head->src = eina_stringshare_add(src);
        if (!ty_head->src) goto error;
        ty_head->url = ecore_con_url_new(src);
        if (!ty_head->url) goto error;
        if (!ecore_con_url_head(ty_head->url)) goto error;
        ty_head->url_complete = ecore_event_handler_add
          (ECORE_CON_EVENT_URL_COMPLETE, _media_http_head_complete, ty_head);
        ty_head->timeout = ecore_timer_add(2.5, _media_http_head_timeout, ty_head);
        if (!ty_head->url_complete) goto error;
        ty_head->term = term;
        edje_object_signal_emit(term->bg, "busy", "terminology");
        term_ref(term);
        return;

error:
        _ty_http_head_delete(ty_head);
#endif

        media_unknown_handle(config->helper.local.general, src);
     }
   else
     {
        _popmedia_show(term, src, type);
     }
}

static void
_term_miniview_check(Term *term)
{
   Eina_List *l, *wn_list;

   EINA_SAFETY_ON_NULL_RETURN(term);
   EINA_SAFETY_ON_NULL_RETURN(term->miniview);

   wn_list = win_terms_get(term_win_get(term));

   EINA_LIST_FOREACH(wn_list, l, term)
     {
        if (term->miniview_shown)
          {
             DBG("is focused? tc:%p", term->container);
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
_on_popover_done(Win *wn)
{
   Term_Container *tc = (Term_Container*) wn;
   Eina_List *l;
   Term *term;

   wn->on_popover--;
   if (wn->on_popover)
     return;

   if (!_win_is_focused(wn))
     return;
   EINA_LIST_FOREACH(wn->terms, l, term)
     {
        DBG("is focused? tc:%p", term->container);
        if (_term_is_focused(term))
          return;
     }
   DBG("focus tc:%p", tc);
   tc->focus(tc, tc);
}

static void
_set_title_ok_cb(void *data,
                 Evas_Object *_obj EINA_UNUSED,
                 void *_event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Term *term = evas_object_data_get(popup, "term");
   Evas_Object *entry = elm_object_content_get(popup);
   const char *title = elm_entry_entry_get(entry);

   if (!title || !strlen(title))
     title = NULL;

   termio_title_set(term->termio, title);
   elm_object_focus_set(entry, EINA_FALSE);
   elm_popup_dismiss(popup);
}

static void
_set_title_cancel_cb(void *data,
                     Evas_Object *_obj EINA_UNUSED,
                     void *_event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Evas_Object *entry = elm_object_content_get(popup);

   elm_object_focus_set(entry, EINA_FALSE);
   elm_popup_dismiss(popup);
}

static void
_cb_title_popup_hide(void *data,
                     Evas *_e EINA_UNUSED,
                     Evas_Object *_obj EINA_UNUSED,
                     void *_event EINA_UNUSED)
{
   Term *term = data;
   Win *wn = term->wn;

   _on_popover_done(wn);

   term_unref(term);
}

void
term_set_title(Term *term)
{
   Evas_Object *o;
   Evas_Object *popup;
   Term_Container *tc = term->container;

   EINA_SAFETY_ON_NULL_RETURN(term);
   term->wn->on_popover++;

   term_ref(term);
   tc->unfocus(tc, NULL);

   popup = elm_popup_add(term->wn->win);
   evas_object_data_set(popup, "term", term);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_HIDE,
                                  _cb_title_popup_hide, term);

   elm_object_part_text_set(popup, "title,text", _("Set title"));

   o = elm_button_add(popup);
   evas_object_smart_callback_add(o, "clicked", _set_title_ok_cb, popup);
   elm_object_text_set(o, _("Ok"));
   elm_object_part_content_set(popup, "button1", o);

   o = elm_button_add(popup);
   evas_object_smart_callback_add(o, "clicked", _set_title_cancel_cb, popup);
   elm_object_text_set(o, _("Cancel"));
   elm_object_part_content_set(popup, "button2", o);

   o = elm_entry_add(popup);
   elm_entry_single_line_set(o, EINA_TRUE);
   evas_object_smart_callback_add(o, "activated", _set_title_ok_cb, popup);
   evas_object_smart_callback_add(o, "aborted", _set_title_cancel_cb, popup);
   elm_object_content_set(popup, o);

   evas_object_show(o);

   evas_object_show(popup);

   elm_object_focus_set(o, EINA_TRUE);
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
   _popmedia(term, src);
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
_cb_popup(void *data,
          Evas_Object *_obj EINA_UNUSED,
          void *event)
{
   Term *term = data;
   const char *src = event;

   if (!src) src = termio_link_get(term->termio);
   if (!src) return;
   _popmedia(term, src);
}

static void
_cb_popup_queue(void *data,
                Evas_Object *_obj EINA_UNUSED,
                void *event)
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
_sendfile_progress_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *info EINA_UNUSED)
{
   Evas_Object *o = obj;
   Term *term = evas_object_data_get(o, "sendfile-progress-term");
   Ecore_Timer *t;

   evas_object_data_del(o, "sendfile-progress-term");
   if (term)
     {
        term->sendfile_progress = NULL;
        term->sendfile_progress_bar = NULL;
     }
   t = evas_object_data_get(o, "sendfile-progress-timer");
   evas_object_data_del(o, "sendfile-progress-term");
   if (t) ecore_timer_del(t);
}

static Eina_Bool
_sendfile_progress_reset(void *data)
{
   Evas_Object *o = data;
   Term *term = evas_object_data_get(o, "sendfile-progress-term");

   if (term)
     {
        term->sendfile_progress = NULL;
        term->sendfile_progress_bar = NULL;
     }
   evas_object_data_del(o, "sendfile-progress-timer");
   evas_object_data_del(o, "sendfile-progress-term");
   evas_object_del(o);
   return EINA_FALSE;
}

static Eina_Bool
_sendfile_progress_hide_delay(void *data)
{
   Term *term = data;
   Ecore_Timer *t;

   term->sendfile_progress_hide_timer = NULL;
   if (!term->sendfile_progress_enabled) return EINA_FALSE;
   term->sendfile_progress_enabled = EINA_FALSE;
   edje_object_signal_emit(term->bg, "sendfile,progress,off", "terminology");
   t = evas_object_data_get(term->sendfile_progress, "sendfile-progress-timer");
   if (t) ecore_timer_del(t);
   t = ecore_timer_add(10.0, _sendfile_progress_reset, term->sendfile_progress);
   evas_object_data_set(term->sendfile_progress, "sendfile-progress-timer", t);
   return EINA_FALSE;
}

static void
_sendfile_progress_hide(Term *term)
{
   if (!term->sendfile_progress_enabled) return;
   if (term->sendfile_progress_hide_timer)
     ecore_timer_del(term->sendfile_progress_hide_timer);
   term->sendfile_progress_hide_timer =
     ecore_timer_add(0.5, _sendfile_progress_hide_delay, term);
   if (elm_object_focus_get(term->sendfile_progress))
     {
        elm_object_focus_set(term->sendfile_progress, EINA_FALSE);
        term_focus(term);
     }
}

static void
_sendfile_progress_cancel(void *data, Evas_Object *obj EINA_UNUSED, void *info EINA_UNUSED)
{
   Term *term = data;

   if (!term->sendfile_progress) return;
   termio_file_send_cancel(term->termio);
   _sendfile_progress_hide(term);
}

static void
_sendfile_progress(Term *term)
{
   Evas_Object *o, *base;

   if (term->sendfile_progress)
     {
        evas_object_del(term->sendfile_progress);
        term->sendfile_progress = NULL;
     }
   if (!edje_object_part_exists(term->bg, "terminology.sendfile.progress"))
     {
        return;
     }
   if (term->sendfile_progress_hide_timer)
     {
        ecore_timer_del(term->sendfile_progress_hide_timer);
        term->sendfile_progress_hide_timer = NULL;
     }
   o = elm_box_add(term->wn->win);
   evas_object_data_set(o, "sendfile-progress-term", term);
   base = o;
   term->sendfile_progress = o;
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _sendfile_progress_del, NULL);
   elm_box_horizontal_set(o, EINA_TRUE);

   o = elm_button_add(term->wn->win);
   elm_object_text_set(o, "Cancel");
   evas_object_smart_callback_add(o, "clicked", _sendfile_progress_cancel, term);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(base, o);
   evas_object_show(o);

   o = elm_progressbar_add(term->wn->win);
   term->sendfile_progress_bar = o;
   elm_progressbar_unit_format_set(o, "%1.0f%%");
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(base, o);
   evas_object_show(o);

   term->sendfile_progress_enabled = EINA_TRUE;
   edje_object_part_swallow(term->bg, "terminology.sendfile.progress", base);
   evas_object_show(base);
   edje_object_signal_emit(term->bg, "sendfile,progress,on", "terminology");
}

static void
_sendfile_request_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *info EINA_UNUSED)
{
   Evas_Object *o = obj;
   Term *term = evas_object_data_get(o, "sendfile-request-term");
   Ecore_Timer *t;

   evas_object_data_del(o, "sendfile-request-term");
   if (term) term->sendfile_request = NULL;
   t = evas_object_data_get(o, "sendfile-request-timer");
   evas_object_data_del(o, "sendfile-request-term");
   if (t) ecore_timer_del(t);
}

static Eina_Bool
_sendfile_request_reset(void *data)
{
   Evas_Object *o = data;
   Term *term = evas_object_data_get(o, "sendfile-request-term");

   if (term) term->sendfile_request = NULL;
   evas_object_data_del(o, "sendfile-request-timer");
   evas_object_data_del(o, "sendfile-request-term");
   evas_object_del(o);
   return EINA_FALSE;
}

static Eina_Bool
_sendfile_request_hide_delay(void *data)
{
   Term *term = data;
   Ecore_Timer *t;

   term->sendfile_request_hide_timer = NULL;
   if (!term->sendfile_request_enabled) return EINA_FALSE;
   term->sendfile_request_enabled = EINA_FALSE;
   edje_object_signal_emit(term->bg, "sendfile,request,off", "terminology");
   t = evas_object_data_get(term->sendfile_request, "sendfile-request-timer");
   if (t) ecore_timer_del(t);
   t = ecore_timer_add(10.0, _sendfile_request_reset, term->sendfile_request);
   evas_object_data_set(term->sendfile_request, "sendfile-request-timer", t);
   if (elm_object_focus_get(term->sendfile_request))
     {
        elm_object_focus_set(term->sendfile_request, EINA_FALSE);
        term_focus(term);
     }
   return EINA_FALSE;
}

static void
_sendfile_request_hide(Term *term)
{
   if (!term->sendfile_request_enabled) return;
   if (term->sendfile_request_hide_timer)
     ecore_timer_del(term->sendfile_request_hide_timer);
   term->sendfile_request_hide_timer =
     ecore_timer_add(0.2, _sendfile_request_hide_delay, term);
}

static void
_sendfile_request_done(void *data, Evas_Object *obj EINA_UNUSED, void *info)
{
   Term *term = data;
   const char *path, *selpath = info;

   if (!term->sendfile_request) return;

   path = elm_fileselector_path_get(term->sendfile_request);
   eina_stringshare_replace(&term->sendfile_dir, path);

   if (selpath)
     {
        _sendfile_progress(term);
        termio_file_send_ok(term->termio, selpath);
     }
   else  termio_file_send_cancel(term->termio);
   _sendfile_request_hide(term);
}

static void
_sendfile_request(Term *term, const char *path)
{
   Evas_Object *o;
   const char *p;

   if (term->sendfile_request)
     {
        evas_object_del(term->sendfile_request);
        term->sendfile_request = NULL;
     }
   if (!edje_object_part_exists(term->bg, "terminology.sendfile.request"))
     {
        termio_file_send_cancel(term->termio);
        return;
     }
   if (term->sendfile_request_hide_timer)
     {
        ecore_timer_del(term->sendfile_request_hide_timer);
        term->sendfile_request_hide_timer = NULL;
     }
   o = elm_fileselector_add(term->wn->win);
   evas_object_data_set(o, "sendfile-request-term", term);
   term->sendfile_request = o;
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _sendfile_request_del, NULL);
   elm_fileselector_is_save_set(o, EINA_TRUE);
   elm_fileselector_expandable_set(o, EINA_FALSE);
   if (!term->sendfile_dir)
     {
        const char *dir = eina_environment_home_get();

        if (dir) term->sendfile_dir = eina_stringshare_add(dir);
     }
   if (term->sendfile_dir) elm_fileselector_path_set(o, term->sendfile_dir);
   p = strrchr(path, '/');
   if (p) elm_fileselector_current_name_set(o, p + 1);
   else elm_fileselector_current_name_set(o, path);
   evas_object_smart_callback_add(o, "done", _sendfile_request_done, term);
   term->sendfile_request_enabled = EINA_TRUE;
   edje_object_part_swallow(term->bg, "terminology.sendfile.request", o);
   evas_object_show(o);
   edje_object_signal_emit(term->bg, "sendfile,request,on", "terminology");
   elm_object_focus_set(o, EINA_TRUE);
}

static void
_cb_command(void *data,
            Evas_Object *_obj EINA_UNUSED,
            void *event)
{
   Term *term = data;
   const char *cmd = event;

   if (cmd[0] == 'p') // popmedia
     {
        if (cmd[1] == 'n') // now
          {
             _popmedia(term, cmd + 2);
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
                  Config *new_config = config_fork(config);

                  new_config->temporary = EINA_TRUE;
                  if (cmd[2])
                    eina_stringshare_replace(&(new_config->background), cmd + 2);
                  else
                    eina_stringshare_replace(&(new_config->background), NULL);
                  _term_config_set(term, new_config);
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
   else if (cmd[0] == 'f') // file...
     {
        if (cmd[1] == 'r') // receive
          {
             _sendfile_request(term, cmd + 2);
          }
        else if (cmd[1] == 'd') // data packet
          {
          }
        else if (cmd[1] == 'x') // exit data stream
          {
          }
     }
}

static void
_cb_tabcount_go(void *data,
                Evas_Object *_obj EINA_UNUSED,
                const char *_sig EINA_UNUSED,
                const char *_src EINA_UNUSED)
{
   _cb_select(data, NULL, NULL);
}

static void
_cb_prev(void *data,
         Evas_Object *_obj EINA_UNUSED,
         void *_event EINA_UNUSED)
{
   Term *term = data;

   term_prev(term);
}

static void
_cb_next(void *data,
         Evas_Object *_obj EINA_UNUSED,
         void *_event EINA_UNUSED)
{
   Term *term = data;

   term_next(term);
}

static void
_cb_split_h(void *data,
            Evas_Object *_obj EINA_UNUSED,
            void *_event EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc = term->container;

   assert(tc->type == TERM_CONTAINER_TYPE_SOLO);
   tc->split(tc, tc, term, NULL, EINA_TRUE);
}

static void
_cb_split_v(void *data,
            Evas_Object *_obj EINA_UNUSED,
            void *_event EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc = term->container;

   assert(tc->type == TERM_CONTAINER_TYPE_SOLO);
   tc->split(tc, tc, term, NULL, EINA_FALSE);
}

static void
_cb_title(void *data,
          Evas_Object *_obj EINA_UNUSED,
          void *_event EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc = term->container;
   const char *title = termio_title_get(term->termio);

   if (title)
     tc->set_title(tc, tc, title);
}

static void
_cb_icon(void *data,
         Evas_Object *_obj EINA_UNUSED,
         void *_event EINA_UNUSED)
{
   Term *term = data;
   DBG("is focused? tc:%p", term->container);
   if (_term_is_focused(term))
     elm_win_icon_name_set(term->wn->win, termio_icon_name_get(term->termio));
}

static void
_cb_send_progress(void *data,
                  Evas_Object *_obj EINA_UNUSED,
                  void *_event EINA_UNUSED)
{
   Term *term = data;

   elm_progressbar_value_set(term->sendfile_progress_bar,
                             termio_file_send_progress_get(term->termio));
}

static void
_cb_send_end(void *data,
             Evas_Object *_obj EINA_UNUSED,
             void *_event EINA_UNUSED)
{
   Term *term = data;
   if (!term->sendfile_progress) return;
   _sendfile_request_hide(term);
   _sendfile_progress_hide(term);
}

static Eina_Bool
_cb_cmd_focus(void *data)
{
   Win *wn = data;
   Term *term;
   Term_Container *tc;

   wn->cmdbox_focus_timer = NULL;
   tc = (Term_Container*) wn;
   term = tc->focused_term_get(tc);
   if (term && term->wn->cmdbox)
     elm_object_focus_set(wn->cmdbox, EINA_TRUE);
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
_cb_cmd_activated(void *data,
                  Evas_Object *_obj EINA_UNUSED,
                  void *_event EINA_UNUSED)
{
   Win *wn = data;
   char *cmd = NULL;
   Term *term;
   Term_Container *tc;

   if (wn->cmdbox) elm_object_focus_set(wn->cmdbox, EINA_FALSE);
   edje_object_signal_emit(wn->base, "cmdbox,hide", "terminology");
   tc = (Term_Container *) wn;
   term = tc->focused_term_get(tc);
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
                Evas_Object *_obj EINA_UNUSED,
                void *_event EINA_UNUSED)
{
   Win *wn = data;

   if (wn->cmdbox) elm_object_focus_set(wn->cmdbox, EINA_FALSE);
   edje_object_signal_emit(wn->base, "cmdbox,hide", "terminology");
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
                Evas_Object *_obj EINA_UNUSED,
                void *_event EINA_UNUSED)
{
   Win *wn = data;
   char *cmd = NULL;
   Term *term;
   Term_Container *tc;

   tc = (Term_Container *) wn;
   term = tc->focused_term_get(tc);
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
_cb_cmd_hints_changed(void *data,
                      Evas *_e EINA_UNUSED,
                      Evas_Object *_obj EINA_UNUSED,
                      void *_event_info EINA_UNUSED)
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
           Evas_Object *_obj EINA_UNUSED,
           void *_event EINA_UNUSED)
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
_cb_media_del(void *data,
              Evas *_e EINA_UNUSED,
              Evas_Object *_obj EINA_UNUSED,
              void *_event_info EINA_UNUSED)
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

   if (term->sendfile_request)
     {
        evas_object_del(term->sendfile_request);
        term->sendfile_request = NULL;
     }
   if (term->sendfile_progress)
     {
        evas_object_del(term->sendfile_progress);
        term->sendfile_progress = NULL;
     }
   if (term->sendfile_request_hide_timer)
     {
        ecore_timer_del(term->sendfile_request_hide_timer);
        term->sendfile_request_hide_timer = NULL;
     }
   if (term->sendfile_progress_hide_timer)
     {
        ecore_timer_del(term->sendfile_progress_hide_timer);
        term->sendfile_progress_hide_timer = NULL;
     }
   if (term->sendfile_dir)
     {
        eina_stringshare_del(term->sendfile_dir);
        term->sendfile_dir = NULL;
     }
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

   edje_object_part_unswallow(term->bg, term->base);
   evas_object_del(term->base);
   term->base = NULL;
   evas_object_del(term->bg);
   term->bg = NULL;

   _term_tabregion_free(term);

   if (term->tabcount_spacer)
     {
        evas_object_del(term->tabcount_spacer);
        term->tabcount_spacer = NULL;
     }
   free(term);
}

static void
_cb_tabcount_prev(void *data,
                  Evas_Object *_obj EINA_UNUSED,
                  const char *_sig EINA_UNUSED,
                  const char *_src EINA_UNUSED)
{
   _cb_prev(data, NULL, NULL);
}

static void
_cb_tabcount_next(void *data,
                  Evas_Object *_obj EINA_UNUSED,
                  const char *_sig EINA_UNUSED,
                  const char *_src EINA_UNUSED)
{
   _cb_next(data, NULL, NULL);
}


static void
_term_bg_config(Term *term)
{
   _set_trans(term->config, term->bg, term->base);
   background_set_shine(term->config, term->bg);

   termio_theme_set(term->termio, term->bg);
   edje_object_signal_callback_add(term->bg, "popmedia,done", "terminology",
                                   _cb_popmedia_done, term);
   edje_object_signal_callback_add(term->bg, "tabcount,go", "terminology",
                                   _cb_tabcount_go, term);
   edje_object_signal_callback_add(term->bg, "tabcount,prev", "terminology",
                                   _cb_tabcount_prev, term);
   edje_object_signal_callback_add(term->bg, "tabcount,next", "terminology",
                                   _cb_tabcount_next, term);
   edje_object_signal_callback_add(term->bg, "tab,close", "terminology",
                                   _cb_tab_close, term);
   edje_object_signal_callback_add(term->bg, "tab,title", "terminology",
                                   _cb_tab_title, term);
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

   DBG("is focused? tc:%p", term->container);
   if (_term_is_focused(term) && (_win_is_focused(term->wn)))
     {
        if (term->config->disable_focus_visuals)
          {
             edje_object_signal_emit(term->bg, "focused,set", "terminology");
             edje_object_signal_emit(term->base, "focused,set", "terminology");
          }
        else
          {
             edje_object_signal_emit(term->bg, "focus,in", "terminology");
             edje_object_signal_emit(term->base, "focus,in", "terminology");
          }
        if (term->wn->cmdbox)
          elm_object_focus_set(term->wn->cmdbox, EINA_FALSE);
     }
   if (term->miniview_shown)
        edje_object_signal_emit(term->bg, "miniview,on", "terminology");

   _term_media_update(term, term->config);
}

static void
_cb_tabregion_change(void *data,
                     Evas *_e EINA_UNUSED,
                     Evas_Object *obj,
                     void *_info EINA_UNUSED)
{
   Term *term = data;
   Evas_Coord w, h;

   evas_object_geometry_get(obj, NULL, NULL, &w, &h);
   evas_object_size_hint_min_set(term->tab_region_base, w, h);
   edje_object_part_swallow(term->base, "terminology.tabregion",
                            term->tab_region_base);
}

static void
_term_tabregion_setup(Term *term)
{
   Evas_Object *o;

   if (term->tab_region_bg) return;
   term->tab_region_bg = o = evas_object_rectangle_add(evas_object_evas_get(term->bg));
   evas_object_color_set(o, 0, 0, 0, 0);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOVE, _cb_tabregion_change, term);
   evas_object_event_callback_add(o, EVAS_CALLBACK_RESIZE, _cb_tabregion_change, term);
   edje_object_part_swallow(term->bg, "terminology.tabregion", o);

   term->tab_region_base = o = evas_object_rectangle_add(evas_object_evas_get(term->bg));
   evas_object_color_set(o, 0, 0, 0, 0);
   edje_object_part_swallow(term->base, "terminology.tabregion", o);
}

static void
_term_tabregion_free(Term *term)
{
   evas_object_del(term->tab_region_bg);
   term->tab_region_bg = NULL;

   evas_object_del(term->tab_region_base);
   term->tab_region_base = NULL;
}

Eina_Bool
main_term_popup_exists(const Term *term)
{
   return term->popmedia || term->popmedia_queue;
}

Win *
term_win_get(const Term *term)
{
   return term->wn;
}


Evas_Object *
term_termio_get(const Term *term)
{
   return term->termio;
}

Evas_Object *
term_miniview_get(const Term *term)
{
   if (term)
     return term->miniview;
   return NULL;
}


static void
_cb_bell(void *data,
         Evas_Object *_obj EINA_UNUSED,
         void *_event EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc;

   tc = term->container;

   tc->bell(tc, tc);
}


static void
_cb_options_done(void *data)
{
   Term *orig_term = data;
   Win *wn = orig_term->wn;

   _on_popover_done(wn);

   term_unref(orig_term);
}

static void
_cb_options(void *data,
            Evas_Object *_obj EINA_UNUSED,
            void *_event EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc = term->container;

   term->wn->on_popover++;

   term_ref(term);
   tc->unfocus(tc, NULL);

   controls_show(term->wn->win, term->wn->base, term->bg, term->termio,
                 _cb_options_done, term);
}

void
term_ref(Term *term)
{
   term->refcnt++;
}

void
term_unref(Term *term)
{
   EINA_SAFETY_ON_NULL_RETURN(term);

   term->refcnt--;
   if (term->refcnt <= 0)
     {
        _term_free(term);
     }
}


Term *
term_new(Win *wn, Config *config, const char *cmd,
         Eina_Bool login_shell, const char *cd,
         int size_w, int size_h, Eina_Bool hold,
         const char *title)
{
   Term *term;
   Evas_Object *o;
   Evas *canvas = evas_object_evas_get(wn->win);

   term = calloc(1, sizeof(Term));
   if (!term) return NULL;
   term_ref(term);

   if (!config) abort();

   /* TODO: clean up that */
   if (_win_log_dom < 0)
     {
        _win_log_dom = eina_log_domain_register("win", NULL);
        if (_win_log_dom < 0)
          EINA_LOG_CRIT("Could not create logging domain '%s'.", "win");
     }
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
   evas_object_data_set(o, "theme_reload_func", _term_bg_config);
   evas_object_data_set(o, "theme_reload_func_data", term);
   evas_object_show(o);

   term->bg = o = edje_object_add(canvas);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   if (!theme_apply(o, config, "terminology/background"))
     {
        CRITICAL(_("Couldn't find terminology theme! Forgot 'ninja install'?"));
        evas_object_del(term->bg);
        free(term);
        return NULL;
     }

   theme_auto_reload_enable(o);
   evas_object_data_set(o, "theme_reload_func", _term_bg_config);
   evas_object_data_set(o, "theme_reload_func_data", term);
   evas_object_show(o);

   _term_tabregion_setup(term);


   if (term->config->mv_always_show)
     term->miniview_shown = EINA_TRUE;

   _set_trans(term->config, term->bg, term->base);
   background_set_shine(term->config, term->bg);

   term->termio = o = termio_add(wn->win, config, cmd, login_shell, cd,
                                 size_w, size_h, term, title);
   evas_object_data_set(o, "term", term);
   colors_term_init(termio_textgrid_get(term->termio), term->bg, config);

   termio_theme_set(o, term->bg);

   term->miniview = o = miniview_add(wn->win, term->termio);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

   o = term->termio;

   evas_object_size_hint_weight_set(o, 0, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, 0, EVAS_HINT_FILL);
   evas_object_event_callback_add(o, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                  _cb_size_hint, term);
   edje_object_part_swallow(term->base, "terminology.content", o);
   edje_object_part_swallow(term->bg, "terminology.content", term->base);
   edje_object_part_swallow(term->bg, "terminology.miniview", term->miniview);
   evas_object_smart_callback_add(o, "options", _cb_options, term);
   evas_object_smart_callback_add(o, "bell", _cb_bell, term);
   evas_object_smart_callback_add(o, "popup", _cb_popup, term);
   evas_object_smart_callback_add(o, "popup,queue", _cb_popup_queue, term);
   evas_object_smart_callback_add(o, "cmdbox", _cb_cmdbox, term);
   evas_object_smart_callback_add(o, "command", _cb_command, term);
   evas_object_smart_callback_add(o, "prev", _cb_prev, term);
   evas_object_smart_callback_add(o, "next", _cb_next, term);
   evas_object_smart_callback_add(o, "new", _cb_new, term);
   evas_object_smart_callback_add(o, "close", _cb_close, term);
   evas_object_smart_callback_add(o, "select", _cb_select, term);
   evas_object_smart_callback_add(o, "split,h", _cb_split_h, term);
   evas_object_smart_callback_add(o, "split,v", _cb_split_v, term);
   evas_object_smart_callback_add(o, "title,change", _cb_title, term);
   evas_object_smart_callback_add(o, "icon,change", _cb_icon, term);
   evas_object_smart_callback_add(o, "send,progress", _cb_send_progress, term);
   evas_object_smart_callback_add(o, "send,end", _cb_send_end, term);
   evas_object_show(o);

   wn->terms = eina_list_append(wn->terms, term);

   _term_bg_config(term);

   return term;
}


/* }}} */



static Eina_Bool
_font_size_set(Term *term, void *data)
{
   int fontsize = (intptr_t) data;

   termio_font_size_set(term->termio, fontsize);

   return ECORE_CALLBACK_PASS_ON;
}

void
win_font_size_set(Win *wn, int new_size)
{
   for_each_term_do(wn, &_font_size_set, (void*)(intptr_t)new_size);
}

static Eina_Bool
_font_update(Term *term, void *_data EINA_UNUSED)
{
   termio_font_update(term->termio);

   return ECORE_CALLBACK_PASS_ON;
}

void
win_font_update(Term *term)
{
   Win *wn = term->wn;
   for_each_term_do(wn, &_font_update, NULL);
}

void
windows_free(void)
{
   Win *wn;

   while (wins)
     {
        wn = eina_list_data_get(wins);
        win_free(wn);
     }

   /* TODO: ugly */
   if (_win_log_dom < 0) return;
   eina_log_domain_unregister(_win_log_dom);
   _win_log_dom = -1;
}

void
windows_update(void)
{
   Eina_List *l;
   Win *wn;

   EINA_LIST_FOREACH(wins, l, wn)
     {
        Term_Container *tc = (Term_Container *) wn;
        tc->update(tc);
     }
}

Eina_Bool
for_each_term_do(Win *wn, For_Each_Term cb, void *data)
{
   Eina_List *l;
   Term *term;
   Eina_Bool res = ECORE_CALLBACK_DONE;

   EINA_LIST_FOREACH(wn->terms, l, term)
     {
        res = cb(term, data);
        if (res == ECORE_CALLBACK_CANCEL)
          return res;
     }
   return res;
}
