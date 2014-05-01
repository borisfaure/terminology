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

   int img_h;
   int img_hist; /* history rendered is between img_hpos(<0) and
                    img_pos - image_height */
   int img_off; /* >0; offset for the visible part */
   int viewport_h;
   int rows;
   int columns;

   int is_shown;
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
                  // TODO: get pixel colors from current themee...
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
_scroll(Miniview *mv, int z)
{
   int history_len = mv->pty->backscroll_num,
       new_offset,
       d;
   Evas_Coord ox, oy;

   /* whether to move img or modify it */
   DBG("history_len:%d z:%d img:h:%d hist:%d off:%d viewport:%d rows:%d",
       history_len, z, mv->img_h, mv->img_hist, mv->img_off, mv->viewport_h,
       mv->rows);
   /* top? */
   if ((mv->img_hist == -history_len && mv->img_off == 0  && z < 0))
     {
        DBG("TOP");
        return;
     }
   /* bottom? */
   if (mv->img_hist + mv->viewport_h + mv->img_off >= mv->rows && z > 0) /* bottom */
     {
        DBG("BOTTOM");
        return;
     }

   evas_object_geometry_get(mv->img, &ox, &oy, NULL, NULL);

   new_offset = mv->img_off + z;

   if (new_offset >= 0 &&
       new_offset + mv->viewport_h <= mv->img_h)
     {
        /* move */
        /*
        if (z > 0)
          // TODO: boundaries
          d = MIN(mv->viewport_h + z, history_len + mv->img_hist);
        else
          d = MIN(mv->viewport_h + z, history_len + mv->img_hist);
        */

        mv->img_off = new_offset;
        evas_object_move(mv->img, ox, oy - z);
     }
   else
     {
        Termcell *cells;
        unsigned int *pixels;
        int y, wret;

        pixels = evas_object_image_data_get(mv->img, EINA_TRUE);
        /*TODO: memmove, be more efficient: ftm, redraw it all */
        if (z > 0)
          {
             /* draw bottom */
             if (mv->img_hist + z + mv->viewport_h > mv->rows)
               {
                  mv->img_hist = mv->rows - mv->viewport_h;
                  mv->img_off = 0;
               }
             else
               {
                  mv->img_hist += mv->viewport_h + z;
                  mv->img_off = mv->viewport_h;
               }

             /* TODO: boris not working: offset issue */
             memset(pixels, 0, sizeof(*pixels) * mv->columns * mv->img_h);
             for (y = 0; y < mv->img_h; y++)
               {
                  cells = termpty_cellrow_get(mv->pty, mv->img_hist + y, &wret);
                  if (cells == NULL)
                    break;

                  _draw_line(&pixels[y * mv->columns], cells, wret);
               }
             mv->img_off = mv->viewport_h;
             evas_object_move(mv->img,
                              ox,
                              -mv->img_off);
          }
        else
          {
             if (mv->viewport_h + z <  history_len + mv->img_hist)
               {
                  d = mv->viewport_h + z;
                  mv->img_off = d;
               }
             else
               {
                  d = history_len + mv->img_hist;
                  mv->img_off = 0;
               }
             mv->img_hist -= d;
             if (!d) return;

             /* draw top */
             memset(pixels, 0, sizeof(*pixels) * mv->columns * mv->img_h);
             for (y = 0; y < mv->img_h; y++)
               {
                  cells = termpty_cellrow_get(mv->pty, mv->img_hist + y, &wret);
                  if (cells == NULL)
                    break;

                  _draw_line(&pixels[y * mv->columns], cells, wret);
               }
             evas_object_move(mv->img,
                              ox,
                              - mv->img_off);
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

   _scroll(mv, ev->z * 10);
}

static void
_smart_cb_key_down(void *data, Evas *e EINA_UNUSED,
                   Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Key_Down *ev = event;
   Miniview *mv = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN(mv);

   /* TODO handle keybinding to hide */

   if (!strcmp(ev->key, "Prior"))
        _scroll(mv, -10);
   else if (!strcmp(ev->key, "Next"))
        _scroll(mv, 10);
}

static void
_smart_cb_mouse_in(void *data, Evas *e EINA_UNUSED,
                   Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Miniview *mv = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN(mv);

   elm_object_focus_set(mv->img, EINA_TRUE);
}

static void
_smart_cb_mouse_out(void *data, Evas *e EINA_UNUSED,
                   Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Miniview *mv = evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN(mv);

   elm_object_focus_set(mv->img, EINA_FALSE);
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
   evas_object_event_callback_add(o, EVAS_CALLBACK_KEY_DOWN,
                                  _smart_cb_key_down, obj);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_IN,
                                  _smart_cb_mouse_in, obj);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_OUT,
                                  _smart_cb_mouse_out, obj);
}

