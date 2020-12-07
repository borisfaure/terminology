#include "private.h"

#include <assert.h>
#include <Elementary.h>
#include <Elementary_Cursor.h>
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
#include "theme.h"
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

#define DRAG_TIMEOUT 0.4

/* {{{ Structs */

typedef struct _Split Split;
typedef struct _Tabbar Tabbar;
typedef struct _Solo Solo;
typedef struct _Tabs Tabs;
typedef struct _Tab_Item Tab_Item;
typedef struct _Tab_Drag Tab_Drag;


struct _Tab_Drag
{
   Evas_Coord mdx;     /* Mouse-down x */
   Evas_Coord mdy;     /* Mouse-down y */
   Split_Direction split_direction;
   Term *term_over;
   Term *term;
   Evas_Object *icon;
   Evas_Object *img;
   Evas *e;
   Ecore_Timer *timer;
   /* To be able to restore */
   Term_Container_Type parent_type;
   union {
        struct {
             int previous_position;
             Term_Container *tabs_child;
        };
        struct {
             Term_Container *other;
             double left_size;
             Eina_Bool is_horizontal;
             Eina_Bool is_first_child;
        }; };
};

struct _Tabbar
{
   struct {
      Evas_Object *box;
   } l, r;
};

struct _Term
{
   Win         *wn;
   Config      *config;

   Term_Container *container;
   Evas_Object *bg;
   Evas_Object *bg_edj;
   Evas_Object *core;
   Evas_Object *termio;
   Evas_Object *media;
   Evas_Object *popmedia;
   Evas_Object *miniview;
   Evas_Object *sel;
   Evas_Object *sendfile_request;
   Evas_Object *sendfile_progress;
   Evas_Object *sendfile_progress_bar;
   Evas_Object *tab_spacer;
   Evas_Object *tab_region_base;
   Evas_Object *tab_region_bg;
   Evas_Object *tab_inactive;
   Tab_Item    *tab_item;
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
   unsigned char has_bg_cursor : 1;
   unsigned char core_cursor_set: 1;

   Eina_Bool sendfile_request_enabled : 1;
   Eina_Bool sendfile_progress_enabled : 1;
};

struct _Solo {
     Term_Container tc;
     Term *term;
};

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
     double v1_orig;
     double v2_orig;
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
   Ecore_Timer *hide_cursor_timer;
   Ecore_Event_Handler *config_elm_handler;
   unsigned char focused : 1;
   unsigned char cmdbox_up : 1;
   unsigned char group_input : 1;
   unsigned char group_only_visible : 1;
   unsigned char group_once_handled : 1;
   unsigned char translucent : 1;

   unsigned int  on_popover;
};

/* }}} */
/* {{{ static */
static Eina_List   *wins = NULL;
static Tab_Drag *_tab_drag = NULL;

static Eina_Bool _win_is_focused(Win *wn);
static Term_Container *_solo_new(Term *term, Win *wn);
static Term_Container *_split_new(Term_Container *tc1, Term_Container *tc2,
                                  double left_size, Eina_Bool is_horizontal);
static Term_Container *_tabs_new(Term_Container *child, Term_Container *parent);
static void _term_free(Term *term);
static void _term_media_update(Term *term, const Config *config);
static void _term_miniview_check(Term *term);
static void _popmedia_queue_process(Term *term);
static void _cb_size_hint(void *data, Evas *_e EINA_UNUSED, Evas_Object *obj, void *_event EINA_UNUSED);
static void _tab_new_cb(void *data, Evas_Object *_obj EINA_UNUSED, void *_event_info EINA_UNUSED);
static Tab_Item* tab_item_new(Tabs *tabs, Term_Container *child);
static void _tabs_recreate(Tabs *tabs);
static void _tab_drag_free(void);
static void _term_tabregion_free(Term *term);
static void _imf_event_commit_cb(void *data, Ecore_IMF_Context *_ctx EINA_UNUSED, void *event);

/* }}} */
/* {{{ Utils */
#ifndef NDEBUG
static void
_focus_validator(void)
{
   Win *wn;
   Term *term;
   Eina_List *l, *ll;

   EINA_LIST_FOREACH(wins, l, wn)
     {
        Term_Container *focused_found = NULL;

        if (wn->group_input)
          continue;

        EINA_LIST_FOREACH(wn->terms, ll, term)
          {
             Term_Container *tc = term->container;

             if (focused_found)
               {
                  assert(!tc->is_focused);
               }
             else
               {
                  if (tc->is_focused)
                    {
                       Term *term_focused;
                       Term_Container *tc_parent = tc;

                       focused_found = tc;
                       do
                         {
                            assert (tc_parent->is_focused);
                            tc_parent = tc_parent->parent;
                            if (tc_parent->type == TERM_CONTAINER_TYPE_TABS)
                              {
                                 Tabs *tabs = (Tabs*) tc_parent;
                                 if (tabs->selector_bg)
                                   return;
                              }
                         }
                       while (tc_parent->type != TERM_CONTAINER_TYPE_WIN);
                       assert (tc_parent->is_focused);
                       term_focused = tc_parent->focused_term_get(tc_parent);
                       assert(term_focused == term);
                    }
               }
          }
     }
}
#else
static void
_focus_validator(void)
{}
#endif

#define GROUPED_INPUT_TERM_FOREACH(_wn, _list, _term) \
   EINA_LIST_FOREACH(_wn->terms, _list, _term) \
     if (!_wn->group_only_visible || term_is_visible(_term))

/* }}} */
/* {{{ Scale */
static void
_scale_round(void *data       EINA_UNUSED,
             Evas_Object     *obj,
             void *event_info EINA_UNUSED)
{
   double val = elm_slider_value_get(obj);
   double v;

   v = ((double)((int)(val * 10.0))) / 10.0;
   if (v != val) elm_slider_value_set(obj, v);
}

static void
_scale_change(void *data       EINA_UNUSED,
              Evas_Object     *obj,
              void *event_info EINA_UNUSED)
{
   double scale = elm_config_scale_get();
   double val = elm_slider_value_get(obj);

   if (scale == val)
     return;
   elm_config_scale_set(val);
   elm_config_all_flush();
}

typedef struct _Scale_Ctx
{
   Evas_Object *hv;
   Term *term;
} Scale_Ctx;

static void
_scale_done(void *data,
            Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Scale_Ctx *ctx = data;

   evas_object_smart_callback_del_full(ctx->hv, "dismissed",
                                       _scale_done, ctx);
   evas_object_del(ctx->hv);
   ctx->term->wn->on_popover--;
   term_unref(ctx->term);
   elm_config_save();
   config_save(ctx->term->config);
   free(ctx);
}

