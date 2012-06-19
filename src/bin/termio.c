#include "private.h"

#include <Elementary.h>
#include "termio.h"
#include "termpty.h"
#include "utf8.h"
#include "col.h"
#include "keyin.h"
#include "config.h"

typedef struct _Termio Termio;

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
      Evas_Object *obj;
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
   struct {
      struct {
         int x, y;
      } sel1, sel2;
      Eina_Bool sel : 1;
   } backup;
   int scroll;
   Evas_Object *event;
   Termpty *pty;
   Ecore_Animator *anim;
   Ecore_Timer *delayed_size_timer;
   Evas_Object *win;
   Config *config;
#if HAVE_ECORE_IMF
   Ecore_IMF_Context *imf;
#endif
   Eina_Bool jump_on_change : 1;
};

static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

static void _smart_calculate(Evas_Object *obj);

static void
_reload_theme(void *data __UNUSED__, Evas_Object *obj,
	      const char *emission __UNUSED__, const char *source __UNUSED__)
{
   const char *file;
   const char *group;

   edje_object_file_get(obj, &file, &group);
   edje_object_file_set(obj, file, group);
   fprintf(stderr, "RELOADING THEME termio\n");
}

static void
_smart_apply(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh;
   int j, x, y, w, ch1 = 0, ch2 = 0;

   if (!sd) return;
   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);

   for (y = 0; y < sd->grid.h; y++)
     {
        Termcell *cells;
        Evas_Textgrid_Cell *tc;

        w = 0; j = 0;
        cells = termpty_cellrow_get(sd->pty, y - sd->scroll, &w);
        tc = evas_object_textgrid_cellrow_get(sd->grid.obj, y);
        if (!tc) continue;
        ch1 = -1;
        for (x = 0; x < sd->grid.w; x++)
          {
             if ((!cells) || (x >= w))
               {
                  if ((tc[x].codepoint != 0) ||
                      (tc[x].bg != COL_INVIS) ||
                      (tc[x].bg_extended))
                    {
                       if (ch1 < 0) ch1 = x;
                       ch2 = x;
                    }
                  tc[x].codepoint = 0;
                  tc[x].bg = COL_INVIS;
                  tc[x].bg_extended = 0;
               }
             else
               {
                  if (cells[j].att.invisible)
                    {
                       if ((tc[x].codepoint != 0) ||
                           (tc[x].bg != COL_INVIS) ||
                           (tc[x].bg_extended))
                         {
                            if (ch1 < 0) ch1 = x;
                            ch2 = x;
                         }
                       tc[x].codepoint = 0;
                       tc[x].bg = COL_INVIS;
                       tc[x].bg_extended = 0;
                    }
                  else
                    {
                       int bold, fg, bg, fgext, bgext, glyph;
                       
                       // colors
                       bold = cells[j].att.bold;
                       fgext = cells[j].att.fg256;
                       bgext = cells[j].att.bg256;
                       glyph = cells[j].glyph;
                       
                       if (cells[j].att.inverse)
                         {
                            int t;
                            
                            fgext = 0;
                            bgext = 0;
                            fg = cells[j].att.fg;
                            bg = cells[j].att.bg;
                            if (fg == COL_DEF) fg = COL_INVERSEBG;
                            if (bg == COL_DEF) bg = COL_INVERSE;
                            t = bg; bg = fg; fg = t;
                            if (bold)
                              {
                                 fg += 12;
                                 bg += 12;
                              }
                            if (cells[j].att.faint)
                              {
                                 fg += 24;
                                 bg += 24;
                              }
                            if (cells[j].att.fgintense) fg += 48;
                            if (cells[j].att.bgintense) bg += 48;
                         }
                       else
                         {
                            fg = cells[j].att.fg;
                            bg = cells[j].att.bg;
                            
                            if (!fgext)
                              {
                                 if (bold) fg += 12;
                              }
                            if (!bgext)
                              {
                                 if (bg == COL_DEF) bg = COL_INVIS;
                              }
                            if (cells[j].att.faint)
                              {
                                 if (!fgext) fg += 24;
                                 if (!bgext) bg += 24;
                              }
                            if (cells[j].att.fgintense) fg += 48;
                            if (cells[j].att.bgintense) bg += 48;
                            if ((glyph == ' ') || (glyph == 0))
                              fg = COL_INVIS;
                         }
                       if ((tc[x].codepoint != glyph) ||
                           (tc[x].fg != fg) ||
                           (tc[x].bg != bg) ||
                           (tc[x].fg_extended != fgext) ||
                           (tc[x].bg_extended != bgext) ||
                           (tc[x].underline != cells[j].att.underline) ||
                           (tc[x].strikethrough != cells[j].att.strike))
                         {
                            if (ch1 < 0) ch1 = x;
                            ch2 = x;
                         }
                       tc[x].fg_extended = fgext;
                       tc[x].bg_extended = bgext;
                       tc[x].underline = cells[j].att.underline;
                       tc[x].strikethrough = cells[j].att.strike;
                       tc[x].fg = fg;
                       tc[x].bg = bg;
                       tc[x].codepoint = glyph;
                       // cells[j].att.italic // never going 2 support
                       // cells[j].att.blink
                       // cells[j].att.blink2
                    }
               }
             j++;
          }
        evas_object_textgrid_cellrow_set(sd->grid.obj, y, tc);
        /* only bothering to keep 1 change span per row - not worth doing
         * more really */
        if (ch1 >= 0)
          evas_object_textgrid_update_add(sd->grid.obj, ch1, y,
                                          ch2 - ch1 + 1, 1);
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
        int start_x, start_y, end_x, end_y;

        start_x = sd->cur.sel1.x;
        start_y = sd->cur.sel1.y;
        end_x = sd->cur.sel2.x;
        end_y = sd->cur.sel2.y;
        if ((start_y > end_y) || ((start_y == end_y) && (end_x < start_x)))
          {
             int t;

             t = start_x; start_x = end_x; end_x = t;
             t = start_y; start_y = end_y; end_y = t;
          }

        if (end_y > start_y)
          {
             evas_object_move(sd->cur.selo1,
                              ox + (start_x * sd->font.chw),
                              oy + ((start_y + sd->scroll) * sd->font.chh));
             evas_object_resize(sd->cur.selo1,
                                (sd->grid.w - start_x) * sd->font.chw,
                                sd->font.chh);
             evas_object_show(sd->cur.selo1);

             evas_object_move(sd->cur.selo3,
                              ox, oy + ((end_y + sd->scroll) * sd->font.chh));
             evas_object_resize(sd->cur.selo3,
                                (end_x + 1) * sd->font.chw,
                                sd->font.chh);
             evas_object_show(sd->cur.selo3);
          }
        else
          {
             evas_object_move(sd->cur.selo1,
                              ox + (start_x * sd->font.chw),
                              oy + ((start_y + sd->scroll) * sd->font.chh));
             evas_object_resize(sd->cur.selo1,
                                (end_x - start_x + 1) * sd->font.chw,
                                sd->font.chh);
             evas_object_show(sd->cur.selo1);
             evas_object_hide(sd->cur.selo3);
          }
        if (end_y > (start_y + 1))
          {
             evas_object_move(sd->cur.selo2,
                              ox, oy + ((start_y + 1 + sd->scroll) * sd->font.chh));
             evas_object_resize(sd->cur.selo2,
                                sd->grid.w * sd->font.chw,
                                (end_y - start_y - 1) * sd->font.chh);
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
_smart_size(Evas_Object *obj, int w, int h, Eina_Bool force)
{
   Termio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;

   if (w < 1) w = 1;
   if (h < 1) h = 1;
   if (!force)
     {
        if ((w == sd->grid.w) && (h == sd->grid.h)) return;
     }

   evas_event_freeze(evas_object_evas_get(obj));
   evas_object_textgrid_size_set(sd->grid.obj, w, h);
   sd->grid.w = w;
   sd->grid.h = h;
   evas_object_resize(sd->cur.obj, sd->font.chw, sd->font.chh);
   evas_object_size_hint_min_set(obj, sd->font.chw, sd->font.chh);
   evas_object_size_hint_request_set(obj,
                                     sd->font.chw * sd->grid.w,
                                     sd->font.chh * sd->grid.h);
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
   _smart_size(obj, w, h, EINA_FALSE);
   return EINA_FALSE;
}

static Eina_Bool
_smart_cb_change(void *data)
{
   Evas_Object *obj = data;
   Termio *sd;
   sd = evas_object_smart_data_get(obj);
   if (!sd) return EINA_FALSE;
   sd->anim = NULL;
   _smart_apply(obj);
   evas_object_smart_callback_call(obj, "changed", NULL);
   return EINA_FALSE;
}

static void
_smart_update_queue(Evas_Object *obj, Termio *sd)
{
   if (sd->anim) return;
   sd->anim = ecore_animator_add(_smart_cb_change, obj);
}

static void
_take_selection(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   int start_x, start_y, end_x, end_y;
   char *s;

   if (!sd) return;
   start_x = sd->cur.sel1.x;
   start_y = sd->cur.sel1.y;
   end_x = sd->cur.sel2.x;
   end_y = sd->cur.sel2.y;
   if ((start_y > end_y) || ((start_y == end_y) && (end_x < start_x)))
     {
        int t;

        t = start_x; start_x = end_x; end_x = t;
        t = start_y; start_y = end_y; end_y = t;
     }
   s = termio_selection_get(obj, start_x, start_y, end_x, end_y);
   if (s)
     {
        if (sd->win)
          elm_cnp_selection_set(sd->win, ELM_SEL_TYPE_PRIMARY,
                                ELM_SEL_FORMAT_TEXT, s, strlen(s));
        free(s);
     }
}

static Eina_Bool
_getsel_cb(void *data, Evas_Object *obj __UNUSED__, Elm_Selection_Data *ev)
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

#ifdef HAVE_ECORE_IMF
static void
_smart_cb_key_up(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   Evas_Event_Key_Up *ev = event_info;
   Termio *sd;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;

   if (sd->imf)
     {
        Ecore_IMF_Event_Key_Up imf_ev;
        ecore_imf_evas_event_key_up_wrap(ev, &imf_ev);
        if (ecore_imf_context_filter_event
            (sd->imf, ECORE_IMF_EVENT_KEY_UP, (Ecore_IMF_Event *)&imf_ev))
          return;
     }
}
#endif

void
_smart_cb_key_down(void *data, Evas *e __UNUSED__, Evas_Object *obj, void *event)
{
   Evas_Event_Key_Down *ev = event;
   Termio *sd;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;

#ifdef HAVE_ECORE_IMF
   if (sd->imf)
     {
        Ecore_IMF_Event_Key_Down imf_ev;
        ecore_imf_evas_event_key_down_wrap(ev, &imf_ev);
        if (ecore_imf_context_filter_event
            (sd->imf, ECORE_IMF_EVENT_KEY_DOWN, (Ecore_IMF_Event *)&imf_ev))
          return;
     }
#endif

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
                  _smart_update_queue(data, sd);
                  return;
               }
             else if (!strcmp(ev->keyname, "Next"))
               {
                  sd->scroll -= by;
                  if (sd->scroll < 0) sd->scroll = 0;
                  _smart_update_queue(data, sd);
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

#ifdef HAVE_ECORE_IMF
static void
_imf_cursor_set(Termio *sd)
{
   /* TODO */
   Evas_Coord cx, cy, cw, ch;
   evas_object_geometry_get(sd->cur.obj, &cx, &cy, &cw, &ch);
   ecore_imf_context_cursor_location_set(sd->imf, cx, cy, cw, ch);
   /*
   ecore_imf_context_cursor_position_set(sd->imf, 0); // how to get it?
   */
}
#endif

void
_smart_cb_focus_in(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Termio *sd;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   edje_object_signal_emit(sd->cur.obj, "focus,in", "terminology");
   if (!sd->win) return;
   elm_win_keyboard_mode_set(sd->win, ELM_WIN_KEYBOARD_TERMINAL);

#ifdef HAVE_ECORE_IMF
   if (sd->imf)
     {
        ecore_imf_context_reset(sd->imf);
        ecore_imf_context_focus_in(sd->imf);
        _imf_cursor_set(sd);
     }
#endif
}

void
_smart_cb_focus_out(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Termio *sd;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   edje_object_signal_emit(sd->cur.obj, "focus,out", "terminology");
   if (!sd->win) return;
   elm_win_keyboard_mode_set(sd->win, ELM_WIN_KEYBOARD_OFF);

#ifdef HAVE_ECORE_IMF
   if (sd->imf)
     {
        ecore_imf_context_reset(sd->imf);
        _imf_cursor_set(sd);
        ecore_imf_context_focus_out(sd->imf);
     }
#endif
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
_glyph_is_wordsep(const Config *config, int g)
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
        if (_glyph_is_wordsep(sd->config, cells[x].glyph)) break;
        sd->cur.sel1.x = x;
     }
   sd->cur.sel2.x = cx;
   sd->cur.sel2.y = cy;
   for (x = sd->cur.sel2.x; x < sd->grid.w; x++)
     {
        if (x >= w) break;
        if (_glyph_is_wordsep(sd->config, cells[x].glyph)) break;
        sd->cur.sel2.x = x;
     }
}

static void
_sel_word_to(Evas_Object *obj, int cx, int cy)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Termcell *cells;
   int x, w = 0;
   if (!sd) return;

   cells = termpty_cellrow_get(sd->pty, cy - sd->scroll, &w);
   if (!cells) return;
   if (sd->cur.sel1.x > cx || sd->cur.sel1.y > cy)
     {
        sd->cur.sel1.x = cx;
        sd->cur.sel1.y = cy;
        for (x = sd->cur.sel1.x; x >= 0; x--)
          {
             if (x >= w) break;
             if (_glyph_is_wordsep(sd->config, cells[x].glyph)) break;
             sd->cur.sel1.x = x;
          }
     }
   else if (sd->cur.sel2.x < cx || sd->cur.sel2.y < cy)
     {
        sd->cur.sel2.x = cx;
        sd->cur.sel2.y = cy;
        for (x = sd->cur.sel2.x; x < sd->grid.w; x++)
          {
             if (x >= w) break;
             if (_glyph_is_wordsep(sd->config, cells[x].glyph)) break;
             sd->cur.sel2.x = x;
          }
     }
}

static void
_smart_cb_mouse_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
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
             if (evas_key_modifier_is_set(ev->modifiers, "Shift") && sd->backup.sel)
               {
                  sd->cur.sel = 1;
                  sd->cur.sel1.x = sd->backup.sel1.x;
                  sd->cur.sel1.y = sd->backup.sel1.y;
                  sd->cur.sel2.x = sd->backup.sel2.x;
                  sd->cur.sel2.y = sd->backup.sel2.y;
                  
                  _sel_word_to(data, cx, cy - sd->scroll);
               }
             else
               {
                  _sel_word(data, cx, cy - sd->scroll);
               }
             if (sd->cur.sel) _take_selection(data);
          }
        else
          {
             sd->backup.sel = sd->cur.sel;
             sd->backup.sel1.x = sd->cur.sel1.x;
             sd->backup.sel1.y = sd->cur.sel1.y;
             sd->backup.sel2.x = sd->cur.sel2.x;
             sd->backup.sel2.y = sd->cur.sel2.y;
             if (sd->cur.sel) sd->cur.sel = 0;
             sd->cur.makesel = 1;
             sd->cur.sel1.x = cx;
             sd->cur.sel1.y = cy - sd->scroll;
             sd->cur.sel2.x = cx;
             sd->cur.sel2.y = cy - sd->scroll;
          }
        _smart_update_queue(data, sd);
     }
   else if (ev->button == 2)
     _paste_selection(data);
   else if (ev->button == 3)
     evas_object_smart_callback_call(data, "options", NULL);
}

