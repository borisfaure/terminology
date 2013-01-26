#include "private.h"

#include <Ecore_Getopt.h>
#include <Elementary.h>
#include "main.h"
#include "win.h"
#include "termio.h"
#include "termcmd.h"
#include "config.h"
#include "controls.h"
#include "media.h"
#include "utils.h"
#include "ipc.h"

typedef struct _Win   Win;
typedef struct _Term  Term;
typedef struct _Split Split;

struct _Win
{
   Evas_Object *win;
   Evas_Object *conform;
   Evas_Object *backbg;
   Evas_Object *table;
   Eina_List   *terms;
   Split       *split;
   Ecore_Job   *size_job;
   Eina_Bool    focused : 1;
};

struct _Term
{
   Win         *wn;
   Config      *config;
   Evas_Object *bg;
   Evas_Object *term;
   Evas_Object *media;
   Evas_Object *popmedia;
   Evas_Object *cmdbox;
   Ecore_Timer *cmdbox_focus_timer;
   Eina_List   *popmedia_queue;
   int          step_x, step_y, min_w, min_h, req_w, req_h;
   struct {
      int       x, y;
   } down;
   Eina_Bool    focused : 1;
   Eina_Bool    cmdbox_up : 1;
   Eina_Bool    hold : 1;
};

struct _Split
{
   Win         *wn; // win this split belongs to
   Split       *parent; // the parent split or null if toplevel
   Split       *s1, *s2; // left/right or top/bottom child splits, null if leaf
   Term        *term; // if leaf node this is not null
   Evas_Object *panes;
   Eina_Bool    horizontal : 1;
};

int _log_domain = -1;

static Eina_List   *wins = NULL;
static Ecore_Timer *flush_timer = NULL;

static void _popmedia_queue_process(Term *term);
static void main_win_free(Win *wn);
static void main_term_free(Term *term);
static void main_term_bg_redo(Term *term);
static Term *main_term_new(Win *wn, Config *config, const char *cmd, Eina_Bool login_shell, const char *cd, int size_w, int size_h, Eina_Bool hold);

static void
_split_free(Split *sp)
{
   if (sp->s1) _split_free(sp->s1);
   if (sp->s2) _split_free(sp->s2);
   if (sp->panes) evas_object_del(sp->panes);
   free(sp);
}

static Split *
_split_split_find(Split *sp, Evas_Object *term)
{
   Split *sp2;
   
   if (sp->term)
     {
        if (sp->term->term == term) return sp;
     }
   if (sp->s1)
     {
        sp2 = _split_split_find(sp->s1, term);
        if (sp2) return sp2;
     }
   if (sp->s2)
     {
        sp2 = _split_split_find(sp->s2, term);
        if (sp2) return sp2;
     }
   return NULL;
}

static Split *
_split_find(Evas_Object *win, Evas_Object *term)
{
   Win *wn;
   Eina_List *l;
   
   EINA_LIST_FOREACH(wins, l, wn)
     {
        if (wn->win == win) return _split_split_find(wn->split, term);
     }
   return NULL;
}

static void
_split_split(Split *sp, Eina_Bool horizontal)
{
   Split *sp2;
   Evas_Object *o;
   Config *config;

   if (!sp->term) return;
   
   o = sp->panes = elm_panes_add(sp->wn->win);
   elm_object_style_set(o, "flush");
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   sp->horizontal = horizontal;
   elm_panes_horizontal_set(o, sp->horizontal);
   
   sp2 = sp->s1 = calloc(1, sizeof(Split));
   sp2->parent = sp;
   sp2->wn = sp->wn;
   sp2->term = sp->term;
   
   if (!sp->parent) elm_table_unpack(sp->wn->table, sp->term->bg);
   main_term_bg_redo(sp->term);
   
   sp2 = sp->s2 = calloc(1, sizeof(Split));
   sp2->parent = sp;
   sp2->wn = sp->wn;
   config = config_load("config");
   sp2->term = main_term_new(sp->wn, config,
                             NULL, EINA_FALSE, NULL,
                             80, 24, EINA_FALSE);
   evas_object_data_set(sp2->term->term, "sizedone", sp2->term->term);
   

   elm_object_part_content_set(sp->panes, "top", sp->s1->term->bg);
   elm_object_part_content_set(sp->panes, "bottom", sp->s2->term->bg);
   
   if (!sp->parent)
     elm_table_pack(sp->wn->table, sp->panes, 0, 0, 1, 1);
   else
     {
        if (sp == sp->parent->s1)
          {
             elm_object_part_content_unset(sp->parent->panes, "top");
             elm_object_part_content_set(sp->parent->panes, "top", sp->panes);
          }
        else
          {
             elm_object_part_content_unset(sp->parent->panes, "bottom");
             elm_object_part_content_set(sp->parent->panes, "bottom", sp->panes);
          }
     }
   evas_object_show(sp->panes);
   sp->term = NULL;
}

void
main_split_h(Evas_Object *win, Evas_Object *term)
{
   Split *sp = _split_find(win, term);
   
   if (!sp) return;
   _split_split(sp, EINA_TRUE);
}

void
main_split_v(Evas_Object *win, Evas_Object *term)
{
   Split *sp = _split_find(win, term);
   
   if (!sp) return;
   _split_split(sp, EINA_FALSE);
}

static void
_split_append(Split *sp, Eina_List **flat)
{
   if (sp->term) *flat = eina_list_append(*flat, sp);
   else
     {
        _split_append(sp->s1, flat);
        _split_append(sp->s2, flat);
     }
}

static Eina_List *
_split_flatten(Split *sp)
{
   Eina_List *flat = NULL;
   
   _split_append(sp, &flat);
   return flat;
}

static Term *
_term_next_get(Term *termin)
{
   Split *sp;
   Eina_List *flat, *l;
   
   sp = _split_find(termin->wn->win, termin->term);
   if (!sp->parent) return NULL;
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
        return sp->term;
     }
   sp = flat->data;
   eina_list_free(flat);
   return sp->term;
}

static Term *
_term_prev_get(Term *termin)
{
   Split *sp;
   Eina_List *flat, *l;
   
   sp = _split_find(termin->wn->win, termin->term);
   if (!sp->parent) return NULL;
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
        return sp->term;
     }
   sp = eina_list_last_data_get(flat);
   eina_list_free(flat);
   return sp->term;
}

