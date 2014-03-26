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
   Evas_Object_Smart_Clipped_Data __clipped_data;
   Evas_Object *obj, *image_obj, *edje_obj, *screen_obj;
   struct {
      int size;
      const char *name;
      int chw, chh;
   } font;
   struct {
      int w, h;
      Evas_Object *obj;
   } grid;
   Evas_Object *termio;
   Termpty *pty;
   int parent_x, parent_y, parent_w, parent_h;
   int scroll;
   int scrollback;
   double miniview_screen_size, miniview_screen_step, miniview_screen_default_pos,
          lines_drawn, screen_obj_scroll_val, screen_obj_custom_pos;
};

static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

static void
_smart_add(Evas_Object *obj)
{
   Miniview *mv;
   Evas_Object *o;

   mv = calloc(1, sizeof(Miniview));
   EINA_SAFETY_ON_NULL_RETURN(mv);
   evas_object_smart_data_set(obj, mv);

   _parent_sc.add(obj);

   /* miniview output widget  */
   o = evas_object_image_add(evas_object_evas_get(obj));
   //evas_object_image_file_set(o, "data/themes/images/miniview_white_bg.png", NULL);
   //err = evas_object_image_load_error_get(o);
   //if (err != EVAS_LOAD_ERROR_NONE)
   //  printf("error\n");
   evas_object_image_alpha_set(o, EINA_TRUE);
   evas_object_smart_member_add(o, obj);
   mv->image_obj = o;
}

static void
_smart_del()
{

}

static void
_smart_move()
{

}

static void
_smart_calculate(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);
   if (!mv) return;

   //evas_object_geometry_get(mv->obj, &ox, &oy, &ow, &oh);
   //evas_object_move(mv->grid.obj, ox+(mv->grid.w*0.8), oy);
   //evas_object_resize(mv->grid.obj,
   //                   mv->grid.w * mv->font.chw,
   //                   mv->grid.h * mv->font.chh);
   evas_object_image_size_set(mv->image_obj, mv->parent_w / mv->font.chw,
                              (mv->parent_h / mv->font.chh) + mv->scrollback);
   evas_object_image_fill_set(mv->image_obj, 0, 0, mv->parent_w * 0.15,
                              mv->parent_h);
   evas_object_resize(mv->image_obj, mv->parent_w * 0.15, mv->parent_h);
   evas_object_move(mv->image_obj, mv->parent_x + (mv->parent_w * 0.85),
                    mv->parent_y);

}

static void
_smart_apply(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   evas_object_show(mv->image_obj);
   evas_object_show(mv->edje_obj);
   evas_object_show(mv->screen_obj);
}

static void
_smart_size(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   if (!mv) return;
   mv->miniview_screen_size = (double) mv->parent_h /
                         (double) (mv->scrollback * mv->font.chh);
   mv->miniview_screen_step = (double) mv->parent_h / (double) mv->scrollback;
   mv->miniview_screen_default_pos = (((double) mv->parent_h/(double) mv->scrollback)
                                   * (mv->lines_drawn - mv->scroll))
                                   / (double) mv->parent_h
                                   - mv->miniview_screen_size;

   evas_object_image_size_set(mv->image_obj, (mv->parent_w / mv->font.chw),
                                              mv->parent_h);
   evas_object_move(mv->edje_obj, mv->parent_x, mv->parent_y);
   evas_object_resize(mv->edje_obj, mv->parent_w, mv->parent_h);

   edje_object_part_drag_size_set(mv->screen_obj, "miniview_screen", 1.0,
                                  mv->miniview_screen_size);
   if (!edje_object_part_drag_step_set(mv->screen_obj, "miniview_screen", 0.0,
                                       mv->miniview_screen_step))
     printf("error when setting drag step size.\n");
   evas_object_move(mv->screen_obj, mv->parent_x, mv->parent_y);
   evas_object_resize(mv->screen_obj, mv->parent_w, mv->parent_h);

   _smart_calculate(obj);
   _smart_apply(obj);
}