static void
_smart_cb_mouse_up(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
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
             _smart_update_queue(data, sd);
             _take_selection(data);
          }
     }
}

static void
_smart_cb_mouse_move(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
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
        _smart_update_queue(data, sd);
     }
}

static void
_smart_cb_mouse_wheel(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
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
   _smart_update_queue(data, sd);
}

static void
_win_obj_del(void *data, Evas *e __UNUSED__, Evas_Object *obj, void *event __UNUSED__)
{
   Termio *sd;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   if (obj == sd->win)
     {
        evas_object_event_callback_del_full(sd->win, EVAS_CALLBACK_DEL,
                                            _win_obj_del, data);
        sd->win = NULL;
     }
}

static void
_termio_config_set(Evas_Object *obj, Config *config)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord w = 2, h = 2;

   sd->config = config;

   sd->jump_on_change = config->jump_on_change;

   if (config->font.bitmap)
     {
        char buf[PATH_MAX];
        snprintf(buf, sizeof(buf), "%s/fonts/%s",
                 elm_app_data_dir_get(), config->font.name);
        sd->font.name = eina_stringshare_add(buf);
     }
   else
     sd->font.name = eina_stringshare_add(config->font.name);
   sd->font.size = config->font.size;

   evas_object_scale_set(sd->grid.obj, elm_config_scale_get());
   evas_object_textgrid_font_set(sd->grid.obj, sd->font.name, sd->font.size);
   evas_object_textgrid_size_set(sd->grid.obj, 1, 1);
   evas_object_textgrid_cell_size_get(sd->grid.obj, &w, &h);
   if (w < 1) w = 1;
   if (h < 1) h = 1;
   sd->font.chw = w;
   sd->font.chh = h;

   edje_object_file_set(sd->cur.obj,
                        config_theme_path_get(config), "terminology/cursor");
   edje_object_signal_callback_add(sd->cur.obj, "edje,change,file", "edje", _reload_theme, NULL);
   evas_object_resize(sd->cur.obj, sd->font.chw, sd->font.chh);
   evas_object_show(sd->cur.obj);
}

