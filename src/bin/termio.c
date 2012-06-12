#include <Elementary.h>
#include "termio.h"
#include "termpty.h"
#include "utf8.h"
#include "col.h"
#include "keyin.h"
#include "config.h"

typedef struct _Termio Termio;
typedef struct _Termch Termch;

struct _Termio
{
   Evas_Object_Smart_Clipped_Data __clipped_data;
   struct {
      int size;
      const char *name;
      int chw, chh;
   } font;
   struct {
      int w, h;
      Termch *array;
   } grid;
   struct {
      Evas_Object *obj, *selo1, *selo2, *selo3;
      int x, y;
      struct {
         int x, y;
      } sel1, sel2;
      Eina_Bool sel : 1;
      Eina_Bool makesel : 1;
   } cur;
   int scroll;
   Evas_Object *event;
   Termpty *pty;
   Ecore_Job *job;
   Ecore_Timer *delayed_size_timer;
   Evas_Object *win;
   Eina_Bool jump_on_change : 1;
};

struct _Termch
{
   Evas_Object *bg;
   Evas_Object *tx;
};

static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _termio_sc = EVAS_SMART_CLASS_INIT_NULL;

static void _smart_calculate(Evas_Object *obj);

static void
_smart_apply(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh;
   char txt[8];
   if (!sd) return;
   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   
   if (sd->grid.array)
     {
        int i, j, x, y, w;
        
        i = 0;
        for (y = 0; y < sd->grid.h; y++)
          {
             Termcell *cells;
             
             w = 0;
             cells = termpty_cellrow_get(sd->pty, y - sd->scroll, &w);
             j = 0;
             for (x = 0; x < sd->grid.w; x++)
               {
                  Evas_Object *bg = sd->grid.array[i].bg;
                  Evas_Object *tx = sd->grid.array[i].tx;
                  
                  if ((!cells) || (x >= w))
                    {
                       evas_object_hide(bg);
                       evas_object_hide(tx);
                    }
                  else
                    {
                       Color c1, c2;
                       
                       if (cells[j].att.invisible)
                         {
                            evas_object_hide(tx);
                            evas_object_hide(bg);
                         }
                       else
                         {
                            int cbd, cbdbg, cfg, cbg;
                            
                            // colors
                            cbd = cells[j].att.bold;
                            cbdbg = 0;
                            cfg = cells[j].att.fg;
                            cbg = cells[j].att.bg;
                            
                            if (cells[j].att.inverse)
                              {
                                 cfg = COL_INVERSE;
                                 cbg = COL_INVERSEBG;
                                 cbdbg = cbd;
                                 c1 = colors[cbd][cfg];
                                 c2 = colors[cbdbg][cbg];
                                 if (cbg == COL_DEF) evas_object_hide(bg);
                                 else evas_object_show(bg);
                              }
                            else
                              {
                                 if (cells[j].att.fg256)
                                   c1 = colors256[cfg];
                                 else
                                   c1 = colors[cbd][cfg];
                                 if (cells[j].att.bg256)
                                   {
                                      c2 = colors256[cbg];
                                      evas_object_show(bg);
                                   }
                                 else
                                   {
                                      c2 = colors[cbdbg][cbg];
                                      if (cbg == COL_DEF) evas_object_hide(bg);
                                      else evas_object_show(bg);
                                   }
                              }
                            if (cells[j].att.faint)
                              {
                                 c1.r /= 2;
                                 c1.g /= 2;
                                 c1.b /= 2;
                                 c1.a /= 2;

                                 c2.r /= 2;
                                 c2.g /= 2;
                                 c2.b /= 2;
                                 c2.a /= 2;
                              }
//                            if (cells[j].att.unerline) {}
//                            if (cells[j].att.italic) {} // never going 2 support
//                            if (cells[j].att.strike) {}
//                            if (cells[j].att.blink) {}
//                            if (cells[j].att.blink2) {}
                            evas_object_color_set(tx, c1.r, c1.g, c1.b, c1.a);
                            evas_object_color_set(bg, c2.r, c2.g, c2.b, c2.a);
                            
                            // text - convert glyph back to utf8 str seq
                            if (cells[j].glyph > 0)
                              {
                                 int g = cells[j].glyph;
                                 
                                 glyph_to_utf8(g, txt);
                                 // special case for whitespace :)
                                 if (cells[j].glyph == ' ')
                                   {
                                      evas_object_hide(tx);
                                   }
                                 else
                                   {
                                      evas_object_text_text_set(tx, txt);
                                      evas_object_show(tx);
                                   }
                              }
                            else
                              {
                                 evas_object_hide(tx);
                                 if (cbg == COL_DEF) evas_object_hide(bg);
                                 else evas_object_show(bg);
                              }
                         }
                    }
                  j++;
                  i++;
               }
          }
     }
   if ((sd->scroll != 0) || (sd->pty->state.hidecursor))
     evas_object_hide(sd->cur.obj);
   else
     evas_object_show(sd->cur.obj);
   sd->cur.x = sd->pty->state.cx;
   sd->cur.y = sd->pty->state.cy;
   evas_object_move(sd->cur.obj, 
                    ox + (sd->cur.x * sd->font.chw),
                    oy + (sd->cur.y * sd->font.chh));
   if (sd->cur.sel)
     {
        int x1, y1, x2, y2;
        
        x1 = sd->cur.sel1.x;
        y1 = sd->cur.sel1.y;
        x2 = sd->cur.sel2.x;
        y2 = sd->cur.sel2.y;
        if ((y1 > y2) || ((y1 == y2) && (x2 < x1)))
          {
             int t;
             
             t = x1; x1 = x2; x2 = t;
             t = y1; y1 = y2; y2 = t;
          }
        
        if (y2 > y1)
          {
             evas_object_move(sd->cur.selo1,
                              ox + (x1 * sd->font.chw),
                              oy + ((y1 + sd->scroll) * sd->font.chh));
             evas_object_resize(sd->cur.selo1,
                                (sd->grid.w - x1) * sd->font.chw,
                                sd->font.chh);
             evas_object_show(sd->cur.selo1);
             
             evas_object_move(sd->cur.selo3,
                              ox, oy + ((y2 + sd->scroll) * sd->font.chh));
             evas_object_resize(sd->cur.selo3,
                                (x2 + 1) * sd->font.chw,
                                sd->font.chh);
             evas_object_show(sd->cur.selo3);
          }
        else
          {
             evas_object_move(sd->cur.selo1,
                              ox + (x1 * sd->font.chw),
                              oy + ((y1 + sd->scroll) * sd->font.chh));
             evas_object_resize(sd->cur.selo1,
                                (x2 - x1 + 1) * sd->font.chw,
                                sd->font.chh);
             evas_object_show(sd->cur.selo1);
             evas_object_hide(sd->cur.selo3);
          }
        if (y2 > (y1 + 1))
          {
             evas_object_move(sd->cur.selo2,
                              ox, oy + ((y1 + 1 + sd->scroll) * sd->font.chh));
             evas_object_resize(sd->cur.selo2,
                                sd->grid.w * sd->font.chw,
                                (y2 - y1 - 1) * sd->font.chh);
             evas_object_show(sd->cur.selo2);
          }
        else
          evas_object_hide(sd->cur.selo2);
     }
   else
     {
        evas_object_hide(sd->cur.selo1);
        evas_object_hide(sd->cur.selo2);
        evas_object_hide(sd->cur.selo3);
     }
}