static void
_smart_del(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   DBG("%p", obj);
   if (!mv) return;
   /* TODO */
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

    DBG("smart show obj:%p mv:%p", obj, mv);
   if (!mv) return;

   if (!mv->is_shown)
     {
        mv->is_shown = 1;
        miniview_redraw(obj, mv->columns, mv->rows);
        evas_object_show(mv->img);
     }
   /*
   Evas_Coord ox, oy, ow, oh;
   evas_object_geometry_get(mv->img, &ox, &oy, &ow, &oh);
   DBG("ox:%d oy:%d ow:%d oh:%d visible:%d|%d %d %d %d",
       ox, oy, ow, oh,
       evas_object_visible_get(obj),
       evas_object_visible_get(mv->img),
       evas_object_layer_get(mv->img),
       evas_object_layer_get(obj),
       evas_object_layer_get(mv->termio));
       */

}

static void
_smart_hide(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);

    DBG("smart hide obj:%p mv:%p", obj, mv);
   if (!mv) return;

   if (mv->is_shown)
     {
        mv->is_shown = 0;
        evas_object_hide(mv->img);
     }
}

void
miniview_redraw(Evas_Object *obj, int columns, int rows)
{
   Miniview *mv;
   Evas_Coord ox, oy, ow, oh;
   int history_len, h, y, wret;
   unsigned int *pixels;
   Termcell *cells;

   if (!obj) return;
   mv = evas_object_smart_data_get(obj);
   if (!mv) return;

   mv->columns = columns;
   mv->rows = rows;
   if (!mv->is_shown) return;
   DBG("smart size %p", obj);

   evas_object_geometry_get(mv->termio, &ox, &oy, &ow, &oh);
   if (ow == 0 || oh == 0) return;

   mv->img_h = 3 * oh;

   DBG("ox:%d oy:%d ow:%d oh:%d columns:%d rows:%d",
       ox, oy, ow, oh, mv->columns, mv->rows);

   evas_object_resize(mv->img, mv->columns, mv->img_h);
   evas_object_image_size_set(mv->img, mv->columns, mv->img_h);

   evas_object_image_fill_set(mv->img, 0, 0, mv->columns, mv->img_h);

   history_len = mv->pty->backscroll_num;

   DBG("backscroll_num:%d backmax:%d backpos:%d",
       mv->pty->backscroll_num, mv->pty->backmax, mv->pty->backpos);

   pixels = evas_object_image_data_get(mv->img, EINA_TRUE);
   memset(pixels, 0, sizeof(*pixels) * mv->columns * mv->img_h);

   mv->viewport_h = oh;
   h = mv->img_h - mv->rows;
   if (h < history_len)
     {
        mv->img_hist = mv->rows - mv->img_h;
     }
   else
     {
        mv->img_hist = -history_len;
     }

   DBG("img_h:%d history_len:%d h:%d img_hist:%d vph:%d",
       mv->img_h, history_len, h, mv->img_hist, mv->viewport_h);

   for (y = 0; y < mv->img_h; y++)
     {
        cells = termpty_cellrow_get(mv->pty, mv->img_hist + y, &wret);
        if (cells == NULL)
          {
             DBG("y:%d get:%d", y, mv->img_hist + y);
          break;
          }

        _draw_line(&pixels[y * mv->columns], cells, wret);
     }


   if (y > mv->viewport_h)
     {
        mv->img_off = y - mv->viewport_h;
        evas_object_move(mv->img,
                         ox + ow - mv->columns,
                         - mv->img_off);
     }
   else
     {
        mv->img_off = 0;
        evas_object_move(mv->img,
                         ox + ow - mv->columns,
                         0);
     }

   DBG("history_len:%d img:h:%d hist:%d off:%d viewport:%d",
       history_len, mv->img_h, mv->img_hist, mv->img_off,mv->viewport_h);
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
   Evas_Coord ow, oh, font_w, font_h;

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

   evas_object_geometry_get(mv->termio, NULL, NULL, &ow, &oh);
   if (ow == 0 || oh == 0) return obj;
   evas_object_size_hint_min_get(mv->termio, &font_w, &font_h);

   if (font_w <= 0 || font_h <= 0) return obj;

   miniview_redraw(obj, ow / font_w, oh / font_h);

   return obj;
}

