/**
 * edje_cc -id images/ dnd.edc && gcc -o dnd dnd.c `pkg-config --libs --cflags elementary`
 */

#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Edje.h>
#include <Elementary.h>

#define CRITICAL(...) EINA_LOG_CRIT(__VA_ARGS__)
#define ERR(...)      EINA_LOG_ERR(__VA_ARGS__)
#define WRN(...)      EINA_LOG_WARN(__VA_ARGS__)
#define INF(...)      EINA_LOG_INFO(__VA_ARGS__)
#define DBG(...)      EINA_LOG_DBG(__VA_ARGS__)

#define WIDTH  400
#define HEIGHT 400

#define DRAG_TIMEOUT 0.3
#define ANIM_TIME 0.8

typedef struct _Tab_Item Tab_Item;
struct _Tab_Item {
     const char *title;
     Eina_Bool has_bell;
};

typedef struct _Anim_Icon Anim_Icon;
struct _Anim_Icon
{
   int start_x;
   int start_y;
   Evas_Object *o;
};
typedef struct _Drag_Anim Drag_Anim;
struct _Drag_Anim
{
   Evas_Object *icwin;
   Evas *e;
   Evas_Coord mdx;     /* Mouse-down x */
   Evas_Coord mdy;     /* Mouse-down y */
   Eina_List *icons;   /* List of icons to animate (Anim_Icon) */
   Ecore_Timer *tm;
   Ecore_Animator *ea;
   Evas_Object *gl;
};

static const char  *_edje_file = "dnd.edj";
static Ecore_Evas  *_ee = NULL;
static Evas        *_evas = NULL;
static Evas_Object *_win = NULL;
static Evas_Object *_bg = NULL;
static Evas_Object *_tab_bar= NULL;
static Evas_Object *_tab_main = NULL;
static Evas_Object *_left_box = NULL;
static Evas_Object *_right_box = NULL;
static Evas_Object *_spacer = NULL;
static int _tab_active_idx = 2;
static double _hysteresis_step = 0.0;
static double _tab_orig = 0.0;
static Eina_Bool _on_drag_long = EINA_FALSE;

static void _tab_bar_fill(void);
static void _drag_anim_free(Drag_Anim *anim_st);

#define NB_TABS  4
static Tab_Item _tab_items[NB_TABS] = {
       {
          .title="tab 1 alpha bravo charlie delta",
          .has_bell=EINA_FALSE,
       },
       {
          .title="tab 2 echo foxtrot golf hotel",
          .has_bell=EINA_FALSE,
       },
       {
          .title="tab 3 india juliett kilo lima",
          .has_bell=EINA_FALSE,
       },
       {
          .title="tab 4 mike november oscar papa",
          .has_bell=EINA_FALSE,
       },
};
static Eina_List *_tabs = NULL;


/* Generic {{{ */


/* here just to keep our example's window size and background image's
 * size in synchrony */
static void
_on_canvas_resize(void *data EINA_UNUSED,
                  Evas *_e EINA_UNUSED,
                  Evas_Object *obj EINA_UNUSED,
                  void *_info EINA_UNUSED)
{
   int          w;
   int          h;

   ecore_evas_geometry_get(_ee, NULL, NULL, &w, &h);
   evas_object_resize(_bg, w, h);

   evas_object_geometry_get(_tab_bar, NULL, NULL, &w, &h);
   ERR("tab bar w:%d h:%d", w, h);
   evas_object_geometry_get(_tab_main, NULL, NULL, &w, &h);
   ERR("main tab w:%d h:%d", w, h);
   evas_object_size_hint_min_get(_tab_main, &w, &h);
   ERR("main tab min w:%d h:%d", w, h);
}

static void
_cb_tab_main_change(void *data EINA_UNUSED,
                   Evas *_e EINA_UNUSED,
                   Evas_Object *obj,
                   void *_info EINA_UNUSED)
{
   Evas_Coord w, h;

   evas_object_geometry_get(obj, NULL, NULL, &w, &h);
   ERR("main tab w:%d h:%d", w, h);
   evas_object_size_hint_min_set(_spacer, 1, h);
}