static void
_smart_size(Evas_Object *obj, int w, int h)
{
   Termio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;

   if (w < 1) w = 1;
   if (h < 1) h = 1;
   if ((w == sd->grid.w) && (h == sd->grid.h)) return;

   evas_event_freeze(evas_object_evas_get(obj));
   if (sd->grid.array)
     {
        int i, size = sd->grid.w * sd->grid.h;

        for (i = 0; i < size; i++)
          {
             if (sd->grid.array[i].bg) evas_object_del(sd->grid.array[i].bg);
             if (sd->grid.array[i].tx) evas_object_del(sd->grid.array[i].tx);
          }
        free(sd->grid.array);
        sd->grid.array = NULL;
     }
   sd->grid.w = w;
   sd->grid.h = h;
   sd->grid.array = calloc(1, sizeof(Termch) * sd->grid.w * sd->grid.h);
   if (sd->grid.array)
     {
        int i, x, y;
        
        i = 0;
        for (y = 0; y < sd->grid.h; y++)
          {
             for (x = 0; x < sd->grid.w; x++)
               {
                  Evas_Object *bg, *tx;
                  
                  bg = evas_object_rectangle_add(evas_object_evas_get(obj));
                  tx = evas_object_text_add(evas_object_evas_get(obj));
                  evas_object_pass_events_set(bg, EINA_TRUE);
                  evas_object_pass_events_set(tx, EINA_TRUE);
                  evas_object_propagate_events_set(bg, EINA_FALSE);
                  evas_object_propagate_events_set(tx, EINA_FALSE);
                  sd->grid.array[i].bg = bg;
                  sd->grid.array[i].tx = tx;
                  evas_object_smart_member_add(bg, obj);
                  evas_object_smart_member_add(tx, obj);
                  evas_object_resize(bg, sd->font.chw, sd->font.chh);
                  evas_object_text_font_set(tx, sd->font.name, sd->font.size);
                  evas_object_color_set(tx, 0, 0, 0, 0);
                  evas_object_color_set(bg, 0, 0, 0, 0);
                  i++;
                }
          }
     }
   evas_object_raise(sd->cur.selo1);
   evas_object_raise(sd->cur.selo2);
   evas_object_raise(sd->cur.selo3);
   evas_object_raise(sd->cur.obj);
   evas_object_resize(sd->cur.obj, sd->font.chw, sd->font.chh);
   evas_object_size_hint_min_set(obj, sd->font.chw, sd->font.chh);
   evas_object_size_hint_request_set(obj, 
                                     sd->font.chw * sd->grid.w,
                                     sd->font.chh * sd->grid.h);
   evas_object_raise(sd->event);
   termpty_resize(sd->pty, w, h);
   _smart_calculate(obj);
   _smart_apply(obj);
   evas_event_thaw(evas_object_evas_get(obj));
}