#ifdef HAVE_ECORE_IMF
static void
_imf_event_commit_cb(void *data, Ecore_IMF_Context *ctx __UNUSED__, void *event_info)
{
   Termio *sd = data;
   char *str = event_info;
   DBG("IMF committed '%s'", str);
   if (!str) return;
   termpty_write(sd->pty, str, strlen(str));
}
#endif

static void
_smart_add(Evas_Object *obj)
{
   Termio *sd;
   Evas_Object_Smart_Clipped_Data *cd;
   Evas_Object *o;
   int i, j, k, l, n;

   _parent_sc.add(obj);
   cd = evas_object_smart_data_get(obj);
   if (!cd) return;
   sd = calloc(1, sizeof(Termio));
   if (!sd) return;
   sd->__clipped_data = *cd;
   free(cd);
   evas_object_smart_data_set(obj, sd);

   o = evas_object_textgrid_add(evas_object_evas_get(obj));
   evas_object_pass_events_set(o, EINA_TRUE);
   evas_object_propagate_events_set(o, EINA_FALSE);
   evas_object_smart_member_add(o, obj);
   evas_object_show(o);
   sd->grid.obj = o;

   for (n = 0, l = 0; l < 2; l++) // normal/intense
     {
        for (k = 0; k < 2; k++) // normal/faint
          {
             for (j = 0; j < 2; j++) // normal/bright
               {
                  for (i = 0; i < 12; i++, n++) //colors
                    evas_object_textgrid_palette_set
                    (o, EVAS_TEXTGRID_PALETTE_STANDARD, n,
                        colors[l][j][i].r / (k + 1),
                        colors[l][j][i].g / (k + 1),
                        colors[l][j][i].b / (k + 1),
                        colors[l][j][i].a / (k + 1));
               }
          }
     }
   for (n = 0; n < 256; n++)
     {
        evas_object_textgrid_palette_set
          (o, EVAS_TEXTGRID_PALETTE_EXTENDED, n,
              colors256[n].r, colors256[n].g,
              colors256[n].b, colors256[n].a);
     }
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

   evas_object_event_callback_add(obj, EVAS_CALLBACK_KEY_DOWN,
                                  _smart_cb_key_down, obj);
#ifdef HAVE_ECORE_IMF
   evas_object_event_callback_add(obj, EVAS_CALLBACK_KEY_UP,
                                  _smart_cb_key_up, obj);
#endif
   evas_object_event_callback_add(obj, EVAS_CALLBACK_FOCUS_IN,
                                  _smart_cb_focus_in, obj);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_FOCUS_OUT,
                                  _smart_cb_focus_out, obj);

