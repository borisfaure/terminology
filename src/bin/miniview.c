#include <Elementary.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "private.h"
#include "miniview.h"
#include "col.h"
#include "termpty.h"
#include "termio.h"
#include "utils.h"
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

static Eina_Bool _deferred_renderer(void *data);

void
miniview_init(void)
{
   if (_miniview_log_dom >= 0) return;

   _miniview_log_dom = eina_log_domain_register("miniview", NULL);
   if (_miniview_log_dom < 0)
     EINA_LOG_CRIT(_("Could not create logging domain '%s'."), "miniview");
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
   Evas_Object *base;
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
   unsigned int initial_pos : 1;

   Eina_Bool fits_to_img;

   struct _screen {
      double size;
      double pos_val;
   }screen;
};

static Evas_Smart *_smart = NULL;

static void
_draw_cell(const Termpty *ty, unsigned int *pixel, const Termcell *cell, unsigned int *colors)
{
   int fg, bg, fgext, bgext;
   int inv = ty->state.reverse;
   Eina_Unicode codepoint;

   codepoint = cell->codepoint;
   if ((codepoint == 0) || (cell->att.newline) || (cell->att.invisible))
     {
        *pixel = 0;
        return;
     }
   // colors
   fg = cell->att.fg;
   bg = cell->att.bg;
   fgext = cell->att.fg256;
   bgext = cell->att.bg256;

   if ((fg == COL_DEF) && (cell->att.inverse ^ inv)) fg = COL_INVERSEBG;
   if (bg == COL_DEF)
     {
        if (cell->att.inverse ^ inv) bg = COL_INVERSE;
        else if (!bgext) bg = COL_INVIS;
     }
   if ((cell->att.fgintense) && (!fgext)) fg += 48;
   if ((cell->att.bgintense) && (!bgext)) bg += 48;
   if (cell->att.inverse ^ inv)
     {
        int t;
        t = fgext; fgext = bgext; bgext = t;
        t = fg; fg = bg; bg = t;
     }
   if ((cell->att.bold) && (!fgext)) fg += 12;
   if ((cell->att.faint) && (!fgext)) fg += 24;
   
   if (bgext) *pixel = colors[bg + 256];
   else if (bg && ((bg % 12) != COL_INVIS)) *pixel = colors[bg];
   else if ((codepoint > 32) && (codepoint < 0x00110000) &&
            (!isspace(codepoint)))
     {
        if (fgext) *pixel = colors[fg + 256];
        else *pixel = colors[fg];
     }
   else
     *pixel = 0;
}

static void
_draw_line(const Termpty *ty, unsigned int *pixels,
           const Termcell *cells, int length, unsigned int *colors)
{
   int x;

   for (x = 0 ; x < length; x++)
     {
        _draw_cell(ty, pixels + x, cells + x, colors);
     }
}

Eina_Bool
_is_top_bottom_reached(Miniview *mv)
{
   Termpty *ty;
   int history_len;

   EINA_SAFETY_ON_NULL_RETURN_VAL(mv, EINA_FALSE);
   ty = termio_pty_get(mv->termio);
   history_len = ty->backscroll_num;

   if (( (- mv->img_hist) > (int)(mv->img_h - mv->rows - (mv->rows / 2))) &&
       ( (- mv->img_hist) < (int)(history_len + (mv->rows / 2))))
          return EINA_FALSE;

   return EINA_TRUE;
}

static void
_screen_visual_bounds(Miniview *mv)
{
   if ((mv->screen.pos_val > 1) || (mv->screen.pos_val < 0))
     {
        edje_object_part_drag_value_set(mv->base, "miniview_screen",
                                        0.0, mv->screen.pos_val);
        edje_object_signal_emit(mv->base, "miniview_screen,outbounds",
                                "miniview");
     }
   else
     {
        edje_object_part_drag_value_set(mv->base, "miniview_screen",
                                        0.0, mv->screen.pos_val);
        edje_object_signal_emit(mv->base, "miniview_screen,inbounds",
                                "miniview");
     }
}