static Eina_Bool
_smart_cb_delayed_size(void *data)
{
   Evas_Object *obj = data;
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord ow = 0, oh = 0;
   int w, h;
   
   if (!sd) return EINA_FALSE;
   sd->delayed_size_timer = NULL;

   evas_object_geometry_get(obj, NULL, NULL, &ow, &oh);
   
   w = ow / sd->font.chw;
   h = oh / sd->font.chh;
   _smart_size(obj, w, h);
   return EINA_FALSE;
}

static void
_smart_cb_change(void *data)
{
   Evas_Object *obj = data;
   Termio *sd;
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   sd->job = NULL;
   _smart_apply(obj);
}

static void
_take_selection(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   int x1, y1, x2, y2;
   char *s;
   
   if (!sd) return;
   x1 = sd->cur.sel1.x;
   y1 = sd->cur.sel1.y;
   x2 = sd->cur.sel2.x;
   y2 = sd->cur.sel2.y;
   if ((y1 > y2) || ((y1 == y2) && (x2 < x1)))
     {
        int t;
        
        t = x1; x1 = x2; x2 = t;
        t = y1; y1 = y2; y2 = t;
     }
   s = termio_selection_get(obj, x1, y1, x2, y2);
   if (s)
     {
        if (sd->win)
          elm_cnp_selection_set(sd->win, ELM_SEL_TYPE_PRIMARY,
                                ELM_SEL_FORMAT_TEXT, s, strlen(s));
        free(s);
     }
}

static void
_clear_selection(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (!sd->win) return;
   elm_object_cnp_selection_clear(sd->win, ELM_SEL_TYPE_PRIMARY);
}

static Eina_Bool
_getsel_cb(void *data, Evas_Object *obj, Elm_Selection_Data *ev)
{
   Termio *sd = evas_object_smart_data_get(data);
   if (!sd) return EINA_FALSE;
   
   if (ev->format == ELM_SEL_FORMAT_TEXT)
     {
        if (ev->len > 0)
          termpty_write(sd->pty, ev->data, ev->len - 1);
     }
   return EINA_TRUE;
}

