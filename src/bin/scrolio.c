#include <Elementary.h>
#include <stdio.h>
#include "col.h"
#include "termpty.h"
#include "termio.h"

// Scrolio is the smart object responsible for miniview
// feature. (this scroll thing at your right)

typedef struct _Scrolio Scrolio;

struct _Scrolio
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
   Scrolio *sd;
   Evas_Object *o;
   Evas_Load_Error err;

   sd = calloc(1, sizeof(Scrolio));
   EINA_SAFETY_ON_NULL_RETURN(sd);
   evas_object_smart_data_set(obj, sd);

   _parent_sc.add(obj);

   /* miniview output widget  */
   o = evas_object_image_add(evas_object_evas_get(obj));
   //evas_object_image_file_set(o, "data/themes/images/miniview_white_bg.png", NULL);
   //err = evas_object_image_load_error_get(o);
   //if (err != EVAS_LOAD_ERROR_NONE)
   //  printf("error\n");
   evas_object_image_alpha_set(o, EINA_TRUE);
   evas_object_smart_member_add(o, obj);
   sd->image_obj = o;
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
   Scrolio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;

   //evas_object_geometry_get(sd->obj, &ox, &oy, &ow, &oh);
   //evas_object_move(sd->grid.obj, ox+(sd->grid.w*0.8), oy);
   //evas_object_resize(sd->grid.obj,
   //                   sd->grid.w * sd->font.chw,
   //                   sd->grid.h * sd->font.chh);
   evas_object_image_size_set(sd->image_obj, sd->parent_w / sd->font.chw,
                              (sd->parent_h / sd->font.chh) + sd->scrollback);
   evas_object_image_fill_set(sd->image_obj, 0, 0, sd->parent_w * 0.15,
                              sd->parent_h);
   evas_object_resize(sd->image_obj, sd->parent_w * 0.15, sd->parent_h);
   evas_object_move(sd->image_obj, sd->parent_x + (sd->parent_w * 0.85),
                    sd->parent_y);

}

static void
_smart_apply(Evas_Object *obj)
{
   Scrolio *sd = evas_object_smart_data_get(obj);
   
   evas_object_show(sd->image_obj);
   evas_object_show(sd->edje_obj);
   evas_object_show(sd->screen_obj);
}

static void
_smart_size(Evas_Object *obj)
{
   Scrolio *sd = evas_object_smart_data_get(obj);
   
   if (!sd) return;
   sd->miniview_screen_size = (double) sd->parent_h /
                         (double) (sd->scrollback * sd->font.chh);
   sd->miniview_screen_step = (double) sd->parent_h / (double) sd->scrollback;
   sd->miniview_screen_default_pos = (((double) sd->parent_h/(double) sd->scrollback)
                                   * (sd->lines_drawn - sd->scroll))
                                   / (double) sd->parent_h
                                   - sd->miniview_screen_size;

   evas_object_image_size_set(sd->image_obj, (sd->parent_w / sd->font.chw),
                                              sd->parent_h);
   evas_object_move(sd->edje_obj, sd->parent_x, sd->parent_y);
   evas_object_resize(sd->edje_obj, sd->parent_w, sd->parent_h);
   
   edje_object_part_drag_size_set(sd->screen_obj, "miniview_screen", 1.0, 
                                  sd->miniview_screen_size);
   if (!edje_object_part_drag_step_set(sd->screen_obj, "miniview_screen", 0.0,
                                       sd->miniview_screen_step))
     printf("error when setting drag step size.\n");
   evas_object_move(sd->screen_obj, sd->parent_x, sd->parent_y);
   evas_object_resize(sd->screen_obj, sd->parent_w, sd->parent_h);
   
   _smart_calculate(obj);
   _smart_apply(obj);
}

static void
_smart_resize()
{
/*
   Scrolio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;

   _smart_size(obj);
   _scrolio_draw(obj, sd->grid.obj, sd->parent_w, sd->parent_h);
*/
}


static void
_smart_init(void)
{
    static Evas_Smart_Class sc;

    evas_object_smart_clipped_smart_set(&_parent_sc);
    sc           = _parent_sc;
    sc.name      = "scrolio";
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
   Scrolio *sd = evas_object_smart_data_get(data);
   double val;

   edje_object_part_drag_value_get(o, "miniview_screen", NULL, &val);
   sd->scroll = sd->lines_drawn -
                ((sd->lines_drawn - (sd->parent_h / sd->font.chh)) *
                (val + sd->miniview_screen_size));

   // if miniview lines are less than scrollback lines
   // (miniview obj isn't fully drawn from top to bottom)
   if (sd->lines_drawn < sd->scrollback + (sd->parent_h / sd->font.chh) - 1)
     {
        if (val > sd->miniview_screen_default_pos)
          {
             val = sd->miniview_screen_default_pos;
             sd->scroll = 0;
          }
        else
          {
             sd->scroll = sd->lines_drawn -
                          ((sd->lines_drawn - (sd->parent_h / sd->font.chh)) *
                          ((val / sd->miniview_screen_default_pos))) -
                          sd->parent_h / sd->font.chh;

          }
     }
   if (sd->scroll < 0) sd->scroll = 0;
   printf("sd->scroll is:: %d and lines are: %f\n", sd->scroll, sd->lines_drawn);
   termio_scroll_set(sd->termio, sd->scroll);
}