static void
_split_merge(Split *spp, Split *sp, const char *slot)
{
   Evas_Object *o = NULL;
   
   if (sp->term)
     {
        main_term_bg_redo(sp->term);
        spp->term = sp->term;
        sp->term = NULL;
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
          elm_table_pack(spp->wn->table, o, 0, 0, 1, 1);
     }
   else
     {
        spp->s1 = sp->s1;
        spp->s2 = sp->s2;
        spp->s1->parent = spp;
        spp->s2->parent = spp;
        spp->horizontal = sp->horizontal;
        o = sp->panes;
        if (spp->parent)
          {
             elm_object_part_content_unset(spp->parent->panes, slot);
             elm_object_part_content_set(spp->parent->panes, slot, o);
          }
        else
          elm_table_pack(spp->wn->table, o, 0, 0, 1, 1);
        evas_object_del(spp->panes);
        spp->panes = o;
        sp->s1 = NULL;
        sp->s2 = NULL;
        sp->panes = NULL;
     }
   _split_free(sp);
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
                  elm_object_focus_set(term2->term, EINA_FALSE);
               }
          }
     }
   term->focused = EINA_TRUE;
   edje_object_signal_emit(term->bg, "focus,in", "terminology");
   elm_object_focus_set(term->term, EINA_TRUE);
}

void
main_close(Evas_Object *win, Evas_Object *term)
{
   Split *sp = _split_find(win, term);
   Split *spp, *spkeep = NULL;
   Term *termfoc = NULL;
   const char *slot = "top";
   
   if (!sp) return;
   if (!sp->term) return;
   spp = sp->parent;
   if ((sp->term->focused) && (spp)) termfoc = _term_next_get(sp->term);
   sp->wn->terms = eina_list_remove(sp->wn->terms, sp->term);
   if (spp)
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
        
        main_term_free(sp->term);
        sp->term = NULL;
        _split_free(sp);
        
        if ((spp->parent) && (spp->parent->s2 == spp)) slot = "bottom";
        _split_merge(spp, spkeep, slot);
        
        if (termfoc) _term_focus(termfoc);
     }
   else
     {
        elm_table_unpack(sp->wn->table, sp->term->bg);
        main_term_free(sp->term);
        sp->term = NULL;
        if (!sp->wn->terms) evas_object_del(sp->wn->win);
     }
}

static Term *
main_win_focused_term_get(Win *wn)
{
   Term *term;
   Eina_List *l;
   
   EINA_LIST_FOREACH(wn->terms, l, term)
     {
        if (term->focused) return term;
     }
   return NULL;
}

static void
_cb_del(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Win *wn = data;

   // already obj here is deleted - dont do it again
   wn->win = NULL;
   main_win_free(wn);
}

static void
_cb_focus_in(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Win *wn = data;
   Term *term;
   
   if (!wn->focused) elm_win_urgent_set(wn->win, EINA_FALSE);
   wn->focused = EINA_TRUE;
   term = main_win_focused_term_get(wn);
   if (!term) return;
   edje_object_signal_emit(term->bg, "focus,in", "terminology");
   if (term->cmdbox_up) elm_object_focus_set(term->cmdbox, EINA_TRUE);
   else elm_object_focus_set(term->term, EINA_TRUE);
}

static void
_cb_focus_out(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Win *wn = data;
   Term *term;
   
   wn->focused = EINA_FALSE;
   term = main_win_focused_term_get(wn);
   if (!term) return;
   edje_object_signal_emit(term->bg, "focus,out", "terminology");
   if (term->cmdbox_up) elm_object_focus_set(term->cmdbox, EINA_FALSE);
   else elm_object_focus_set(term->term, EINA_FALSE);
   elm_cache_all_flush();
}

static void
_cb_term_mouse_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event )
{
   Evas_Event_Mouse_Down *ev = event;
   Term *term = data;
   Term *term2;
   
   term2 = main_win_focused_term_get(term->wn);
   if (term == term2) return;
   if (ev->button == 1)
     {  
        term->down.x = ev->canvas.x;
        term->down.y = ev->canvas.y;
        _term_focus(term);
     }
}

static void
_cb_term_mouse_up(void *data __UNUSED__, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
/*   
   Evas_Event_Mouse_Up *ev = event;
   Term *term = data;
   Term *term2;
   
   term2 = main_win_focused_term_get(term->wn);
   if (term == term2) return;
   if (ev->button == 1)
     {
        int dx, dy, f;
        
        dx = term->down.x - ev->canvas.x;
        dy = term->down.y - ev->canvas.y;
        f = elm_config_finger_size_get();
        if ((f * f) > ((dx * dx) + (dy * dy)))
          {
             _term_focus(term);
          }
     }
 */
}

typedef struct _Sizeinfo Sizeinfo;

struct _Sizeinfo
{
   int min_w, min_h;
   int step_x, step_y;
   int req_w, req_h;
   int req;
};

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

static void
_size_job(void *data)
{
   Win *wn = data;
   Sizeinfo info = { 0, 0, 0, 0, 0, 0, 0 };
   Evas_Coord mw = 0, mh = 0;
   
   wn->size_job = NULL;
   _split_size_walk(wn->split, &info);
   if (wn->split->panes)
     evas_object_size_hint_min_get(wn->split->panes, &mw, &mh);
   else
     evas_object_size_hint_min_get(wn->split->term->bg, &mw, &mh);
   elm_win_size_base_set(wn->win, mw - info.step_x, mh - info.step_y);
   elm_win_size_step_set(wn->win, info.step_x, info.step_y);
   evas_object_size_hint_min_set(wn->backbg, mw, mh);
   if (info.req) evas_object_resize(wn->win, info.req_w, info.req_h);
}

static void
main_win_sizing_handle(Win *wn)
{
   if (wn->size_job) ecore_job_del(wn->size_job);
   _size_job(wn);
}

static void
_cb_size_hint(void *data, Evas *e __UNUSED__, Evas_Object *obj, void *event __UNUSED__)
{
   Term *term = data;
   Evas_Coord mw, mh, rw, rh, w = 0, h = 0;

   evas_object_size_hint_min_get(obj, &mw, &mh);
   evas_object_size_hint_request_get(obj, &rw, &rh);

   edje_object_size_min_calc(term->bg, &w, &h);
   evas_object_size_hint_min_set(term->bg, w, h);
   term->step_x = mw;
   term->step_y = mh;
   term->min_w = w - mw;
   term->min_h = h - mh;
   term->req_w = w - mw + rw;
   term->req_h = w - mw + rh;

   if (term->wn->size_job) ecore_job_del(term->wn->size_job);
   term->wn->size_job = ecore_job_add(_size_job, term->wn);
}

static void
_cb_options(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Term *term = data;

   controls_toggle(term->wn->win, term->bg, term->term);
}

static Eina_Bool
_cb_flush(void *data __UNUSED__)
{
   flush_timer = NULL;
   elm_cache_all_flush();
   return EINA_FALSE;
}

static void
_cb_change(void *data __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   if (!flush_timer) flush_timer = ecore_timer_add(0.25, _cb_flush, NULL);
   else ecore_timer_delay(flush_timer, 0.25);
}

static void
_cb_exited(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Term *term = data;
   
   if (!term->hold) main_close(term->wn->win, term->term);
}