static void
_paste_selection(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (!sd->win) return;
   elm_cnp_selection_get(sd->win, ELM_SEL_TYPE_PRIMARY, ELM_SEL_FORMAT_TEXT,
                         _getsel_cb, obj);
}

void
_smart_cb_key_down(void *data, Evas *e, Evas_Object *obj, void *event)
{
   Evas_Event_Key_Down *ev = event;
   Termio *sd;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   if (evas_key_modifier_is_set(ev->modifiers, "Shift"))
     {
        if (ev->keyname)
          {
             int by = sd->grid.h - 2;
             
             if (by < 1) by = 1;
             if (!strcmp(ev->keyname, "Prior"))
               {
                  sd->scroll += by;
                  if (sd->scroll > sd->pty->backscroll_num)
                    sd->scroll = sd->pty->backscroll_num;
                  if (sd->job) ecore_job_del(sd->job);
                  sd->job = ecore_job_add(_smart_cb_change, obj);
                  return;
               }
             else if (!strcmp(ev->keyname, "Next"))
               {
                  sd->scroll -= by;
                  if (sd->scroll < 0) sd->scroll = 0;
                  if (sd->job) ecore_job_del(sd->job);
                  sd->job = ecore_job_add(_smart_cb_change, obj);
                  return;
               }
             else if (!strcmp(ev->keyname, "Insert"))
               {
                  _paste_selection(data);
                  return;
               }
          }
     }
   keyin_handle(sd->pty, ev);
}

void
_smart_cb_focus_in(void *data, Evas *e, Evas_Object *obj, void *event)
{
   Termio *sd;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   edje_object_signal_emit(sd->cur.obj, "focus,in", "terminology");
   if (!sd->win) return;
   elm_win_keyboard_mode_set(sd->win, ELM_WIN_KEYBOARD_TERMINAL);
}

void
_smart_cb_focus_out(void *data, Evas *e, Evas_Object *obj, void *event)
{
   Termio *sd;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   edje_object_signal_emit(sd->cur.obj, "focus,out", "terminology");
   if (!sd->win) return;
   elm_win_keyboard_mode_set(sd->win, ELM_WIN_KEYBOARD_OFF);
}

static void
_smart_xy_to_cursor(Evas_Object *obj, Evas_Coord x, Evas_Coord y, int *cx, int *cy)
{
   Termio *sd;
   Evas_Coord ox, oy;
   
   sd = evas_object_smart_data_get(obj);
   if (!sd)
     {
        *cx = 0;
        *cy = 0;
        return;
     }
   evas_object_geometry_get(obj, &ox, &oy, NULL, NULL);
   *cx = (x - ox) / sd->font.chw;
   *cy = (y - oy) / sd->font.chh;
   if (*cx < 0) *cx = 0;
   else if (*cx >= sd->grid.w) *cx = sd->grid.w - 1;
   if (*cy < 0) *cy = 0;
   else if (*cy >= sd->grid.h) *cy = sd->grid.h - 1;
}

static void
_sel_line(Evas_Object *obj, int cx, int cy)
{
   Termio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;

   sd->cur.sel = 1;
   sd->cur.makesel = 0;
   sd->cur.sel1.x = 0;
   sd->cur.sel1.y = cy;
   sd->cur.sel2.x = sd->grid.w - 1;
   sd->cur.sel2.y = cy;
}

static Eina_Bool
_glyph_is_wordsep(int g)
{
   int i;

   if (g == 0) return EINA_TRUE;
   if (!config->wordsep) return EINA_FALSE;
   for (i = 0;;)
     {
        int g2 = 0;

        if (!config->wordsep[i]) break;
        i = evas_string_char_next_get(config->wordsep, i, &g2);
        if (i < 0) break;
        if (g == g2) return EINA_TRUE;
     }
   return EINA_FALSE;
}