#ifdef HAVE_ECORE_IMF
   if (ecore_imf_init())
     {
        const char *imf_id = ecore_imf_context_default_id_get();
        Evas *e;

        if (!imf_id) sd->imf = NULL;
        else
          {
             const Ecore_IMF_Context_Info *imf_info;

             imf_info = ecore_imf_context_info_by_id_get(imf_id);
             if ((!imf_info->canvas_type) ||
                 (strcmp(imf_info->canvas_type, "evas") == 0))
               sd->imf = ecore_imf_context_add(imf_id);
             else
               {
                  imf_id = ecore_imf_context_default_id_by_canvas_type_get("evas");
                  if (imf_id)
                    sd->imf = ecore_imf_context_add(imf_id);
               }
          }

        if (!sd->imf) goto imf_done;

        e = evas_object_evas_get(o);
        ecore_imf_context_client_window_set
          (sd->imf, (void *)ecore_evas_window_get(ecore_evas_ecore_evas_get(e)));
        ecore_imf_context_client_canvas_set(sd->imf, e);

        ecore_imf_context_event_callback_add
          (sd->imf, ECORE_IMF_CALLBACK_COMMIT, _imf_event_commit_cb, sd);

        /* make IMF usable by a terminal - no preedit, prediction... */
        ecore_imf_context_use_preedit_set(sd->imf, EINA_FALSE);
        ecore_imf_context_prediction_allow_set(sd->imf, EINA_FALSE);
        ecore_imf_context_autocapital_type_set(sd->imf, ECORE_IMF_AUTOCAPITAL_TYPE_NONE);

     imf_done:
        if (sd->imf) DBG("Ecore IMF Setup");
        else WRN("Ecore IMF failed");
     }