static void
_cb_bell(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Term *term = data;
   Config *config = termio_config_get(term->term);

   if (!config) return;
   if (!config->disable_visual_bell)
     edje_object_signal_emit(term->bg, "bell", "terminology");
   if (config->urg_bell)
     {
        if (!term->wn->focused) elm_win_urgent_set(term->wn->win, EINA_TRUE);
     }
}

static void
_cb_popmedia_del(void *data, Evas *e __UNUSED__, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Term *term = data;
   
   edje_object_signal_emit(term->bg, "popmedia,off", "terminology");
}

static void
_cb_popmedia_done(void *data, Evas_Object *obj __UNUSED__, const char *sig __UNUSED__, const char *src __UNUSED__)
{
   Term *term = data;
   
   if (term->popmedia)
     {
        evas_object_event_callback_del(term->popmedia, EVAS_CALLBACK_DEL,
                                       _cb_popmedia_del);
        evas_object_del(term->popmedia);
        term->popmedia = NULL;
        termio_mouseover_suspend_pushpop(term->term, -1);
        _popmedia_queue_process(term);
     }
}

static void
_cb_media_loop(void *data, Evas_Object *obj __UNUSED__, void *info __UNUSED__)
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
   Config *config = termio_config_get(term->term);
   int type = 0;

   if (!config) return;
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
   termio_mouseover_suspend_pushpop(term->term, 1);
   term->popmedia = o = media_add(term->wn->win, src, config, MEDIA_POP, &type);
   evas_object_smart_callback_add(o, "loop", _cb_media_loop, term);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _cb_popmedia_del, term);
   edje_object_part_swallow(term->bg, "terminology.popmedia", o);
   evas_object_show(o);
   if (type == TYPE_IMG)
     edje_object_signal_emit(term->bg, "popmedia,image", "terminology");
   else if (type == TYPE_SCALE)
     edje_object_signal_emit(term->bg, "popmedia,scale", "terminology");
   else if (type == TYPE_EDJE)
     edje_object_signal_emit(term->bg, "popmedia,edje", "terminology");
   else if (type == TYPE_MOV)
     edje_object_signal_emit(term->bg, "popmedia,movie", "terminology");
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
_cb_popup(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Term *term = data;
   const char *src = termio_link_get(term->term);
   
   if (!src) return;
   _popmedia_show(term, src);
}

static void
_cb_command(void *data, Evas_Object *obj __UNUSED__, void *event)
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
             Config *config = termio_config_get(term->term);

             if (config)
               {
                  config->temporary = EINA_TRUE;
                  eina_stringshare_replace(&(config->background), cmd + 2);
                  main_media_update(config);
               }
          }
        else if (cmd[1] == 'p') // permanent
          {
             Config *config = termio_config_get(term->term);

             if (config)
               {
                  config->temporary = EINA_FALSE;
                  eina_stringshare_replace(&(config->background), cmd + 2);
                  main_media_update(config);
               }
          }
     }
   else if (cmd[0] == 'a') // set alpha
     {
        if (cmd[1] == 't') // temporary
          {
             Config *config = termio_config_get(term->term);

             if (config)
               {
                  config->temporary = EINA_TRUE;
                  if ((cmd[2] == '1') ||
                      (!strcasecmp(cmd + 2, "on")) ||
                      (!strcasecmp(cmd + 2, "true")) ||
                      (!strcasecmp(cmd + 2, "yes")))
                    config->translucent = EINA_FALSE;
                  else
                    config->translucent = EINA_FALSE;
                  main_trans_update(config);
               }
          }
        else if (cmd[1] == 'p') // permanent
          {
             Config *config = termio_config_get(term->term);

             if (config)
               {
                  config->temporary = EINA_FALSE;
                  if ((cmd[2] == '1') ||
                      (!strcasecmp(cmd + 2, "on")) ||
                      (!strcasecmp(cmd + 2, "true")) ||
                      (!strcasecmp(cmd + 2, "yes")))
                    config->translucent = EINA_FALSE;
                  else
                    config->translucent = EINA_FALSE;
                  main_trans_update(config);
               }
          }
     }
}

static void
_cb_prev(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Term *term = data;
   Term *term2 = NULL;

   if (term->focused) term2 = _term_prev_get(term);
   if (term2) _term_focus(term2);
}

static void
_cb_next(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Term *term = data;
   Term *term2 = NULL;
   
   if (term->focused) term2 = _term_next_get(term);
   if (term2) _term_focus(term2);
}

static void
_cb_new(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Term *term = data;
}

static void
_cb_split_h(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Term *term = data;

   main_split_h(term->wn->win, term->term);
}

static void
_cb_split_v(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Term *term = data;
   
   main_split_v(term->wn->win, term->term);
}

static Eina_Bool
_cb_cmd_focus(void *data)
{
   Term *term = data;
   
   term->cmdbox_focus_timer = NULL;
   elm_object_focus_set(term->cmdbox, EINA_TRUE);
   return EINA_FALSE;
}

static void
_cb_cmdbox(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Term *term = data;
   
   term->cmdbox_up = EINA_TRUE;
   elm_object_focus_set(term->term, EINA_FALSE);
   edje_object_signal_emit(term->bg, "cmdbox,show", "terminology");
   elm_entry_entry_set(term->cmdbox, "");
   evas_object_show(term->cmdbox);
   if (term->cmdbox_focus_timer) ecore_timer_del(term->cmdbox_focus_timer);
   term->cmdbox_focus_timer = ecore_timer_add(0.1, _cb_cmd_focus, term);
}

static void
_cb_cmd_activated(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Term *term = data;
   char *cmd;
   
   elm_object_focus_set(term->cmdbox, EINA_FALSE);
   edje_object_signal_emit(term->bg, "cmdbox,hide", "terminology");
   elm_object_focus_set(term->term, EINA_TRUE);
   cmd = (char *)elm_entry_entry_get(term->cmdbox);
   if (cmd)
     {
        cmd = elm_entry_markup_to_utf8(cmd);
        if (cmd)
          {
             termcmd_do(term->term, term->wn->win, term->bg, cmd);
             free(cmd);
          }
     }
   if (term->cmdbox_focus_timer)
     {
        ecore_timer_del(term->cmdbox_focus_timer);
        term->cmdbox_focus_timer = NULL;
     }
   term->cmdbox_up = EINA_FALSE;
}

static void
_cb_cmd_aborted(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Term *term = data;
   
   elm_object_focus_set(term->cmdbox, EINA_FALSE);
   edje_object_signal_emit(term->bg, "cmdbox,hide", "terminology");
   elm_object_focus_set(term->term, EINA_TRUE);
   if (term->cmdbox_focus_timer)
     {
        ecore_timer_del(term->cmdbox_focus_timer);
        term->cmdbox_focus_timer = NULL;
     }
   term->cmdbox_up = EINA_FALSE;
}