static void
_sel_word(Evas_Object *obj, int cx, int cy)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Termcell *cells;
   int x, w = 0;
   if (!sd) return;

   cells = termpty_cellrow_get(sd->pty, cy - sd->scroll, &w);
   if (!cells) return;
   sd->cur.sel = 1;
   sd->cur.makesel = 0;
   sd->cur.sel1.x = cx;
   sd->cur.sel1.y = cy;
   for (x = sd->cur.sel1.x; x >= 0; x--)
     {
        if (x >= w) break;
        if (_glyph_is_wordsep(cells[x].glyph)) break;
        sd->cur.sel1.x = x;
     }
   sd->cur.sel2.x = cx;
   sd->cur.sel2.y = cy;
   for (x = sd->cur.sel2.x; x < sd->grid.w; x++)
     {
        if (x >= w) break;
        if (_glyph_is_wordsep(cells[x].glyph)) break;
        sd->cur.sel2.x = x;
     }
}

static void
_smart_cb_mouse_down(void *data, Evas *e, Evas_Object *obj, void *event)
{
   Evas_Event_Mouse_Down *ev = event;
   Termio *sd;
   int cx, cy;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   _smart_xy_to_cursor(data, ev->canvas.x, ev->canvas.y, &cx, &cy);
   if (ev->button == 1)
     {
        if (ev->flags & EVAS_BUTTON_TRIPLE_CLICK)
          {
             _sel_line(data, cx, cy - sd->scroll);
             if (sd->cur.sel) _take_selection(data);
          }
        else if (ev->flags & EVAS_BUTTON_DOUBLE_CLICK)
          {
             _sel_word(data, cx, cy - sd->scroll);
             if (sd->cur.sel) _take_selection(data);
          }
        else
          {
             if (sd->cur.sel)
               {
                  sd->cur.sel = 0;
                  _clear_selection(data);
               }
             sd->cur.makesel = 1;
             sd->cur.sel1.x = cx;
             sd->cur.sel1.y = cy - sd->scroll;
             sd->cur.sel2.x = cx;
             sd->cur.sel2.y = cy - sd->scroll;
          }
        if (sd->job) ecore_job_del(sd->job);
        sd->job = ecore_job_add(_smart_cb_change, data);
     }
   else if (ev->button == 2)
     _paste_selection(data);
   else if (ev->button == 3)
     {
        // XXX: popup config panel
     }
}

static void
_smart_cb_mouse_up(void *data, Evas *e, Evas_Object *obj, void *event)
{
   Evas_Event_Mouse_Up *ev = event;
   Termio *sd;
   int cx, cy;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   _smart_xy_to_cursor(data, ev->canvas.x, ev->canvas.y, &cx, &cy);
   if (sd->cur.makesel)
     {
        sd->cur.makesel = 0;
        if (sd->cur.sel)
          {
             sd->cur.sel2.x = cx;
             sd->cur.sel2.y = cy - sd->scroll;
             if (sd->job) ecore_job_del(sd->job);
             sd->job = ecore_job_add(_smart_cb_change, data);
             _take_selection(data);
          }
     }
}

static void
_smart_cb_mouse_move(void *data, Evas *e, Evas_Object *obj, void *event)
{
   Evas_Event_Mouse_Move *ev = event;
   Termio *sd;
   int cx, cy;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   _smart_xy_to_cursor(data, ev->cur.canvas.x, ev->cur.canvas.y, &cx, &cy);
   if (sd->cur.makesel)
     {
        if (!sd->cur.sel)
          {
             if ((cx != sd->cur.sel1.x) ||
                 ((cy - sd->scroll) != sd->cur.sel1.y))
               sd->cur.sel = 1;
          }
        sd->cur.sel2.x = cx;
        sd->cur.sel2.y = cy - sd->scroll;
        if (sd->job) ecore_job_del(sd->job);
        sd->job = ecore_job_add(_smart_cb_change, data);
     }
}

static void
_smart_cb_mouse_wheel(void *data, Evas *e, Evas_Object *obj, void *event)
{
   Evas_Event_Mouse_Wheel *ev = event;
   Termio *sd;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   if (sd->pty->altbuf) return;
   sd->scroll -= (ev->z * 4);
   if (sd->scroll > sd->pty->backscroll_num)
     sd->scroll = sd->pty->backscroll_num;
   else if (sd->scroll < 0) sd->scroll = 0;
   if (sd->job) ecore_job_del(sd->job);
   sd->job = ecore_job_add(_smart_cb_change, data);
}

