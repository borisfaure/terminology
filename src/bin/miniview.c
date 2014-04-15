#include <Elementary.h>
#include <stdio.h>
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

   ERR("INIT");
}

void
miniview_shutdown(void)
{
   if (_miniview_log_dom < 0) return;
   ERR("SHUTDOWN");
   eina_log_domain_unregister(_miniview_log_dom);
   _miniview_log_dom = -1;
}

typedef struct _Miniview Miniview;

struct _Miniview
{
   Evas_Object *self;
   Evas_Object *image_obj;
   Evas_Object *termio;
   Termpty *pty;
   int scroll;
};

static Evas_Smart *_smart = NULL;

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
   evas_object_color_set(o, 128, 0, 0, 128);

   evas_object_smart_member_add(o, obj);
   mv->image_obj = o;
   evas_object_show(o);
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
   evas_object_move(mv->image_obj, x, y);
}

static void
_smart_show(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   DBG("%p", obj);
   if (!mv) return;

   Evas_Coord ox, oy, ow, oh;
   evas_object_geometry_get(mv->image_obj, &ox, &oy, &ow, &oh);
   DBG("ox:%d oy:%d ow:%d oh:%d visible:%d|%d %d %d %d",
       ox, oy, ow, oh,
       evas_object_visible_get(obj),
       evas_object_visible_get(mv->image_obj),
       evas_object_layer_get(mv->image_obj),
       evas_object_layer_get(obj),
       evas_object_layer_get(mv->termio));
   evas_object_show(mv->image_obj);
}

static void
_smart_hide(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   DBG("%p", obj);
   if (!mv) return;

   evas_object_hide(mv->image_obj);
}


static void
_miniview_draw(Miniview *mv, int columns, int oh)
{
   unsigned int *pixels;
   int x, y;

   pixels = evas_object_image_data_get(mv->image_obj, EINA_TRUE);
   memset(pixels, 0, sizeof(*pixels) * columns * oh);


   DBG("DRAW");
   for (y = 0; y < oh; y++)
     {
        Termcell *cells;
        int wret;

        cells = termpty_cellrow_get(mv->pty, -y, &wret);
        if (cells == NULL)
          break;
        for (x = 0 ; x < wret; x++)
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
                  pixels[x + y * columns] = (r << 16) | (g << 8) | (b);
               }
          }
     }
}

static void
_smart_size(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh, font_w;
   int columns;

   if (!mv) return;

   DBG("smart size %p", obj);

   evas_object_geometry_get(mv->termio, &ox, &oy, &ow, &oh);
   if (ow == 0 || oh == 0) return;
   evas_object_size_hint_min_get(mv->termio, &font_w, NULL);

   if (font_w <= 0) return;

   columns = ow / font_w;

   DBG("ox:%d oy:%d ow:%d oh:%d font_w:%d columns:%d",
       ox, oy, ow, oh, font_w, columns);

   evas_object_resize(mv->image_obj, columns, oh);
   evas_object_image_size_set(mv->image_obj, columns, oh);
   evas_object_move(mv->image_obj, ox + ow - columns, oy);

   evas_object_image_fill_set(mv->image_obj, 0, 0, columns,
                              oh);

   _miniview_draw(mv, columns, oh);
   evas_object_show(mv->image_obj);
}





static void
_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   Miniview *mv = evas_object_smart_data_get(obj);
   if (!mv) return;

   DBG("smart resize %p w:%d h:%d", obj, w, h);
   evas_object_resize(mv->image_obj, w, h);
   _smart_size(obj);
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
    //sc.calculate = _smart_calculate;
    sc.show      = _smart_show;
    sc.hide      = _smart_hide;
    _smart = evas_smart_class_new(&sc);
}


void
miniview_update_scroll(Evas_Object *obj, int scroll_position)
{
   Miniview *mv = evas_object_smart_data_get(obj);
   if (!mv) return;

   DBG("obj:%p mv:%p scroll_position:%d", obj, mv, scroll_position);
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

   mv->scroll = 0;

   _smart_size(obj);

   return obj;
}