static void
_cb_cmd_changed(void *data, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Term *term = data;
   char *cmd;
   
   cmd = (char *)elm_entry_entry_get(term->cmdbox);
   if (cmd)
     {
        cmd = elm_entry_markup_to_utf8(cmd);
        if (cmd)
          {
             termcmd_watch(term->term, term->wn->win, term->bg, cmd);
             free(cmd);
          }
     }
}

static void
_cb_cmd_hints_changed(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   Term *term = data;
   
   evas_object_show(term->cmdbox);
   edje_object_part_swallow(term->bg, "terminology.cmdbox", term->cmdbox);
}

void
main_trans_update(const Config *config)
{
   Win *wn;
   Term *term;
   Eina_List *l, *ll;
   
   EINA_LIST_FOREACH(wins, l, wn)
     {
        if (config->translucent)
          {
             EINA_LIST_FOREACH(wn->terms, ll, term)
               {
                  edje_object_signal_emit(term->bg, "translucent,on", "terminology");
               }
             elm_win_alpha_set(wn->win, EINA_TRUE);
             evas_object_hide(wn->backbg);
          }
        else
          {
             EINA_LIST_FOREACH(wn->terms, ll, term)
               {
                  edje_object_signal_emit(term->bg, "translucent,off", "terminology");
               }
             elm_win_alpha_set(wn->win, EINA_FALSE);
             evas_object_show(wn->backbg);
          }
     }
}

static void
_cb_media_del(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   Term *term = data;
   Config *config = termio_config_get(term->term);

   term->media = NULL;
   edje_object_signal_emit(term->bg, "media,off", "terminology");
   if (!config) return;
   if (config->temporary)
     eina_stringshare_replace(&(config->background), NULL);
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
             if ((config->background) && (config->background[0]))
               {
                  Evas_Object *o;
                  int type = 0;
             
                  if (term->media)
                    {
                       evas_object_event_callback_del(term->media,
                                                      EVAS_CALLBACK_DEL,
                                                      _cb_media_del);
                       evas_object_del(term->media);
                    }
                  term->media = o = media_add(term->wn->win,
                                              config->background, config,
                                              MEDIA_BG, &type);
                  evas_object_event_callback_add(o, EVAS_CALLBACK_DEL,
                                                 _cb_media_del, term);
                  edje_object_part_swallow(term->bg, "terminology.background", o);
                  evas_object_show(o);
                  if (type == TYPE_IMG)
                    edje_object_signal_emit(term->bg, "media,image", "terminology");
                  else if (type == TYPE_SCALE)
                    edje_object_signal_emit(term->bg, "media,scale", "terminology");
                  else if (type == TYPE_EDJE)
                    edje_object_signal_emit(term->bg, "media,edje", "terminology");
                  else if (type == TYPE_MOV)
                    edje_object_signal_emit(term->bg, "media,movie", "terminology");
               }
             else
               {
                  if (term->media)
                    {
                       edje_object_signal_emit(term->bg, "media,off", "terminology");
                       evas_object_del(term->media);
                       term->media = NULL;
                    }
               }
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
             if (term->config != config) continue;
             if (term->media) media_mute_set(term->media, config->mute);
          }
     }
}

static void
main_win_free(Win *wn)
{
   Term *term;

   wins = eina_list_remove(wins, wn);
   EINA_LIST_FREE(wn->terms, term)
     {
        main_term_free(term);
     }
   if (wn->split) _split_free(wn->split);
   if (wn->win)
     {
        evas_object_event_callback_del_full(wn->win, EVAS_CALLBACK_DEL, _cb_del, wn);
        evas_object_del(wn->win);
     }
   if (wn->size_job) ecore_job_del(wn->size_job);
   free(wn);
}

static Win *
main_win_new(const char *name, const char *role,
             const char *title, const char *icon_name,
             Eina_Bool fullscreen, Eina_Bool iconic,
             Eina_Bool borderless, Eina_Bool override,
             Eina_Bool maximized)
{
   Win *wn;
   Evas_Object *o;
   
   wn = calloc(1, sizeof(Win));
   if (!wn) return NULL;

   wn->win = tg_win_add(name, role, title, icon_name);
   if (!wn->win)
     {
        free(wn);
        return NULL;
     }

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
   
   wn->table = o = elm_table_add(wn->win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(wn->conform, o);
   evas_object_show(o);
   
   evas_object_smart_callback_add(wn->win, "focus,in", _cb_focus_in, wn);
   evas_object_smart_callback_add(wn->win, "focus,out", _cb_focus_out, wn);
   
   wins = eina_list_append(wins, wn);
   return wn;
}

static void
main_term_free(Term *term)
{
   const char *s;
   
   EINA_LIST_FREE(term->popmedia_queue, s)
     {
        eina_stringshare_del(s);
     }
   if (term->cmdbox_focus_timer)
     {
        ecore_timer_del(term->cmdbox_focus_timer);
        term->cmdbox_focus_timer = NULL;
     }
   evas_object_del(term->term);
   term->term = NULL;
   evas_object_del(term->cmdbox);
   term->cmdbox = NULL;
   evas_object_del(term->bg);
   term->bg = NULL;
   if (term->popmedia) evas_object_del(term->popmedia);
   term->popmedia = NULL;
   if (term->media) evas_object_del(term->media);
   term->media = NULL;
   if (term->config) config_del(term->config);
   term->config = NULL;
   free(term);
}

static void
main_term_bg_redo(Term *term)
{
   Evas_Object *o;

   evas_object_del(term->bg);
   
   term->bg = o = edje_object_add(evas_object_evas_get(term->wn->win));
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   theme_apply(o, term->config, "terminology/background");

   theme_auto_reload_enable(o);
   evas_object_show(o);

   edje_object_signal_callback_add(o, "popmedia,done", "terminology",
                                   _cb_popmedia_done, term);
   edje_object_part_swallow(term->bg, "terminology.cmdbox", term->cmdbox);
   termio_theme_set(term->term, term->bg);
   edje_object_part_swallow(term->bg, "terminology.content", term->term);
   if ((term->focused) && (term->wn->focused))
     {
        edje_object_signal_emit(term->bg, "focus,in", "terminology");
        elm_object_focus_set(term->term, EINA_TRUE);
     }
}

static Term *
main_term_new(Win *wn, Config *config, const char *cmd,
              Eina_Bool login_shell, const char *cd,
              int size_w, int size_h, Eina_Bool hold)
{
   Term *term;
   Evas_Object *o;
   
   term = calloc(1, sizeof(Term));
   if (!term) return NULL;

   if (!config) abort();
   
   term->wn = wn;
   term->hold = hold;
   term->config = config;
   
   term->bg = o = edje_object_add(evas_object_evas_get(wn->win));
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   if (!theme_apply(o, config, "terminology/background"))
     {
        CRITICAL("Couldn't find terminology theme! Forgot 'make install'?");
        evas_object_del(term->bg);
        free(term);
        return NULL;
     }

   theme_auto_reload_enable(o);
   evas_object_show(o);

   edje_object_signal_callback_add(o, "popmedia,done", "terminology",
                                   _cb_popmedia_done, term);

   term->cmdbox = o = elm_entry_add(wn->win);
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
   evas_object_smart_callback_add(o, "activated", _cb_cmd_activated, term);
   evas_object_smart_callback_add(o, "aborted", _cb_cmd_aborted, term);
   evas_object_smart_callback_add(o, "changed,user", _cb_cmd_changed, term);
   evas_object_event_callback_add(o, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                  _cb_cmd_hints_changed, term);
   edje_object_part_swallow(term->bg, "terminology.cmdbox", o);
   
   term->term = o = termio_add(wn->win, config, cmd, login_shell, cd,
                               size_w, size_h);
   colors_term_init(termio_textgrid_get(term->term), term->bg);

   termio_win_set(o, wn->win);
   termio_theme_set(o, term->bg);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_event_callback_add(o, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                  _cb_size_hint, term);
   edje_object_part_swallow(term->bg, "terminology.content", o);
   evas_object_smart_callback_add(o, "options", _cb_options, term);
   evas_object_smart_callback_add(o, "change", _cb_change, term);
   evas_object_smart_callback_add(o, "exited", _cb_exited, term);
   evas_object_smart_callback_add(o, "bell", _cb_bell, term);
   evas_object_smart_callback_add(o, "popup", _cb_popup, term);
   evas_object_smart_callback_add(o, "cmdbox", _cb_cmdbox, term);
   evas_object_smart_callback_add(o, "command", _cb_command, term);
   evas_object_smart_callback_add(o, "prev", _cb_prev, term);
   evas_object_smart_callback_add(o, "next", _cb_next, term);
   evas_object_smart_callback_add(o, "new", _cb_new, term);
   evas_object_smart_callback_add(o, "split,h", _cb_split_h, term);
   evas_object_smart_callback_add(o, "split,v", _cb_split_v, term);
   evas_object_show(o);
   
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _cb_term_mouse_down, term);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_UP,
                                  _cb_term_mouse_up, term);
   
   if (!wn->terms)
     {
        term->focused = EINA_TRUE;
//        edje_object_signal_emit(term->bg, "focus,in", "terminology");
     }
   wn->terms = eina_list_append(wn->terms, term);
   
   return term;
}