#endif
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
   if (sd->anim) ecore_animator_del(sd->anim);
   if (sd->delayed_size_timer) ecore_timer_del(sd->delayed_size_timer);
   if (sd->font.name) eina_stringshare_del(sd->font.name);
   if (sd->pty) termpty_free(sd->pty);
   sd->cur.obj = NULL;
   sd->event = NULL;
   sd->cur.selo1 = NULL;
   sd->cur.selo2 = NULL;
   sd->cur.selo3 = NULL;
   sd->anim = NULL;
   sd->delayed_size_timer = NULL;
   sd->font.name = NULL;
   sd->pty = NULL;

#ifdef HAVE_ECORE_IMF
   if (sd->imf)
     {
        ecore_imf_context_event_callback_del
          (sd->imf, ECORE_IMF_CALLBACK_COMMIT, _imf_event_commit_cb);
        ecore_imf_context_del(sd->imf);
        sd->imf = NULL;
     }
   ecore_imf_shutdown();
#endif

   _parent_sc.del(obj);
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
   if (!sd->delayed_size_timer) sd->delayed_size_timer = 
     ecore_timer_add(0.0, _smart_cb_delayed_size, obj);
   else ecore_timer_delay(sd->delayed_size_timer, 0.0);
   evas_object_resize(sd->event, ow, oh);
}