static void
_queue_render(Miniview *mv)
{
   mv->to_render = 1;
   if ((!mv->deferred_renderer) && (mv->is_shown))
     mv->deferred_renderer = ecore_timer_add(0.1, _deferred_renderer, mv);
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
   mv->img_hist += ev->z * 25;

   if (!mv->fits_to_img && !_is_top_bottom_reached(mv))
     {
        mv->screen.pos_val = mv->screen.pos_val -
                             (double) (ev->z * 25) / (mv->img_h - mv->rows);
        _screen_visual_bounds(mv);
     }
   _queue_render(mv);
}

void
miniview_position_offset(Evas_Object *obj, int by, Eina_Bool sanitize)
{
   Miniview *mv = evas_object_smart_data_get(obj);
   int remain = 0;

   termio_scroll_get(mv->termio);
   EINA_SAFETY_ON_NULL_RETURN(mv);
   if ((mv->screen.pos_val <= 1.0) && (mv->screen.pos_val >= 0.0))
     edje_object_signal_emit(mv->base, "miniview_screen,inbounds", "miniview");

   if (!mv->fits_to_img)
     {
        mv->screen.pos_val += (double) by / (mv->img_h - mv->rows);
        edje_object_part_drag_value_set(mv->base, "miniview_screen",
                                        0.0, mv->screen.pos_val);
        if ((mv->screen.pos_val <= 0) && (sanitize))
          {
             /* This is what remains when screen pos has to
                go negative by some portion "by" */
             remain = (int) round(mv->screen.pos_val * (mv->img_h - mv->rows));
             mv->img_hist += remain;
             mv->screen.pos_val = 0;
          }
        if ((mv->screen.pos_val > 1) && (sanitize))
          {
             remain = (int) round((1 - mv->screen.pos_val) *
                      (mv->img_h - mv->rows));
             mv->img_hist -= remain;
             mv->screen.pos_val = 1;
          }
     }
   else
     {
        mv->screen.pos_val += (double) by / (mv->img_h - mv->rows);
        edje_object_part_drag_value_set(mv->base, "miniview_screen",
                                        0.0, mv->screen.pos_val);
        if (mv->screen.pos_val < 0 && sanitize) mv->screen.pos_val = 0;
        if (mv->screen.pos_val > 1 && sanitize) mv->screen.pos_val = 1;
     }
}

Eina_Bool
miniview_handle_key(Evas_Object *obj, Evas_Event_Key_Down *ev)
{
   Miniview *mv;
   Evas_Coord ox, oy, ow, oh, mx, my;
   int z = 25;

   EINA_SAFETY_ON_NULL_RETURN_VAL(obj, EINA_FALSE);

   mv = evas_object_smart_data_get(obj);
   if ((!mv) || (!mv->is_shown)) return EINA_FALSE;

   evas_object_geometry_get(mv->img, &ox, &oy, &ow, &oh);
   evas_pointer_canvas_xy_get(evas_object_evas_get(mv->base), &mx, &my);

   if ((mx < ox) || (mx > ox + ow) || (my < oy) || (my > oy + oh))
     return EINA_FALSE;

   if (evas_key_modifier_is_set(ev->modifiers, "Alt") ||
       evas_key_modifier_is_set(ev->modifiers, "Shift") ||
       evas_key_modifier_is_set(ev->modifiers, "Control"))
     z = mv->img_h;

   if (!strcmp(ev->key, "Prior"))
     {
        if (!mv->fits_to_img)
          {
             mv->img_hist -= z;
             if (_is_top_bottom_reached(mv))
               {
                  miniview_position_offset(obj, -z, EINA_FALSE);
               }
             miniview_position_offset(obj, z, EINA_FALSE);
             _screen_visual_bounds(mv);
          }
        _queue_render(mv);
        return EINA_TRUE;
     }
   else if (!strcmp(ev->key, "Next"))
     {
        if (!mv->fits_to_img)
          {
             mv->img_hist += z;
             if (_is_top_bottom_reached(mv))
               {
                  miniview_position_offset(obj, z, EINA_FALSE);
               }
             miniview_position_offset(obj, -z, EINA_FALSE);
             _screen_visual_bounds(mv);
          }
        _queue_render(mv);
        return EINA_TRUE;
     }
   return EINA_FALSE;
}