void
_scrolio_draw(Evas_Object *obj)
{
   Scrolio *sd = evas_object_smart_data_get(obj);
   Termcell *cells;
   
   unsigned int *pixels, *p;
   int x=0, y=0, r, g, b, i;
   int wret=0;
   int pty_scan_width = 0;
   Eina_Bool pty_compact_w = EINA_FALSE;

   sd->lines_drawn = 0;
   pixels = evas_object_image_data_get(sd->image_obj, EINA_TRUE);
   p = pixels;
   for (y = 0 - sd->pty->backscroll_num; y < sd->parent_h / sd->font.chh; y++)
     {
        cells = termpty_cellrow_get(sd->pty, y, &wret);
        if (wret < sd->parent_w / sd->font.chw) 
          {
             pty_scan_width = wret; 
             pty_compact_w = EINA_TRUE;
          }
        else 
          {
             pty_scan_width = sd->parent_w / sd->font.chw;
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
              for (i=0; i < (sd->parent_w / sd->font.chw) - wret; i++) 
                {
                   r=0; g = 0; b = 0;
                   *p = (r << 16) | (g << 8) | (b);
                   p++;
                }
          sd->lines_drawn++;
     }
   evas_object_image_data_set(sd->image_obj, pixels);
   evas_object_image_data_update_add(sd->image_obj, 0, 0, sd->parent_w * 0.85, sd->parent_h * sd->parent_h);
}

void 
scrolio_miniview_resize(Evas_Object *obj, Termpty *pty, int w, int h)
{
   Scrolio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;

   sd->parent_w = w;
   sd->parent_h = h;
   sd->pty = pty;
   _smart_size(obj);
}

void
scrolio_miniview_move(Evas_Object *obj, int x, int y)
{
   Scrolio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;

   sd->parent_x = x;
   sd->parent_y = y;
   _smart_size(obj);
}

void
scrolio_miniview_update_scroll(Evas_Object *obj, int scroll_position)
{
   Scrolio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;

   sd->scroll = scroll_position;
   _smart_size(obj);
   edje_object_part_drag_value_set(sd->screen_obj, "miniview_screen", 0.0, sd->miniview_screen_default_pos);

   _scrolio_draw(obj);
}

Evas_Object *
scrolio_miniview_add(Evas_Object *parent, int fontw, int fonth, Termpty *pty, 
                     int scroll, int scroll_position, int x, int y, int w, int h)
{
   Evas *e;
   Evas_Object *obj, *edje_obj;
   Config *config = termio_config_get(parent);
   Scrolio *sd;

   EINA_SAFETY_ON_NULL_RETURN_VAL(parent, NULL);
   e = evas_object_evas_get(parent);
   if (!e) return NULL;

   if (!_smart) _smart_init();
   obj = evas_object_smart_add(e, _smart);
   sd = evas_object_smart_data_get(obj);
   if (!sd) return obj;

   sd->parent_x = x;
   sd->parent_y = y;
   sd->parent_w = w;
   sd->parent_h = h;
   sd->font.chw = fontw;
   sd->font.chh = fonth;
   sd->pty = pty;
   sd->scrollback = config->scrollback;
   sd->scroll = scroll_position;
   sd->miniview_screen_size = (double) sd->parent_h / sd->font.chh * sd->scrollback;
                         (double) (sd->scrollback * sd->font.chh);
                         (double) (sd->scrollback * sd->font.chh);
   sd->miniview_screen_step = (double) sd->parent_h / (double) sd->scrollback;
   sd->miniview_screen_default_pos = (((double) sd->parent_h/(double) sd->scrollback)// How many pixels we need for each line drawn
                                   * (sd->lines_drawn - sd->scroll))     // Multiplied by how many lines are drawn
                                   / (double) sd->parent_h  // Divided by height to find the percentage
                                   - sd->miniview_screen_size;
   sd->termio = parent;
   edje_obj = edje_object_add(e);
   edje_object_file_set(edje_obj, config_theme_path_get(config), 
                        "terminology/miniview");
   edje_object_part_swallow(edje_obj, "miniview", obj);
   evas_object_move(edje_obj, x, y);
   evas_object_resize(edje_obj, w, h);
   sd->edje_obj = edje_obj;

   edje_obj = edje_object_add(e);
   edje_object_file_set(edje_obj, config_theme_path_get(config), 
                        "terminology/miniview");
   edje_object_part_drag_size_set(edje_obj, "miniview_screen", 1.0, 
                                  sd->miniview_screen_size);
   if (!edje_object_part_drag_step_set(edje_obj, "miniview_screen", 0.0, sd->miniview_screen_step))
     printf("error when setting drag step size.\n");
   edje_object_signal_callback_add(edje_obj, "drag", "miniview_screen", _on_knob_moved, obj);

   evas_object_move(edje_obj, x, y);
   evas_object_resize(edje_obj, w, h);
   sd->screen_obj = edje_obj;
   
   _smart_size(obj);
   _scrolio_draw(obj);

   scrolio_miniview_update_scroll(obj, sd->scroll);

   return obj;
}

void
scrolio_miniview_hide(Evas_Object *obj)
{
   Scrolio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;

   evas_object_hide(sd->image_obj);
   evas_object_hide(sd->edje_obj);
   evas_object_hide(sd->screen_obj);
 
   sd->image_obj = NULL;
   sd->edje_obj = NULL;
   sd->screen_obj = NULL;
}