void
win_scale_wizard(Evas_Object *win, Term *term)
{
   Evas_Object *bx, *lbl, *sl, *fr, *bt;
   const char *txt;
   Scale_Ctx *ctx;

   EINA_SAFETY_ON_NULL_RETURN(term);

   ctx = calloc(1, sizeof(*ctx));
   if (!ctx)
     return;

   ctx->term = term;

   term->wn->on_popover++;

   term_ref(term);

   ctx->hv = elm_hover_add(win);
   evas_object_size_hint_weight_set(ctx->hv, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(ctx->hv, EVAS_HINT_FILL, 0.5);
   elm_hover_parent_set(ctx->hv, win);
   elm_hover_target_set(ctx->hv, win);
   evas_object_smart_callback_add(ctx->hv, "dismissed", _scale_done, ctx);

   fr = elm_frame_add(win);
   evas_object_size_hint_weight_set(fr, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(fr, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(fr, _("Scale"));
   elm_object_part_content_set(ctx->hv, "middle", fr);
   evas_object_show(fr);

   bx = elm_box_add(win);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.5);
   elm_object_content_set(fr, bx);
   evas_object_show(bx);

   fr = elm_frame_add(win);
   evas_object_size_hint_weight_set(fr, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(fr, EVAS_HINT_FILL, 0.5);
   elm_object_style_set(fr, "pad_medium");
   elm_box_pack_end(bx, fr);
   evas_object_show(fr);

   lbl = elm_label_add(win);
   evas_object_size_hint_weight_set(lbl, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lbl, EVAS_HINT_FILL, 0.5);
   txt = eina_stringshare_printf("<hilight>%s</>",_("Scale"));
   elm_object_text_set(lbl, txt);
   eina_stringshare_del(txt);
   elm_object_content_set(fr, lbl);
   elm_box_pack_end(bx, lbl);
   evas_object_show(lbl);

   sl = elm_slider_add(win);
   evas_object_size_hint_weight_set(sl, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sl, EVAS_HINT_FILL, 0.5);
   elm_slider_span_size_set(sl, 120);
   elm_slider_unit_format_set(sl, "%1.2f");
   elm_slider_indicator_format_set(sl, "%1.2f");
   elm_slider_min_max_set(sl, 0.25, 5.0);
   elm_slider_value_set(sl, elm_config_scale_get());
   elm_box_pack_end(bx, sl);
   evas_object_show(sl);
   evas_object_smart_callback_add(sl, "changed", _scale_round, NULL);
   evas_object_smart_callback_add(sl, "delay,changed", _scale_change, NULL);

   bt = elm_button_add(win);
   elm_object_text_set(bt, _("Done"));
   elm_box_pack_end(bx, bt);
   evas_object_smart_callback_add(bt, "clicked", _scale_done, ctx);
   evas_object_show(bt);

   lbl = elm_label_add(win);
   evas_object_size_hint_weight_set(lbl, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lbl, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(lbl, _("Select preferred size so that this text is readable"));
   elm_label_line_wrap_set(lbl, ELM_WRAP_WORD);
   elm_box_pack_end(bx, lbl);
   evas_object_show(lbl);

   lbl = elm_label_add(win);
   evas_object_size_hint_weight_set(lbl, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lbl, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(lbl, _("The scale configuration can be changed in the Settings (right click on the terminal) →  Toolkit, or by starting the command <keyword>elementary_config</keyword>"));
   elm_label_line_wrap_set(lbl, ELM_WRAP_WORD);
   elm_box_pack_end(bx, lbl);
   evas_object_show(lbl);

   evas_object_show(ctx->hv);

   elm_object_focus_set(ctx->hv, EINA_TRUE);
   elm_object_focus_set(bt, EINA_TRUE);
}

/* }}} */
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
_solo_focused_term_get(const Term_Container *tc)
{
   Solo *solo;
   Term *term = NULL;

   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)tc;

   if (tc->is_focused)
     term = solo->term;
   return term;
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

static int
_solo_split_direction(Term_Container *tc,
                      Term_Container *child_orig EINA_UNUSED,
                      Term_Container *child_new,
                      Split_Direction direction)
{
   return tc->parent->split_direction(tc->parent, tc, child_new, direction);
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
   Solo *solo;
   Term *term;

   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;
   term = solo->term;

   eina_stringshare_del(tc->title);
   tc->title = eina_stringshare_add(title);
   if (term->config->show_tabs)
     {
        elm_layout_text_set(term->bg, "terminology.tab.title", title);
     }
   if (_tab_drag && _tab_drag->term == term && _tab_drag->icon)
     {
        elm_layout_text_set(_tab_drag->icon,
                            "terminology.title", title);
     }
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

   if (!tc->is_focused)
     term->missed_bell = EINA_TRUE;

   if (!tc->wn->config->disable_visual_bell)
     {
        elm_layout_signal_emit(term->bg, "bell", "terminology");
        elm_layout_signal_emit(term->core, "bell", "terminology");
        if (tc->wn->config->bell_rings)
          {
             elm_layout_signal_emit(term->bg, "bell,ring", "terminology");
             elm_layout_signal_emit(term->core, "bell,ring", "terminology");
          }
        if ((_tab_drag != NULL) && (_tab_drag->term == term))
          {
             elm_layout_signal_emit(_tab_drag->icon, "bell", "terminology");
          }
     }
   if ((term->missed_bell) && (term->config->show_tabs)
       && (tc->parent->type == TERM_CONTAINER_TYPE_SPLIT))
     {
        elm_layout_signal_emit(term->bg, "tab,bell,on", "terminology");
     }
   edje_object_message_signal_process(term->bg_edj);
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

   DBG("tc:%p tc->is_focused:%d from_parent:%d term:%p",
       tc, tc->is_focused, tc->parent == relative, term);
   if (!tc->is_focused)
     return;

   tc->is_focused = EINA_FALSE;
   termio_focus_out(term->termio);

   if (tc->parent != relative)
     tc->parent->unfocus(tc->parent, tc);

   if (!term->config->disable_focus_visuals)
     {
        elm_layout_signal_emit(term->bg, "focus,out", "terminology");
        elm_layout_signal_emit(term->core, "focus,out", "terminology");
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

   DBG("tc:%p tc->is_focused:%d from_parent:%d term:%p",
       tc, tc->is_focused, tc->parent == relative, term);
   if (tc->is_focused)
     return;

   term->missed_bell = EINA_FALSE;
   if ((term->config->show_tabs)
       && (tc->parent->type == TERM_CONTAINER_TYPE_SPLIT))
     {
        elm_layout_signal_emit(term->bg, "tab,bell,off", "terminology");
     }

   if (term->config->disable_focus_visuals)
     {
        elm_layout_signal_emit(term->bg, "focused,set", "terminology");
        elm_layout_signal_emit(term->core, "focused,set", "terminology");
     }
   else
     {
        elm_layout_signal_emit(term->bg, "focus,in", "terminology");
        elm_layout_signal_emit(term->core, "focus,in", "terminology");
     }
   termio_event_feed_mouse_in(term->termio);
   termio_focus_in(term->termio);

   title = termio_title_get(term->termio);
   if (title)
      tc->set_title(tc, tc, title);

   if (term->missed_bell)
     term->missed_bell = EINA_FALSE;
   edje_object_message_signal_process(term->bg_edj);
   if (!tc->is_focused && relative != tc->parent)
     {
       tc->is_focused = EINA_TRUE;
       tc->parent->focus(tc->parent, tc);
     }
   tc->is_focused = EINA_TRUE;
   _focus_validator();
}

static Eina_Bool
_solo_is_visible(const Term_Container *tc, const Term_Container *_child EINA_UNUSED)
{
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   return tc->parent->is_visible(tc->parent, tc);
}

static void
_solo_tab_show(Term_Container *tc)
{
   Solo *solo;
   Term *term;

   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;
   term = solo->term;
   DBG("tab show tc:%p", tc);

   if (!term->tab_spacer)
     {
        Evas_Coord w = 0, h = 0;

        term->tab_spacer = evas_object_rectangle_add(
           evas_object_evas_get(term->bg));
        evas_object_color_set(term->tab_spacer, 0, 0, 0, 0);
        elm_coords_finger_size_adjust(1, &w, 1, &h);
        evas_object_size_hint_min_set(term->tab_spacer, w, h);
        elm_layout_content_set(term->bg, "terminology.tab", term->tab_spacer);
        edje_object_part_drag_value_set(term->bg_edj,
                                        "terminology.tabl", 0.0, 0.0);
        edje_object_part_drag_value_set(term->bg_edj,
                                        "terminology.tabr", 1.0, 0.0);
        elm_layout_text_set(term->bg, "terminology.tab.title",
                            solo->tc.title);
        elm_layout_signal_emit(term->bg, "tabbar,on", "terminology");
        edje_object_message_signal_process(term->bg_edj);
     }
   else
     {
        edje_object_part_drag_value_set(term->bg_edj, "terminology.tabl", 0.0, 0.0);
        edje_object_part_drag_value_set(term->bg_edj, "terminology.tabr", 1.0, 0.0);
        edje_object_message_signal_process(term->bg_edj);
     }
}

static void
_solo_tab_hide(Term_Container *tc)
{
   Solo *solo;
   Term *term;

   DBG("title hide tc:%p", tc);
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;
   term = solo->term;

   if (term->tab_spacer)
     {
        elm_layout_signal_emit(term->bg, "tabbar,off", "terminology");
        edje_object_message_signal_process(term->bg_edj);
        elm_layout_content_unset(term->bg, "terminology.tab");
        evas_object_del(term->tab_spacer);
        term->tab_spacer = NULL;
     }
}

static void
_solo_update(Term_Container *tc)
{
   Solo *solo;
   Term *term;
   Term_Container *tc_parent = tc->parent;

   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*) tc;
   term = solo->term;

   if (tc_parent->type == TERM_CONTAINER_TYPE_SPLIT)
     {
        if (term->config->show_tabs)
          _solo_tab_show(tc);
        else
          _solo_tab_hide(tc);
     }
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
   tc->split_direction = _solo_split_direction;
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
   tc->detach = NULL;
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
   if (wn->on_popover)
       return;

   term = tc->focused_term_get(tc);

   if (wn->group_input)
     {
        Eina_List *l;
        Term *t;

        GROUPED_INPUT_TERM_FOREACH(wn, l, t)
          {
             elm_layout_signal_emit(t->bg, "focus,in", "terminology");
             termio_event_feed_mouse_in(t->termio);
             termio_focus_in(t->termio);
          }
     }
   else if ( wn->config->mouse_over_focus )
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
                       elm_layout_signal_emit(term->bg, "focus,out", "terminology");
                       elm_layout_signal_emit(term->core, "focus,out", "terminology");
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

   if (wn->on_popover)
     return;

   DBG("FOCUS OUT tc:%p tc->is_focused:%d",
       tc, tc->is_focused);
   tc->unfocus(tc, NULL);
   if (wn->group_input)
     {
        Eina_List *l;
        Term *term;

        GROUPED_INPUT_TERM_FOREACH(wn, l, term)
          {
             elm_layout_signal_emit(term->bg, "focus,out", "terminology");
             termio_focus_out(term->termio);
          }
     }
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

   tc_child = _solo_new(term, wn);
   if (!tc_child)
     goto bad;

   tc_win = (Term_Container*) wn;

   tc_win->swallow(tc_win, NULL, tc_child);

   _cb_size_hint(term, NULL, term->termio, NULL);

   return 0;

bad:
   free(tc_child);
   return -1;
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
_term_trans(Term *term)
{
   Edje_Message_Int msg;
   Evas_Object *edje = elm_layout_edje_get(term->core);
   Win *wn = term->wn;

   if (term->config->translucent)
     msg.val = term->config->opacity;
   else
     msg.val = 100;
   edje_object_message_send(term->bg_edj, EDJE_MESSAGE_INT, 1, &msg);
   edje_object_message_send(edje, EDJE_MESSAGE_INT, 1, &msg);

   if (term->config->translucent != wn->translucent)
     {
        if (term->config->translucent)
          {
             elm_win_alpha_set(wn->win, EINA_TRUE);
             evas_object_hide(wn->backbg);
             wn->translucent = EINA_TRUE;
          }
        else
          {
             elm_win_alpha_set(wn->win, EINA_FALSE);
             evas_object_show(wn->backbg);
             wn->translucent = EINA_FALSE;
          }
     }
}

void
main_trans_update(void)
{
   Win *wn;
   Term *term;
   Eina_List *l, *ll;

   EINA_LIST_FOREACH(wins, l, wn)
     {
        EINA_LIST_FOREACH(wn->terms, ll, term)
          {
             _term_trans(term);
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
   if (wn->config_elm_handler)
     {
        ecore_event_handler_del(wn->config_elm_handler);
        wn->config_elm_handler = NULL;
     }
   EINA_LIST_FREE(wn->terms, term)
     {
        term_unref(term);
     }
   if (wn->cmdbox_del_timer)
     {
        ecore_timer_del(wn->cmdbox_del_timer);
        wn->cmdbox_del_timer = NULL;
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
   config_del(wn->config);
   eina_stringshare_del(wn->preedit_str);
   keyin_compose_seq_reset(&wn->khdl);
   if (wn->khdl.imf)
     {
        ecore_imf_context_event_callback_del
          (wn->khdl.imf, ECORE_IMF_CALLBACK_COMMIT, _imf_event_commit_cb);
        ecore_imf_context_del(wn->khdl.imf);
     }
   if (wn->hide_cursor_timer)
     {
        ecore_timer_del(wn->hide_cursor_timer);
     }
   ecore_imf_shutdown();
   free(wn);
}

Win *
win_evas_object_to_win(const Evas_Object *win)
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

   wn = win_evas_object_to_win(win);
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
   Term *term = NULL;

   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;
   if (tc->is_focused && wn->child)
     term = wn->child->focused_term_get(wn->child);
   return term;
}

static Term *
_win_find_term_at_coords(const Term_Container *tc,
                         Evas_Coord mx, Evas_Coord my)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;

   if (!wn->child)
     return NULL;

   return wn->child->find_term_at_coords(wn->child, mx, my);
}

static void
_win_size_eval(Term_Container *tc, Sizeinfo *info)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;

   assert(wn->child);
   wn->child->size_eval(wn->child, info);
}

static void
_win_swallow(Term_Container *tc, Term_Container *orig,
             Term_Container *new_child)
{
   Win *wn;
   Evas_Object *o;

   assert (tc->type == TERM_CONTAINER_TYPE_WIN);

   wn = (Win*) tc;

   DBG("orig:%p", orig);

   if (orig)
     {
        elm_layout_content_unset(wn->base, "terminology.content");
     }

   o = new_child->get_evas_object(new_child);
   elm_layout_content_set(wn->base, "terminology.content", o);

   if ((new_child->type == TERM_CONTAINER_TYPE_SOLO)
       && (wn->config->show_tabs))
     {
        if (_tab_drag && _tab_drag->term && (_tab_drag->term->wn == wn) &&
            _tab_drag->icon)
          _solo_tab_show(new_child);
        else
          _solo_tab_hide(new_child);
     }

   evas_object_show(o);
   new_child->parent = tc;
   wn->child = new_child;

   if (_tab_drag && _tab_drag->icon)
     {
        evas_object_raise(_tab_drag->icon);
     }
}

static void
_win_close(Term_Container *tc,
           Term_Container *_child EINA_UNUSED)
{
   Win *wn;
   assert (tc->type == TERM_CONTAINER_TYPE_WIN);
   wn = (Win*) tc;

   DBG("win close");
   if (_tab_drag && _tab_drag->term && (_tab_drag->term->wn == wn))
     {
        _tab_drag->parent_type = TERM_CONTAINER_TYPE_WIN;
        _tab_drag_free();
        return;
     }
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

   if (!tc->is_focused)
     elm_win_urgent_set(wn->win, EINA_FALSE);

   tc->is_focused = EINA_TRUE;
   if ((relative != wn->child) || (!wn->focused))
     {
        DBG("focus tc:%p", tc);
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

        elm_layout_content_unset(wn->base, "terminology.content");

        tc_split = _split_new(child, tc_solo_new, 0.5, is_horizontal);
        if (wn->config->show_tabs)
          {
             if (child->type == TERM_CONTAINER_TYPE_SOLO)
               {
                  _solo_tab_show(child);
               }
             _solo_tab_show(tc_solo_new);
          }

        child->unfocus(child, tc_split);
        tc->swallow(tc, NULL, tc_split);
        tc_split->is_focused = EINA_TRUE;
        tc_split->focus(tc_split, tc_solo_new);
        tc_solo_new->focus(tc_solo_new, tc_split);
     }
   else
     {
        ERR("term is not splittable");
     }
}

static int
_win_split_direction(Term_Container *tc,
                     Term_Container *child_orig,
                     Term_Container *child_new,
                     Split_Direction direction)
{
   Term_Container *child1, *child2, *tc_split;
   Win *wn;
   Eina_Bool is_horizontal =
      (direction == SPLIT_DIRECTION_LEFT) || (direction == SPLIT_DIRECTION_RIGHT) ?
      EINA_FALSE : EINA_TRUE;

   assert (tc->type == TERM_CONTAINER_TYPE_WIN);
   wn = (Win*) tc;

   if (!_term_container_is_splittable(tc, is_horizontal))
     {
        ERR("term is not splittable");
        return -1;
     }

   if ((direction == SPLIT_DIRECTION_TOP) ||
       (direction == SPLIT_DIRECTION_LEFT))
     {
        child1 = child_new;
        child2 = child_orig;
     }
   else
     {
        child1 = child_orig;
        child2 = child_new;
     }

   wn->tc.unfocus(&wn->tc, NULL); /* unfocus from top */

   tc_split = _split_new(child1, child2, 0.5, is_horizontal);

   if (wn->config->show_tabs)
     {
        if (child_orig->type == TERM_CONTAINER_TYPE_SOLO)
          {
             _solo_tab_show(child_orig);
          }
        _solo_tab_show(child_new);
     }

   tc_split->is_focused = tc->is_focused;
   tc->swallow(tc, NULL, tc_split);

   child_new->focus(child_new, NULL); /* refocus from bottom */

   return 0;
}

static Eina_Bool
_set_cursor(Term *term, void *data)
{
   const char *cursor = data;

   assert(term->core);
   if (cursor)
     {
        elm_object_cursor_set(term->core, cursor);
        term->core_cursor_set = 1;
     }
   else
     {
        if (term->core_cursor_set)
          elm_object_cursor_unset(term->core);
        term->core_cursor_set = 0;
     }

   return ECORE_CALLBACK_PASS_ON;
}

static void
_win_update(Term_Container *tc)
{
   Win *wn;

   assert (tc->type == TERM_CONTAINER_TYPE_WIN);
   wn = (Win*) tc;

   if (wn->config->hide_cursor >= CONFIG_CURSOR_IDLE_TIMEOUT_MAX)
     {
        ecore_timer_del(wn->hide_cursor_timer);
        wn->hide_cursor_timer = NULL;

        for_each_term_do(wn, &_set_cursor, NULL);
     }

   wn->child->update(wn->child);
}

Eina_Bool
win_has_single_child(const Win *wn)
{
   const Term_Container *child = wn->child;

   return (child->type == TERM_CONTAINER_TYPE_SOLO);
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
   eina_stringshare_del(wn->preedit_str);
   wn->preedit_str = NULL;
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
   _focus_validator();
   if ((wn->on_popover) || (wn->cmdbox_up)) return;

   ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   alt = evas_key_modifier_is_set(ev->modifiers, "Alt");
   shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   win = evas_key_modifier_is_set(ev->modifiers, "Super");
   meta = evas_key_modifier_is_set(ev->modifiers, "Meta") ||
      evas_key_modifier_is_set(ev->modifiers, "AltGr") ||
      evas_key_modifier_is_set(ev->modifiers, "ISO_Level3_Shift");
   hyper = evas_key_modifier_is_set(ev->modifiers, "Hyper");

   /* 1st/ Tab selector */
     {
        Term_Container *tc = (Term_Container*) wn;

        term = tc->focused_term_get(tc);
        if (term)
          {
             Term_Container *tc_parent = tc->parent;

             tc = term->container;
             tc_parent = tc->parent;

             if (tc_parent->type == TERM_CONTAINER_TYPE_TABS)
               {
                  Tabs *tabs = (Tabs*) tc_parent;

                  if (tabs->selector != NULL)
                    {
                       sel_key_down(tabs->selector, ev);
                       return;
                    }
               }
          }
     }

   /* 2nd/ Miniview */
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
          {
             DBG("no focused term");
             return;
          }
        done = miniview_handle_key(term_miniview_get(term), ev);
     }
   if (done)
     {
        keyin_compose_seq_reset(&wn->khdl);
        goto end;
     }


   /* 3rd/ PopMedia */
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
          {
             DBG("no focused term");
             return;
          }
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

   /* 4th/ Handle key bindings */
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

   /* 5th/ Composing */
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

   /* 6th/ send key to pty */
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
        DBG("ty:%p", ty);
        if (ty && termpty_can_handle_key(ty, &wn->khdl, ev))
          keyin_handle_key_to_pty(ty, ev, alt, shift, ctrl);
     }

   /* 7th: specifics: jump on keypress / flicker on key */
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

   /* Focus In event will handle that */
   if (!tc->is_focused)
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
   DBG("focus tc_child:%p", tc_child);
   tc_child->focus(tc_child, tc);
}

static Eina_Bool
_hide_cursor(void *data)
{
   Win *wn = data;

   wn->hide_cursor_timer = NULL;
   for_each_term_do(wn, &_set_cursor, (void*)"blank");
   return ECORE_CALLBACK_CANCEL;
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

   if (wn->on_popover)
     return;

   if (wn->config->hide_cursor < CONFIG_CURSOR_IDLE_TIMEOUT_MAX)
     {
        if (wn->hide_cursor_timer)
          {
             ecore_timer_interval_set(wn->hide_cursor_timer,
                                      wn->config->hide_cursor);
          }
        else
          {
             for_each_term_do(wn, &_set_cursor, NULL);
             wn->hide_cursor_timer = ecore_timer_add(
                wn->config->hide_cursor, _hide_cursor, wn);
          }
     }

   if (wn->group_input || !tc->is_focused || !wn->config->mouse_over_focus)
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
_config_font_size_set(Term *term, void *data EINA_UNUSED)
{
   Config *config = termio_config_get(term->termio);

   termio_font_size_set(term->termio, config->font.size);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_elm_config_change(void *data, int event EINA_UNUSED, void *info EINA_UNUSED)
{
   Win *wn = data;

   for_each_term_do(wn, &_config_font_size_set, NULL);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_win_is_visible(const Term_Container *tc, const Term_Container *_child EINA_UNUSED)
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

   if (_win_log_dom < 0)
     {
        _win_log_dom = eina_log_domain_register("win", NULL);
        if (_win_log_dom < 0)
          EINA_LOG_CRIT("Could not create logging domain '%s'", "win");
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
   tc->split_direction = _win_split_direction;
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
   tc->detach = NULL;
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

   wn->base = o = elm_layout_add(wn->win);
   elm_object_focus_allow_set(o, EINA_TRUE);
   theme_apply(o, config, "terminology/base", NULL, NULL, EINA_TRUE);
   evas_object_propagate_events_set(o, EINA_FALSE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(wn->conform, o);

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
   evas_object_show(o);
   elm_object_focus_set(wn->base, EINA_TRUE);

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

   if (wn->config->hide_cursor < CONFIG_CURSOR_IDLE_TIMEOUT_MAX)
     {
        wn->hide_cursor_timer = ecore_timer_add(
           wn->config->hide_cursor, _hide_cursor, wn);
     }

   wins = eina_list_append(wins, wn);

   wn->tc.is_focused = EINA_TRUE;

   wn->config_elm_handler = ecore_event_handler_add
     (ELM_EVENT_CONFIG_ALL_CHANGED, _cb_elm_config_change, wn);
   return wn;
}

void
term_close(Evas_Object *win, Evas_Object *term, Eina_Bool hold_if_requested)
{
   Term *tm;
   Term_Container *tc;
   Win *wn = win_evas_object_to_win(win);

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
win_is_group_input(const Win *wn)
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
             elm_layout_signal_emit(term->bg, "focus,in", "terminology");
             elm_layout_signal_emit(term->bg, "grouped,on", "terminology");
             if (term->tab_inactive)
               edje_object_signal_emit(term->tab_inactive,
                                       "grouped,on", "terminology");
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
        /* Better disable it for all of them in case of change of policy
         * between only visible or all.
         * Using the GROUPED_INPUT_TERM_FOREACH macro would miss some terms */
        EINA_LIST_FOREACH(wn->terms, l, term)
          {
             elm_layout_signal_emit(term->bg, "focus,out", "terminology");
             elm_layout_signal_emit(term->bg, "grouped,off", "terminology");
             if (term->tab_inactive)
               edje_object_signal_emit(term->tab_inactive,
                                       "grouped,off", "terminology");
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

   if ((_tab_drag) && (_tab_drag->parent_type == TERM_CONTAINER_TYPE_SPLIT) &&
       (_tab_drag->other == orig))
     {
        _tab_drag->other = new_child;
     }

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
   Term *term = NULL;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split*) tc;

   if (tc->is_focused)
      term = split->last_focus->focused_term_get(split->last_focus);
   return term;
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
   assert ((child == split->tc1) || (child == split->tc2));

   DBG("close");

   if ((_tab_drag) && (_tab_drag->parent_type == TERM_CONTAINER_TYPE_SPLIT) &&
       (_tab_drag->other == child))
     {
        _tab_drag->other = tc->parent;
     }

   top = elm_object_part_content_unset(split->panes, PANES_TOP);
   bottom = elm_object_part_content_unset(split->panes, PANES_BOTTOM);
   evas_object_hide(top);
   evas_object_hide(bottom);

   parent = tc->parent;
   other_child = (child == split->tc1) ? split->tc2 : split->tc1;
   parent->swallow(parent, tc, other_child);

   if (tc->is_focused)
     {
        child->unfocus(child, tc);
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
        /* top to bottom */
        if (!tc->is_focused)
          {
             Term_Container *last_focus = split->last_focus;
             Term_Container *other = (split->tc1 == last_focus) ?
                split->tc2 : split->tc1;

             tc->is_focused = EINA_TRUE;
             other->unfocus(other, tc);
             last_focus->focus(last_focus, tc);
          }
     }
   else
     {
        /* bottom to top */
        if (split->last_focus != relative)
          split->last_focus->unfocus(split->last_focus, tc);
        split->last_focus = relative;
        if (!tc->is_focused)
          {
             /* was not focused, bring focus up */
             tc->is_focused = EINA_TRUE;
             tc->parent->focus(tc->parent, tc);
          }
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
   Eina_Bool child_is_focused;

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

   child_is_focused = child->is_focused;

   /* force unfocus animation */
   tc_solo_new->is_focused = EINA_TRUE;
   tc_solo_new->unfocus(tc_solo_new, NULL);

   tc_split = _split_new(child, tc_solo_new, 0.5, is_horizontal);

   obj_split = tc_split->get_evas_object(tc_split);

   tc_split->is_focused = child_is_focused;
   tc->swallow(tc, child, tc_split);

   if (wn->config->show_tabs)
     {
        _solo_tab_show(tc_solo_new);
     }

   child->unfocus(child, tc_split);
   tc_split->focus(tc_split, tc_solo_new);
   tc_solo_new->focus(tc_solo_new, tc_split);

   evas_object_show(obj_split);
   _focus_validator();
}

static int
_split_split_direction(Term_Container *tc,
                       Term_Container *child_orig,
                       Term_Container *child_new,
                       Split_Direction direction)
{
   Split *split;
   Win *wn;
   Term_Container *child1, *child2, *tc_split;
   Eina_Bool is_horizontal =
      (direction == SPLIT_DIRECTION_LEFT) || (direction == SPLIT_DIRECTION_RIGHT) ?
      EINA_FALSE : EINA_TRUE;

   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   split = (Split *)tc;
   wn = tc->wn;

   if (!_term_container_is_splittable(tc, is_horizontal))
     {
        ERR("term is not splittable");
        return -1;
     }

   if ((direction == SPLIT_DIRECTION_TOP) ||
       (direction == SPLIT_DIRECTION_LEFT))
     {
        child1 = child_new;
        child2 = child_orig;
     }
   else
     {
        child1 = child_orig;
        child2 = child_new;
     }

   if (child_orig == split->tc1)
     elm_object_part_content_unset(split->panes, PANES_TOP);
   else
     elm_object_part_content_unset(split->panes, PANES_BOTTOM);

   wn->tc.unfocus(&wn->tc, NULL); /* unfocus from top */

   tc_split = _split_new(child1, child2, 0.5, is_horizontal);

   if (wn->config->show_tabs)
     {
        if (child_orig->type == TERM_CONTAINER_TYPE_SOLO)
          {
             _solo_tab_show(child_orig);
          }
        _solo_tab_show(child_new);
     }

   tc_split->is_focused = tc->is_focused;
   tc->swallow(tc, child_orig, tc_split);

   child_new->focus(child_new, NULL); /* refocus from bottom */

   return 0;
}

static Eina_Bool
_split_is_visible(const Term_Container *tc,
                  const Term_Container *_child EINA_UNUSED)
{
   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   /* Could return True with the current design because splits are at a higher
    * level than tabs */
   return tc->parent->is_visible(tc->parent, tc);
}

static void
_split_detach(Term_Container *tc, Term_Container *solo_child)
{
   Evas_Object *o;
   assert (tc->type == TERM_CONTAINER_TYPE_SPLIT);
   assert (solo_child->type == TERM_CONTAINER_TYPE_SOLO);

   _split_close(tc, solo_child);
   solo_child->is_focused = EINA_FALSE;

   o = solo_child->get_evas_object(solo_child);
   evas_object_hide(o);
   solo_child->parent = (Term_Container*) solo_child->wn;
}

static Term_Container *
_split_new(Term_Container *tc1, Term_Container *tc2,
           double left_size,
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

   DBG("split new %p 1:%p 2:%p (1 is %sfocused) (2 is %sfocused)", split, tc1, tc2,
       tc1->is_focused ? "" : "not ",
       tc2->is_focused ? "" : "not ");

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
   tc->split_direction = _split_split_direction;
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
   tc->detach = _split_detach;
   tc->title = eina_stringshare_add("Terminology");
   tc->type = TERM_CONTAINER_TYPE_SPLIT;


   tc->parent = NULL;
   tc->wn = tc1->wn;

   tc1->parent = tc2->parent = tc;

   split->tc1 = tc1;
   split->tc2 = tc2;
   split->last_focus = tc2;
   if (tc1->is_focused)
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
   elm_panes_content_left_size_set(o, left_size);

   tc->is_focused = tc1->is_focused | tc2->is_focused;
   assert(!(tc1->is_focused && tc2->is_focused));
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

   elm_win_size_step_set(wn->win, info.step_x, info.step_y);
   if (info.bg_min_w > 0 && info.bg_min_h > 0)
     {
        elm_win_size_base_set(wn->win,
                              info.min_w, info.min_h);
        evas_object_size_hint_min_set(wn->backbg,
                                      info.bg_min_w,
                                      info.bg_min_h);
        if (info.req)
          evas_object_resize(wn->win, info.req_w, info.req_h);
     }
}

void
win_sizing_handle(Win *wn)
{
   if (wn->size_job)
     ecore_job_del(wn->size_job);
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
   Evas_Object *edje_base = elm_layout_edje_get(term->core);

   evas_object_size_hint_min_get(obj, &mw, &mh);
   evas_object_size_hint_request_get(obj, &rw, &rh);
   edje_object_size_min_calc(edje_base, &w, &h);
   evas_object_size_hint_min_set(term->core, w, h);
   edje_object_size_min_calc(term->bg_edj, &w, &h);
   evas_object_size_hint_min_set(term->bg, w, h);
   term->step_x = mw;
   term->step_y = mh;
   term->min_w = w - mw;
   term->min_h = h - mh;
   term->req_w = w - mw + rw;
   term->req_h = h - mh + rh;

   if (term->wn->size_job)
     ecore_job_del(term->wn->size_job);
   term->wn->size_job = ecore_job_add(_size_job, term->wn);
}

void
split_horizontally(Evas_Object *term,
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
split_vertically(Evas_Object *term,
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

static Term *
_tab_item_to_term(const Tab_Item *tab_item)
{
   Solo *solo;

   assert (tab_item->tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)tab_item->tc;
   assert (solo->term);
   assert (solo->term->tab_item == tab_item);
   return solo->term;
}

static void
_cb_tab_activate(void *data,
                 Evas_Object *_obj EINA_UNUSED,
                 const char *_sig EINA_UNUSED,
                 const char *_src EINA_UNUSED)
{
   Tab_Item *tab_item = data;
   Term *term = _tab_item_to_term(tab_item);

   term_focus(term);
}


static void
_tabbar_clear(Term *term)
{
   if (term->tabbar.l.box)
     {
        elm_box_unpack_all(term->tabbar.l.box);
        evas_object_del(term->tabbar.l.box);
        term->tabbar.l.box = NULL;
     }
   if (term->tabbar.r.box)
     {
        elm_box_unpack_all(term->tabbar.r.box);
        evas_object_del(term->tabbar.r.box);
        term->tabbar.r.box = NULL;
     }

   if (term->tab_spacer)
     {
        elm_layout_signal_emit(term->bg, "tabbar,off", "terminology");
        edje_object_message_signal_process(term->bg_edj);
        elm_layout_content_unset(term->bg, "terminology.tab");
        evas_object_del(term->tab_spacer);
        term->tab_spacer = NULL;
     }
   if (term->tab_inactive)
     {
        evas_object_hide(term->tab_inactive);
        edje_object_signal_callback_del(term->tab_inactive,
                                        "tab,activate", "terminology",
                                        _cb_tab_activate);
     }
}

static void
_tab_item_free(Tab_Item *tab_item)
{
   Term *term;

   if (!tab_item)
     return;

   term = _tab_item_to_term(tab_item);
   term->tab_item = NULL;
   if (term->tab_inactive)
     edje_object_signal_callback_del(term->tab_inactive,
                                     "tab,activate", "terminology",
                                     _cb_tab_activate);
   free(tab_item);
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
_tabs_recompute_drag(Tabs *tabs)
{
   Term *term = NULL;
   int n = eina_list_count(tabs->tabs);
   int idx = -1;
   Tab_Item *tab_item;
   Eina_List *l;
   double v1 = 0.0, v2 = 1.0;

   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        idx++;
        if (tab_item == tabs->current)
          {
            term = _tab_item_to_term(tab_item);
            break;
          }
     }
   assert(term != NULL);
   if (n > 1)
     {
        v1 = (double)(idx) / (double)n;
        v2 = (double)(idx+1) / (double)n;
     }
   tabs->v1_orig = v1;
   tabs->v2_orig = v2;
   edje_object_part_drag_value_set(term->bg_edj, "terminology.tabl", v1, 0.0);
   edje_object_part_drag_value_set(term->bg_edj, "terminology.tabr", v2, 0.0);
}

static Eina_Bool
_term_hdrag_on(Term *term, void *data EINA_UNUSED)
{
   elm_layout_signal_emit(term->bg, "hdrag,on", "terminology");

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_term_hdrag_off(Term *term, void *data EINA_UNUSED)
{
   elm_layout_signal_emit(term->bg, "hdrag,off", "terminology");

   return ECORE_CALLBACK_PASS_ON;
}

static void
_tab_drag_disable_anim_over(void)
{
   if ((!_tab_drag) || (!_tab_drag->term_over) ||
       (_tab_drag->split_direction == SPLIT_DIRECTION_NONE))
     return;

   switch (_tab_drag->split_direction)
     {
      case SPLIT_DIRECTION_LEFT:
         elm_layout_signal_emit(_tab_drag->term_over->bg,
                                "drag_left,off", "terminology");
         break;
      case SPLIT_DIRECTION_RIGHT:
         elm_layout_signal_emit(_tab_drag->term_over->bg,
                                "drag_right,off", "terminology");
         break;
      case SPLIT_DIRECTION_TOP:
         elm_layout_signal_emit(_tab_drag->term_over->bg,
                                "drag_top,off", "terminology");
         break;
      case SPLIT_DIRECTION_BOTTOM:
         elm_layout_signal_emit(_tab_drag->term_over->bg,
                                "drag_bottom,off", "terminology");
         break;
      case SPLIT_DIRECTION_TABS:
         elm_layout_signal_emit(_tab_drag->term_over->bg,
                                "drag_over_tabs,off", "terminology");
         break;
      default:
         break;
     }
   elm_layout_signal_emit(_tab_drag->term_over->bg,
                          "hdrag,off", "terminology");
}

static void
_tab_drag_rollback_split(void)
{
   Eina_Bool is_horizontal = _tab_drag->is_horizontal;
   Term_Container *tc_split = NULL;
   Term_Container *child1 = NULL, *child2 = NULL;
   Term_Container *other = _tab_drag->other;
   Term_Container *parent = NULL;
   Win *wn = _tab_drag->term->wn;
   Term_Container *tc_win = (Term_Container*)wn;
   Term_Container *tc = _tab_drag->term->container;

   if (!_tab_drag->other)
     {
        other = wn->child;
     }
   parent = other->parent;

   if (_tab_drag->is_first_child)
     {
        child1 = tc;
        child2 = other;
     }
   else
     {
        child1 = other;
        child2 = tc;
     }

   assert(!tc->is_focused);
   tc_split = _split_new(child1, child2, _tab_drag->left_size, is_horizontal);
   parent->swallow(parent, other, tc_split);
   /* Ensure the other child is unfocused */
   other->unfocus(other, tc_split);
   /* Unfocus from the window down to a single term */
   tc_win->unfocus(tc_win, NULL);
   /* Focus the dragged term, up to the window */
   tc->focus(tc, NULL);
}

static void
_tabs_attach(Term_Container *tc, Term_Container *tc_new)
{
   Tabs *tabs;
   Tab_Item *tab_item;
   Evas_Object *o;
   Evas_Coord x, y, w, h;
   Term_Container *tc_old, *tc_parent;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   assert (tc_new->type == TERM_CONTAINER_TYPE_SOLO);

   tabs = (Tabs*) tc;

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

   tc->swallow(tc, tc_old, tc_new);
   tabs->current = tab_item;

   /* XXX: need to refresh parent */
   tc_parent->swallow(tc_parent, tc, tc);

   if (tc->is_focused)
     tc_new->focus(tc_new, tc);
   else
     tc_new->unfocus(tc_new, tc);
}


static void
_solo_attach(Term_Container *tc, Term_Container *tc_to_add)
{
   assert(tc->type == TERM_CONTAINER_TYPE_SOLO);
   assert(tc_to_add->type == TERM_CONTAINER_TYPE_SOLO);

   if (tc->parent->type != TERM_CONTAINER_TYPE_TABS)
     _tabs_new(tc, tc->parent);

   _tabs_attach(tc->parent, tc_to_add);
   assert(eina_list_count(((Tabs*)(tc->parent))->tabs) > 1);
}

static void
_tab_drag_reparented(void)
{
   assert(_tab_drag);
   _tab_drag->parent_type = TERM_CONTAINER_TYPE_UNKNOWN;
}

static void
_term_on_horizontal_drag(void *data,
                         Evas_Object *o EINA_UNUSED,
                         const char *emission EINA_UNUSED,
                         const char *source EINA_UNUSED)
{
   Eina_List *l, *next, *prev;
   int tab_active_idx;
   int n;
   Tabs *tabs;
   Tab_Item *tab_item;
   Tab_Item *item_moved;
   Term *term = data;
   Term_Container *tc = term->container;
   Term_Container *tc_parent = tc->parent;
   Term *term_moved;
   double v1, v2, m;

   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);
   if (tc->parent->type != TERM_CONTAINER_TYPE_TABS)
     {
        edje_object_part_drag_value_set(term->bg_edj, "terminology.tabl",
                                        0.0, 0.0);
        edje_object_part_drag_value_set(term->bg_edj, "terminology.tabr",
                                        1.0, 0.0);
        return;
     }

   tabs = (Tabs*) tc_parent;
   n = eina_list_count(tabs->tabs);
   if (n <= 1)
     return;

   tab_item = tabs->current;

   _tab_drag_free();

   tab_active_idx = -1;
   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        tab_active_idx++;
        if (tab_item == tabs->current)
          break;
     }
   tab_item = tabs->current;

   edje_object_part_drag_value_get(term->bg_edj, "terminology.tabl",
                                   &v1, NULL);
   edje_object_part_drag_value_get(term->bg_edj, "terminology.tabr",
                                   &v2, NULL);
   m = 1.2 / ((double)(2 * n)); /* 1.2, to have some sense of hysteresis */
   while ((tab_active_idx < n - 1) &&
       ((v2 > (tabs->v2_orig + m)) ||
        ((v1 == v2) && (v2 > tabs->v2_orig))))
     {
        /* To the right */
        l = eina_list_nth_list(tabs->tabs, tab_active_idx);
        next = eina_list_next(l);
        item_moved = next->data;
        term_moved = _tab_item_to_term(item_moved);
        elm_box_unpack(term->tabbar.r.box, term_moved->tab_inactive);
        elm_box_pack_end(term->tabbar.l.box, term_moved->tab_inactive);

        tabs->tabs = eina_list_remove_list(tabs->tabs, l);
        tabs->tabs = eina_list_append_relative_list(tabs->tabs,
                                                    eina_list_data_get(l),
                                                    next);
        _tabs_recompute_drag(tabs);
        tab_active_idx++;
        if (v2 <= tabs->v2_orig)
          return;
     }
   while ((tab_active_idx > 0) &&
       ((v1 < tabs->v1_orig - m) ||
        ((v1 == v2) && v1 < tabs->v1_orig)))
     {
        /* To the left */
        l = eina_list_nth_list(tabs->tabs, tab_active_idx);
        prev = eina_list_prev(l);
        item_moved = prev->data;
        term_moved = _tab_item_to_term(item_moved);
        elm_box_unpack(term->tabbar.l.box, term_moved->tab_inactive);
        elm_box_pack_start(term->tabbar.r.box, term_moved->tab_inactive);

        tabs->tabs = eina_list_remove_list(tabs->tabs, prev);
        tabs->tabs = eina_list_append_relative_list(tabs->tabs,
                                               eina_list_data_get(prev),
                                               l);
        _tabs_recompute_drag(tabs);
        tab_active_idx--;
     }
}

static void
_tabs_set_main_tab(Term *term,
                   Tab_Item *tab_item)
{
   if (!term->tab_spacer)
     {
        Evas_Coord w = 0, h = 0;
        Evas *canvas = evas_object_evas_get(term->bg);

        term->tab_spacer = evas_object_rectangle_add(canvas);
        evas_object_color_set(term->tab_spacer, 0, 0, 0, 0);
        elm_coords_finger_size_adjust(1, &w, 1, &h);
        evas_object_size_hint_min_set(term->tab_spacer, w, h);
        elm_layout_content_set(term->bg, "terminology.tab", term->tab_spacer);
     }
   evas_object_show(term->tab_spacer);
   elm_layout_text_set(term->bg, "terminology.tab.title", tab_item->tc->title);
   elm_layout_signal_emit(term->bg, "tabbar,on", "terminology");
   elm_layout_signal_emit(term->bg, "tab_btn,on", "terminology");
   edje_object_message_signal_process(term->bg_edj);
}

static Evas_Object*
_tab_inactive_get_or_create(Evas *canvas,
                            Term *term,
                            Tab_Item *tab_item)
{
   Evas_Object *o;
   Evas_Coord w, h;

   if (term->tab_inactive)
     {
        o = term->tab_inactive;
        goto created;
     }

   term->tab_inactive = o = edje_object_add(canvas);

   theme_apply(o, term->config, "terminology/tabbar_back",
               NULL, NULL, EINA_FALSE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   edje_object_size_min_calc(o, &w, &h);
   evas_object_size_hint_min_set(o, w, h);
   evas_object_data_set(o, "term", term);

created:
   edje_object_signal_callback_add(o, "tab,activate", "terminology",
                                   _cb_tab_activate, tab_item);

   if (term->missed_bell)
     edje_object_signal_emit(o, "bell", "terminology");
   else
     edje_object_signal_emit(o, "bell,off", "terminology");
   edje_object_part_text_set(o, "terminology.title",
                             tab_item->tc->title);
   return o;
}


static void
_tabbar_fill(Tabs *tabs)
{
   Eina_List *l;
   Eina_Bool after_current = EINA_FALSE;
   Tab_Item *tab_item;
   Term *main_term = _tab_item_to_term(tabs->current);
   Evas *canvas = evas_object_evas_get(main_term->bg);

   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        Term *term = _tab_item_to_term(tab_item);

        if (tab_item == tabs->current)
          {
             _tabs_set_main_tab(term, tab_item);
             assert(main_term == term);
             evas_object_hide(term->tab_inactive);
             edje_object_signal_callback_del(term->tab_inactive,
                                             "tab,activate", "terminology",
                                             _cb_tab_activate);
             after_current = EINA_TRUE;
          }
        else
          {
             Evas_Object *o;

             _tabbar_clear(term);

             o = _tab_inactive_get_or_create(canvas, term, tab_item);
             evas_object_show(o);
             if (after_current)
               elm_box_pack_end(main_term->tabbar.r.box, o);
             else
               elm_box_pack_end(main_term->tabbar.l.box, o);
          }
     }
   _tabs_recompute_drag(tabs);
}

static void
_tabs_get_or_create_boxes(Term *term, Term *src)
{
   Evas_Object *o;

   assert(term->tabbar.l.box == NULL);
   assert(term->tabbar.r.box == NULL);

   /* Left */
   if (src && src->tabbar.l.box)
     {
        term->tabbar.l.box = o = src->tabbar.l.box;
        elm_box_unpack_all(term->tabbar.l.box);
        src->tabbar.l.box = NULL;
     }
   else
     {
        term->tabbar.l.box = o = elm_box_add(term->bg);
        elm_box_horizontal_set(o, EINA_TRUE);
        elm_box_homogeneous_set(o, EINA_TRUE);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
     }
   elm_layout_content_set(term->bg, "terminology.tabl.content", o);
   evas_object_show(o);

   /* Right */
   if (src && src->tabbar.r.box)
     {
        term->tabbar.r.box = o = src->tabbar.r.box;
        elm_box_unpack_all(term->tabbar.r.box);
        src->tabbar.r.box = NULL;
     }
   else
     {
        term->tabbar.r.box = o = elm_box_add(term->bg);
        elm_box_horizontal_set(o, EINA_TRUE);
        elm_box_homogeneous_set(o, EINA_TRUE);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
     }
   elm_layout_content_set(term->bg, "terminology.tabr.content", o);
   evas_object_show(o);
}

static void
_tab_drag_rollback_win(void)
{
   Term *term = _tab_drag->term;
   Win *wn = term->wn;
   Term_Container *tc_win = (Term_Container*)wn;
   Term_Container *tc = term->container;

   if (term->unswallowed)
     elm_layout_content_set(term->bg, "terminology.content", term->core);
   term->unswallowed = EINA_FALSE;

   tc_win->swallow(tc_win, NULL, tc);
   tc_win->unfocus(tc_win, NULL);
   tc->focus(tc, NULL);

   _tab_drag_reparented();
}

static void
_tab_drag_rollback_tabs(void)
{
   Term *term = _tab_drag->term;
   Win *wn = term->wn;
   Term_Container *tc_win = (Term_Container*)wn;
   Term_Container *tc = term->container;
   Term_Container *tc_tabs = _tab_drag->tabs_child;
   int n;
   Tabs *tabs;

   if (tc_tabs->type == TERM_CONTAINER_TYPE_TABS)
     {
        tabs = (Tabs*) tc_tabs;

        /* reinsert at correct place */
        _solo_attach(tabs->current->tc, tc);
     }
   else
     {
        assert(tc_tabs->type == TERM_CONTAINER_TYPE_SOLO);

        /* Create tabs with solo */
        assert(term->tab_item == NULL);
        _solo_attach(_tab_drag->tabs_child, term->container);
        tabs = (Tabs *) term->container->parent;
     }

   n = eina_list_count(tabs->tabs);
   assert (n >= 2);

   /* move tab_item to expected place */
   if (_tab_drag->previous_position < n)
     {
        Tab_Item *tab_item = term->tab_item;

        tabs->tabs = eina_list_remove(tabs->tabs, tab_item);
        if (_tab_drag->previous_position == n-1)
          {
             tabs->tabs = eina_list_append(tabs->tabs, term->tab_item);
          }
        else
          {
             int i = 0;
             Eina_List *l;

             EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
               {
                  if (i == _tab_drag->previous_position)
                    {
                       tabs->tabs = eina_list_prepend_relative_list(tabs->tabs,
                                                                    term->tab_item,
                                                                    l);
                       break;
                    }
                  i++;
               }
          }
     }

   tc_win->unfocus(tc_win, NULL);
   tc->focus(tc, NULL);

   /* Repack in correct boxes */
   elm_box_unpack_all(term->tabbar.l.box);
   elm_box_unpack_all(term->tabbar.r.box);
   _tabbar_fill(tabs);

   _tab_drag_reparented();
}

static void
_tab_drag_rollback(void)
{
   _focus_validator();
   switch (_tab_drag->parent_type)
     {
      case TERM_CONTAINER_TYPE_TABS:
         _tab_drag_rollback_tabs();
         break;
      case TERM_CONTAINER_TYPE_SPLIT:
         _tab_drag_rollback_split();
         break;
      case TERM_CONTAINER_TYPE_WIN:
         _tab_drag_rollback_win();
         break;
      default:
         ERR("invalid parent type:%d", _tab_drag->parent_type);
         abort();
     }
   _focus_validator();
}

static void
_tab_drag_save_state(Term_Container *tc)
{
   assert(_tab_drag);

   _tab_drag->parent_type = tc->parent->type;
   switch (_tab_drag->parent_type)
     {
      case TERM_CONTAINER_TYPE_TABS:
           {
              int position = 0;
              Tabs *tabs;
              Eina_List *l;
              Tab_Item *tab_item;

              tabs = (Tabs*) tc->parent;
              EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
                {
                   if (tab_item->tc == tc)
                     break;
                   position++;
                }
              _tab_drag->previous_position = position;
              _tab_drag->tabs_child = tc->parent;
           }
         break;
      case TERM_CONTAINER_TYPE_SPLIT:
           {
              Split *split;

              split = (Split*)tc->parent;
              _tab_drag->is_horizontal = split->is_horizontal;
              if ((_tab_drag->is_first_child = (split->tc1 == tc)))
                _tab_drag->other = split->tc2;
              else
                _tab_drag->other = split->tc1;
              _tab_drag->left_size = elm_panes_content_left_size_get(
                 split->panes);
           }
         break;
      default:
         ERR("invalid parent type:%d", tc->parent->type);
         abort();
     }
}

static void
_tab_drag_free(void)
{
   if (!_tab_drag)
     return;
   if (_tab_drag->term_over && _tab_drag->term_over->has_bg_cursor)
     {
        elm_object_cursor_unset(_tab_drag->term_over->bg);
        _tab_drag->term_over->has_bg_cursor = EINA_FALSE;
     }
   if (_tab_drag->term->has_bg_cursor)
     {
        elm_object_cursor_unset(_tab_drag->term->bg);
        _tab_drag->term->has_bg_cursor = EINA_FALSE;
     }

   /* free _tab_drag->icon to mark we're freeing _tab_drag */
   evas_object_del(_tab_drag->icon);
   _tab_drag->icon = NULL;

   if (_tab_drag->parent_type != TERM_CONTAINER_TYPE_UNKNOWN)
     _tab_drag_rollback();

   _tab_drag_disable_anim_over();
   for_each_term_do(_tab_drag->term->wn, &_term_hdrag_on, NULL);

   ecore_timer_del(_tab_drag->timer);
   _tab_drag->timer = NULL;

   evas_object_del(_tab_drag->img);
   _tab_drag->img = NULL;

   term_unref(_tab_drag->term);
   free(_tab_drag);
   _tab_drag = NULL;
}

static void
_tab_drag_reinsert(Term *term, double mid)
{
   Term_Container *tc = term->container;
   Term_Container *tc_parent = tc->parent;
   Tabs *tabs;

   tc = term->container;
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);

   if (tc_parent->type != TERM_CONTAINER_TYPE_TABS)
     return;

   tabs = (Tabs*) tc_parent;

   edje_object_part_drag_value_set(term->bg_edj, "terminology.tabl", mid, 0.0);
   edje_object_part_drag_value_set(term->bg_edj, "terminology.tabr", mid, 0.0);

   _term_on_horizontal_drag(term, NULL, NULL, NULL);
   /* In case there is no drag, need to recompute to something valid */
   _tabs_recompute_drag(tabs);
}

static void
_tab_reorg(Term *term, Term *to_term, Evas_Coord mx)
{
   Term_Container *tc_orig = term->container;
   Term_Container *to_tc = to_term->container;

   assert(tc_orig->type == TERM_CONTAINER_TYPE_SOLO);
   assert(to_tc->type == TERM_CONTAINER_TYPE_SOLO);

   if (_tab_drag->split_direction == SPLIT_DIRECTION_TABS)
     {
        Evas_Coord x = 0, w = 0;
        double mid;

        edje_object_part_geometry_get(term->bg_edj, "terminology.tabregion",
                                      &x, NULL, &w, NULL);

        mid = (double)(mx - x) / (double)w;

        _solo_attach(to_tc, tc_orig);

        /* reinsert at correct place */
        _tab_drag_reparented();
        _tab_drag_reinsert(term, mid);
        return;
     }

   to_tc->split_direction(to_tc, to_tc, tc_orig, _tab_drag->split_direction);
   _tab_drag_reparented();
}

static void
_tab_drag_stop(void)
{
   Evas_Coord mx = 0, my = 0;
   Win *wn;
   Term_Container *tc_wn;
   Term *term;
   Term *term_at_coords;

   assert(_tab_drag);

   term = _tab_drag->term;
   wn = term->wn;
   tc_wn = (Term_Container*) wn;

   evas_pointer_canvas_xy_get(_tab_drag->e, &mx, &my);
   term_at_coords = tc_wn->find_term_at_coords(tc_wn, mx, my);
   if (!term_at_coords)
     goto end;

   evas_object_image_source_visible_set(_tab_drag->img, EINA_TRUE);
   if (term->has_bg_cursor)
     {
        elm_object_cursor_unset(term->bg);
        term->has_bg_cursor = EINA_FALSE;
     }
   elm_layout_content_unset(_tab_drag->icon, "terminology.content");
   elm_layout_content_set(term->bg, "terminology.content", term->core);
   term->unswallowed = EINA_FALSE;
   evas_object_show(term->core);
   evas_object_show(term->bg);

   if (term_at_coords == term)
     {
        Evas_Coord x = 0, y = 0, w = 0, h = 0, off_x = 0, off_y = 0;
        double mid;

        /* Reinsert in same set of Tabs or same "tab" (could be a split) */
        evas_object_geometry_get(term->bg_edj, &off_x, &off_y, NULL, NULL);
        edje_object_part_geometry_get(term->bg_edj, "terminology.tabregion",
                                      &x, &y, &w, &h);
        if (!ELM_RECTS_INTERSECT(x,y,w,h, mx,my,1,1))
          goto end;

        mid = (double)(mx - x) / (double)w;
        _tab_drag_reparented();
        _tab_drag_reinsert(term, mid);
     }
   else if (_tab_drag->split_direction != SPLIT_DIRECTION_NONE)
     {
        /* Move to different set of Tabs */
        _tab_reorg(term, term_at_coords, mx);
     }

end:
   _tab_drag_free();
}

static void
_term_on_drag_stop(void *data,
                   Evas_Object *o EINA_UNUSED,
                   const char *emission EINA_UNUSED,
                   const char *source EINA_UNUSED)
{
   Term *term = data;
   Term_Container *tc;

   if (_tab_drag && _tab_drag->icon)
     {
        _tab_drag_stop();
        return;
     }
   _tab_drag_free();

   tc = term->container;
   assert (tc->type == TERM_CONTAINER_TYPE_SOLO);

   if (tc->parent->type == TERM_CONTAINER_TYPE_TABS)
     {
        Term_Container *tc_parent = tc->parent;
        Tabs *tabs = (Tabs*) tc_parent;

        edje_object_part_drag_value_set(term->bg_edj, "terminology.tabl",
                                        tabs->v1_orig, 0.0);
        edje_object_part_drag_value_set(term->bg_edj, "terminology.tabr",
                                        tabs->v2_orig, 0.0);
     }
   _focus_validator();
}


static void
_tabs_drag_mouse_move(
   void *data EINA_UNUSED,
   Evas_Object *obj EINA_UNUSED,
   const char *emission EINA_UNUSED,
   const char *source EINA_UNUSED)
{
   Evas_Coord x, y, w, h, off_x, off_y, mx, my;
   Win *wn;
   Term_Container *tc_wn;
   Term *term_at_coords;
   Split_Direction split_direction = SPLIT_DIRECTION_NONE;

   if (!_tab_drag || !_tab_drag->icon)
     return;

   wn = _tab_drag->term->wn;
   tc_wn = (Term_Container*) wn;

   evas_object_geometry_get(_tab_drag->icon, NULL, NULL, &w, &h);
   evas_pointer_canvas_xy_get(_tab_drag->e, &mx, &my);
   x = (mx - (w/2));
   y = (my - (h/2));
   evas_object_move(_tab_drag->icon, x, y);

   term_at_coords = tc_wn->find_term_at_coords(tc_wn, mx, my);
   if (!term_at_coords)
     return;
   evas_object_geometry_get(term_at_coords->bg_edj, &off_x, &off_y, NULL, NULL);

   edje_object_part_geometry_get(term_at_coords->bg_edj, "terminology.tabregion",
                                 &x, &y, &w, &h);
   if (ELM_RECTS_INTERSECT(x+off_x, y+off_y, w, h, mx, my, 1, 1))
     {
        split_direction = SPLIT_DIRECTION_TABS;
        goto found;
     }

   edje_object_part_geometry_get(term_at_coords->bg_edj, "drag_left_outline",
                                 &x, &y, &w, &h);
   if (ELM_RECTS_INTERSECT(x+off_x, y+off_y, w, h, mx, my, 1, 1))
     {
        split_direction = SPLIT_DIRECTION_LEFT;
        goto found;
     }

   edje_object_part_geometry_get(term_at_coords->bg_edj, "drag_right_outline",
                                 &x, &y, &w, &h);
   if (ELM_RECTS_INTERSECT(x+off_x, y+off_y, w, h, mx, my, 1, 1))
     {
        split_direction = SPLIT_DIRECTION_RIGHT;
        goto found;
     }

   edje_object_part_geometry_get(term_at_coords->bg_edj, "drag_top_outline",
                                 &x, &y, &w, &h);
   if (ELM_RECTS_INTERSECT(x+off_x, y+off_y, w, h, mx, my, 1, 1))
     {
        split_direction = SPLIT_DIRECTION_TOP;
        goto found;
     }

   edje_object_part_geometry_get(term_at_coords->bg_edj, "drag_bottom_outline",
                                 &x, &y, &w, &h);
   if (ELM_RECTS_INTERSECT(x+off_x, y+off_y, w, h, mx, my, 1, 1))
     {
        split_direction = SPLIT_DIRECTION_BOTTOM;
        goto found;
     }

found:
   if ((_tab_drag->term_over != NULL) &&
       ((_tab_drag->term_over != term_at_coords) ||
        (_tab_drag->split_direction != split_direction)))
     {
        _tab_drag_disable_anim_over();
     }
   if ((split_direction != SPLIT_DIRECTION_NONE) &&
       ((_tab_drag->term_over != term_at_coords) ||
        (_tab_drag->split_direction != split_direction)))
     {
        switch (split_direction)
          {
           case SPLIT_DIRECTION_LEFT:
              elm_layout_signal_emit(term_at_coords->bg,
                                     "drag_left,on", "terminology");
              break;
           case SPLIT_DIRECTION_RIGHT:
              elm_layout_signal_emit(term_at_coords->bg,
                                     "drag_right,on", "terminology");
              break;
           case SPLIT_DIRECTION_TOP:
              elm_layout_signal_emit(term_at_coords->bg,
                                     "drag_top,on", "terminology");
              break;
           case SPLIT_DIRECTION_BOTTOM:
              elm_layout_signal_emit(term_at_coords->bg,
                                     "drag_bottom,on", "terminology");
              break;
           case SPLIT_DIRECTION_TABS:
              elm_layout_signal_emit(term_at_coords->bg,
                                     "drag_over_tabs,on", "terminology");
           default:
              break;
          }
     }
   _tab_drag->term_over = term_at_coords;
   _tab_drag->split_direction = split_direction;
}

static Eina_Bool
_tab_drag_start(void *data EINA_UNUSED)
{
   /* Start icons animation before actually drag-starts */
   Evas_Coord mx, my, w, h, ch_w, ch_h, core_w, core_h;
   Term *term = _tab_drag->term;
   Evas_Object *o = elm_layout_add(term->bg);
   Evas_Object *img;
   Term_Container *tc = term->container;
   float ratio;

   if (!term->container)
     {
        _tab_drag_free();
        return ECORE_CALLBACK_CANCEL;
     }

   for_each_term_do(_tab_drag->term->wn, &_term_hdrag_off, NULL);

   _tab_drag->icon = o;
   theme_apply(o, term->config, "terminology/tab_drag_thumb",
               NULL, NULL, EINA_TRUE);
   elm_layout_text_set(o, "terminology.title",
                       term->container->title);
   elm_layout_content_unset(term->bg, "terminology.content");
   term->unswallowed = EINA_TRUE;
   img = evas_object_image_filled_add(evas_object_evas_get(term->core));
   evas_object_lower(term->core);
   evas_object_move(term->core, -9999, -9999);
   evas_object_show(term->core);
   evas_object_clip_unset(term->core);
   evas_object_image_source_set(img, term->core);
   evas_object_geometry_get(term->core, NULL, NULL, &core_w, &core_h);
   evas_object_resize(img, core_w, core_h);
   _tab_drag->img = img;
   elm_layout_content_set(o, "terminology.content", img);
   evas_object_size_hint_min_get(term->core, &ch_w, &ch_h);

   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

   w = ch_w * 10;
   h = ch_h * 5;
   ratio = (float) core_w / (float) core_h;
   if (h * ratio > w)
     h = w / ratio;
   else
     w = h * ratio;
   evas_object_resize(o, w, h);
   evas_pointer_canvas_xy_get(_tab_drag->e, &mx, &my);
   evas_object_move(_tab_drag->icon, mx - w/2, my - h/2);
   evas_object_raise(o);
   elm_object_cursor_set(term->bg, ELM_CURSOR_HAND2);
   term->has_bg_cursor = EINA_TRUE;
   evas_object_show(o);

   _tab_drag_save_state(tc);
   DBG("detaching %p from %p", tc, tc->parent);
   tc->parent->detach(tc->parent, tc);
   assert(term->tab_item == NULL);
   _focus_validator();
   assert(!tc->is_focused);

   _tab_drag->timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_tabs_mouse_down(
   void *data,
   Evas_Object *obj EINA_UNUSED,
   const char *emission EINA_UNUSED,
   const char *source EINA_UNUSED)
{
   /* Launch a timer to start drag animation */
   Term *term = data;
   Evas_Coord mx = 0, my = 0;

   assert(term->container != NULL);
   assert(_tab_drag == NULL);

   _tab_drag = calloc(1, sizeof(*_tab_drag));
   if (!_tab_drag)
     return;

   _tab_drag->e = evas_object_evas_get(term->bg);
   evas_pointer_canvas_xy_get(_tab_drag->e, &mx, &my);

   term_ref(term);

   _tab_drag->mdx = mx;
   _tab_drag->mdy = my;
   _tab_drag->term = term;
   _tab_drag->timer = ecore_timer_add(DRAG_TIMEOUT,
                                      _tab_drag_start, NULL);
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
   Win *wn = tc->wn;
   Evas_Object *selector = tabs->selector;
   Evas_Object *selector_bg = tabs->selector_bg;

   if (!tabs->selector)
     return;

   EINA_LIST_FOREACH(wn->terms, l, term)
     {
        if (term->unswallowed)
          {
             evas_object_image_source_visible_set(term->sel, EINA_TRUE);
             elm_layout_content_set(term->bg, "terminology.content", term->core);
             term->unswallowed = EINA_FALSE;
             evas_object_show(term->core);
          }
     }

   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        tab_item->selector_entry = NULL;
        if (tab_item->tc->is_focused)
          tab_item->tc->unfocus(tab_item->tc, tc);
     }

   evas_object_smart_callback_del_full(selector, "selected",
                                  _tabs_selector_cb_selected, tabs);
   evas_object_smart_callback_del_full(selector, "exit",
                                  _tabs_selector_cb_exit, tabs);
   evas_object_smart_callback_del_full(selector, "ending",
                                  _tabs_selector_cb_ending, tabs);


   tabs->selector = NULL;
   tabs->selector_bg = NULL;

   /* XXX: reswallow in parent */
   tc->parent->swallow(tc->parent, tc, tc);
   solo = (Solo*)tabs->current->tc;
   term = solo->term;
   _tabbar_clear(term);

   /* Restore -> recreate the whole tabbar */
   _tabs_recreate(tabs);
   tabs->current->tc->unfocus(tabs->current->tc, tabs->current->tc);
   tabs->current->tc->focus(tabs->current->tc, tabs->current->tc);

   elm_object_focus_set(selector, EINA_FALSE);

   evas_object_del(selector);
   evas_object_del(selector_bg);
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
   Edje_Message_Int msg;

   if (tabs->selector_bg)
     return;

   o = tc->get_evas_object(tc);
   evas_object_geometry_get(o, &x, &y, &w, &h);

   tabs->selector_bg = edje_object_add(evas_object_evas_get(tc->wn->win));
   theme_apply(tabs->selector_bg, wn->config, "terminology/sel/base",
               NULL, NULL, EINA_FALSE);

   evas_object_geometry_set(tabs->selector_bg, x, y, w, h);
   evas_object_hide(o);

   if (wn->config->translucent)
     msg.val = wn->config->opacity;
   else
     msg.val = 100;
   edje_object_message_send(tabs->selector_bg, EDJE_MESSAGE_INT, 1, &msg);
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

        elm_layout_content_unset(term->bg, "terminology.content");
        term->unswallowed = EINA_TRUE;
        img = evas_object_image_filled_add(evas_object_evas_get(wn->win));
        o = term->core;
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
   if (config->show_tabs)
     {
        info->step_x = 1;
        info->step_y = 1;
     }
}

static Eina_List *
_tab_item_find(const Tabs *tabs, const Term_Container *child,
               int *pos)
{
   Eina_List *l;
   Tab_Item *tab_item;
   int i = 0;

   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        if (tab_item->tc == child)
          {
             if (pos)
               *pos = i;
             return l;
          }
        i++;
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
   int pos = 0;

   /* TODO: figure out whether to move position if tab_drag */

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)tc;

   tc_parent = tc->parent;

   l = _tab_item_find(tabs, child, &pos);
   item = l->data;

   next = eina_list_next(l);
   if (!next)
     next = tabs->tabs;

   tabs->tabs = eina_list_remove_list(tabs->tabs, l);

   next_item = next->data;
   next_child = next_item->tc;
   assert (next_child->type == TERM_CONTAINER_TYPE_SOLO);

   assert (child->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)child;
   term = solo->term;
   child->unfocus(child, tc);

   elm_layout_signal_emit(term->bg, "tabcount,off", "terminology");
   elm_layout_signal_emit(term->bg, "tab_btn,off", "terminology");

   count = eina_list_count(tabs->tabs);
   if (count == 1)
     {
        Term *next_term;
        Solo *next_solo;
        Config *config;

        assert (next_child->type == TERM_CONTAINER_TYPE_SOLO);
        next_solo = (Solo*)next_child;
        next_term = next_solo->term;
        assert(next_term != term);
        assert(tc != next_child);
        config = next_term->config;

        _tabbar_clear(term);

        evas_object_del(next_term->tab_spacer);
        next_term->tab_spacer = NULL;
        if (next_term->tab_inactive)
          {
             evas_object_hide(next_term->tab_inactive);
             edje_object_signal_callback_del(next_term->tab_inactive,
                                             "tab,activate", "terminology",
                                             _cb_tab_activate);
          }
        elm_layout_signal_emit(next_term->bg, "tabcount,off", "terminology");
        elm_layout_signal_emit(next_term->bg, "tab_btn,off", "terminology");

        if (tabs->selector)
          _tabs_restore(tabs);

        if (config->show_tabs)
          _solo_tab_show(next_child);

        eina_stringshare_del(tc->title);

        tc_parent->swallow(tc_parent, tc, next_child);
        if (tc->is_focused)
          next_child->focus(next_child, tc);

        if ((_tab_drag) && (_tab_drag->parent_type == TERM_CONTAINER_TYPE_TABS)
            && (_tab_drag->tabs_child == tc))
          {
             _tab_drag->tabs_child = next_child;
             _solo_tab_show(next_child);
          }

        _tab_item_free(item);
        _tab_item_free(next_item);
        EINA_LIST_FREE(tabs->tabs, item) {}
        free(tc);

        return;
     }

   if ((_tab_drag) && (_tab_drag->parent_type == TERM_CONTAINER_TYPE_TABS)
       && (_tab_drag->tabs_child == tc))
     {
        if (pos < _tab_drag->previous_position)
          _tab_drag->previous_position--;
     }

     if (item->tc->selector_img)
       {
          Evas_Object *o;
          o = item->tc->selector_img;
          item->tc->selector_img = NULL;
          evas_object_del(o);
       }

     count--;

     if (item == tabs->current)
       {
          tc->swallow(tc, child, next_child);
          if (tc->is_focused)
            next_child->focus(next_child, tc);
          _tab_item_free(item);
          assert(tabs->current != item);
          return;
       }
     _tab_item_free(item);

     _tabs_recompute_drag(tabs);
}

static void
_tabs_update(Term_Container *tc)
{
   Tabs *tabs;
   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)tc;

   /* Update -> recreate the whole tabbar */
   _tabs_recreate(tabs);
}

static Term *
_tabs_term_next(const Term_Container *tc, const Term_Container *child)
{
   Tabs *tabs;
   Tab_Item *tab_item;
   Eina_List *l;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*)tc;
   l = _tab_item_find(tabs, child, NULL);
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
   l = _tab_item_find(tabs, child, NULL);
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
_tabcount_refresh(Tabs *tabs)
{
   Eina_List *l;
   Tab_Item *tab_item;
   Term *main_term = _tab_item_to_term(tabs->current);
   Evas *canvas = evas_object_evas_get(main_term->bg);
   char buf[32], bufmissed[32];
   int n = eina_list_count(tabs->tabs);
   Evas_Coord w = 0, h = 0;
   unsigned int missed = 0;
   int i;

   if (n <= 0)
     {
        ERR("no tab");
        return;
     }

   buf[0] = '\0';
   i = 0;
   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        Solo *solo;
        Term *term;

        i++;
        assert (tab_item->tc->type == TERM_CONTAINER_TYPE_SOLO);
        solo = (Solo*) tab_item->tc;
        term = solo->term;

        if (term->tab_inactive)
          {
             evas_object_hide(term->tab_inactive);
             edje_object_signal_callback_del(term->tab_inactive,
                                             "tab,activate", "terminology",
                                             _cb_tab_activate);
          }

        if (tabs->current == tab_item)
          {
             snprintf(buf, sizeof(buf), "%i/%i", i, n);
          }
        else if (term->missed_bell)
          missed++;
     }
   if (missed > 0)
     snprintf(bufmissed, sizeof(bufmissed), "%i", missed);
   else
     bufmissed[0] = '\0';

   if (!main_term->tab_spacer)
     {
        main_term->tab_spacer = evas_object_rectangle_add(canvas);
        evas_object_color_set(main_term->tab_spacer, 0, 0, 0, 0);
     }
   elm_coords_finger_size_adjust(1, &w, 1, &h);
   evas_object_size_hint_min_set(main_term->tab_spacer, w, h);

   elm_layout_content_set(main_term->bg, "terminology.tabcount.control",
                          main_term->tab_spacer);
   elm_layout_text_set(main_term->bg, "terminology.tabcount.label", buf);
   elm_layout_text_set(main_term->bg, "terminology.tabmissed.label", bufmissed);
   elm_layout_signal_emit(main_term->bg, "tabcount,on", "terminology");
   _tabbar_clear(main_term);
   if (missed > 0)
     elm_layout_signal_emit(main_term->bg, "tabmissed,on", "terminology");
   else
     elm_layout_signal_emit(main_term->bg, "tabmissed,off", "terminology");
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
   Term *term_orig, *term_new;
   Solo *solo_orig, *solo_new;
   Term_Container *tc_parent = tc->parent;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*) tc;

   l = _tab_item_find(tabs, new_child, NULL);
   tab_item = l->data;

   if (tabs->selector)
     {
        Evas_Object *img = tab_item->tc->selector_img;
        evas_object_image_source_set(img,
                                     new_child->get_evas_object(new_child));
        evas_object_data_set(img, "tc", new_child);
        if (tab_item->selector_entry)
          sel_entry_update(tab_item->selector_entry);
        return;
     }
   if (orig == new_child)
     {
        assert(tabs->current == tab_item);
        return;
     }

   /* Occurs when closing current tab */
   if (tc->is_focused)
     tabs->current->tc->unfocus(tabs->current->tc, tc);

   assert (orig->type == TERM_CONTAINER_TYPE_SOLO);
   solo_orig = (Solo*)orig;
   term_orig = solo_orig->term;
   assert (new_child->type == TERM_CONTAINER_TYPE_SOLO);
   solo_new = (Solo*)new_child;
   term_new = solo_new->term;

   tabs->current = tab_item;
   /* Remove tab effects from the previous focused tab */
   elm_layout_signal_emit(term_orig->bg, "tabcount,off", "terminology");

   if (term_new->config->show_tabs)
     {
        _tabs_get_or_create_boxes(term_new, term_orig);
        assert(term_new == _tab_item_to_term(tabs->current));
        _tabbar_fill(tabs);
     }
   else
     {
        _tabcount_refresh(tabs);
     }

    o = orig->get_evas_object(orig);
    evas_object_geometry_get(o, &x, &y, &w, &h);
    evas_object_hide(o);
    o = new_child->get_evas_object(new_child);
    evas_object_geometry_set(o, x, y, w, h);
    evas_object_show(o);
    /* XXX: need to refresh */
    tc_parent->swallow(tc_parent, tc, tc);
}


static void
_tab_new_cb(void *data,
            Evas_Object *_obj EINA_UNUSED,
            void *_event_info EINA_UNUSED)
{
   Tabs *tabs = data;
   Term_Container *tc = (Term_Container*) tabs,
                  *tc_new;
   Term *tm_new;
   Win *wn = tc->wn;
   char *wdir = NULL;
   char buf[PATH_MAX];

   // copy the current path to wdir if we should change the directory,
   // passing wdir NULL otherwise:
   if (wn->config->changedir_to_current)
     {
        Term *tm;
        Term_Container *tc_old = tabs->current->tc;
        tm = tc_old->term_first(tc_old);

        if (tm && termio_cwd_get(tm->termio, buf, sizeof(buf)))
          wdir = buf;
     }

   tm_new = term_new(wn, wn->config,
                     NULL, wn->config->login_shell, wdir,
                     80, 24, EINA_FALSE, NULL);
   tc_new = _solo_new(tm_new, wn);
   evas_object_data_set(tm_new->termio, "sizedone", tm_new->termio);

   _tabs_attach(tc, tc_new);
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
   _focus_validator();
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
main_new(Evas_Object *term)
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

        l = _tab_item_find(tabs, relative, NULL);
        if (!l)
          return;

        tc->is_focused = EINA_TRUE;

        tab_item = l->data;
        if (tab_item != tabs->current)
          {
             Config *config = tc->wn->config;
             tabs->current->tc->unfocus(tabs->current->tc, tc);

             if (config->tab_zoom >= 0.01 && !config->show_tabs)
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
           Term_Container *child)
{
   Tabs *tabs;
   Term *term;
   Solo *solo;
   char bufmissed[32];
   Eina_List *l;
   Tab_Item *tab_item;
   int missed = 0;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*) tc;

   assert (child->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)child;
   term = solo->term;

   if (tc->is_focused && child->is_focused)
     return;

   if (term->tab_inactive && term->missed_bell)
     edje_object_signal_emit(term->tab_inactive, "bell", "terminology");

   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        assert (tab_item->tc->type == TERM_CONTAINER_TYPE_SOLO);
        solo = (Solo*) tab_item->tc;
        term = solo->term;

        if (term->missed_bell)
          missed++;
     }

   tab_item = tabs->current;
   assert (tab_item->tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)tab_item->tc;
   term = solo->term;
   if (missed > 0)
     {
        snprintf(bufmissed, sizeof(bufmissed), "%i", missed);
        elm_layout_text_set(term->bg, "terminology.tabmissed.label", bufmissed);
        elm_layout_signal_emit(term->bg, "tabmissed,on", "terminology");
     }
   else
     elm_layout_signal_emit(term->bg, "tabmissed,off", "terminology");
   edje_object_message_signal_process(term->bg_edj);

   tc->parent->bell(tc->parent, tc);
}

static void
_tabs_set_title(Term_Container *tc, Term_Container *child,
                const char *title)
{
   Tabs *tabs;
   Tab_Item *tab_item;
   Eina_List *l;
   Solo *solo;
   Term *term;

   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*) tc;

   l = _tab_item_find(tabs, child, NULL);
   if (!l)
     return;
   tab_item = l->data;

   if (tabs->selector && tab_item->selector_entry)
     {
        sel_entry_title_set(tab_item->selector_entry, title);
     }

   assert (tab_item->tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)tab_item->tc;
   term = solo->term;

   if (tab_item == tabs->current)
     {
        eina_stringshare_del(tc->title);
        tc->title =  eina_stringshare_ref(title);
        tc->parent->set_title(tc->parent, tc, title);

        if (term->config->show_tabs)
          {
             elm_layout_text_set(term->bg, "terminology.tab.title", title);
          }
     }
   else
     {
        if (term->tab_inactive)
          edje_object_part_text_set(term->tab_inactive,
                                    "terminology.title",
                                    title);
     }
}

static void
_tabs_recreate(Tabs *tabs)
{
   Eina_List *l;
   Solo *solo;
   Term *term;
   Tab_Item *tab_item;
   Evas_Coord w = 0, h = 0;
   int missed = 0;
   int n = eina_list_count(tabs->tabs);

   if (n <= 0)
     {
        ERR("no tab");
        return;
     }

   EINA_LIST_FOREACH(tabs->tabs, l, tab_item)
     {
        assert (tab_item->tc->type == TERM_CONTAINER_TYPE_SOLO);
        solo = (Solo*) tab_item->tc;
        term = solo->term;

        if (term->tab_inactive)
          {
             evas_object_hide(term->tab_inactive);
             edje_object_signal_callback_del(term->tab_inactive,
                                             "tab,activate", "terminology",
                                             _cb_tab_activate);
          }

        if (term->missed_bell)
          missed++;
     }

   tab_item = tabs->current;
   assert (tab_item->tc->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)tab_item->tc;
   term = solo->term;


   // this is all below just for tab bar at the top
   if (term->config->show_tabs)
     {
        _tabbar_clear(term);

        if (!term->tab_spacer)
          {
             term->tab_spacer = evas_object_rectangle_add(evas_object_evas_get(term->bg));
             evas_object_color_set(term->tab_spacer, 0, 0, 0, 0);
          }
        elm_coords_finger_size_adjust(1, &w, 1, &h);
        evas_object_size_hint_min_set(term->tab_spacer, w, h);
        elm_layout_content_set(term->bg, "terminology.tab_btn",
                               term->tab_spacer);

        elm_layout_signal_emit(term->bg, "tabcount,off", "terminology");

        elm_layout_signal_emit(term->bg, "tabbar,on", "terminology");
        elm_layout_signal_emit(term->bg, "tab_btn,on", "terminology");
        assert(term == _tab_item_to_term(tabs->current));
        _tabs_get_or_create_boxes(term, NULL);
        _tabbar_fill(tabs);
        edje_object_message_signal_process(term->bg_edj);
     }
   else
     {
        _tabcount_refresh(tabs);
     }
}

static Tab_Item*
tab_item_new(Tabs *tabs, Term_Container *child)
{
   Tab_Item *tab_item;
   Solo *solo;
   Term *term;

   tab_item = calloc(1, sizeof(Tab_Item));
   if (!tab_item)
     return NULL;
   tab_item->tc = child;
   assert(child != NULL);
   assert(child->type == TERM_CONTAINER_TYPE_SOLO);
   solo = (Solo*)child;
   term = solo->term;
   assert(term->tab_item == NULL);
   term->tab_item = tab_item;

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

static int
_tabs_split_direction(Term_Container *tc,
                      Term_Container *child_orig EINA_UNUSED,
                      Term_Container *child_new,
                      Split_Direction direction)
{
   return tc->parent->split_direction(tc->parent, tc, child_new, direction);
}

static Eina_Bool
_tabs_is_visible(const Term_Container *tc, const Term_Container *child)
{
   Tabs *tabs;
   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   tabs = (Tabs*) tc;
   return child == tabs->current->tc;
}


static void
_tabs_detach(Term_Container *tc, Term_Container *solo_child)
{
   Evas_Object *o;
   assert (tc->type == TERM_CONTAINER_TYPE_TABS);
   assert (solo_child->type == TERM_CONTAINER_TYPE_SOLO);

   _tabs_close(tc, solo_child);
   solo_child->is_focused = EINA_FALSE;

   o = solo_child->get_evas_object(solo_child);
   evas_object_hide(o);
   solo_child->parent = (Term_Container*) solo_child->wn;
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
   tc->split_direction = _tabs_split_direction;
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
   tc->detach = _tabs_detach;
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
/* {{{ Popup Media */
struct Pop_Media {
     const char *src;
     Eina_Bool from_user_interaction;
};

Eina_Bool
term_has_popmedia(const Term *term)
{
   return !!term->popmedia;
}

void
term_popmedia_close(Term *term)
{
   if (term->popmedia)
     elm_layout_signal_emit(term->bg, "popmedia,off", "terminology");
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
   elm_layout_signal_emit(term->bg, "popmedia,off", "terminology");
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
        if (term->popmedia)
            media_play_set(term->popmedia, EINA_FALSE);
        elm_layout_signal_emit(term->bg, "popmedia,off", "terminology");
     }
}

static void
_popmedia_queue_free(Term *term)
{
   struct Pop_Media *pm;
   if (!term->popmedia_queue)
     return;

   EINA_LIST_FREE(term->popmedia_queue, pm)
     {
        eina_stringshare_del(pm->src);
        free(pm);
     }
}

static void
_popmedia_queue_add(Term *term, const char *src,
                    Eina_Bool from_user_interaction)
{
   struct Pop_Media *pm = calloc(1, sizeof(struct Pop_Media));

   if (!pm)
     return;

   pm->src = eina_stringshare_add(src);
   pm->from_user_interaction = from_user_interaction;

   term->popmedia_queue = eina_list_append(term->popmedia_queue, pm);
   if (!term->popmedia)
     _popmedia_queue_process(term);
}

static void
_popmedia_now(Term *term, const char *src,
              Eina_Bool from_user_interaction)
{
   struct Pop_Media *pm;

   /* Flush queue */
   EINA_LIST_FREE(term->popmedia_queue, pm)
     {
        eina_stringshare_del(pm->src);
     }
   elm_layout_signal_emit(term->bg, "popmedia,off", "terminology");

   _popmedia_queue_add(term, src, from_user_interaction);
}


static void
_popmedia_show(Term *term, const char *src, Media_Type type)
{
   Evas_Object *o;
   Config *config = termio_config_get(term->termio);

   assert(!term->popmedia);
   EINA_SAFETY_ON_NULL_RETURN(config);
   termio_mouseover_suspend_pushpop(term->termio, 1);
   term->popmedia = o = media_add(win_evas_object_get(term->wn),
                                  src, config, MEDIA_POP, type);
   term->popmedia_deleted = EINA_FALSE;
   evas_object_smart_callback_add(o, "loop", _cb_media_loop, term);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _cb_popmedia_del, term);
   elm_layout_content_set(term->bg, "terminology.popmedia", o);
   evas_object_show(o);
   term->poptype = type;
   switch (type)
     {
      case MEDIA_TYPE_IMG:
         elm_layout_signal_emit(term->bg, "popmedia,image", "terminology");
         break;
      case MEDIA_TYPE_SCALE:
         elm_layout_signal_emit(term->bg, "popmedia,scale", "terminology");
         break;
      case MEDIA_TYPE_EDJE:
         elm_layout_signal_emit(term->bg, "popmedia,edje", "terminology");
         break;
      case MEDIA_TYPE_MOV:
         elm_layout_signal_emit(term->bg, "popmedia,movie", "terminology");
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
     Eina_Bool fallback_allowed;
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
        elm_layout_signal_emit(ty_head->term->bg, "done", "terminology");
        term_unref(ty_head->term);
     }
   ecore_timer_del(ty_head->timeout);

   free(ty_head);
}

static Eina_Bool
_media_http_head_timeout(void *data)
{
   Ty_Http_Head *ty_head = data;

   ty_head->timeout = NULL;
   if (ty_head->fallback_allowed)
     {
        media_unknown_handle(ty_head->handler, ty_head->src);
     }
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
   if (ty_head->fallback_allowed)
     {
        media_unknown_handle(ty_head->handler, ty_head->src);
     }
   _ty_http_head_delete(ty_head);
   return EINA_TRUE;
}
#endif

static void
_popmedia_unknown(Term *term, const char *src, Eina_Bool from_user_interaction)
{
   Media_Type type;
   Config *config = termio_config_get(term->termio);

   type = media_src_type_get(src);
   if (type == MEDIA_TYPE_UNKNOWN)
     {
#ifdef HAVE_ECORE_CON_URL_HEAD
        Ty_Http_Head *ty_head = calloc(1, sizeof(Ty_Http_Head));
        if (!ty_head)
          return;

        if (config->helper.local.general && config->helper.local.general[0])
          {
             ty_head->handler = eina_stringshare_add(config->helper.local.general);
             if (!ty_head->handler)
               goto error;
          }
        /* If it comes from a user interaction (click on a link), allow
         * fallback to "xdg-open"
         * Otherwise, it's from terminology's escape code. Thus don't allow
         * fallback and only display content "inline".
         */
        ty_head->fallback_allowed = from_user_interaction;
        ty_head->src = eina_stringshare_add(src);
        if (!ty_head->src)
          goto error;
        ty_head->url = ecore_con_url_new(src);
        if (!ty_head->url)
          goto error;
        if (!ecore_con_url_head(ty_head->url))
          goto error;
        ty_head->url_complete = ecore_event_handler_add
          (ECORE_CON_EVENT_URL_COMPLETE, _media_http_head_complete, ty_head);
        ty_head->timeout = ecore_timer_add(2.5, _media_http_head_timeout, ty_head);
        if (!ty_head->url_complete)
          goto error;
        ty_head->term = term;
        elm_layout_signal_emit(term->bg, "busy", "terminology");
        term_ref(term);
        return;

error:
        _ty_http_head_delete(ty_head);
#endif
        if (from_user_interaction)
          {
             media_unknown_handle(config->helper.local.general, src);
          }
     }
   else
     {
        _popmedia_show(term, src, type);
     }
}

static void
_popmedia_queue_process(Term *term)
{
   struct Pop_Media *pm;

   if (!term->popmedia_queue)
     return;
   pm = term->popmedia_queue->data;
   term->popmedia_queue = eina_list_remove_list(term->popmedia_queue,
                                                term->popmedia_queue);
   if (!pm)
     return;
   _popmedia_unknown(term, pm->src, pm->from_user_interaction);
   eina_stringshare_del(pm->src);
   free(pm);
}

static void
_cb_popup(void *data,
          Evas_Object *_obj EINA_UNUSED,
          void *event)
{
   Term *term = data;
   const char *src = event;
   Eina_Bool from_user_interaction = EINA_FALSE;

   if (!src)
     {
        /* Popup a link, there was user interaction on it. */
        from_user_interaction = EINA_TRUE;
        src = termio_link_get(term->termio, NULL);
     }
   if (!src)
     return;
   _popmedia_unknown(term, src, from_user_interaction);
   if (!event)
     free((void*)src);
}

static void
_cb_popup_queue(void *data,
                Evas_Object *_obj EINA_UNUSED,
                void *event)
{
   Term *term = data;
   const char *src = event;
   Eina_Bool from_user_interaction = EINA_FALSE;

   if (!src)
     {
        from_user_interaction = EINA_TRUE;
        src = termio_link_get(term->termio, NULL);
     }
   if (!src)
     return;
   _popmedia_queue_add(term, src, from_user_interaction);
   if (!event)
     free((void*)src);
}

/* }}} */
/* {{{ Term */

Eina_Bool
term_is_visible(const Term *term)
{
   const Term_Container *tc;

   if (!term)
     return EINA_FALSE;

   tc = term->container;
   if (!tc)
     return EINA_FALSE;

   return tc->is_visible(tc, tc);
}

void
background_set_shine(const Config *config, Evas_Object *bg_edj)
{
   Edje_Message_Int msg;

   if (config)
     msg.val = config->shine;
   else
     msg.val = 255;

   if (bg_edj)
       edje_object_message_send(bg_edj, EDJE_MESSAGE_INT, 2, &msg);
}

void
term_apply_shine(Term *term, int shine)
{
   Config *config = term->config;

   if (config->shine != shine)
     {
        config->shine = shine;
        background_set_shine(config, term->bg_edj);
        config_save(config);
     }
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
term_is_focused(const Term *term)
{
   Term_Container *tc;

   if (!term)
     return EINA_FALSE;

   tc = term->container;
   if (!tc)
     return EINA_FALSE;

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
        if (!theme_apply(term->bg, config, "terminology/background",
                         NULL, NULL, EINA_TRUE))
          ERR("Couldn't find terminology theme!");
        colors_term_init(termio_textgrid_get(term->termio),
                         config->color_scheme);
        termio_config_set(term->termio, config);
     }

   l = elm_theme_overlay_list_get(NULL);
   if (l) l = eina_list_last(l);
   if (l) elm_theme_overlay_del(NULL, l->data);
   elm_theme_overlay_add(NULL, config_theme_path_get(config));
   main_trans_update();
}

void
term_focus(Term *term)
{
   Term_Container *tc;

   DBG("is focused? tc:%p", term->container);
   if (term_is_focused(term))
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
   if (!term_is_focused(term))
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
             if (term_is_focused(term))
               elm_layout_signal_emit(term->bg, "miniview,on", "terminology");
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
        elm_layout_signal_emit(term->bg, "miniview,off", "terminology");
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
        elm_layout_signal_emit(term->bg, "miniview,off", "terminology");
        term->miniview_shown = EINA_FALSE;
     }
   else
     {
        elm_layout_signal_emit(term->bg, "miniview,on", "terminology");
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
        if (term_is_focused(term))
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

   termio_user_title_set(term->termio, title);
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
   const char *prev_title;

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
   elm_entry_editable_set(o, EINA_TRUE);
   prev_title = termio_user_title_get(term->termio);
   if (prev_title)
     {
        elm_entry_entry_set(o, prev_title);
        elm_entry_cursor_pos_set(o, strlen(prev_title));
     }
   evas_object_smart_callback_add(o, "activated", _set_title_ok_cb, popup);
   evas_object_smart_callback_add(o, "aborted", _set_title_cancel_cb, popup);
   elm_object_content_set(popup, o);

   evas_object_show(o);

   evas_object_show(popup);

   elm_object_focus_set(o, EINA_TRUE);
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
   if (save)
     config_save(config);
   main_trans_update();
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
   elm_layout_signal_emit(term->bg, "sendfile,progress,off", "terminology");
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
   if (!edje_object_part_exists(term->bg_edj, "terminology.sendfile.progress"))
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
   elm_layout_content_set(term->bg, "terminology.sendfile.progress", base);
   evas_object_show(base);
   elm_layout_signal_emit(term->bg, "sendfile,progress,on", "terminology");
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
   elm_layout_signal_emit(term->bg, "sendfile,request,off", "terminology");
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
   if (!edje_object_part_exists(term->bg_edj, "terminology.sendfile.request"))
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
   elm_layout_content_set(term->bg, "terminology.sendfile.request", o);
   evas_object_show(o);
   elm_layout_signal_emit(term->bg, "sendfile,request,on", "terminology");
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
             _popmedia_now(term, cmd + 2, EINA_FALSE);
          }
        else if (cmd[1] == 'q') // queue it to display after current one
          {
             _popmedia_queue_add(term, cmd + 2, EINA_FALSE);
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
                  config_save(config);
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
_cb_tab_go(void *data,
           Evas_Object *_obj EINA_UNUSED,
           const char *_sig EINA_UNUSED,
           const char *_src EINA_UNUSED)
{
   _cb_select(data, NULL, NULL);
}

static void
_cb_tab_new(void *data,
           Evas_Object *_obj EINA_UNUSED,
           const char *_sig EINA_UNUSED,
           const char *_src EINA_UNUSED)
{
   Term *term = data;
   main_new(term->termio);
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
   if (term_is_focused(term))
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

   elm_layout_signal_emit(wn->base, "cmdbox,hide", "terminology");
   tc = (Term_Container *) wn;
   term = tc->focused_term_get(tc);
   if (wn->cmdbox) cmd = (char *)elm_entry_entry_get(wn->cmdbox);
   if (cmd)
     {
        cmd = elm_entry_markup_to_utf8(cmd);
        if (cmd)
          {
             if (term)
                 termcmd_do(term->termio, term->wn->win, term->bg, cmd);
             free(cmd);
          }
     }
   elm_object_focus_set(wn->base, EINA_TRUE);
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

   elm_layout_signal_emit(wn->base, "cmdbox,hide", "terminology");
   elm_object_focus_set(wn->base, EINA_TRUE);
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
        elm_layout_content_set(wn->base, "terminology.cmdbox", wn->cmdbox);
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
        elm_layout_content_set(wn->base, "terminology.cmdbox", o);
     }
   elm_layout_signal_emit(term->wn->base, "cmdbox,show", "terminology");
   elm_entry_entry_set(term->wn->cmdbox, "");
   evas_object_show(term->wn->cmdbox);
   elm_object_focus_set(term->wn->cmdbox, EINA_TRUE);
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
        elm_layout_signal_emit(term->bg, "media,off", "terminology");
        elm_layout_signal_emit(term->core, "media,off", "terminology");
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
        elm_layout_content_set(term->core, "terminology.background", o);
        evas_object_show(o);
        term->mediatype = type;
        switch (type)
          {
           case MEDIA_TYPE_IMG:
              elm_layout_signal_emit(term->bg, "media,image", "terminology");
              elm_layout_signal_emit(term->core, "media,image", "terminology");
              break;
           case MEDIA_TYPE_SCALE:
              elm_layout_signal_emit(term->bg, "media,scale", "terminology");
              elm_layout_signal_emit(term->core, "media,scale", "terminology");
              break;
           case MEDIA_TYPE_EDJE:
              elm_layout_signal_emit(term->bg, "media,edje", "terminology");
              elm_layout_signal_emit(term->core, "media,edje", "terminology");
              break;
           case MEDIA_TYPE_MOV:
              elm_layout_signal_emit(term->bg, "media,movie", "terminology");
              elm_layout_signal_emit(term->core, "media,movie", "terminology");
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
             elm_layout_signal_emit(term->bg, "media,off", "terminology");
             elm_layout_signal_emit(term->core, "media,off", "terminology");
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
   if (_tab_drag && _tab_drag->term == term)
     {
        _tab_drag_free();
     }
   if (_tab_drag && _tab_drag->term_over == term)
     {
        _tab_drag->term_over = NULL;
        _tab_drag->split_direction = SPLIT_DIRECTION_NONE;
     }
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
   eina_stringshare_del(term->sendfile_dir);
   term->sendfile_dir = NULL;
   _popmedia_queue_free(term);
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

   _term_tabregion_free(term);

   if (term->tabbar.l.box)
     {
        elm_box_unpack_all(term->tabbar.l.box);
        evas_object_del(term->tabbar.l.box);
        term->tabbar.l.box = NULL;
     }
   if (term->tabbar.r.box)
     {
        elm_box_unpack_all(term->tabbar.r.box);
        evas_object_del(term->tabbar.r.box);
        term->tabbar.r.box = NULL;
     }

   evas_object_del(term->tab_inactive);
   term->tab_inactive = NULL;
   term->tab_item = NULL;

   evas_object_del(term->termio);
   term->termio = NULL;

   evas_object_del(term->core);
   term->core = NULL;
   evas_object_del(term->bg);
   term->bg = NULL;
   term->bg_edj = NULL;

   if (term->tab_spacer)
     {
        evas_object_del(term->tab_spacer);
        term->tab_spacer = NULL;
     }
   free(term);
}

static void
_cb_tab_prev(void *data,
                  Evas_Object *_obj EINA_UNUSED,
                  const char *_sig EINA_UNUSED,
                  const char *_src EINA_UNUSED)
{
   _cb_prev(data, NULL, NULL);
}

static void
_cb_tab_next(void *data,
                  Evas_Object *_obj EINA_UNUSED,
                  const char *_sig EINA_UNUSED,
                  const char *_src EINA_UNUSED)
{
   _cb_next(data, NULL, NULL);
}


static void
_term_bg_config(Term *term)
{
   _term_trans(term);
   background_set_shine(term->config, term->bg_edj);

   termio_theme_set(term->termio, term->bg_edj);
   elm_layout_signal_callback_add(term->bg, "popmedia,done", "terminology",
                                   _cb_popmedia_done, term);
   elm_layout_signal_callback_add(term->bg, "tab,go", "terminology",
                                  _cb_tab_go, term);
   elm_layout_signal_callback_add(term->bg, "tab,new", "terminology",
                                  _cb_tab_new, term);
   elm_layout_signal_callback_add(term->bg, "tab,prev", "terminology",
                                  _cb_tab_prev, term);
   elm_layout_signal_callback_add(term->bg, "tab,next", "terminology",
                                  _cb_tab_next, term);
   elm_layout_signal_callback_add(term->bg, "tab,close", "terminology",
                                  _cb_tab_close, term);
   elm_layout_signal_callback_add(term->bg, "tab,title", "terminology",
                                   _cb_tab_title, term);
   elm_layout_signal_callback_add(term->bg, "tab,hdrag", "*",
                                  _term_on_horizontal_drag, term);
   elm_layout_signal_callback_add(term->bg, "tab,drag,move", "*",
                                  _tabs_drag_mouse_move, term);
   elm_layout_signal_callback_add(term->bg, "tab,drag,stop", "*",
                                  _term_on_drag_stop, term);
   elm_layout_signal_callback_add(term->bg, "tab,mouse,down", "*",
                                  _tabs_mouse_down, term);
   elm_layout_content_set(term->core, "terminology.content", term->termio);
   elm_layout_content_set(term->bg, "terminology.content", term->core);
   elm_layout_content_set(term->bg, "terminology.miniview", term->miniview);
   if (term->popmedia)
     {
        elm_layout_content_set(term->bg, "terminology.popmedia", term->popmedia);
        switch (term->poptype)
          {
           case MEDIA_TYPE_IMG:
              elm_layout_signal_emit(term->bg, "popmedia,image", "terminology");
              break;
           case MEDIA_TYPE_SCALE:
              elm_layout_signal_emit(term->bg, "popmedia,scale", "terminology");
              break;
           case MEDIA_TYPE_EDJE:
              elm_layout_signal_emit(term->bg, "popmedia,edje", "terminology");
              break;
           case MEDIA_TYPE_MOV:
              elm_layout_signal_emit(term->bg, "popmedia,movie", "terminology");
              break;
           default:
              break;
          }
     }
   if (term->media)
     {
        elm_layout_content_set(term->core, "terminology.background", term->media);
        switch (term->mediatype)
          {
           case MEDIA_TYPE_IMG:
              elm_layout_signal_emit(term->bg, "media,image", "terminology");
              elm_layout_signal_emit(term->core, "media,image", "terminology");
              break;
           case MEDIA_TYPE_SCALE:
              elm_layout_signal_emit(term->bg, "media,scale", "terminology");
              elm_layout_signal_emit(term->core, "media,scale", "terminology");
              break;
           case MEDIA_TYPE_EDJE:
             elm_layout_signal_emit(term->bg, "media,edje", "terminology");
             elm_layout_signal_emit(term->core, "media,edje", "terminology");
             break;
           case MEDIA_TYPE_MOV:
             elm_layout_signal_emit(term->bg, "media,movie", "terminology");
             elm_layout_signal_emit(term->core, "media,movie", "terminology");
             break;
           case MEDIA_TYPE_UNKNOWN:
           default:
             break;
          }
     }

   DBG("is focused? tc:%p", term->container);
   if (term_is_focused(term) && (_win_is_focused(term->wn)))
     {
        if (term->config->disable_focus_visuals)
          {
             elm_layout_signal_emit(term->bg, "focused,set", "terminology");
             elm_layout_signal_emit(term->core, "focused,set", "terminology");
          }
        else
          {
             elm_layout_signal_emit(term->bg, "focus,in", "terminology");
             elm_layout_signal_emit(term->core, "focus,in", "terminology");
          }
     }
   if (term->miniview_shown)
        elm_layout_signal_emit(term->bg, "miniview,on", "terminology");

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
   elm_layout_content_set(term->core, "terminology.tabregion",
                          term->tab_region_base);
}

static void
_term_tabregion_setup(Term *term)
{
   Evas_Object *o;

   if (term->tab_region_bg) return;
   term->tab_region_bg = o = evas_object_rectangle_add(evas_object_evas_get(term->bg));
   evas_object_color_set(o, 0, 0, 0, 0);
   evas_object_event_callback_add(o, EVAS_CALLBACK_RESIZE, _cb_tabregion_change, term);
   elm_layout_content_set(term->bg, "terminology.tabregion", o);

   term->tab_region_base = o = evas_object_rectangle_add(evas_object_evas_get(term->bg));
   evas_object_color_set(o, 0, 0, 0, 0);
   elm_layout_content_set(term->core, "terminology.tabregion", o);
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

Evas_Object *
term_bg_get(const Term *term)
{
   if (term)
     return term->bg_edj;
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

   term->wn->on_popover++;

   term_ref(term);

   controls_show(term->wn->win, term->wn->base, term->bg_edj, term->termio,
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

   term = calloc(1, sizeof(Term));
   if (!term) return NULL;
   term_ref(term);

   if (!config) abort();

   termpty_init();
   miniview_init();
   gravatar_init();

   term->wn = wn;
   term->hold = hold;
   term->config = config;

   term->core = o = elm_layout_add(wn->win);
   theme_apply(o, term->config, "terminology/core", NULL, NULL, EINA_TRUE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

   theme_auto_reload_enable(o);
   evas_object_data_set(o, "theme_reload_func", _term_bg_config);
   evas_object_data_set(o, "theme_reload_func_data", term);
   evas_object_show(o);

   term->bg = o = elm_layout_add(wn->win);
   term->bg_edj = elm_layout_edje_get(o);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   if (!theme_apply(o, config, "terminology/background", NULL, NULL, EINA_TRUE))
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

   _term_trans(term);

   background_set_shine(term->config, term->bg_edj);

   term->termio = o = termio_add(wn->win, config, cmd, login_shell, cd,
                                 size_w, size_h, term, title);
   evas_object_data_set(o, "term", term);
   colors_term_init(termio_textgrid_get(term->termio),
                    term->config->color_scheme);

   termio_theme_set(o, term->bg_edj);

   term->miniview = o = miniview_add(wn->win, term->termio);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

   o = term->termio;
   evas_object_size_hint_weight_set(o, 0, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, 0, EVAS_HINT_FILL);
   evas_object_event_callback_add(o, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                  _cb_size_hint, term);
   elm_layout_content_set(term->core, "terminology.content", o);

   elm_layout_content_set(term->bg, "terminology.content", term->core);
   elm_layout_content_set(term->bg, "terminology.miniview", term->miniview);

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
   Eina_List *l, *l_next;
   Win *wn;

   EINA_LIST_FOREACH_SAFE(wins, l, l_next, wn)
     {
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