static void
main_ipc_new(Ipc_Instance *inst)
{
   Win *wn;
   Term *term;
   Config *config;
   Split *sp;
   int pargc = 0, nargc, i;
   char **pargv = NULL, **nargv = NULL, geom[256];

   if (inst->startup_id)
     {
        char buf[4096];
        
        snprintf(buf, sizeof(buf), "DESKTOP_STARTUP_ID=%s", inst->startup_id);
        putenv(buf);
     }
   ecore_app_args_get(&pargc, &pargv);
   nargc = 1;

   if (inst->cd) nargc += 2;
   if (inst->background) nargc += 2;
   if (inst->name) nargc += 2;
   if (inst->role) nargc += 2;
   if (inst->title) nargc += 2;
   if (inst->font) nargc += 2;
   if ((inst->pos) || (inst->w > 0) || (inst->h > 0)) nargc += 2;
   if (inst->login_shell) nargc += 1;
   if (inst->fullscreen) nargc += 1;
   if (inst->iconic) nargc += 1;
   if (inst->borderless) nargc += 1;
   if (inst->override) nargc += 1;
   if (inst->maximized) nargc += 1;
   if (inst->hold) nargc += 1;
   if (inst->nowm) nargc += 1;
   if (inst->cmd) nargc += 2;
   
   nargv = calloc(nargc + 1, sizeof(char *));
   if (!nargv) return;
   
   i = 0;
   nargv[i++] = pargv[0];
   if (inst->cd)
     {
        nargv[i++] = "-d";
        nargv[i++] = (char *)inst->cd;
     }
   if (inst->background)
     {
        nargv[i++] = "-b";
        nargv[i++] = (char *)inst->background;
     }
   if (inst->name)
     {
        nargv[i++] = "-n";
        nargv[i++] = (char *)inst->name;
     }
   if (inst->role)
     {
        nargv[i++] = "-r";
        nargv[i++] = (char *)inst->role;
     }
   if (inst->title)
     {
        nargv[i++] = "-t";
        nargv[i++] = (char *)inst->title;
     }
   if (inst->font)
     {
        nargv[i++] = "-f";
        nargv[i++] = (char *)inst->font;
     }
   if ((inst->pos) || (inst->w > 0) || (inst->h > 0))
     {
        if (!inst->pos)
          snprintf(geom, sizeof(geom), "%ix%i", inst->w, inst->h);
        else
          {
             if ((inst->w > 0) && (inst->h > 0))
               {
                  if (inst->x >= 0)
                    {
                       if (inst->y >= 0)
                         snprintf(geom, sizeof(geom), "%ix%i+%i+%i",
                                  inst->w, inst->h, inst->x, inst->y);
                       else
                         snprintf(geom, sizeof(geom), "%ix%i+%i%i",
                                  inst->w, inst->h, inst->x, inst->y);
                    }
                  else
                    {
                       if (inst->y >= 0)
                         snprintf(geom, sizeof(geom), "%ix%i%i+%i",
                                  inst->w, inst->h, inst->x, inst->y);
                       else
                         snprintf(geom, sizeof(geom), "%ix%i%i%i",
                                  inst->w, inst->h, inst->x, inst->y);
                    }
               }
             else
               {
                  if (inst->x >= 0)
                    {
                       if (inst->y >= 0)
                         snprintf(geom, sizeof(geom), "+%i+%i",
                                  inst->x, inst->y);
                       else
                         snprintf(geom, sizeof(geom), "+%i%i",
                                  inst->x, inst->y);
                    }
                  else
                    {
                       if (inst->y >= 0)
                         snprintf(geom, sizeof(geom), "%i+%i",
                                  inst->x, inst->y);
                       else
                         snprintf(geom, sizeof(geom), "%i%i",
                                  inst->x, inst->y);
                    }
               }
          }
        nargv[i++] = "-g";
        nargv[i++] = geom;
     }
   if (inst->login_shell)
     {
        nargv[i++] = "-l";
     }
   if (inst->fullscreen)
     {
        nargv[i++] = "-F";
     }
   if (inst->iconic)
     {
        nargv[i++] = "-I";
     }
   if (inst->borderless)
     {
        nargv[i++] = "-B";
     }
   if (inst->override)
     {
        nargv[i++] = "-O";
     }
   if (inst->maximized)
     {
        nargv[i++] = "-M";
     }
   if (inst->hold)
     {
        nargv[i++] = "-H";
     }
   if (inst->nowm)
     {
        nargv[i++] = "-W";
     }
   if (inst->cmd)
     {
        nargv[i++] = "-e";
        nargv[i++] = (char *)inst->cmd;
     }
   ecore_app_args_set(nargc, (const char **)nargv);
   wn = main_win_new(inst->name, inst->role, inst->title, inst->icon_name,
                     inst->fullscreen, inst->iconic, inst->borderless,
                     inst->override, inst->maximized);
   if (!wn)
     {
        ecore_app_args_set(pargc, (const char **)pargv);
        free(nargv);
        return;
     }
   config = config_load("config");
   
   unsetenv("DESKTOP_STARTUP_ID");
   if (inst->background)
     {
        eina_stringshare_replace(&(config->background), inst->background);
        config->temporary = EINA_TRUE;
     }

   if (inst->font)
     {
        if (strchr(inst->font, '/'))
          {
             char *fname = alloca(strlen(inst->font) + 1);
             char *p;
             
             strcpy(fname, inst->font);
             p = strrchr(fname, '/');
             if (p)
               {
                  int sz;
                  
                  *p = 0;
                  p++;
                  sz = atoi(p);
                  if (sz > 0) config->font.size = sz;
                  eina_stringshare_replace(&(config->font.name), fname);
               }
             config->font.bitmap = 0;
          }
        else
          {
             char buf[4096], *file;
             Eina_List *files;
             int n = strlen(inst->font);
             
             snprintf(buf, sizeof(buf), "%s/fonts", elm_app_data_dir_get());
             files = ecore_file_ls(buf);
             EINA_LIST_FREE(files, file)
               {
                  if (n > 0)
                    {
                       if (!strncasecmp(file, inst->font, n))
                         {
                            n = -1;
                            eina_stringshare_replace(&(config->font.name), file);
                            config->font.bitmap = 1;
                         }
                    }
                  free(file);
               }
          }
        config->temporary = EINA_TRUE;
     }

   if (inst->w <= 0) inst->w = 80;
   if (inst->h <= 0) inst->h = 24;
   term = main_term_new(wn, config, inst->cmd, inst->login_shell,
                        inst->cd, inst->w, inst->h, inst->hold);
   if (!term)
     {
        main_win_free(wn);
        ecore_app_args_set(pargc, (const char **)pargv);
        free(nargv);
        return;
     }
   else
     {
        elm_table_pack(wn->table, term->bg, 0, 0, 1, 1);
        _cb_size_hint(term, evas_object_evas_get(wn->win), term->term, NULL);
     }
   
   sp = wn->split = calloc(1, sizeof(Split));
   sp->wn = wn;
   sp->term = term;
   
   main_trans_update(config);
   main_media_update(config);
   if (inst->pos)
     {
        int screen_w, screen_h;
        
        elm_win_screen_size_get(wn->win, NULL, NULL, &screen_w, &screen_h);
        if (inst->x < 0) inst->x = screen_w + inst->x;
        if (inst->y < 0) inst->y = screen_h + inst->y;
        evas_object_move(wn->win, inst->x, inst->y);
     }
   main_win_sizing_handle(wn);
   evas_object_show(wn->win);
   if (inst->nowm)
     ecore_evas_focus_set
     (ecore_evas_ecore_evas_get(evas_object_evas_get(wn->win)), 1);
   ecore_app_args_set(pargc, (const char **)pargv);
   free(nargv);
}