static void
_smart_cb_mouse_down(void *data, Evas *e EINA_UNUSED,
                     Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Mouse_Down *ev = event;
   Miniview *mv= evas_object_smart_data_get(data);
   int pos, pos2;
   Evas_Coord oy;

   EINA_SAFETY_ON_NULL_RETURN(mv);

   evas_object_geometry_get(mv->img, NULL, &oy, NULL, NULL);
   pos = oy - ev->canvas.y;
   pos -= mv->img_hist;
   if (pos < 0) pos = 0;
   else pos += mv->rows / 2;
   termio_scroll_set(mv->termio, pos);

   pos2 = ev->canvas.y - oy - (mv->rows / 2);
   if (pos2 < 0) pos2 = 0;
   if (pos2 > -mv->img_hist) pos2 = -mv->img_hist;
   mv->screen.pos_val = (double) pos2 / (mv->img_h - mv->rows);
   edje_object_part_drag_value_set(mv->base, "miniview_screen", 0.0, mv->screen.pos_val);
   _screen_visual_bounds(mv);
}

static void
_on_screen_stoped(void *data, Evas_Object *o EINA_UNUSED,
                  const char *emission EINA_UNUSED,
                  const char *source EINA_UNUSED)
{
   Miniview *mv= evas_object_smart_data_get(data);

   EINA_SAFETY_ON_NULL_RETURN(mv);

   edje_object_part_drag_value_set(mv->base, "miniview_screen", 0.0,
                                   mv->screen.pos_val);
}

static void
_on_screen_moved(void *data, Evas_Object *o, const char *emission EINA_UNUSED,
                 const char *source EINA_UNUSED)
{
   Miniview *mv = evas_object_smart_data_get(data);
   double val = 0.0, pos = 0.0, bottom_bound = 0.0;

   edje_object_part_drag_value_get(o, "miniview_screen", NULL, &val);
   bottom_bound = ((double) (-mv->img_hist )) / (mv->img_h - mv->rows);
   if (!mv->fits_to_img)
     {
        pos = (bottom_bound - val) * (mv->img_h - mv->rows);
        mv->screen.pos_val = val;
     }
   else
     {
        bottom_bound = ((double) (-mv->img_hist )) / (mv->img_h - mv->rows);
        pos = (bottom_bound - val) * (mv->img_h - mv->rows);
        if (val < bottom_bound)
          mv->screen.pos_val = val;
        if (val > bottom_bound)
          {
             mv->screen.pos_val = bottom_bound;
             pos = 0;
          }
     }
   termio_scroll_set(mv->termio, (int) pos);
   _screen_visual_bounds(mv);
}

static void
_smart_add(Evas_Object *obj)
{
   Miniview *mv;

   mv = calloc(1, sizeof(Miniview));
   EINA_SAFETY_ON_NULL_RETURN(mv);
   evas_object_smart_data_set(obj, mv);
   mv->self = obj;
}

static void
_smart_del(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   if (!mv) return;
   ecore_timer_del(mv->deferred_renderer);
   evas_object_del(mv->base);
   evas_object_del(mv->img);
   free(mv);
}

static void
_do_configure(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh, font_w, font_h, w;

   if (!mv) return;

   evas_object_geometry_get(mv->termio, &ox, &oy, &ow, &oh);
   evas_object_size_hint_min_get(mv->termio, &font_w, &font_h);

   if ((font_w <= 0) || (font_h <= 0) || (ow == 0) || (oh == 0)) return;

   mv->img_h = oh;
   mv->rows = oh / font_h;
   mv->cols = ow / font_w;
   
   if ((mv->rows == 0) || (mv->cols == 0)) return;
   
   mv->screen.size = (double) mv->rows / (double) mv->img_h;
   edje_object_part_drag_size_set(mv->base, "miniview_screen", 1.0, mv->screen.size);

   w = (mv->cols * font_w) / font_h;
   
   evas_object_resize(mv->base, w, mv->img_h);
   evas_object_move(mv->base, ox + ow - w, oy);
}