static void
_smart_resize()
{
/*
   Miniview *mv = evas_object_smart_data_get(obj);
   if (!mv) return;

   _smart_size(obj);
   _miniview_draw(obj, mv->grid.obj, mv->parent_w, mv->parent_h);
*/
}


static void
_smart_init(void)
{
    static Evas_Smart_Class sc;

    evas_object_smart_clipped_smart_set(&_parent_sc);
    sc           = _parent_sc;
    sc.name      = "miniview";
    sc.version   = EVAS_SMART_CLASS_VERSION;
    sc.add       = _smart_add;
    sc.del       = _smart_del;
    sc.resize    = _smart_resize;
    sc.move      = _smart_move;
    sc.calculate = _smart_calculate;
    _smart = evas_smart_class_new(&sc);
}

static void
_on_knob_moved(void *data, Evas_Object *o, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Miniview *mv = evas_object_smart_data_get(data);
   double val;

   edje_object_part_drag_value_get(o, "miniview_screen", NULL, &val);
   mv->scroll = mv->lines_drawn -
                ((mv->lines_drawn - (mv->parent_h / mv->font.chh)) *
                (val + mv->miniview_screen_size));

   // if miniview lines are less than scrollback lines
   // (miniview obj isn't fully drawn from top to bottom)
   if (mv->lines_drawn < mv->scrollback + (mv->parent_h / mv->font.chh) - 1)
     {
        if (val > mv->miniview_screen_default_pos)
          {
             val = mv->miniview_screen_default_pos;
             mv->scroll = 0;
          }
        else
          {
             mv->scroll = mv->lines_drawn -
                          ((mv->lines_drawn - (mv->parent_h / mv->font.chh)) *
                          ((val / mv->miniview_screen_default_pos))) -
                          mv->parent_h / mv->font.chh;

          }
     }
   if (mv->scroll < 0) mv->scroll = 0;
   printf("mv->scroll is:: %d and lines are: %f\n", mv->scroll, mv->lines_drawn);
   termio_scroll_set(mv->termio, mv->scroll);
}

static void
_miniview_draw(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);
   Termcell *cells;

   unsigned int *pixels, *p;
   int x=0, y=0, r, g, b, i;
   int wret=0;
   int pty_scan_width = 0;
   Eina_Bool pty_compact_w = EINA_FALSE;

   mv->lines_drawn = 0;
   pixels = evas_object_image_data_get(mv->image_obj, EINA_TRUE);
   p = pixels;
   for (y = 0 - mv->pty->backscroll_num; y < mv->parent_h / mv->font.chh; y++)
     {
        cells = termpty_cellrow_get(mv->pty, y, &wret);
        if (wret < mv->parent_w / mv->font.chw)
          {
             pty_scan_width = wret;
             pty_compact_w = EINA_TRUE;
          }
        else
          {
             pty_scan_width = mv->parent_w / mv->font.chw;
             pty_compact_w = EINA_FALSE;
          }
        for (x=0; x < pty_scan_width; x++)
          {
             if (cells[x].codepoint == 0 || cells[x].codepoint == ' ' ||
                 cells[x].att.newline || cells[x].att.tab ||
                 cells[x].codepoint == '\0' || cells[x].codepoint <= 0)
               {
                  r=0; g = 0; b = 0;
                  *p = (r << 16) | (g << 8) | (b);
                  p++;
               }
             else
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
                  *p = (r << 16) | (g << 8) | (b);
                  p++;
               }
          }
          if (pty_compact_w)
              for (i=0; i < (mv->parent_w / mv->font.chw) - wret; i++)
                {
                   r=0; g = 0; b = 0;
                   *p = (r << 16) | (g << 8) | (b);
                   p++;
                }
          mv->lines_drawn++;
     }
   evas_object_image_data_set(mv->image_obj, pixels);
   evas_object_image_data_update_add(mv->image_obj, 0, 0, mv->parent_w * 0.85, mv->parent_h * mv->parent_h);
}