static void
_smart_add(Evas_Object *obj)
{
   Termio *sd;
   Evas_Object_Smart_Clipped_Data *cd;
   _termio_sc.add(obj);
   cd = evas_object_smart_data_get(obj);
   if (!cd) return;
   sd = calloc(1, sizeof(Termio));
   if (!sd) return;
   sd->__clipped_data = *cd;
   free(cd);
   evas_object_smart_data_set(obj, sd);

   sd->jump_on_change = config->jump_on_change;
   
     {
        Evas_Object *o;
        Evas_Coord w = 2, h = 2;
        char buf[4096];

        if (config->font.bitmap)
          {
             snprintf(buf, sizeof(buf), "%s/fonts/%s",
                      elm_app_data_dir_get(), config->font.name);
             sd->font.name = eina_stringshare_add(buf);
          }
        else
          sd->font.name = eina_stringshare_add(config->font.name);
        sd->font.size = config->font.size;
        o = evas_object_text_add(evas_object_evas_get(obj));
        evas_object_text_font_set(o, sd->font.name, sd->font.size);
        evas_object_text_text_set(o, "X");
        evas_object_geometry_get(o, NULL, NULL, &w, &h);
        evas_object_del(o);
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        sd->font.chw = w;
        sd->font.chh = h;
        
        o = evas_object_rectangle_add(evas_object_evas_get(obj));
        evas_object_pass_events_set(o, EINA_TRUE);
        evas_object_propagate_events_set(o, EINA_FALSE);
        evas_object_smart_member_add(o, obj);
        sd->cur.selo1 = o;
        evas_object_color_set(o, 64, 64, 64, 64);
        o = evas_object_rectangle_add(evas_object_evas_get(obj));
        evas_object_pass_events_set(o, EINA_TRUE);
        evas_object_propagate_events_set(o, EINA_FALSE);
        evas_object_smart_member_add(o, obj);
        sd->cur.selo2 = o;
        evas_object_color_set(o, 64, 64, 64, 64);
        o = evas_object_rectangle_add(evas_object_evas_get(obj));
        evas_object_pass_events_set(o, EINA_TRUE);
        evas_object_propagate_events_set(o, EINA_FALSE);
        evas_object_smart_member_add(o, obj);
        sd->cur.selo3 = o;
        evas_object_color_set(o, 64, 64, 64, 64);
        
        o = edje_object_add(evas_object_evas_get(obj));
        evas_object_pass_events_set(o, EINA_TRUE);
        evas_object_propagate_events_set(o, EINA_FALSE);
        evas_object_smart_member_add(o, obj);
        sd->cur.obj = o;
        snprintf(buf, sizeof(buf), "%s/themes/%s",
                 elm_app_data_dir_get(), config->theme);
        edje_object_file_set(o, buf, "terminology/cursor");
        evas_object_resize(o, sd->font.chw, sd->font.chh);
        evas_object_show(o);

        o = evas_object_rectangle_add(evas_object_evas_get(obj));
        evas_object_smart_member_add(o, obj);
        sd->event = o;
        evas_object_color_set(o, 0, 0, 0, 0);
        evas_object_show(o);
        
        evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                       _smart_cb_mouse_down, obj);
        evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_UP,
                                       _smart_cb_mouse_up, obj);
        evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_MOVE,
                                       _smart_cb_mouse_move, obj);
        evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_WHEEL,
                                       _smart_cb_mouse_wheel, obj);
     }

   evas_object_event_callback_add(obj, EVAS_CALLBACK_KEY_DOWN,
                                  _smart_cb_key_down, obj);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_FOCUS_IN,
                                  _smart_cb_focus_in, obj);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_FOCUS_OUT,
                                  _smart_cb_focus_out, obj);
}

