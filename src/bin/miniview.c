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

   int img_hist; /* history rendered is between img_hist (<0) and
                    img_hist + img_h */
   unsigned img_h;
   unsigned rows;
   unsigned cols;

   Ecore_Timer *deferred_renderer;

   unsigned int is_shown : 1;
   unsigned int to_render : 1;
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
_smart_cb_mouse_down(void *data, Evas *e EINA_UNUSED,
                     Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Mouse_Down *ev = event;
   Miniview *mv= evas_object_smart_data_get(data);
   int pos;
   Evas_Coord oy;

   EINA_SAFETY_ON_NULL_RETURN(mv);

   evas_object_geometry_get(mv->img, NULL, &oy, NULL, NULL);
   pos = oy - ev->canvas.y;
   pos -= mv->img_hist;
   if (pos < 0)
     pos = 0;
   else
     pos += mv->rows / 2;
   termio_scroll_set(mv->termio, pos);
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

   //evas_object_smart_member_add(o, obj);
   mv->img = o;
   mv->is_shown = 0;
   mv->to_render = 0;
   mv->img_h = 0;
   mv->rows = 0;
   mv->cols = 0;

   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_WHEEL,
                                  _smart_cb_mouse_wheel, obj);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _smart_cb_mouse_down, obj);
}

static void
_smart_del(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   if (!mv) return;
   DBG("del obj:%p mv:%p", obj, mv);

   ecore_timer_del(mv->deferred_renderer);

   //evas_object_smart_member_del(mv->img);
   evas_object_del(mv->img);
   free(mv);
}

static void
_smart_move(Evas_Object *obj, Evas_Coord x EINA_UNUSED, Evas_Coord y EINA_UNUSED)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   if (!mv) return;
   DBG("%p x:%d y:%d", obj, x, y);
   evas_object_move(mv->img, x + mv->img_h - mv->cols, y);
   mv->to_render = 1;
}

static void
_smart_show(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   DBG("mv:%p is_shown:%d", mv, mv != NULL ? mv->is_shown : -1);

   if (!mv) return;

   Evas_Coord ox, oy, ow, oh;
   evas_object_geometry_get(mv->img, &ox, &oy, &ow, &oh);
   DBG("ox:%d oy:%d ow:%d oh:%d", ox, oy, ow, oh);
   if (!mv->is_shown)
     {
        Evas_Coord /*ox, oy, ow, oh,*/ font_w, font_h;

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

        DBG("ox:%d ow:%d cols:%d oy:%d", ox, ow, mv->cols, oy);
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
   Termpty *ty;

   if (!mv || !mv->is_shown || !mv->to_render || mv->img_h == 0)
     return EINA_TRUE;

   ty = termio_pty_get(mv->termio);
   evas_object_geometry_get(mv->termio, &ox, &oy, &ow, &oh);
   if (ow == 0 || oh == 0) return EINA_TRUE;

   history_len = ty->backscroll_num;

   evas_object_image_size_get(mv->img, &ow, &oh);
   if (ow != (Evas_Coord)mv->cols || oh != (Evas_Coord)mv->img_h)
     return EINA_TRUE;


   DBG("ow:%d oh:%d cols:%d img_h:%d", ow, oh, mv->cols, mv->img_h);
   pixels = evas_object_image_data_get(mv->img, EINA_TRUE);
   memset(pixels, 0, sizeof(*pixels) * ow * oh);
   mv->img_h = oh;

   DBG("history_len:%d hist:%d img_h:%d rows:%d cols:%d",
       history_len, mv->img_hist, mv->img_h, mv->rows, mv->cols);
   /* "current"? */
   if (mv->img_hist >= - ((int)mv->img_h - (int)mv->rows))
     mv->img_hist = -((int)mv->img_h - (int)mv->rows);
   if (mv->img_hist < -history_len)
     mv->img_hist = -history_len;

   for (y = 0; y < mv->img_h; y++)
     {
        cells = termpty_cellrow_get(ty, mv->img_hist + y, &wret);
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
   evas_object_image_pixels_dirty_set(mv->img, EINA_FALSE);
   evas_object_image_data_update_add(mv->img, 0, 0, ow, oh);

   mv->to_render = 0;

   return EINA_TRUE;
}


static void
_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   Evas_Coord font_w, font_h, ox, oy;
   Miniview *mv = evas_object_smart_data_get(obj);
   if (!mv) return;

   DBG("smart resize %p w:%d h:%d", obj, w, h);
   mv->img_h = h;

   evas_object_geometry_get(mv->termio, &ox, &oy, NULL, NULL);
   evas_object_size_hint_min_get(mv->termio, &font_w, &font_h);
   if (font_w <= 0 || font_h <= 0) return;

   mv->rows = h / font_h;
   mv->cols = w / font_w;

   if (mv->rows == 0 || mv->cols == 0) return;

   evas_object_resize(mv->img, mv->cols, mv->img_h);
   evas_object_image_size_set(mv->img, mv->cols, mv->img_h);

   evas_object_image_fill_set(mv->img, 0, 0, mv->cols, mv->img_h);
   evas_object_move(mv->img, ox + w - mv->cols, oy);
   mv->to_render = 1;
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


   if (!_smart) _smart_init();

   obj = evas_object_smart_add(e, _smart);
   mv = evas_object_smart_data_get(obj);
   if (!mv) return obj;
   DBG("ADD parent:%p mv:%p", parent, mv);

   mv->termio = termio;
   mv->deferred_renderer = ecore_timer_add(0.1, _deferred_renderer, mv);

   return obj;
}

