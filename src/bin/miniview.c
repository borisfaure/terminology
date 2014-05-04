#include <Elementary.h>
#include <stdio.h>
#include <assert.h>

#include "miniview.h"
#include "col.h"
#include "termpty.h"
#include "termio.h"
#include "main.h"

/* specific log domain to help debug only miniview */
int _miniview_log_dom = -1;

#undef CRITICAL
#undef ERR
#undef WRN
#undef INF
#undef DBG

#define CRIT(...)     EINA_LOG_DOM_CRIT(_miniview_log_dom, __VA_ARGS__)
#define ERR(...)      EINA_LOG_DOM_ERR(_miniview_log_dom, __VA_ARGS__)
#define WRN(...)      EINA_LOG_DOM_WARN(_miniview_log_dom, __VA_ARGS__)
#define INF(...)      EINA_LOG_DOM_INFO(_miniview_log_dom, __VA_ARGS__)
#define DBG(...)      EINA_LOG_DOM_DBG(_miniview_log_dom, __VA_ARGS__)

/*TODO: work with splits */

void
miniview_init(void)
{
   if (_miniview_log_dom >= 0) return;

   _miniview_log_dom = eina_log_domain_register("miniview", NULL);
   if (_miniview_log_dom < 0)
     EINA_LOG_CRIT("could not create log domain 'miniview'.");
}

void
miniview_shutdown(void)
{
   if (_miniview_log_dom < 0) return;
   eina_log_domain_unregister(_miniview_log_dom);
   _miniview_log_dom = -1;
}

typedef struct _Miniview Miniview;

struct _Miniview
{
   Evas_Object *self;
   Evas_Object *img;
   Evas_Object *termio;
   Termpty *pty;

   int img_hist; /* history rendered is between img_hist (<0) and
                    img_hist + img_h */
   unsigned img_h;
   unsigned rows;
   unsigned cols;

   Ecore_Timer *deferred_renderer;

   int is_shown : 1;
   int to_render : 1;
};

static Evas_Smart *_smart = NULL;



static void
_draw_line(unsigned int *pixels, Termcell *cells, int length)
{
   int x;

   for (x = 0 ; x < length; x++)
     {
        int r, g, b;

        if (cells[x].codepoint > 0 && !isspace(cells[x].codepoint) &&
            !cells[x].att.newline && !cells[x].att.tab &&
            !cells[x].att.invisible && cells[x].att.bg != COL_INVIS)
          {
             switch (cells[x].att.fg)
               {
                  // TODO: get pixel colors from current theme...
                case 0:
                   r = 180; g = 180; b = 180;
                   break;
                case 2:
                   r = 204; g = 51; b = 51;
                   break;
                case 3:
                   r = 51; g = 204; b = 51;
                   break;
                case 4:
                   r = 204; g = 136; b = 51;
                   break;
                case 5:
                   r = 51; g = 51; b = 204;
                   break;
                case 6:
                   r = 204; g = 51; b = 204;
                   break;
                case 7:
                   r = 51; g = 204; b = 204;
                   break;
                default:
                   r = 180; g = 180; b = 180;
               }
             pixels[x] = (0xff << 24) | (r << 16) | (g << 8) | b;
          }
     }
}

static void
_smart_cb_mouse_wheel(void *data, Evas *e EINA_UNUSED,
                      Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Mouse_Wheel *ev = event;
   Miniview *mv = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN(mv);

   /* do not handle horizontal scrolling */
   if (ev->direction) return;

   DBG("ev->z:%d", ev->z);

   mv->img_hist += ev->z * 10;
   mv->to_render = 1;
}

static void
_smart_add(Evas_Object *obj)
{
   Miniview *mv;
   Evas_Object *o;
   Evas *canvas = evas_object_evas_get(obj);

   DBG("%p", obj);

   mv = calloc(1, sizeof(Miniview));
   EINA_SAFETY_ON_NULL_RETURN(mv);
   evas_object_smart_data_set(obj, mv);

   mv->self = obj;

   /* miniview output widget  */
   o = evas_object_image_add(canvas);
   evas_object_image_alpha_set(o, EINA_TRUE);

   evas_object_smart_member_add(o, obj);
   mv->img = o;

   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_WHEEL,
                                  _smart_cb_mouse_wheel, obj);
}

static void
_smart_del(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   DBG("%p", obj);
   if (!mv) return;

   ecore_timer_del(mv->deferred_renderer);

   evas_object_del(mv->img);
   free(mv);
   DBG("%p", obj);
}

static void
_smart_move(Evas_Object *obj, Evas_Coord x EINA_UNUSED, Evas_Coord y EINA_UNUSED)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   if (!mv) return;
   /* TODO */
   DBG("%p x:%d y:%d", obj, x, y);
   evas_object_move(mv->img, x, y);
}