static void
_smart_move(Evas_Object *obj, Evas_Coord x EINA_UNUSED,
            Evas_Coord y EINA_UNUSED)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   if (!mv) return;
   _do_configure(obj);
   _queue_render(mv);
}

static void
_smart_show(Evas_Object *obj)
{
   Miniview *mv = evas_object_smart_data_get(obj);

   if (!mv) return;
   if (!mv->is_shown)
     {
        mv->is_shown = 1;
        mv->img_hist = 0;
        mv->initial_pos = 1;

        _do_configure(obj);
        _queue_render(mv);
        evas_object_show(mv->base);
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
        evas_object_hide(mv->base);
     }
}

void
miniview_redraw(Evas_Object *obj)
{
   Miniview *mv;

   if (!obj) return;
   mv = evas_object_smart_data_get(obj);
   if ((!mv) || (!mv->is_shown)) return;
   _queue_render(mv);
}

static void
miniview_colors_get(Miniview *mv, unsigned int *colors)
{
   Evas_Object *tg = termio_textgrid_get(mv->termio);
   int r, g, b, a, c;
   
   for (c = 0; c < 256; c++)
     {
        evas_object_textgrid_palette_get
        (tg, EVAS_TEXTGRID_PALETTE_STANDARD, c, &r, &g, &b, &a);
        colors[c] = (a << 24) | (r << 16) | (g << 8) | b;
     }
   for (c = 0; c < 256; c++)
     {
        evas_object_textgrid_palette_get
        (tg, EVAS_TEXTGRID_PALETTE_EXTENDED, c, &r, &g, &b, &a);
        colors[c + 256] = (a << 24) | (r << 16) | (g << 8) | b;
     }
}

static Eina_Bool
_deferred_renderer(void *data)
{
   Miniview *mv = data;
   Evas_Coord ox, oy, ow, oh;
   int history_len, wret, pos;
   unsigned int *pixels, y;
   Termcell *cells;
   Termpty *ty;
   unsigned int colors[512];
   double bottom_bound;

   if (!mv) return EINA_FALSE;

   if ((!mv->is_shown) || (!mv->to_render) || (mv->img_h == 0))
     {
        mv->deferred_renderer = NULL;
        return EINA_FALSE;
     }

   miniview_colors_get(mv, colors);

   ty = termio_pty_get(mv->termio);
   evas_object_geometry_get(mv->termio, &ox, &oy, &ow, &oh);
   if ((ow == 0) || (oh == 0)) return EINA_TRUE;

   history_len = ty->backscroll_num;

   evas_object_image_size_set(mv->img, mv->cols, mv->img_h);
   ow = mv->cols;
   oh = mv->img_h;

   pixels = evas_object_image_data_get(mv->img, EINA_TRUE);
   memset(pixels, 0, sizeof(*pixels) * ow * oh);
   mv->img_h = oh;

   /* "current"? */
   if (mv->img_hist >= - ((int)mv->img_h - (int)mv->rows))
     mv->img_hist = -((int)mv->img_h - (int)mv->rows);
   if (mv->img_hist < -history_len)
     mv->img_hist = -history_len;

   for (y = 0; y < mv->img_h; y++)
     {
        cells = termpty_cellrow_get(ty, mv->img_hist + y, &wret);
        if (!cells) break;
        _draw_line(ty, &pixels[y * mv->cols], cells, wret, colors);
     }
   evas_object_image_data_set(mv->img, pixels);
   evas_object_image_pixels_dirty_set(mv->img, EINA_FALSE);
   evas_object_image_data_update_add(mv->img, 0, 0, ow, oh);

   if (history_len > (int)(mv->img_h - mv->rows)) mv->fits_to_img = EINA_FALSE;
   else mv->fits_to_img = EINA_TRUE;

   bottom_bound = ((double) (-mv->img_hist )) / (mv->img_h - mv->rows);
   pos = termio_scroll_get(mv->termio);
   if ((pos != 0) && (mv->initial_pos))
     {
        double val;
        val = (double) pos / (mv->img_h - mv->rows);
        if ((mv->fits_to_img))
          {
             mv->screen.pos_val = bottom_bound - val;
             mv->initial_pos = 0;
          }
        else
          {
             if (val > 1)
               {
                  mv->img_hist += (mv->img_hist * (int)val);
                  mv->screen.pos_val = (double)(1 - ( val - (int) val));
                  mv->initial_pos = 0;
                  mv->to_render = 1;
                  return EINA_TRUE;
               }
             else
               {
                  mv->screen.pos_val = bottom_bound - val;
                  mv->initial_pos = 0;
               }
          }
     }
   if (pos == 0)
     mv->screen.pos_val = bottom_bound;

   edje_object_part_drag_value_set(mv->base, "miniview_screen", 0.0, mv->screen.pos_val);

   mv->to_render = 0;
   mv->deferred_renderer = NULL;
   _screen_visual_bounds(mv);
   return EINA_FALSE;
}