static void
_smart_calculate(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh;

   if (!sd) return;

   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   evas_object_move(sd->grid.obj, ox, oy);
   evas_object_resize(sd->grid.obj,
                      sd->grid.w * sd->font.chw,
                      sd->grid.h * sd->font.chh);
   evas_object_move(sd->cur.obj,
                    ox + (sd->cur.x * sd->font.chw),
                    oy + (sd->cur.y * sd->font.chh));
   evas_object_move(sd->event, ox, oy);
   evas_object_resize(sd->event, ow, oh);

#ifdef HAVE_ECORE_IMF
   if (sd->imf) _imf_cursor_set(sd);
#endif
}

static void
_smart_move(Evas_Object *obj, Evas_Coord x __UNUSED__, Evas_Coord y __UNUSED__)
{
   Termio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   evas_object_smart_changed(obj);
}

static void
_smart_init(void)
{
   static Evas_Smart_Class sc;

   evas_object_smart_clipped_smart_set(&_parent_sc);
   sc           = _parent_sc;
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

// if scroll to bottom on updates
   if (sd->jump_on_change)  sd->scroll = 0;
   _smart_update_queue(data, sd);
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
   if (changed) _smart_update_queue(data, sd);
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
        sd->cur.makesel = 0;
        _smart_update_queue(data, sd);
     }
}