static void
_tab_bar_clear(void)
{
   elm_box_clear(_left_box);
   elm_box_clear(_right_box);
}

static void
_cb_tab_activate(void *data,
                 Evas_Object *_obj EINA_UNUSED,
                 const char *_sig EINA_UNUSED,
                 const char *_src EINA_UNUSED)
{
   Tab_Item *to_focus = data;
   Eina_List *l;
   Tab_Item *item;
   int i = 0;

   EINA_LIST_FOREACH(_tabs, l, item)
     {
        if (item == to_focus)
          {
             _tab_active_idx = i;
             _tab_bar_fill();
             return;
          }
        i++;
     }
}

static void
_tab_bar_fill(void)
{
   int i = -1;
   Tab_Item *item;
   Eina_List *l;
   int n = eina_list_count(_tabs);

   if (n > 1)
       _hysteresis_step = 0.6 / (n - 1);

   _tab_bar_clear();

   EINA_LIST_FOREACH(_tabs, l, item)
     {
        Evas_Object *tab;

        i++;
        if (i == _tab_active_idx)
          {
             double step;

             edje_object_part_text_set(_tab_main, "terminology.title",
                                       item->title);

             if (n > 1)
               {
                  step = 1.0 / (n);
                  _tab_orig = (double)(i) / (double)(n - 1);
               }
             else
               {
                  step = 1.0;
                  _tab_orig = 0.0;
               }

             assert(edje_object_part_drag_size_set(_bg,
                                                   "terminology.main_tab.drag",
                                            step, 0.0) == EINA_TRUE);
             assert(edje_object_part_drag_value_set(_bg, "terminology.main_tab.drag",
                                             _tab_orig, 0.0) == EINA_TRUE);
             continue;
          }
        /* inactive tab */
        if ((i > 0 && i < _tab_active_idx) ||
            (i > _tab_active_idx + 1))
          {
             /* add separator */
             Evas_Object *sep = elm_layout_add(_win);
             elm_layout_file_set(sep, _edje_file, "tab_separator");
             evas_object_size_hint_weight_set(sep, 0.0, EVAS_HINT_EXPAND);
             evas_object_size_hint_fill_set(sep, 0.0, EVAS_HINT_FILL);
             if (i < _tab_active_idx)
               elm_box_pack_end(_left_box, sep);
             else
               elm_box_pack_end(_right_box, sep);
             evas_object_show(sep);
          }
        tab = elm_layout_add(_win);
        elm_layout_file_set(tab, _edje_file, "tab_inactive");
        elm_layout_text_set(tab, "terminology.title", item->title);
        evas_object_size_hint_weight_set(tab, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_fill_set(tab, EVAS_HINT_FILL, EVAS_HINT_FILL);
        if (i < _tab_active_idx)
          elm_box_pack_end(_left_box, tab);
        else
          elm_box_pack_end(_right_box, tab);
        elm_layout_signal_callback_add(tab, "tab,activate", "terminology",
                                       _cb_tab_activate, item);
        evas_object_show(tab);
     }
}

/* }}} */
/* Horizontal drag {{{ */

static void
_on_drag_start(void *data EINA_UNUSED,
               Evas_Object *o EINA_UNUSED,
               const char *emission EINA_UNUSED,
               const char *source EINA_UNUSED)
{
   ERR("START");
}

static void
_on_drag_stop(void *data EINA_UNUSED,
              Evas_Object *o EINA_UNUSED,
              const char *emission EINA_UNUSED,
              const char *source EINA_UNUSED)
{
   ERR("STOP (_on_drag_long:%d)", _on_drag_long);
   if (_on_drag_long)
     {
        edje_object_part_drag_value_set(_bg, "terminology.main_tab.drag",
                                               _tab_orig, 0.0);
        return;
     }
   _tab_bar_fill();
}

static void
_on_drag(void *data EINA_UNUSED,
         Evas_Object *o,
         const char *emission EINA_UNUSED,
         const char *source EINA_UNUSED)
{
   double val;
   int n;

   ERR("drag (_on_drag_long:%d)", _on_drag_long);
   if (_on_drag_long)
     {
        edje_object_part_drag_value_set(_bg, "terminology.main_tab.drag",
                                               _tab_orig, 0.0);
        return;
     }

   n = eina_list_count(_tabs);

   edje_object_part_drag_value_get(o, "terminology.main_tab.drag", &val, NULL);
   if ((_tab_active_idx + 1 < n) && (val > _tab_orig + _hysteresis_step))
     {
        Eina_List *l = eina_list_nth_list(_tabs, _tab_active_idx);
        Eina_List *next = eina_list_next(l);

        _tabs = eina_list_remove_list(_tabs, l);
        _tabs = eina_list_append_relative_list(_tabs,
                                               eina_list_data_get(l),
                                               next);
        _tab_active_idx++;
        _tab_bar_fill();
        return;
     }
   if ((_tab_active_idx > 0) && (val < _tab_orig - _hysteresis_step))
     {
        Eina_List *l = eina_list_nth_list(_tabs, _tab_active_idx);
        Eina_List *prev = eina_list_prev(l);

        _tabs = eina_list_remove_list(_tabs, prev);
        _tabs = eina_list_append_relative_list(_tabs,
                                               eina_list_data_get(prev),
                                               l);
        _tab_active_idx--;
        _tab_bar_fill();
        return;
     }
   ERR("value changed to: %0.3f", val);
   return;
}

static void
_on_bg_resize(void *data EINA_UNUSED,
              Evas *_e EINA_UNUSED,
              Evas_Object *obj EINA_UNUSED,
              void *_info EINA_UNUSED)
{
}

static void
_on_title(void *data EINA_UNUSED,
         Evas_Object *o EINA_UNUSED,
         const char *emission,
         const char *source)
{
   ERR("Received signal '%s' from '%s'", emission, source);
}

/* }}} */
/* Anim {{{ */

static Eina_Bool
_drag_anim_play(void *data, double pos)
{
   /* Impl of the animation of icons, called on frame time */
   Drag_Anim *anim_st = data;
   printf("<%s> <%d>\n", __func__, __LINE__);
   Eina_List *l;
   Anim_Icon *st;

   if (anim_st)
     {
        if (pos > 0.99)
          {
             anim_st->ea = NULL;  /* Avoid deleting on mouse up */

             EINA_LIST_FOREACH(anim_st->icons, l, st)
                evas_object_hide(st->o);   /* Hide animated icons */
             _drag_anim_free(anim_st);
             return ECORE_CALLBACK_CANCEL;
          }

        EINA_LIST_FOREACH(anim_st->icons, l, st)
          {
             int x, y, w, h;
             Evas_Coord xm, ym;
             evas_object_geometry_get(st->o, NULL, NULL, &w, &h);
             evas_pointer_canvas_xy_get(anim_st->e, &xm, &ym);
             x = st->start_x + (pos * (xm - (st->start_x + (w/2))));
             y = st->start_y + (pos * (ym - (st->start_y + (h/2))));
             evas_object_move(st->o, x, y);
          }

        return ECORE_CALLBACK_RENEW;
     }

   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_drag_anim_start(void *data)
{
   /* Start icons animation before actually drag-starts */
   Drag_Anim *anim_st = data;
   int w, h;
   Anim_Icon *st = calloc(1, sizeof(*st));
   Evas_Object *ic = elm_icon_add(anim_st->gl);

   ERR("anim start");
   edje_object_signal_emit(_bg, "hdrag,off", "terminology");

   elm_image_file_set(ic, _edje_file, "terminology/tab_icon");
   evas_object_geometry_get(_tab_main, &st->start_x, &st->start_y, &w, &h);
   evas_object_size_hint_align_set(ic,
                                   EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(ic,
                                    EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

   evas_object_move(ic, st->start_x, st->start_y);
   evas_object_resize(ic, w, h);
   evas_object_show(ic);

   _on_drag_long = EINA_TRUE;
   st->o = ic;
   anim_st->icons = eina_list_append(anim_st->icons, st);

   anim_st->tm = NULL;
   anim_st->ea = ecore_animator_timeline_add(DRAG_TIMEOUT,
         _drag_anim_play, anim_st);

   return ECORE_CALLBACK_CANCEL;
}

static void
_tab_main_mouse_up(
   void *data,
   Evas *e EINA_UNUSED,
   Evas_Object *obj EINA_UNUSED,
   void *event_info EINA_UNUSED)
{
   /* Cancel any drag waiting to start on timeout */
   Drag_Anim *anim_st = data;

   edje_object_signal_emit(_bg, "hdrag,on", "terminology");
   _drag_anim_free(anim_st);
}

static void
_tab_main_mouse_move(
   void *data,
   Evas *e EINA_UNUSED,
   Evas_Object *obj EINA_UNUSED,
   void *event_info)
{
   /* Cancel any drag waiting to start on timeout */
   if (((Evas_Event_Mouse_Move *)event_info)->event_flags & EVAS_EVENT_FLAG_ON_HOLD)
     {
        Drag_Anim *anim_st = data;

        _drag_anim_free(anim_st);
     }
}

static void
_tab_main_mouse_down(
   void *data,
   Evas *e EINA_UNUSED,
   Evas_Object *obj EINA_UNUSED,
   void *event_info)
{
   /* Launch a timer to start drag animation */
   Evas_Event_Mouse_Down *ev = event_info;
   Drag_Anim *anim_st = calloc(1, sizeof(*anim_st));

   anim_st->e = e;
   anim_st->mdx = ev->canvas.x;
   anim_st->mdy = ev->canvas.y;
   anim_st->gl = data;
   anim_st->tm = ecore_timer_add(DRAG_TIMEOUT, _drag_anim_start, anim_st);
   evas_object_event_callback_add(data, EVAS_CALLBACK_MOUSE_UP,
         _tab_main_mouse_up, anim_st);
   evas_object_event_callback_add(data, EVAS_CALLBACK_MOUSE_MOVE,
         _tab_main_mouse_move, anim_st);
}

static void
_drag_anim_free(Drag_Anim *anim_st)
{
   /* Stops and free mem of ongoing animation */
   printf("<%s> <%d>\n", __func__, __LINE__);
   if (!anim_st)
     return;
   evas_object_event_callback_del_full(
      anim_st->gl, EVAS_CALLBACK_MOUSE_MOVE,
      _tab_main_mouse_move, anim_st);
   evas_object_event_callback_del_full(
      anim_st->gl, EVAS_CALLBACK_MOUSE_UP,
      _tab_main_mouse_up, anim_st);

   ecore_timer_del(anim_st->tm);
   anim_st->tm = NULL;

   ecore_animator_del(anim_st->ea);
   anim_st->ea = NULL;

   /*
      Anim_Icon *st;

      EINA_LIST_FREE(anim_st->icons, st)
      {
      evas_object_hide(st->o);
      evas_object_del(st->o);
      free(st);
      }

*/
   free(anim_st);
}

/* }}} */
/* Setup {{{ */

static void
_tab_bar_setup(void)
{
   Evas_Coord w = 0, h = 0;
   int i;

   for (i = 0; i < NB_TABS; i++)
       _tabs = eina_list_append(_tabs, _tab_items + i);

   _spacer = evas_object_rectangle_add(_evas);
   evas_object_color_set(_spacer, 0, 0, 0, 0);
   elm_coords_finger_size_adjust(1, &w, 1, &h);
   evas_object_size_hint_min_set(_spacer, w, h);
   edje_object_part_swallow(_bg, "terminology.tab",
                            _spacer);
   evas_object_show(_spacer);

   _tab_main = elm_layout_add(_win);
   elm_layout_file_set(_tab_main, _edje_file, "terminology/tab_main");
   edje_object_part_swallow(_bg, "terminology.main_tab",
                            _tab_main);
   elm_layout_text_set(_tab_main, "terminology.title", "foo bar 42");
   evas_object_size_hint_weight_set(_tab_main, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(_tab_main, EVAS_HINT_FILL, EVAS_HINT_FILL);
   edje_object_signal_callback_add(_bg, "tab,drag", "*",
                                   _on_drag, NULL);
   edje_object_signal_callback_add(_bg, "tab,drag,start", "*",
                                   _on_drag_start, NULL);
   edje_object_signal_callback_add(_bg, "tab,drag,stop", "*",
                                   _on_drag_stop, NULL);
   edje_object_signal_callback_add(_tab_main, "tab,title", "*",
                                   _on_title, NULL);
   evas_object_event_callback_add(_tab_main, EVAS_CALLBACK_MOUSE_DOWN,
                                  _tab_main_mouse_down, _tab_main);

   edje_object_size_min_calc(_tab_main, &w, &h);
   ERR("min: %d %d", w, h);
   evas_object_size_hint_min_set(_tab_main, w, h);
   evas_object_event_callback_add(_tab_main, EVAS_CALLBACK_RESIZE,
                                  _cb_tab_main_change, NULL);
   evas_object_show(_tab_main);

   _left_box = elm_box_add(_win);
   assert(_left_box);
   elm_box_horizontal_set(_left_box, EINA_TRUE);
   elm_box_homogeneous_set(_left_box, EINA_FALSE);
   evas_object_size_hint_weight_set(_left_box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(_left_box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   edje_object_part_swallow(_bg, "terminology.tabl.content", _left_box);

   _right_box = elm_box_add(_win);
   assert(_right_box);
   elm_box_horizontal_set(_right_box, EINA_TRUE);
   elm_box_homogeneous_set(_right_box, EINA_FALSE);
   evas_object_size_hint_weight_set(_right_box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(_right_box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   edje_object_part_swallow(_bg, "terminology.tabr.content", _right_box);


   edje_object_signal_emit(_bg, "tab_bar,on", "terminology");
   evas_object_show(_tab_bar);
}

EAPI_MAIN int
elm_main(int argc EINA_UNUSED, char **argv EINA_UNUSED)
{
   Evas_Object *rect;

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
   _win = elm_win_util_standard_add("tabtest", "Tab Drag Test");
   elm_win_autodel_set(_win, EINA_TRUE);

   /* and now just resize the window to a size you want. normally widgets
    * will determine the initial size though */
   evas_object_resize(_win, 400, 400);
   /* and show the window */
   evas_object_show(_win);
   _evas = evas_object_evas_get(_win);
   assert(_evas);
   _ee = ecore_evas_ecore_evas_get(_evas);
   assert(_ee);

   evas_object_event_callback_add(_win, EVAS_CALLBACK_RESIZE,
                                  _on_canvas_resize, NULL);

   _bg = edje_object_add(_evas);

   if (!edje_object_file_set(_bg, _edje_file, "main"))
     printf("failed to set file %s.\n", _edje_file);

   evas_object_move(_bg, 0, 0);
   evas_object_resize(_bg, WIDTH, HEIGHT);
   evas_object_show(_bg);
   ecore_evas_data_set(_ee, "bg", _bg);

   evas_object_event_callback_add(_bg, EVAS_CALLBACK_RESIZE,
                                  _on_bg_resize, NULL);

   rect = evas_object_rectangle_add(_evas);
   evas_object_color_set(rect, 0, 0, 0, 0);
   edje_object_part_swallow(_bg, "terminology.content", rect);
   evas_object_size_hint_min_set(rect, WIDTH, HEIGHT - 32);
   evas_object_size_hint_weight_set(rect, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(rect, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(rect);


   _tab_bar_setup();
   _tab_bar_fill();

   /* TODO */

   ecore_evas_show(_ee);

   elm_run();

   ecore_evas_free(_ee);

   eina_list_free(_tabs);

   return EXIT_SUCCESS;
}

ELM_MAIN()
/* }}} */