static void
_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   Evas_Coord font_w, font_h, ox, oy;
   Miniview *mv = evas_object_smart_data_get(obj);
   if (!mv) return;

   mv->img_h = h;

   evas_object_geometry_get(mv->termio, &ox, &oy, NULL, NULL);
   evas_object_size_hint_min_get(mv->termio, &font_w, &font_h);
   if ((font_w <= 0) || (font_h <= 0)) return;

   mv->rows = h / font_h;
   mv->cols = w / font_w;

   if ((mv->rows == 0) || (mv->cols == 0)) return;

   _do_configure(obj);
   _queue_render(mv);
}


static void
_smart_init(void)
{
    static Evas_Smart_Class sc = EVAS_SMART_CLASS_INIT_NULL;

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

static void
_cb_miniview_close(void *data, Evas_Object *obj EINA_UNUSED,
                   const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   Miniview *mv = data;

   EINA_SAFETY_ON_NULL_RETURN(mv);
   EINA_SAFETY_ON_NULL_RETURN(mv->termio);
   if (mv->is_shown) term_miniview_hide(termio_term_get(mv->termio));
}

Evas_Object *
miniview_add(Evas_Object *parent, Evas_Object *termio)
{
   Evas *e;
   Evas_Object *obj, *o;
   Miniview *mv;
   Evas *canvas = evas_object_evas_get(parent);

   EINA_SAFETY_ON_NULL_RETURN_VAL(parent, NULL);
   e = evas_object_evas_get(parent);
   if (!e) return NULL;

   if (!_smart) _smart_init();

   obj = evas_object_smart_add(e, _smart);
   mv = evas_object_smart_data_get(obj);
   if (!mv) return obj;

   mv->termio = termio;

   mv->base = edje_object_add(canvas);
   theme_apply(mv->base, termio_config_get(termio), "terminology/miniview");

   evas_object_smart_member_add(mv->base, obj);

   /* miniview output widget  */
   o = evas_object_image_filled_add(canvas);
   evas_object_image_alpha_set(o, EINA_TRUE);
   edje_object_part_swallow(mv->base, "miniview.img", o);
   evas_object_show(o);

   mv->img = o;

   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_WHEEL,
                                  _smart_cb_mouse_wheel, obj);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _smart_cb_mouse_down, obj);
   edje_object_signal_callback_add(mv->base, "miniview,close", "terminology",
                                   _cb_miniview_close, mv);
   edje_object_signal_callback_add(mv->base, "drag", "miniview_screen",
                                   _on_screen_moved, obj);
   edje_object_signal_callback_add(mv->base, "drag,stop", "miniview_screen",
                                   _on_screen_stoped, obj);
   return obj;
}