Evas_Object *
termio_add(Evas_Object *parent, Config *config, const char *cmd, int w, int h)
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

   _termio_config_set(obj, config);

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
   _smart_size(obj, w, h, EINA_FALSE);
   return obj;
}

void
termio_win_set(Evas_Object *obj, Evas_Object *win)
{
   Termio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->win)
     {
        evas_object_event_callback_del_full(sd->win, EVAS_CALLBACK_DEL,
                                            _win_obj_del, obj);
        sd->win = NULL;
     }
   if (win)
     {
        sd->win = win;
        evas_object_event_callback_add(sd->win, EVAS_CALLBACK_DEL,
                                       _win_obj_del, obj);
     }
}

char *
termio_selection_get(Evas_Object *obj, int c1x, int c1y, int c2x, int c2y)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Eina_Strbuf *sb;
   char *s, txt[8];
   int x, y;

   if (!sd) return NULL;
   sb = eina_strbuf_new();
   for (y = c1y; y <= c2y; y++)
     {
        Termcell *cells;
        int w, last0, v, start_x, end_x;

        w = 0;
        last0 = -1;
        cells = termpty_cellrow_get(sd->pty, y, &w);
        if (w > sd->grid.w) w = sd->grid.w;
        start_x = c1x;
        end_x = c2x;
        if (c1y != c2y)
          {
             if (y == c1y) end_x = w - 1;
             else if (y == c2y) start_x = 0;
             else
               {
                  start_x = 0;
                  end_x = w - 1;
               }
          }
        for (x = start_x; x <= end_x; x++)
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

void
termio_config_update(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord w, h;
   char buf[4096];

   if (!sd) return;

   if (sd->font.name) eina_stringshare_del(sd->font.name);
   sd->font.name = NULL;

   if (sd->config->font.bitmap)
     {
        snprintf(buf, sizeof(buf), "%s/fonts/%s",
                 elm_app_data_dir_get(), sd->config->font.name);
        sd->font.name = eina_stringshare_add(buf);
     }
   else
     sd->font.name = eina_stringshare_add(sd->config->font.name);
   sd->font.size = sd->config->font.size;

   sd->jump_on_change = sd->config->jump_on_change;

   termpty_backscroll_set(sd->pty, sd->config->scrollback);
   sd->scroll = 0;
   
   evas_object_scale_set(sd->grid.obj, elm_config_scale_get());
   evas_object_textgrid_font_set(sd->grid.obj, sd->font.name, sd->font.size);
   evas_object_textgrid_cell_size_get(sd->grid.obj, &w, &h);
   if (w < 1) w = 1;
   if (h < 1) h = 1;
   sd->font.chw = w;
   sd->font.chh = h;
   _smart_size(obj, sd->grid.w, sd->grid.h, EINA_TRUE);
}

Config *
termio_config_get(const Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);
   return sd->config;
}