static void
_smart_del(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->cur.obj) evas_object_del(sd->cur.obj);
   if (sd->event) evas_object_del(sd->event);
   if (sd->cur.selo1) evas_object_del(sd->cur.selo1);
   if (sd->cur.selo2) evas_object_del(sd->cur.selo2);
   if (sd->cur.selo3) evas_object_del(sd->cur.selo3);
   if (sd->job) ecore_job_del(sd->job);
   if (sd->delayed_size_timer) ecore_timer_del(sd->delayed_size_timer);
   if (sd->grid.array) free(sd->grid.array);
   if (sd->font.name) eina_stringshare_del(sd->font.name);
   if (sd->pty) termpty_free(sd->pty);
   sd->cur.obj = NULL;
   sd->event = NULL;
   sd->cur.selo1 = NULL;
   sd->cur.selo2 = NULL;
   sd->cur.selo3 = NULL;
   sd->job = NULL;
   sd->delayed_size_timer = NULL;
   sd->grid.array = NULL;
   sd->font.name = NULL;
   sd->pty = NULL;
   _termio_sc.del(obj);
   evas_object_smart_data_set(obj, NULL);
}

static void
_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord ow, oh;
   if (!sd) return;
   evas_object_geometry_get(obj, NULL, NULL, &ow, &oh);
   if ((ow == w) && (oh == h)) return;
   evas_object_smart_changed(obj);
   if (sd->delayed_size_timer) ecore_timer_del(sd->delayed_size_timer);
   sd->delayed_size_timer = ecore_timer_add(0.02, _smart_cb_delayed_size, obj);
   evas_object_resize(sd->event, ow, oh);
}

static void
_smart_calculate(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh;
   
   if (!sd) return;

   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   if (sd->grid.array)
     {
        int i, x, y;
        
        i = 0;
        for (y = 0; y < sd->grid.h; y++)
          {
             for (x = 0; x < sd->grid.w; x++)
               {
                  evas_object_move(sd->grid.array[i].bg,
                                   ox + (x * sd->font.chw),
                                   oy + (y * sd->font.chh));
                  evas_object_move(sd->grid.array[i].tx,
                                   ox + (x * sd->font.chw),
                                   oy + (y * sd->font.chh));
                  i++;
               }
          }
     }
   evas_object_move(sd->cur.obj, 
                    ox + (sd->cur.x * sd->font.chw),
                    oy + (sd->cur.y * sd->font.chh));
   evas_object_move(sd->event, ox, oy);
   evas_object_resize(sd->event, ow, oh);
}

static void
_smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{  
   Termio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   evas_object_smart_changed(obj);
}

static void
_smart_init(void)
{
   static Evas_Smart_Class sc;
   
   evas_object_smart_clipped_smart_set(&_termio_sc);
   sc           = _termio_sc;
   sc.name      = "termio";
   sc.version   = EVAS_SMART_CLASS_VERSION;
   sc.add       = _smart_add;
   sc.del       = _smart_del;
   sc.resize    = _smart_resize;
   sc.move      = _smart_move;
   sc.calculate = _smart_calculate;
   _smart = evas_smart_class_new(&sc);
}

static void
_smart_pty_change(void *data)
{
   Evas_Object *obj = data;
   Termio *sd;
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;

   if (sd->jump_on_change) // if scroll to bottom on updates
     {
        // if term changed = croll back to bottom
        sd->scroll = 0;
     }
   if (sd->job) ecore_job_del(sd->job);
   sd->job = ecore_job_add(_smart_cb_change, obj);
}

static void
_smart_pty_scroll(void *data)
{
   Evas_Object *obj = data;
   Termio *sd;
   int changed = 0;
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;

   if ((!sd->jump_on_change) && // if NOT scroll to bottom on updates
       (sd->scroll > 0)) 
     {
        // adjust scroll position for added scrollback
        sd->scroll++;
        if (sd->scroll > sd->pty->backscroll_num)
          sd->scroll = sd->pty->backscroll_num;
        changed = 1;
     }
   if (sd->cur.sel)
     {
        sd->cur.sel1.y--;
        sd->cur.sel2.y--;
        changed = 1;
     }
   if (changed)
     {
        if (sd->job) ecore_job_del(sd->job);
        sd->job = ecore_job_add(_smart_cb_change, obj);
     }
}

static void
_smart_pty_title(void *data)
{
   Evas_Object *obj = data;
   Termio *sd;
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (!sd->win) return;
   elm_win_title_set(sd->win, sd->pty->prop.title);
}