static const char *emotion_choices[] = {
  "auto", "gstreamer", "xine", "generic",
  NULL
};

static const Ecore_Getopt options = {
   PACKAGE_NAME,
   "%prog [options]",
   PACKAGE_VERSION,
   "(C) 2012 Carsten Haitzler and others",
   "BSD 2-Clause",
   "Terminal emulator written with Enlightenment Foundation Libraries.",
   EINA_TRUE,
   {
#if (ECORE_VERSION_MAJOR > 1) || (ECORE_VERSION_MINOR >= 8)
      ECORE_GETOPT_BREAK_STR ('e', "exec",
#else
      ECORE_GETOPT_STORE_STR ('e', "exec",
#endif
                              "command to execute. "
                              "Defaults to $SHELL (or passwd shel or /bin/sh)"),
      ECORE_GETOPT_STORE_STR ('d', "current-directory",
                              "Change to directory for execution of terminal command."),
      ECORE_GETOPT_STORE_STR ('t', "theme",
                              "Use the named edje theme or path to theme file."),
      ECORE_GETOPT_STORE_STR ('b', "background",
                              "Use the named file as a background wallpaper."),
      ECORE_GETOPT_STORE_STR ('g', "geometry",
                              "Terminal geometry to use (eg 80x24 or 80x24+50+20 etc.)."),
      ECORE_GETOPT_STORE_STR ('n', "name",
                              "Set window name."),
      ECORE_GETOPT_STORE_STR ('r', "role",
                              "Set window role."),
      ECORE_GETOPT_STORE_STR ('T', "title",
                              "Set window title."),
      ECORE_GETOPT_STORE_STR ('i', "icon-name",
                              "Set icon name."),
      ECORE_GETOPT_STORE_STR ('f', "font",
                              "Set font (NAME/SIZE for scalable, NAME for bitmap."),
      ECORE_GETOPT_CHOICE    ('v', "video-module",
                              "Set emotion module to use.", emotion_choices),
      ECORE_GETOPT_STORE_BOOL('l', "login",
                              "Run the shell as a login shell."),
      ECORE_GETOPT_STORE_BOOL('m', "video-mute",
                              "Set mute mode for video playback."),
      ECORE_GETOPT_STORE_BOOL('c', "cursor-blink",
                              "Set cursor blink mode."),
      ECORE_GETOPT_STORE_BOOL('G', "visual-bell",
                              "Set visual bell mode."),
      ECORE_GETOPT_STORE_TRUE('F', "fullscreen",
                              "Go into the fullscreen mode from start."),
      ECORE_GETOPT_STORE_TRUE('I', "iconic",
                              "Go into an iconic state from the start."),
      ECORE_GETOPT_STORE_TRUE('B', "borderless",
                              "Become a borderless managed window."),
      ECORE_GETOPT_STORE_TRUE('O', "override",
                              "Become an override-redirect window."),
      ECORE_GETOPT_STORE_TRUE('M', "maximized",
                              "Become maximized from the start."),
      ECORE_GETOPT_STORE_TRUE('W', "nowm",
                              "Terminology is run without a wm."),
      ECORE_GETOPT_STORE_TRUE('H', "hold",
                              "Don't exit when the command process exits."),
      ECORE_GETOPT_STORE_TRUE('s', "single",
                              "Force single executable if multi-instance is enabled.."),
        
      ECORE_GETOPT_VERSION   ('V', "version"),
      ECORE_GETOPT_COPYRIGHT ('C', "copyright"),
      ECORE_GETOPT_LICENSE   ('L', "license"),
      ECORE_GETOPT_HELP      ('h', "help"),
      ECORE_GETOPT_SENTINEL
   }
};

EAPI_MAIN int
elm_main(int argc, char **argv)
{
   char *cmd = NULL;
   char *cd = NULL;
   char *theme = NULL;
   char *background = NULL;
   char *geometry = NULL;
   char *name = NULL;
   char *role = NULL;
   char *title = NULL;
   char *icon_name = NULL;
   char *font = NULL;
   char *video_module = NULL;
   Eina_Bool login_shell = 0xff; /* unset */
   Eina_Bool video_mute = 0xff; /* unset */
   Eina_Bool cursor_blink = 0xff; /* unset */
   Eina_Bool visual_bell = 0xff; /* unset */
   Eina_Bool fullscreen = EINA_FALSE;
   Eina_Bool iconic = EINA_FALSE;
   Eina_Bool borderless = EINA_FALSE;
   Eina_Bool override = EINA_FALSE;
   Eina_Bool maximized = EINA_FALSE;
   Eina_Bool nowm = EINA_FALSE;
   Eina_Bool quit_option = EINA_FALSE;
   Eina_Bool hold = EINA_FALSE;
   Eina_Bool single = EINA_FALSE;
#if (ECORE_VERSION_MAJOR > 1) || (ECORE_VERSION_MINOR >= 8)
   Eina_Bool cmd_options = EINA_FALSE;
#endif   
   Ecore_Getopt_Value values[] = {
#if (ECORE_VERSION_MAJOR > 1) || (ECORE_VERSION_MINOR >= 8)
     ECORE_GETOPT_VALUE_BOOL(cmd_options),
#else
     ECORE_GETOPT_VALUE_STR(cmd),
#endif      
     ECORE_GETOPT_VALUE_STR(cd),
     ECORE_GETOPT_VALUE_STR(theme),
     ECORE_GETOPT_VALUE_STR(background),
     ECORE_GETOPT_VALUE_STR(geometry),
     ECORE_GETOPT_VALUE_STR(name),
     ECORE_GETOPT_VALUE_STR(role),
     ECORE_GETOPT_VALUE_STR(title),
     ECORE_GETOPT_VALUE_STR(icon_name),
     ECORE_GETOPT_VALUE_STR(font),
     ECORE_GETOPT_VALUE_STR(video_module),
      
     ECORE_GETOPT_VALUE_BOOL(login_shell),
     ECORE_GETOPT_VALUE_BOOL(video_mute),
     ECORE_GETOPT_VALUE_BOOL(cursor_blink),
     ECORE_GETOPT_VALUE_BOOL(visual_bell),
     ECORE_GETOPT_VALUE_BOOL(fullscreen),
     ECORE_GETOPT_VALUE_BOOL(iconic),
     ECORE_GETOPT_VALUE_BOOL(borderless),
     ECORE_GETOPT_VALUE_BOOL(override),
     ECORE_GETOPT_VALUE_BOOL(maximized),
     ECORE_GETOPT_VALUE_BOOL(nowm),
     ECORE_GETOPT_VALUE_BOOL(hold),
     ECORE_GETOPT_VALUE_BOOL(single),
      
     ECORE_GETOPT_VALUE_BOOL(quit_option),
     ECORE_GETOPT_VALUE_BOOL(quit_option),
     ECORE_GETOPT_VALUE_BOOL(quit_option),
     ECORE_GETOPT_VALUE_BOOL(quit_option),
      
     ECORE_GETOPT_VALUE_NONE
   };
   Win *wn;
   Term *term;
   Config *config;
   Split *sp;
   int args, retval = EXIT_SUCCESS;
   int remote_try = 0;
   int pos_set = 0, size_set = 0;
   int pos_x = 0, pos_y = 0;
   int size_w = 1, size_h = 1;

   _log_domain = eina_log_domain_register("terminology", NULL);
   if (_log_domain < 0)
     {
        EINA_LOG_CRIT("could not create log domain 'terminology'.");
        elm_shutdown();
        return EXIT_FAILURE;
     }

   config_init();

   config = config_load("config");

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
   elm_app_compile_bin_dir_set(PACKAGE_BIN_DIR);
   elm_app_compile_data_dir_set(PACKAGE_DATA_DIR);
   elm_app_info_set(elm_main, "terminology", "themes/default.edj");

   args = ecore_getopt_parse(&options, values, argc, argv);
   if (args < 0)
     {
        ERR("Could not parse command line options.");
        retval = EXIT_FAILURE;
        goto end;
     }

   if (quit_option) goto end;

#if (ECORE_VERSION_MAJOR > 1) || (ECORE_VERSION_MINOR >= 8)
   if (cmd_options)
     {
        int i;
        Eina_Strbuf *strb;

        if (args == argc)
          {
             fprintf(stdout, "ERROR: option %s requires an argument!\n", argv[args-1]);
             fprintf(stdout, "ERROR: invalid options found. See --help.\n");
             goto end;
          }
        
        strb = eina_strbuf_new();
        for(i = args; i < argc; i++)
          {
             eina_strbuf_append(strb, argv[i]);
             eina_strbuf_append_char(strb, ' ');
          }
        cmd = eina_strbuf_string_steal(strb);
        eina_strbuf_free(strb);
     }
#endif
   
   if (theme)
     {
        char path[PATH_MAX];
        char nom[PATH_MAX];

        if (eina_str_has_suffix(theme, ".edj"))
          eina_strlcpy(nom, theme, sizeof(nom));
        else
          snprintf(nom, sizeof(nom), "%s.edj", theme);

        if (strchr(nom, '/'))
          eina_strlcpy(path, nom, sizeof(path));
        else
          snprintf(path, sizeof(path), "%s/themes/%s",
                   elm_app_data_dir_get(), nom);

        eina_stringshare_replace(&(config->theme), path);
        config->temporary = EINA_TRUE;
     }

   if (background)
     {
        eina_stringshare_replace(&(config->background), background);
        config->temporary = EINA_TRUE;
     }

   if (font)
     {
        if (strchr(font, '/'))
          {
             char *fname = alloca(strlen(font) + 1);
             char *p;
             
             strcpy(fname, font);
             p = strrchr(fname, '/');
             if (p)
               {
                  int sz;
                  
                  *p = 0;
                  p++;
                  sz = atoi(p);
                  if (sz > 0) config->font.size = sz;
                  eina_stringshare_replace(&(config->font.name), fname);
               }
             config->font.bitmap = 0;
          }
        else
          {
             char buf[4096], *file;
             Eina_List *files;
             int n = strlen(font);
             
             snprintf(buf, sizeof(buf), "%s/fonts", elm_app_data_dir_get());
             files = ecore_file_ls(buf);
             EINA_LIST_FREE(files, file)
               {
                  if (n > 0)
                    {
                       if (!strncasecmp(file, font, n))
                         {
                            n = -1;
                            eina_stringshare_replace(&(config->font.name), file);
                            config->font.bitmap = 1;
                         }
                    }
                  free(file);
               }
          }
        config->temporary = EINA_TRUE;
     }

   if (video_module)
     {
        int i;
        for (i = 0; i < (int)EINA_C_ARRAY_LENGTH(emotion_choices); i++)
          {
             if (video_module == emotion_choices[i])
               break;
          }

        if (i == EINA_C_ARRAY_LENGTH(emotion_choices))
          i = 0; /* ecore getopt shouldn't let this happen, but... */
        config->vidmod = i;
        config->temporary = EINA_TRUE;
     }

   if (video_mute != 0xff)
     {
        config->mute = video_mute;
        config->temporary = EINA_TRUE;
     }
   if (cursor_blink != 0xff)
     {
        config->disable_cursor_blink = !cursor_blink;
        config->temporary = EINA_TRUE;
     }
   if (visual_bell != 0xff)
     {
        config->disable_visual_bell = !visual_bell;
        config->temporary = EINA_TRUE;
     }
   
   if (geometry)
     {
        if (sscanf(geometry,"%ix%i+%i+%i", &size_w, &size_h, &pos_x, &pos_y) == 4)
          {
             pos_set = 1;
             size_set = 1;
          }
        else if (sscanf(geometry,"%ix%i-%i+%i", &size_w, &size_h, &pos_x, &pos_y) == 4)
          {
             pos_x = -pos_x;
             pos_set = 1;
             size_set = 1;
          }
        else if (sscanf(geometry,"%ix%i-%i-%i", &size_w, &size_h, &pos_x, &pos_y) == 4)
          {
             pos_x = -pos_x;
             pos_y = -pos_y;
             pos_set = 1;
             size_set = 1;
          }
        else if (sscanf(geometry,"%ix%i+%i-%i", &size_w, &size_h, &pos_x, &pos_y) == 4)
          {
             pos_y = -pos_y;
             pos_set = 1;
             size_set = 1;
          }
        else if (sscanf(geometry,"%ix%i", &size_w, &size_h) == 2)
          {
             size_set = 1;
          }
        else if (sscanf(geometry,"+%i+%i", &pos_x, &pos_y) == 2)
          {
             pos_set = 1;
          }
        else if (sscanf(geometry,"-%i+%i", &pos_x, &pos_y) == 2)
          {
             pos_x = -pos_x;
             pos_set = 1;
          }
        else if (sscanf(geometry,"+%i-%i", &pos_x, &pos_y) == 2)
          {
             pos_y = -pos_y;
             pos_set = 1;
          }
        else if (sscanf(geometry,"-%i-%i", &pos_x, &pos_y) == 2)
          {
             pos_x = -pos_x;
             pos_y = -pos_y;
             pos_set = 1;
          }
     }
   
   // later allow default size to be configured
   if (!size_set)
     {
        size_w = 80;
        size_h = 24;
     }
   
   // for now if not set - dont do login shell - later from config
   if (login_shell == 0xff) login_shell = EINA_FALSE;

   ipc_init();
remote:
   if ((!single) && (config->multi_instance))
     {
        Ipc_Instance inst;
        char cwdbuf[4096];
        
        memset(&inst, 0, sizeof(Ipc_Instance));
        
        inst.cmd = cmd;
        if (cd) inst.cd = cd;
        else inst.cd = getcwd(cwdbuf, sizeof(cwdbuf));
        inst.background = background;
        inst.name = name;
        inst.role = role;
        inst.title = title;
        inst.icon_name = icon_name;
        inst.font = font;
        inst.startup_id = getenv("DESKTOP_STARTUP_ID");
        inst.x = pos_x;
        inst.y = pos_y;
        inst.w = size_w;
        inst.h = size_h;
        inst.pos = pos_set;
        inst.login_shell = login_shell;
        inst.fullscreen = fullscreen;
        inst.iconic = iconic;
        inst.borderless = borderless;
        inst.override = override;
        inst.maximized = maximized;
        inst.hold = hold;
        inst.nowm = nowm;
        if (ipc_instance_add(&inst))
          goto end;
     }
   if ((!single) && (config->multi_instance))
     {
        ipc_instance_new_func_set(main_ipc_new);
        if (!ipc_serve())
          {
             if (remote_try < 1)
               {
                  remote_try++;
                  goto remote;
               }
          }
     }

   wn = main_win_new(name, role, title, icon_name,
                     fullscreen, iconic, borderless, override, maximized);
   // set an env so terminal apps can detect they are in terminology :)
   putenv("TERMINOLOGY=1");
   unsetenv("DESKTOP_STARTUP_ID");

   if (!wn)
     {
        config_del(config);
        retval = EXIT_FAILURE;
        goto end;
     }

   term = main_term_new(wn, config, cmd, login_shell, cd,
                        size_w, size_h, hold);
   if (!term)
     {
        retval = EXIT_FAILURE;
        goto end;
     }
   else
     {
        elm_table_pack(wn->table, term->bg, 0, 0, 1, 1);
        _cb_size_hint(term, evas_object_evas_get(wn->win), term->term, NULL);
     }
   
   sp = wn->split = calloc(1, sizeof(Split));
   sp->wn = wn;
   sp->term = term;
   
   main_trans_update(config);
   main_media_update(config);
   if (pos_set)
     {
        int screen_w, screen_h;

        elm_win_screen_size_get(wn->win, NULL, NULL, &screen_w, &screen_h);
        if (pos_x < 0) pos_x = screen_w + pos_x;
        if (pos_y < 0) pos_y = screen_h + pos_y;
        evas_object_move(wn->win, pos_x, pos_y);
     }
   main_win_sizing_handle(wn);
   evas_object_show(wn->win);
   if (nowm)
     ecore_evas_focus_set
     (ecore_evas_ecore_evas_get(evas_object_evas_get(wn->win)), 1);
   elm_run();
 end:
#if (ECORE_VERSION_MAJOR > 1) || (ECORE_VERSION_MINOR >= 8)
   free(cmd);
#endif

   ipc_shutdown();

   while (wins)
     {
        wn = eina_list_data_get(wins);
        main_win_free(wn);
     }

   config_shutdown();
   eina_log_domain_unregister(_log_domain);
   _log_domain = -1;

   elm_shutdown();
   return retval;
}
ELM_MAIN()