void
miniview_resize(Evas_Object *obj, Termpty *pty, int w, int h)
{
   Miniview *mv = evas_object_smart_data_get(obj);
   if (!mv) return;

   mv->parent_w = w;
   mv->parent_h = h;
   mv->pty = pty;
   _smart_size(obj);
}

void
miniview_move(Evas_Object *obj, int x, int y)
{
   Miniview *mv = evas_object_smart_data_get(obj);
   if (!mv) return;

   mv->parent_x = x;
   mv->parent_y = y;
   _smart_size(obj);
}

void
miniview_update_scroll(Evas_Object *obj, int scroll_position)
{
   Miniview *mv = evas_object_smart_data_get(obj);
   if (!mv) return;

   mv->scroll = scroll_position;
   _smart_size(obj);
   edje_object_part_drag_value_set(mv->screen_obj, "miniview_screen", 0.0, mv->miniview_screen_default_pos);

   _miniview_draw(obj);
}

Evas_Object *
miniview_add(Evas_Object *parent, int fontw, int fonth, Termpty *pty,
             int scroll_position, int x, int y, int w, int h)
{
   Evas *e;
   Evas_Object *obj, *edje_obj;
   Config *config = termio_config_get(parent);
   Miniview *mv;

   EINA_SAFETY_ON_NULL_RETURN_VAL(parent, NULL);
   e = evas_object_evas_get(parent);
   if (!e) return NULL;

   if (!_smart) _smart_init();
   obj = evas_object_smart_add(e, _smart);
   mv = evas_object_smart_data_get(obj);
   if (!mv) return obj;

   mv->parent_x = x;
   mv->parent_y = y;
   mv->parent_w = w;
   mv->parent_h = h;
   mv->font.chw = fontw;
   mv->font.chh = fonth;
   mv->pty = pty;
   mv->scrollback = config->scrollback;
   mv->scroll = scroll_position;
   mv->miniview_screen_size = (double) mv->parent_h
      / mv->font.chh * mv->scrollback;
   mv->miniview_screen_step = (double) mv->parent_h / (double) mv->scrollback;
   mv->miniview_screen_default_pos =
      (((double) mv->parent_h/(double) mv->scrollback)// pixels for each line drawn
       * (mv->lines_drawn - mv->scroll))     // number of lines drawn
        / (double) mv->parent_h  // Divided by height to find the percentage
        - mv->miniview_screen_size;
   mv->termio = parent;
   edje_obj = edje_object_add(e);
   edje_object_file_set(edje_obj, config_theme_path_get(config),
                        "terminology/miniview");
   edje_object_part_swallow(edje_obj, "miniview", obj);
   evas_object_move(edje_obj, x, y);
   evas_object_resize(edje_obj, w, h);
   mv->edje_obj = edje_obj;

   edje_obj = edje_object_add(e);
   edje_object_file_set(edje_obj, config_theme_path_get(config),
                        "terminology/miniview");
   edje_object_part_drag_size_set(edje_obj, "miniview_screen", 1.0,
                                  mv->miniview_screen_size);
   if (!edje_object_part_drag_step_set(edje_obj, "miniview_screen", 0.0, mv->miniview_screen_step))
     printf("error when setting drag step size.\n");
   edje_object_signal_callback_add(edje_obj, "drag", "miniview_screen", _on_knob_moved, obj);

   evas_object_move(edje_obj, x, y);
   evas_object_resize(edje_obj, w, h);
   mv->screen_obj = edje_obj;

   _smart_size(obj);
   _miniview_draw(obj);

   miniview_update_scroll(obj, mv->scroll);

   return obj;
}

void
miniview_hide(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);
   if (!mv) return;

   evas_object_hide(mv->image_obj);
   evas_object_hide(mv->edje_obj);
   evas_object_hide(mv->screen_obj);

   mv->image_obj = NULL;
   mv->edje_obj = NULL;
   mv->screen_obj = NULL;
}