static void
_smart_pty_icon(void *data)
{
   Evas_Object *obj = data;
   Termio *sd;
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (!sd->win) return;
   elm_win_icon_name_set(sd->win, sd->pty->prop.icon);
}

static void
_smart_pty_cancel_sel(void *data)
{
   Evas_Object *obj = data;
   Termio *sd;
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->cur.sel)
     {
        sd->cur.sel = 0;
        _clear_selection(data);
        sd->cur.makesel = 0;
        if (sd->job) ecore_job_del(sd->job);
        sd->job = ecore_job_add(_smart_cb_change, data);
     }
}

Evas_Object *
termio_add(Evas_Object *parent, const char *cmd, int w, int h)
{
   Evas *e;
   Evas_Object *obj;
   Termio *sd;

   EINA_SAFETY_ON_NULL_RETURN_VAL(parent, NULL);
   e = evas_object_evas_get(parent);
   if (!e) return NULL;
   
   if (!_smart) _smart_init();
   obj = evas_object_smart_add(e, _smart);
   sd = evas_object_smart_data_get(obj);
   if (!sd) return obj;
   sd->pty = termpty_new(cmd, w, h, config->scrollback);
   sd->pty->cb.change.func = _smart_pty_change;
   sd->pty->cb.change.data = obj;
   sd->pty->cb.scroll.func = _smart_pty_scroll;
   sd->pty->cb.scroll.data = obj;
   sd->pty->cb.set_title.func = _smart_pty_title;
   sd->pty->cb.set_title.data = obj;
   sd->pty->cb.set_icon.func = _smart_pty_icon;
   sd->pty->cb.set_icon.data = obj;
   sd->pty->cb.cancel_sel.func = _smart_pty_cancel_sel;
   sd->pty->cb.cancel_sel.data = obj;
   _smart_size(obj, w, h);
   return obj;
}

void
termio_win_set(Evas_Object *obj, Evas_Object *win)
{
   Termio *sd;
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   sd->win = win;
}

char *
termio_selection_get(Evas_Object *obj, int c1x, int c1y, int c2x, int c2y)
{
   Termio *sd;
   sd = evas_object_smart_data_get(obj);
   Eina_Strbuf *sb;
   char *s, txt[8];
   int x, y;
   
   if (!sd) return NULL;
   sb = eina_strbuf_new();
   for (y = c1y; y <= c2y; y++)
     {
        Termcell *cells;
        int w, last0, v, x1, x2;
        
        w = 0;
        last0 = -1;
        cells = termpty_cellrow_get(sd->pty, y - sd->scroll, &w);
        if (w > sd->grid.w) w = sd->grid.w;
        x1 = c1x;
        x2 = c2x;
        if (c1y != c2y)
          {
             if (y == c1y) x2 = w - 1;
             else if (y == c2y) x1 = 0;
             else
               {
                  x1 = 0;
                  x2 = w - 1;
               }
          }
        for (x = x1; x <= x2; x++)
          {
             if (x >= w) break;
             if (cells[x].glyph == 0)
               {
                  if (last0 < 0) last0 = x;
               }
             else if (cells[x].att.newline)
               {
                  last0 = -1;
                  eina_strbuf_append(sb, "\n");
                  break;
               }
             else if (cells[x].att.tab)
               {
                  eina_strbuf_append(sb, "\t");
                  x = ((x + 8) / 8) * 8;
                  x--;
               }
             else
               {
                  if (last0 >= 0)
                    {
                       v = x - last0 - 1;
                       last0 = -1;
                       while (v >= 0)
                         {
                            eina_strbuf_append(sb, " ");
                            v--;
                         }
                       if (x == (w - 1))
                         {
                            if (!cells[x].att.autowrapped)
                              eina_strbuf_append(sb, "\n");
                         }
                    }
                  glyph_to_utf8(cells[x].glyph, txt);
                  eina_strbuf_append(sb, txt);
               }
          }
        if (last0 >= 0)
          {
             eina_strbuf_append(sb, "\n");
          }
     }
   
   s = eina_strbuf_string_steal(sb);
   eina_strbuf_free(sb);
   return s;
}