static void
_smart_show(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   if (!mv) return;

   if (!mv->is_shown)
     {
        Evas_Coord ox, oy, ow, oh, font_w, font_h;

        mv->is_shown = 1;
        mv->img_hist = 0;

        evas_object_geometry_get(mv->termio, &ox, &oy, &ow, &oh);
        if (ow == 0 || oh == 0) return;
        evas_object_size_hint_min_get(mv->termio, &font_w, &font_h);

        if (font_w <= 0 || font_h <= 0) return;

        mv->img_h = oh;
        mv->rows = oh / font_h;
        mv->cols = ow / font_w;

        if (mv->rows == 0 || mv->cols == 0) return;

        evas_object_resize(mv->img, mv->cols, mv->img_h);
        evas_object_image_size_set(mv->img, mv->cols, mv->img_h);

        evas_object_image_fill_set(mv->img, 0, 0, mv->cols, mv->img_h);
        evas_object_move(mv->img, ox + ow - mv->cols, oy);

        mv->to_render = 1;
        evas_object_show(mv->img);
     }
}

static void
_smart_hide(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   if (!mv) return;

   if (mv->is_shown)
     {
        mv->is_shown = 0;
        evas_object_hide(mv->img);
     }
}

void
miniview_redraw(Evas_Object *obj)
{
   Miniview *mv;
   if (!obj) return;
   mv = evas_object_smart_data_get(obj);
   if (!mv || !mv->is_shown) return;

   mv->to_render = 1;
}

static Eina_Bool
_deferred_renderer(void *data)
{
   Miniview *mv = data;
   Evas_Coord ox, oy, ow, oh;
   int history_len, wret;
   unsigned int *pixels, y;
   Termcell *cells;

   if (!mv || !mv->is_shown || !mv->to_render) return EINA_TRUE;

   evas_object_geometry_get(mv->termio, &ox, &oy, &ow, &oh);
   if (ow == 0 || oh == 0) return EINA_TRUE;

   history_len = mv->pty->backscroll_num;

   pixels = evas_object_image_data_get(mv->img, EINA_TRUE);
   memset(pixels, 0, sizeof(*pixels) * mv->cols * mv->img_h);

   DBG("history_len:%d hist:%d img_h:%d rows:%d cols:%d",
       history_len, mv->img_hist, mv->img_h, mv->rows, mv->cols);
   /* "current"? */
   if (mv->img_hist >= - ((int)mv->img_h - (int)mv->rows))
     mv->img_hist = -((int)mv->img_h - (int)mv->rows);
   if (mv->img_hist < -history_len)
     mv->img_hist = -history_len;

   for (y = 0; y < mv->img_h; y++)
     {
        cells = termpty_cellrow_get(mv->pty, mv->img_hist + y, &wret);
        if (cells == NULL)
          {
             DBG("y:%d get:%d", y, mv->img_hist + y);
          break;
          }

        _draw_line(&pixels[y * mv->cols], cells, wret);
     }
   DBG("history_len:%d hist:%d img_h:%d rows:%d cols:%d",
       history_len, mv->img_hist, mv->img_h, mv->rows, mv->cols);
   evas_object_image_data_set(mv->img, pixels);
   evas_object_image_data_update_add(mv->img, 0, 0, mv->cols, mv->img_h);

   mv->to_render = 0;

   return EINA_TRUE;
}


static void
_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   Miniview *mv = evas_object_smart_data_get(obj);
   if (!mv) return;

   DBG("smart resize %p w:%d h:%d", obj, w, h);
   evas_object_resize(mv->img, w, h);
}


static void
_smart_init(void)
{
    static Evas_Smart_Class sc = EVAS_SMART_CLASS_INIT_NULL;

    DBG("smart init");

    sc.name      = "miniview";
    sc.version   = EVAS_SMART_CLASS_VERSION;
    sc.add       = _smart_add;
    sc.del       = _smart_del;
    sc.resize    = _smart_resize;
    sc.move      = _smart_move;
    sc.show      = _smart_show;
    sc.hide      = _smart_hide;
    _smart = evas_smart_class_new(&sc);
}

Evas_Object *
miniview_add(Evas_Object *parent, Evas_Object *termio)
{
   Evas *e;
   Evas_Object *obj;
   Miniview *mv;

   EINA_SAFETY_ON_NULL_RETURN_VAL(parent, NULL);
   e = evas_object_evas_get(parent);
   if (!e) return NULL;

   DBG("ADD parent:%p", parent);

   if (!_smart) _smart_init();

   obj = evas_object_smart_add(e, _smart);
   mv = evas_object_smart_data_get(obj);
   if (!mv) return obj;

   mv->termio = termio;
   mv->pty = termio_pty_get(termio);
   mv->deferred_renderer = ecore_timer_add(0.1, _deferred_renderer, mv);

   return obj;
}

