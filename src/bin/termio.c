#include "private.h"
#include <Ecore_IMF.h>
#include <Ecore_IMF_Evas.h>
#include <Elementary.h>
#include <Ecore_Input.h>
#include "termio.h"
#include "termiolink.h"
#include "termpty.h"
#include "utf8.h"
#include "col.h"
#include "keyin.h"
#include "config.h"
#include "utils.h"
#include "media.h"

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
      Evas_Object *obj, *selo_top, *selo_bottom, *selo_theme;
      int x, y;
      struct {
         int x, y;
      } sel1, sel2;
      Eina_Bool sel : 1;
      Eina_Bool makesel : 1;
   } cur;
   struct {
      int cx, cy;
   } mouse;
   struct {
      struct {
         int x, y;
      } sel1, sel2;
      Eina_Bool sel : 1;
   } backup;
   struct {
      char *string;
      int x1, y1, x2, y2;
      int suspend;
      Eina_List *objs;
   } link;
   int zoom_fontsize_start;
   int scroll;
   unsigned int last_keyup;
   Eina_List *seq;
   Evas_Object *event;
   Termpty *pty;
   Ecore_Animator *anim;
   Ecore_Timer *delayed_size_timer;
   Ecore_Timer *link_do_timer;
   Ecore_Job *mouse_move_job;
   Evas_Object *win;
   Config *config;
   Ecore_IMF_Context *imf;
   Eina_Bool jump_on_change : 1;
   Eina_Bool jump_on_keypress : 1;
   Eina_Bool have_sel : 1;
   Eina_Bool noreqsize : 1;
   Eina_Bool composing : 1;
   Eina_Bool didclick : 1;
   Eina_Bool bottom_right : 1;
   Eina_Bool top_left : 1;
};

static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

static void _smart_calculate(Evas_Object *obj);

static void
_activate_link(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Config *config = termio_config_get(obj);
   char buf[PATH_MAX], *s;
   const char *path = NULL, *cmd = NULL;
   Eina_Bool url = EINA_FALSE, email = EINA_FALSE, handled = EINA_FALSE;
   int type;
   
   if (!sd) return;
   if (!config) return;
   if (!sd->link.string) return;
   if (link_is_url(sd->link.string))
     url = EINA_TRUE;
   else if ((!strncasecmp(sd->link.string, "file://", 7)) ||
            (!strncasecmp(sd->link.string, "/", 1)))
     {
        path = sd->link.string;
        if (!strncasecmp(sd->link.string, "file://", 7)) path = path + 7;
     }
   else if (strchr(sd->link.string, '@'))
     email = EINA_TRUE;
   
   s = eina_str_escape(sd->link.string);
   if (!s) return;
   if (email)
     {
        // run mail client
        cmd = "xdg-email";
        
        if ((config->helper.email) &&
            (config->helper.email[0]))
          cmd = config->helper.email;
        snprintf(buf, sizeof(buf), "%s %s", cmd, s);
     }
   else if (path)
     {
        // locally accessible file
        cmd = "xdg-open";
        
        type = media_src_type_get(sd->link.string);
        if (config->helper.inline_please)
          {
             if ((type == TYPE_IMG) ||
                 (type == TYPE_SCALE) ||
                 (type == TYPE_EDJE))
               {
                  evas_object_smart_callback_call(obj, "popup", NULL);
                  handled = EINA_TRUE;
               }
             else if (type == TYPE_MOV)
               {
                  evas_object_smart_callback_call(obj, "popup", NULL);
                  handled = EINA_TRUE;
               }
          }
        if (!handled)
          {
             if ((type == TYPE_IMG) ||
                 (type == TYPE_SCALE) ||
                 (type == TYPE_EDJE))
               {
                  if ((config->helper.local.image) &&
                      (config->helper.local.image[0]))
                    cmd = config->helper.local.image;
               }
             else if (type == TYPE_MOV)
               {
                  if ((config->helper.local.video) &&
                      (config->helper.local.video[0]))
                    cmd = config->helper.local.video;
               }
             else
               {
                  if ((config->helper.local.general) &&
                      (config->helper.local.general[0]))
                    cmd = config->helper.local.general;
               }
             snprintf(buf, sizeof(buf), "%s %s", cmd, s);
          }
     }
   else if (url)
     {
        // remote file needs ecore-con-url
        cmd = "xdg-open";
        
        type = media_src_type_get(sd->link.string);
        if (config->helper.inline_please)
          {
             if ((type == TYPE_IMG) ||
                 (type == TYPE_SCALE) ||
                 (type == TYPE_EDJE))
               {
                  // XXX: begin fetch of url, once done, show
                  evas_object_smart_callback_call(obj, "popup", NULL);
                  handled = EINA_TRUE;
               }
             else if (type == TYPE_MOV)
               {
                  // XXX: if no http:// add
                  evas_object_smart_callback_call(obj, "popup", NULL);
                  handled = EINA_TRUE;
               }
          }
        if (!handled)
          {
             if ((type == TYPE_IMG) ||
                 (type == TYPE_SCALE) ||
                 (type == TYPE_EDJE))
               {
                  if ((config->helper.url.image) &&
                      (config->helper.url.image[0]))
                    cmd = config->helper.url.image;
               }
             else if (type == TYPE_MOV)
               {
                  if ((config->helper.url.video) &&
                      (config->helper.url.video[0]))
                    cmd = config->helper.url.video;
               }
             else
               {
                  if ((config->helper.url.general) &&
                      (config->helper.url.general[0]))
                    cmd = config->helper.url.general;
               }
             snprintf(buf, sizeof(buf), "%s %s", cmd, s);
          }
     }
   else
     {
        free(s);
        return;
     }
   free(s);
   if (!handled) ecore_exe_run(buf, NULL);
}

static Eina_Bool
_cb_link_up_delay(void *data)
{
   Termio *sd = evas_object_smart_data_get(data);
   
   if (!sd) return EINA_FALSE;
   sd->link_do_timer = NULL;
   if (!sd->didclick) _activate_link(data);
   sd->didclick = EINA_FALSE;
   return EINA_FALSE;
}

static void
_cb_link_up(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
{
   Evas_Event_Mouse_Up *ev = event;
   Termio *sd = evas_object_smart_data_get(data);
   
   if (!sd) return;
   if (ev->button == 1)
     {
        if (sd->link_do_timer) ecore_timer_del(sd->link_do_timer);
        sd->link_do_timer = ecore_timer_add(0.2, _cb_link_up_delay, data);
     }
}

static void
_update_link(Evas_Object *obj, Eina_Bool same_link, Eina_Bool same_geom)
{
   Termio *sd = evas_object_smart_data_get(obj);
   
   if (!sd) return;
   
   if (!same_link)
     {
        // check link and re-probe/fetch create popup preview
     }
   
   if (!same_geom)
     {
        Evas_Coord ox, oy, ow, oh;
        Evas_Object *o;
        // fix up edje objects "underlining" the link
        int y;
        
        evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
        EINA_LIST_FREE(sd->link.objs, o) evas_object_del(o);
        if ((sd->link.string) && (sd->link.suspend == 0))
          {
             for (y = sd->link.y1; y <= sd->link.y2; y++)
               {
                  o = edje_object_add(evas_object_evas_get(obj));
                  evas_object_smart_member_add(o, obj);
                  theme_apply(o, sd->config, "terminology/link");
                  
                  if (y == sd->link.y1)
                    {
                       evas_object_move(o, ox + (sd->link.x1 * sd->font.chw),
                                        oy + (y * sd->font.chh));
                       if (sd->link.y1 == sd->link.y2)
                         evas_object_resize(o,
                                            ((sd->link.x2 - sd->link.x1 + 1) * sd->font.chw),
                                            sd->font.chh);
                       else
                         evas_object_resize(o,
                                            ((sd->grid.w - sd->link.x1) * sd->font.chw),
                                            sd->font.chh);
                    }
                  else if (y == sd->link.y2)
                    {
                       evas_object_move(o, ox, oy + (y * sd->font.chh));
                       evas_object_resize(o,
                                          ((sd->link.x2 + 1) * sd->font.chw),
                                          sd->font.chh);
                    }
                  else
                    {
                       evas_object_move(o, ox, oy + (y * sd->font.chh));
                       evas_object_resize(o, (sd->grid.w * sd->font.chw),
                                          sd->font.chh);
                    }
                  
                  sd->link.objs = eina_list_append(sd->link.objs, o);
                  evas_object_show(o);
                  evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_UP,
                                                 _cb_link_up, obj);
               }
          }
     }
}

static void
_smart_mouseover_apply(Evas_Object *obj)
{
   char *s;
   int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
   Eina_Bool same_link = EINA_FALSE, same_geom = EINA_FALSE;
   Termio *sd = evas_object_smart_data_get(obj);
   
   if (!sd) return;

   s = _termio_link_find(obj, sd->mouse.cx, sd->mouse.cy,
                         &x1, &y1, &x2, &y2);
   if (!s)
     {
        if (sd->link.string) free(sd->link.string);
        sd->link.string = NULL;
        sd->link.x1 = -1;
        sd->link.y1 = -1;
        sd->link.x2 = -1;
        sd->link.y2 = -1;
        _update_link(obj, same_link, same_geom);
        return;
     }
   
   if ((sd->link.string) && (!strcmp(sd->link.string, s)))
     same_link = EINA_TRUE;
   if (sd->link.string) free(sd->link.string);
   sd->link.string = s;
   if ((x1 == sd->link.x1) && (y1 == sd->link.y1) &&
       (x2 == sd->link.x2) && (y2 == sd->link.y2))
     same_geom = EINA_TRUE;
   if (((sd->link.suspend != 0) && (sd->link.objs)) ||
       ((sd->link.suspend == 0) && (!sd->link.objs)))
     same_geom = EINA_FALSE;
   sd->link.x1 = x1;
   sd->link.y1 = y1;
   sd->link.x2 = x2;
   sd->link.y2 = y2;
   _update_link(obj, same_link, same_geom);
}

static void
_smart_apply(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh;
   int j, x, y, w, ch1 = 0, ch2 = 0, inv = 0;

   if (!sd) return;
   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);

   inv = sd->pty->state.reverse;
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
                  if (inv) tc[x].bg = COL_INVERSEBG;
                  else tc[x].bg = COL_INVIS;
                  tc[x].bg_extended = 0;
                  tc[x].double_width = cells[j].att.dblwidth;
                  if ((tc[x].double_width) && (tc[x].codepoint == 0) &&
                      (ch2 == x - 1))
                    ch2 = x;
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
                       if (inv) tc[x].bg = COL_INVERSEBG;
                       else tc[x].bg = COL_INVIS;
                       tc[x].bg_extended = 0;
                       tc[x].double_width = cells[j].att.dblwidth;
                       if ((tc[x].double_width) && (tc[x].codepoint == 0) &&
                           (ch2 == x - 1))
                         ch2 = x;
                    }
                  else
                    {
                       int bold, fg, bg, fgext, bgext, codepoint;
                       
                       // colors
                       bold = cells[j].att.bold;
                       fgext = cells[j].att.fg256;
                       bgext = cells[j].att.bg256;
                       codepoint = cells[j].codepoint;
                       
                       if (cells[j].att.inverse ^ inv)
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
                            if ((codepoint == ' ') || (codepoint == 0))
                              fg = COL_INVIS;
                         }
                       if ((tc[x].codepoint != codepoint) ||
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
                       tc[x].codepoint = codepoint;
                       tc[x].double_width = cells[j].att.dblwidth;
                       if ((tc[x].double_width) && (tc[x].codepoint == 0) &&
                           (ch2 == x - 1))
                         ch2 = x;
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
	int size_top, size_bottom;

        start_x = sd->cur.sel1.x;
        start_y = sd->cur.sel1.y;
        end_x = sd->cur.sel2.x;
        end_y = sd->cur.sel2.y;
        if ((start_y > end_y) || ((start_y == end_y) && (end_x < start_x)))
          {
             int t;

             t = start_x; start_x = end_x; end_x = t;
             t = start_y; start_y = end_y; end_y = t;

	     if (sd->top_left)
	       {
                  sd->top_left = EINA_FALSE;
                  sd->bottom_right = EINA_TRUE;
                  edje_object_signal_emit(sd->cur.selo_theme, "mouse,out", "zone.top_left");
                  edje_object_signal_emit(sd->cur.selo_theme, "mouse,in", "zone.bottom_right");
	       }
             else if (sd->bottom_right)
               {
                  sd->top_left = EINA_TRUE;
                  sd->bottom_right = EINA_FALSE;
                  edje_object_signal_emit(sd->cur.selo_theme, "mouse,out", "zone.bottom_right");
                  edje_object_signal_emit(sd->cur.selo_theme, "mouse,in", "zone.top_left");
               }
          }

	size_top = start_x * sd->font.chw;

	size_bottom = (sd->grid.w - end_x - 1) * sd->font.chw;

        evas_object_size_hint_min_set(sd->cur.selo_top,
                                      size_top,
                                      sd->font.chh);
        evas_object_size_hint_max_set(sd->cur.selo_top,
                                      size_top,
                                      sd->font.chh);
        evas_object_size_hint_min_set(sd->cur.selo_bottom,
                                      size_bottom,
                                      sd->font.chh);
        evas_object_size_hint_max_set(sd->cur.selo_bottom,
                                      size_bottom,
                                      sd->font.chh);
        evas_object_move(sd->cur.selo_theme,
                         ox,
                         oy + ((start_y + sd->scroll) * sd->font.chh));
        evas_object_resize(sd->cur.selo_theme,
                           sd->grid.w * sd->font.chw,
                           (end_y + 1 - start_y) * sd->font.chh);
        if ((start_y == end_y) ||
            ((start_x == 0) && (end_x == (sd->grid.w - 1))))
          edje_object_signal_emit(sd->cur.selo_theme, 
                                  "mode,oneline", "terminology");
        else if ((start_y == (end_y - 1)) &&
                 (start_x > end_x))
          edje_object_signal_emit(sd->cur.selo_theme, 
                                  "mode,disjoint", "terminology");
        else if (start_x == 0)
          edje_object_signal_emit(sd->cur.selo_theme, 
                                  "mode,topfull", "terminology");
        else if (end_x == (sd->grid.w - 1))
          {
             edje_object_signal_emit(sd->cur.selo_theme, 
                                     "mode,bottomfull", "terminology");
          }
        else
          edje_object_signal_emit(sd->cur.selo_theme,
                                  "mode,multiline", "terminology");
        evas_object_show(sd->cur.selo_theme);
     }
   else
     evas_object_hide(sd->cur.selo_theme);
   _smart_mouseover_apply(obj);
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
   if (!sd->noreqsize)
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
_lost_selection(void *data, Elm_Sel_Type selection)
{
   Termio *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   if (sd->have_sel)
     {
        sd->cur.sel = 0;
        elm_object_cnp_selection_clear(sd->win, selection);
        _smart_update_queue(data, sd);
        sd->have_sel = EINA_FALSE;
     }
}

static void
_take_selection(Evas_Object *obj, Elm_Sel_Type type)
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
          {
             sd->have_sel = EINA_FALSE;
             elm_cnp_selection_set(sd->win, type,
                                   ELM_SEL_FORMAT_TEXT, s, strlen(s));
             elm_cnp_selection_loss_callback_set(type, _lost_selection, obj);
             sd->have_sel = EINA_TRUE;
          }
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
          {
             char *tmp, *s;
             size_t i;

             // apparently we have to convert \n into \r in terminal land.
             tmp = malloc(ev->len);
             if (tmp)
               {
                  s = ev->data;
                  for (i = 0; i < ev->len; i++)
                    {
                       tmp[i] = s[i];
                       if (tmp[i] == '\n') tmp[i] = '\r';
                    }
                  termpty_write(sd->pty, tmp, ev->len - 1);
                  free(tmp);
               }
          }
     }
   return EINA_TRUE;
}

static void
_paste_selection(Evas_Object *obj, Elm_Sel_Type type)
{
   Termio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (!sd->win) return;
   elm_cnp_selection_get(sd->win, type, ELM_SEL_FORMAT_TEXT,
                         _getsel_cb, obj);
}

static void
_font_size_set(Evas_Object *obj, int size)
{
   Termio *sd = evas_object_smart_data_get(obj);
   Config *config = termio_config_get(obj);
   if (!sd) return;

   if (size < 5) size = 5;
   else if (size > 100) size = 100;
   if (config)
     {
        Evas_Coord mw = 1, mh = 1;
        int gw, gh;
        
        config->temporary = EINA_TRUE;
        config->font.size = size;
        gw = sd->grid.w;
        gh = sd->grid.h;
        evas_object_size_hint_min_get(obj, &mw, &mh);
        sd->noreqsize = 1;
        termio_config_update(obj);
        sd->noreqsize = 0;
        evas_object_size_hint_min_get(obj, &mw, &mh);
        evas_object_data_del(obj, "sizedone");
        evas_object_size_hint_request_set(obj, mw * gw, mh * gh);
     }
}

void
termio_font_size_set(Evas_Object *obj, int size)
{
   _font_size_set(obj, size);
}

static void
_smart_cb_key_up(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
{
   Evas_Event_Key_Up *ev = event;
   Termio *sd;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   sd->last_keyup = ev->timestamp;
   if (sd->imf)
     {
        Ecore_IMF_Event_Key_Up imf_ev;
        ecore_imf_evas_event_key_up_wrap(ev, &imf_ev);
        if (ecore_imf_context_filter_event
            (sd->imf, ECORE_IMF_EVENT_KEY_UP, (Ecore_IMF_Event *)&imf_ev))
          return;
     }
}

static Eina_Bool
_is_modifier(const char *key)
{
   if ((!strncmp(key, "Shift", 5)) ||
       (!strncmp(key, "Control", 7)) ||
       (!strncmp(key, "Alt", 3)) ||
       (!strncmp(key, "Meta", 4)) ||
       (!strncmp(key, "Super", 5)) ||
       (!strncmp(key, "Hyper", 5)) ||
       (!strcmp(key, "Scroll_Lock")) ||
       (!strcmp(key, "Num_Lock")) ||
       (!strcmp(key, "Caps_Lock")))
     return EINA_TRUE;
   return EINA_FALSE;
}

static void
_compose_seq_reset(Termio *sd)
{
   char *str;
   
   EINA_LIST_FREE(sd->seq, str) eina_stringshare_del(str);
   sd->composing = EINA_FALSE;
}

void
_smart_cb_key_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
{
   Evas_Event_Key_Down *ev = event;
   Termio *sd;
   Ecore_Compose_State state;
   char *compres = NULL;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   if (sd->imf)
     {
        // EXCEPTION. Don't filter modifiers alt+shift -> breaks emacs
        // and jed (alt+shift+5 for search/replace for example)
        // Don't filter modifiers alt, is used by shells
        if (!evas_key_modifier_is_set(ev->modifiers, "Alt"))
          {
             Ecore_IMF_Event_Key_Down imf_ev;
             
             ecore_imf_evas_event_key_down_wrap(ev, &imf_ev);
             if (!sd->composing)
               {
                  if (ecore_imf_context_filter_event
                      (sd->imf, ECORE_IMF_EVENT_KEY_DOWN, (Ecore_IMF_Event *)&imf_ev))
                    goto end;
               }
          }
     }
   if ((evas_key_modifier_is_set(ev->modifiers, "Alt")) &&
       (!evas_key_modifier_is_set(ev->modifiers, "Shift")) &&
       (!evas_key_modifier_is_set(ev->modifiers, "Control")) &&
       (!strcmp(ev->keyname, "Home")))
     {
        _compose_seq_reset(sd);
        evas_object_smart_callback_call(data, "cmdbox", NULL);
        goto end;
     }
   if (evas_key_modifier_is_set(ev->modifiers, "Shift"))
     {
        if (ev->keyname)
          {
             int by = sd->grid.h - 2;

             if (by < 1) by = 1;
             if (!strcmp(ev->keyname, "Prior"))
               {
                  _compose_seq_reset(sd);
                  sd->scroll += by;
                  if (sd->scroll > sd->pty->backscroll_num)
                    sd->scroll = sd->pty->backscroll_num;
                  _smart_update_queue(data, sd);
                  goto end;
               }
             else if (!strcmp(ev->keyname, "Next"))
               {
                  _compose_seq_reset(sd);
                  sd->scroll -= by;
                  if (sd->scroll < 0) sd->scroll = 0;
                  _smart_update_queue(data, sd);
                  goto end;
               }
             else if (!strcmp(ev->keyname, "Insert"))
               {
                  _compose_seq_reset(sd);
                  if (evas_key_modifier_is_set(ev->modifiers, "Control"))
                    _paste_selection(data, ELM_SEL_TYPE_PRIMARY);
                  else
                    _paste_selection(data, ELM_SEL_TYPE_CLIPBOARD);
                  goto end;
               }
             else if (!strcmp(ev->keyname, "KP_Add"))
               {
                  Config *config = termio_config_get(data);
                  
                  _compose_seq_reset(sd);
                  if (config) _font_size_set(data, config->font.size + 1);
                  goto end;
               }
             else if (!strcmp(ev->keyname, "KP_Subtract"))
               {
                  Config *config = termio_config_get(data);
                  
                  _compose_seq_reset(sd);
                  if (config) _font_size_set(data, config->font.size - 1);
                  goto end;
               }
             else if (!strcmp(ev->keyname, "KP_Multiply"))
               {
                  Config *config = termio_config_get(data);
                  
                  _compose_seq_reset(sd);
                  if (config) _font_size_set(data, 10);
                  goto end;
               }
             else if (!strcmp(ev->keyname, "KP_Divide"))
               {
                  _compose_seq_reset(sd);
                  _take_selection(data, ELM_SEL_TYPE_CLIPBOARD);
                  goto end;
               }
          }
     }
   if (sd->jump_on_keypress)
     {
        if (!_is_modifier(ev->key))
          {
             sd->scroll = 0;
             _smart_update_queue(data, sd);
          }
     }
   // if term app asked fro kbd lock - dont handle here
   if (sd->pty->state.kbd_lock) return;
   // if app asked us to not do autorepeat - ignore pree is it is the same
   // timestamp as last one
   if ((sd->pty->state.no_autorepeat) &&
       (ev->timestamp == sd->last_keyup)) return;
   if (!sd->composing)
     {
        _compose_seq_reset(sd);
        sd->seq = eina_list_append(sd->seq, eina_stringshare_add(ev->key));
        state = ecore_compose_get(sd->seq, &compres);
        if (state == ECORE_COMPOSE_MIDDLE) sd->composing = EINA_TRUE;
        else sd->composing = EINA_FALSE;
        if (!sd->composing) _compose_seq_reset(sd);
        else goto end;
     }
   else
     {
        if (_is_modifier(ev->key)) goto end;
        sd->seq = eina_list_append(sd->seq, eina_stringshare_add(ev->key));
        state = ecore_compose_get(sd->seq, &compres);
        if (state == ECORE_COMPOSE_NONE) _compose_seq_reset(sd);
        else if (state == ECORE_COMPOSE_DONE)
          {
             _compose_seq_reset(sd);
             if (compres)
               {
                  termpty_write(sd->pty, compres, strlen(compres));
                  free(compres);
                  compres = NULL;
               }
             goto end;
          }
        else goto end;
     }
   keyin_handle(sd->pty, ev);
end:
   if (sd->config->flicker_on_key)
     edje_object_signal_emit(sd->cur.obj, "key,down", "terminology");
}

static void
_imf_cursor_set(Termio *sd)
{
   /* TODO */
   Evas_Coord cx, cy, cw, ch;
   evas_object_geometry_get(sd->cur.obj, &cx, &cy, &cw, &ch);
   if (sd->imf)
     ecore_imf_context_cursor_location_set(sd->imf, cx, cy, cw, ch);
   /*
    ecore_imf_context_cursor_position_set(sd->imf, 0); // how to get it?
    */
}

void
_smart_cb_focus_in(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Termio *sd;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   if (sd->config->disable_cursor_blink)
     edje_object_signal_emit(sd->cur.obj, "focus,in,noblink", "terminology");
   else
     edje_object_signal_emit(sd->cur.obj, "focus,in", "terminology");
   if (!sd->win) return;
   elm_win_keyboard_mode_set(sd->win, ELM_WIN_KEYBOARD_TERMINAL);
   if (sd->imf)
     {
        ecore_imf_context_input_panel_show(sd->imf);
        ecore_imf_context_reset(sd->imf);
        ecore_imf_context_focus_in(sd->imf);
        _imf_cursor_set(sd);
     }
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
   if (sd->imf)
     {
        ecore_imf_context_reset(sd->imf);
        _imf_cursor_set(sd);
        ecore_imf_context_focus_out(sd->imf);
        ecore_imf_context_input_panel_hide(sd->imf);
     }
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
_sel_line(Evas_Object *obj, int cx __UNUSED__, int cy)
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
_codepoint_is_wordsep(const Config *config, int g)
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

   cells = termpty_cellrow_get(sd->pty, cy, &w);
   if (!cells) return;
   sd->cur.sel = 1;
   sd->cur.makesel = 0;
   sd->cur.sel1.x = cx;
   sd->cur.sel1.y = cy;
   for (x = sd->cur.sel1.x; x >= 0; x--)
     {
        if ((cells[x].codepoint == 0) && (cells[x].att.dblwidth) &&
            (x > 0))
          x--;
        if (x >= w) break;
        if (_codepoint_is_wordsep(sd->config, cells[x].codepoint)) break;
        sd->cur.sel1.x = x;
     }
   sd->cur.sel2.x = cx;
   sd->cur.sel2.y = cy;
   for (x = sd->cur.sel2.x; x < sd->grid.w; x++)
     {
        if ((cells[x].codepoint == 0) && (cells[x].att.dblwidth) &&
            (x < (sd->grid.w - 1)))
          {
             sd->cur.sel2.x = x;
             x++;
          }
        if (x >= w) break;
        if (_codepoint_is_wordsep(sd->config, cells[x].codepoint)) break;
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

   cells = termpty_cellrow_get(sd->pty, cy, &w);
   if (!cells) return;
   if (sd->cur.sel1.x > cx || sd->cur.sel1.y > cy)
     {
        sd->cur.sel1.x = cx;
        sd->cur.sel1.y = cy;
        for (x = sd->cur.sel1.x; x >= 0; x--)
          {
             if ((cells[x].codepoint == 0) && (cells[x].att.dblwidth) &&
                 (x > 0))
               x--;
             if (x >= w) break;
             if (_codepoint_is_wordsep(sd->config, cells[x].codepoint)) break;
             sd->cur.sel1.x = x;
          }
     }
   else if (sd->cur.sel2.x < cx || sd->cur.sel2.y < cy)
     {
        sd->cur.sel2.x = cx;
        sd->cur.sel2.y = cy;
        for (x = sd->cur.sel2.x; x < sd->grid.w; x++)
          {
             if ((cells[x].codepoint == 0) && (cells[x].att.dblwidth) &&
                 (x < (sd->grid.w - 1)))
               {
                  sd->cur.sel2.x = x;
                  x++;
               }
             if (x >= w) break;
             if (_codepoint_is_wordsep(sd->config, cells[x].codepoint)) break;
             sd->cur.sel2.x = x;
          }
     }
}

static void
_rep_mouse_down(Evas_Object *obj, Evas_Event_Mouse_Down *ev, int cx, int cy)
{
   Termio *sd;
   char buf[64];
   
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->pty->mouse_rep == MOUSE_OFF) return;
   switch (sd->pty->mouse_rep)
     {
      case MOUSE_X10:
        if ((cx < (0xff - ' ')) && (cy < (0xff - ' ')))
          {
             int btn = ev->button - 1;
             
             if (btn <= 2)
               {
                  buf[0] = 0x1b;
                  buf[1] = '[';
                  buf[2] = 'M';
                  buf[3] = btn + ' ';
                  buf[4] = cx + 1 + ' ';
                  buf[5] = cy + 1 + ' ';
                  buf[6] = 0;
                  termpty_write(sd->pty, buf, strlen(buf));
               }
          }
        break;
      case MOUSE_UTF8: // ESC.[.M.BTN/FLGS.UTF8.YUTF8
          {
             int btn = ev->button - 1;
             int shift = evas_key_modifier_is_set(ev->modifiers, "Shift") ? 4 : 0;
             int meta = evas_key_modifier_is_set(ev->modifiers, "Alt") ? 8 : 0;
             int ctrl = evas_key_modifier_is_set(ev->modifiers, "Control") ? 16 : 0;
             int dbl = (ev->flags & EVAS_BUTTON_DOUBLE_CLICK) ? 32 : 0;
             int v, i;
             
             if (btn > 2) btn = 0;
             buf[0] = 0x1b;
             buf[1] = '[';
             buf[2] = 'M';
             buf[3] = (btn | shift | meta | ctrl | dbl) + ' ';
             i = 4;
             v = cx + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             v = cy + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             buf[i] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
          }
        break;
      case MOUSE_SGR: // ESC.[.<.NUM.;.NUM.;.NUM.M
          {
             int btn = ev->button - 1;
             int shift = evas_key_modifier_is_set(ev->modifiers, "Shift") ? 4 : 0;
             int meta = evas_key_modifier_is_set(ev->modifiers, "Alt") ? 8 : 0;
             int ctrl = evas_key_modifier_is_set(ev->modifiers, "Control") ? 16 : 0;
             int dbl = (ev->flags & EVAS_BUTTON_DOUBLE_CLICK) ? 32 : 0;
             
             snprintf(buf, sizeof(buf), "%c[<%i;%i;%iM", 0x1b,
                      (btn | shift | meta | ctrl | dbl), cx + 1, cy + 1);
             termpty_write(sd->pty, buf, strlen(buf));
          }
        break;
      case MOUSE_NORMAL:
      case MOUSE_URXVT: // ESC.[.M.BTN/FLGS.X.Y
        if ((cx < (0xff - ' ')) && (cy < (0xff - ' ')))
          {
             int btn = ev->button - 1;
             int shift = evas_key_modifier_is_set(ev->modifiers, "Shift") ? 4 : 0;
             int meta = evas_key_modifier_is_set(ev->modifiers, "Alt") ? 8 : 0;
             int ctrl = evas_key_modifier_is_set(ev->modifiers, "Control") ? 16 : 0;
             int dbl = (ev->flags & EVAS_BUTTON_DOUBLE_CLICK) ? 32 : 0;
             
             if (btn > 2) btn = 0;
             buf[0] = 0x1b;
             buf[1] = '[';
             buf[2] = 'M';
             buf[3] = (btn | shift | meta | ctrl | dbl) + ' ';
             buf[4] = cx + 1 + ' ';
             buf[5] = cy + 1 + ' ';
             buf[6] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
          }
        break;
      default:
        break;
     }
}

static void
_rep_mouse_up(Evas_Object *obj, Evas_Event_Mouse_Up *ev, int cx, int cy)
{
   Termio *sd;
   char buf[64];
   
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->pty->mouse_rep == MOUSE_OFF) return;
   switch (sd->pty->mouse_rep)
     {
      case MOUSE_UTF8: // ESC.[.M.BTN/FLGS.UTF8.YUTF8
          {
             int btn = 3;
             int shift = evas_key_modifier_is_set(ev->modifiers, "Shift") ? 4 : 0;
             int meta = evas_key_modifier_is_set(ev->modifiers, "Alt") ? 8 : 0;
             int ctrl = evas_key_modifier_is_set(ev->modifiers, "Control") ? 16 : 0;
             int v, i;
             
             btn = 3;
             buf[0] = 0x1b;
             buf[1] = '[';
             buf[2] = 'M';
             buf[3] = (btn | shift | meta | ctrl) + ' ';
             i = 4;
             v = cx + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             v = cy + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             buf[i] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
          }
        break;
      case MOUSE_SGR: // ESC.[.<.NUM.;.NUM.;.NUM.M
          {
             int btn = 3;
             int shift = evas_key_modifier_is_set(ev->modifiers, "Shift") ? 4 : 0;
             int meta = evas_key_modifier_is_set(ev->modifiers, "Alt") ? 8 : 0;
             int ctrl = evas_key_modifier_is_set(ev->modifiers, "Control") ? 16 : 0;
             
             snprintf(buf, sizeof(buf), "%c[<%i;%i;%im", 0x1b,
                      (btn | shift | meta | ctrl), cx + 1, cy + 1);
             termpty_write(sd->pty, buf, strlen(buf));
          }
        break;
      case MOUSE_NORMAL:
      case MOUSE_URXVT: // ESC.[.M.BTN/FLGS.X.Y
        if ((cx < (0xff - ' ')) && (cy < (0xff - ' ')))
          {
             int btn = 3;
             int shift = evas_key_modifier_is_set(ev->modifiers, "Shift") ? 4 : 0;
             int meta = evas_key_modifier_is_set(ev->modifiers, "Alt") ? 8 : 0;
             int ctrl = evas_key_modifier_is_set(ev->modifiers, "Control") ? 16 : 0;
             
             buf[0] = 0x1b;
             buf[1] = '[';
             buf[2] = 'M';
             buf[3] = (btn | shift | meta | ctrl) + ' ';
             buf[4] = cx + 1 + ' ';
             buf[5] = cy + 1 + ' ';
             buf[6] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
          }
        break;
      default:
        break;
     }
}

static void
_rep_mouse_move(Evas_Object *obj, Evas_Event_Mouse_Move *ev __UNUSED__, int cx __UNUSED__, int cy __UNUSED__)
{
   Termio *sd;
   
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   if (sd->pty->mouse_rep == MOUSE_OFF) return;
   // not sure what to d here right now so do nothing.
}

static void
_selection_dbl_fix(Evas_Object *obj)
{
   Termio *sd;
   int w = 0;
   Termcell *cells;
   
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   cells = termpty_cellrow_get(sd->pty, sd->cur.sel2.y - sd->scroll, &w);
   if (cells)
     {
        // if sel2 after sel1
        if ((sd->cur.sel2.y > sd->cur.sel1.y) ||
            ((sd->cur.sel2.y == sd->cur.sel1.y) &&
                (sd->cur.sel2.x >= sd->cur.sel1.x)))
          {
             if (sd->cur.sel2.x < (w - 1))
               {
                  if ((cells[sd->cur.sel2.x].codepoint != 0) &&
                      (cells[sd->cur.sel2.x].att.dblwidth))
                    sd->cur.sel2.x++;
               }
          }
        // else sel1 after sel 2
        else
          {
             if (sd->cur.sel2.x > 0)
               {
                  if ((cells[sd->cur.sel2.x].codepoint == 0) &&
                      (cells[sd->cur.sel2.x].att.dblwidth))
                    sd->cur.sel2.x--;
               }
          }
     }
   cells = termpty_cellrow_get(sd->pty, sd->cur.sel1.y - sd->scroll, &w);
   if (cells)
     {
        // if sel2 after sel1
        if ((sd->cur.sel2.y > sd->cur.sel1.y) ||
            ((sd->cur.sel2.y == sd->cur.sel1.y) &&
                (sd->cur.sel2.x >= sd->cur.sel1.x)))
          {
             if (sd->cur.sel1.x > 0)
               {
                  if ((cells[sd->cur.sel1.x].codepoint == 0) &&
                      (cells[sd->cur.sel1.x].att.dblwidth))
                    sd->cur.sel1.x--;
               }
          }
        // else sel1 after sel 2
        else
          {
             if (sd->cur.sel1.x < (w - 1))
               {
                  if ((cells[sd->cur.sel1.x].codepoint != 0) &&
                      (cells[sd->cur.sel1.x].att.dblwidth))
                    sd->cur.sel1.x++;
               }
          }
     }
}

static void
_smart_cb_mouse_move_job(void *data)
{
   Termio *sd;
   
   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   sd->mouse_move_job = NULL;
   _smart_mouseover_apply(data);
}

static void
_edje_cb_bottom_right_in(void *data, Evas_Object *obj __UNUSED__,
                         const char *emission __UNUSED__, const char *source __UNUSED__)
{
   Termio *sd = data;

   sd->bottom_right = EINA_TRUE;
}

static void
_edje_cb_top_left_in(void *data, Evas_Object *obj __UNUSED__,
                     const char *emission __UNUSED__, const char *source __UNUSED__)
{
   Termio *sd = data;

   sd->top_left = EINA_TRUE;
}

static void
_edje_cb_bottom_right_out(void *data, Evas_Object *obj __UNUSED__,
                          const char *emission __UNUSED__, const char *source __UNUSED__)
{
   Termio *sd = data;

   sd->bottom_right = EINA_FALSE;
}

static void
_edje_cb_top_left_out(void *data, Evas_Object *obj __UNUSED__,
                      const char *emission __UNUSED__, const char *source __UNUSED__)
{
   Termio *sd = data;

   sd->top_left = EINA_FALSE;
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
   _rep_mouse_down(data, ev, cx, cy);
   sd->didclick = EINA_FALSE;
   if (ev->button == 1)
     {
        if (ev->flags & EVAS_BUTTON_TRIPLE_CLICK)
          {
             _sel_line(data, cx, cy - sd->scroll);
             if (sd->cur.sel) _take_selection(data, ELM_SEL_TYPE_PRIMARY);
             sd->didclick = EINA_TRUE;
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
                  _selection_dbl_fix(data);
                  _sel_word_to(data, cx, cy - sd->scroll);
               }
             else
               {
                  _sel_word(data, cx, cy - sd->scroll);
               }
             if (sd->cur.sel) _take_selection(data, ELM_SEL_TYPE_PRIMARY);
             sd->didclick = EINA_TRUE;
          }
        else
          {
             if (sd->top_left || sd->bottom_right)
               {
                  sd->cur.makesel = 1;
                  sd->cur.sel = 1;
                  if (sd->top_left)
                    {
                       sd->cur.sel1.x = cx;
                       sd->cur.sel1.y = cy - sd->scroll;
                    }
                  else
                    {
                       sd->cur.sel2.x = cx;
                       sd->cur.sel2.y = cy - sd->scroll;
                    }
                  _selection_dbl_fix(data);
               }
             else
               {
                  sd->backup.sel = sd->cur.sel;
                  sd->backup.sel1.x = sd->cur.sel1.x;
                  sd->backup.sel1.y = sd->cur.sel1.y;
                  sd->backup.sel2.x = sd->cur.sel2.x;
                  sd->backup.sel2.y = sd->cur.sel2.y;
                  if (sd->cur.sel)
                    {
                       sd->cur.sel = 0;
                       sd->didclick = EINA_TRUE;
                    }
                  sd->cur.makesel = 1;
                  sd->cur.sel1.x = cx;
                  sd->cur.sel1.y = cy - sd->scroll;
                  sd->cur.sel2.x = cx;
                  sd->cur.sel2.y = cy - sd->scroll;
                  _selection_dbl_fix(data);
               }
          }
        _smart_update_queue(data, sd);
     }
   else if (ev->button == 2)
     _paste_selection(data, ELM_SEL_TYPE_PRIMARY);
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
   _rep_mouse_up(data, ev, cx, cy);
   if (sd->cur.makesel)
     {
        sd->cur.makesel = 0;
        if (sd->cur.sel)
          {
             sd->didclick = EINA_TRUE;
             if (sd->top_left)
               {
                  sd->cur.sel1.x = cx;
                  sd->cur.sel1.y = cy - sd->scroll;
               }
             else
               {
                  sd->cur.sel2.x = cx;
                  sd->cur.sel2.y = cy - sd->scroll;
               }
             _selection_dbl_fix(data);
             _smart_update_queue(data, sd);
             _take_selection(data, ELM_SEL_TYPE_PRIMARY);
          }
     }
}

static void
_smart_cb_mouse_move(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
{
   Evas_Event_Mouse_Move *ev = event;
   Termio *sd;
   int cx, cy;
   Eina_Bool mc_change = EINA_FALSE;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   _smart_xy_to_cursor(data, ev->cur.canvas.x, ev->cur.canvas.y, &cx, &cy);
   if ((sd->mouse.cx != cx) || (sd->mouse.cy != cy)) mc_change = EINA_TRUE;
   sd->mouse.cx = cx;
   sd->mouse.cy = cy;
   _rep_mouse_move(data, ev, cx, cy);
   if (sd->cur.makesel)
     {
        if (!sd->cur.sel)
          {
             if ((cx != sd->cur.sel1.x) ||
                 ((cy - sd->scroll) != sd->cur.sel1.y))
               sd->cur.sel = 1;
          }
        if (sd->top_left)
          {
             sd->cur.sel1.x = cx;
             sd->cur.sel1.y = cy - sd->scroll;
          }
        else
          {
             sd->cur.sel2.x = cx;
             sd->cur.sel2.y = cy - sd->scroll;
          }
        _selection_dbl_fix(data);
       _smart_update_queue(data, sd);
     }
   if (mc_change)
     {
        if (sd->mouse_move_job) ecore_job_del(sd->mouse_move_job);
        sd->mouse_move_job = ecore_job_add(_smart_cb_mouse_move_job, data);
     }
}

static void
_smart_cb_mouse_in(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   termio_mouseover_suspend_pushpop(data, -1);
}

static void
_smart_cb_mouse_out(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   termio_mouseover_suspend_pushpop(data, 1);
}

static void
_smart_cb_mouse_wheel(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
{
   Evas_Event_Mouse_Wheel *ev = event;
   Termio *sd;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   if (sd->pty->altbuf) return;
   if (evas_key_modifier_is_set(ev->modifiers, "Control")) return;
   if (evas_key_modifier_is_set(ev->modifiers, "Alt")) return;
   if (evas_key_modifier_is_set(ev->modifiers, "Shift")) return;
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
   sd->jump_on_keypress = config->jump_on_keypress;

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

   theme_apply(sd->cur.obj, config, "terminology/cursor");
   theme_auto_reload_enable(sd->cur.obj);
   evas_object_resize(sd->cur.obj, sd->font.chw, sd->font.chh);
   evas_object_show(sd->cur.obj);

   theme_apply(sd->cur.selo_theme, config, "terminology/selection");
   theme_auto_reload_enable(sd->cur.selo_theme);
   edje_object_part_swallow(sd->cur.selo_theme, "terminology.top_left", sd->cur.selo_top);
   edje_object_part_swallow(sd->cur.selo_theme, "terminology.bottom_right", sd->cur.selo_bottom);
}

static void
_cursor_cb_move(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   Termio *sd;

   sd = evas_object_smart_data_get(data);
   if (!sd) return;
   _imf_cursor_set(sd);
}

static Evas_Event_Flags
_smart_cb_gest_long_move(void *data, void *event __UNUSED__)
{
//   Elm_Gesture_Taps_Info *p = event;
   Termio *sd = evas_object_smart_data_get(data);
   
   if (!sd) return EVAS_EVENT_FLAG_ON_HOLD;
   evas_object_smart_callback_call(data, "options", NULL);
   sd->didclick = EINA_TRUE;
   return EVAS_EVENT_FLAG_ON_HOLD;
}

static Evas_Event_Flags
_smart_cb_gest_zoom_start(void *data, void *event)
{
   Elm_Gesture_Zoom_Info *p = event;
   Termio *sd = evas_object_smart_data_get(data);
   Config *config = termio_config_get(data);
   
   if (!sd) return EVAS_EVENT_FLAG_ON_HOLD;
   if (config)
     {
        sd->zoom_fontsize_start = config->font.size;
        _font_size_set(data, (double)sd->zoom_fontsize_start * p->zoom);
     }
   sd->didclick = EINA_TRUE;
   return EVAS_EVENT_FLAG_ON_HOLD;
}

static Evas_Event_Flags
_smart_cb_gest_zoom_move(void *data, void *event)
{
   Elm_Gesture_Zoom_Info *p = event;
   Termio *sd = evas_object_smart_data_get(data);
   Config *config = termio_config_get(data);
   
   if (!sd) return EVAS_EVENT_FLAG_ON_HOLD;
   if (config)
     {
        sd->zoom_fontsize_start = config->font.size;
        _font_size_set(data, (double)sd->zoom_fontsize_start * p->zoom);
     }
   sd->didclick = EINA_TRUE;
   return EVAS_EVENT_FLAG_ON_HOLD;
}

static Evas_Event_Flags
_smart_cb_gest_zoom_end(void *data, void *event)
{
   Elm_Gesture_Zoom_Info *p = event;
   Termio *sd = evas_object_smart_data_get(data);
   Config *config = termio_config_get(data);
   
   if (!sd) return EVAS_EVENT_FLAG_ON_HOLD;
   if (config)
     {
        sd->zoom_fontsize_start = config->font.size;
        _font_size_set(data, (double)sd->zoom_fontsize_start * p->zoom);
     }
   sd->didclick = EINA_TRUE;
   return EVAS_EVENT_FLAG_ON_HOLD;
}

static Evas_Event_Flags
_smart_cb_gest_zoom_abort(void *data, void *event __UNUSED__)
{
//   Elm_Gesture_Zoom_Info *p = event;
   Termio *sd = evas_object_smart_data_get(data);
   Config *config = termio_config_get(data);
   
   if (!sd) return EVAS_EVENT_FLAG_ON_HOLD;
   if (config)
     {
        sd->zoom_fontsize_start = config->font.size;
        _font_size_set(data, sd->zoom_fontsize_start);
     }
   sd->didclick = EINA_TRUE;
   return EVAS_EVENT_FLAG_ON_HOLD;
}

static void
_imf_event_commit_cb(void *data, Ecore_IMF_Context *ctx __UNUSED__, void *event)
{
   Termio *sd = data;
   char *str = event;
   DBG("IMF committed '%s'", str);
   if (!str) return;
   termpty_write(sd->pty, str, strlen(str));
}

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

   /* Terminal output widget */
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

   /* Setup cursor */
   o = edje_object_add(evas_object_evas_get(obj));
   evas_object_pass_events_set(o, EINA_TRUE);
   evas_object_propagate_events_set(o, EINA_FALSE);
   evas_object_smart_member_add(o, obj);
   sd->cur.obj = o;

   evas_object_event_callback_add(o, EVAS_CALLBACK_MOVE, _cursor_cb_move, obj);

   /* Setup the selection widget */
   o = evas_object_rectangle_add(evas_object_evas_get(obj));
   evas_object_pass_events_set(o, EINA_TRUE);
   evas_object_propagate_events_set(o, EINA_FALSE);
   sd->cur.selo_top = o;
   o = evas_object_rectangle_add(evas_object_evas_get(obj));
   evas_object_pass_events_set(o, EINA_TRUE);
   evas_object_propagate_events_set(o, EINA_FALSE);
   sd->cur.selo_bottom = o;
   o = edje_object_add(evas_object_evas_get(obj));
   evas_object_smart_member_add(o, obj);
   sd->cur.selo_theme = o;
   edje_object_signal_callback_add(o, "mouse,in", "zone.bottom_right", _edje_cb_bottom_right_in, sd);
   edje_object_signal_callback_add(o, "mouse,in", "zone.top_left", _edje_cb_top_left_in, sd);
   edje_object_signal_callback_add(o, "mouse,out", "zone.bottom_right", _edje_cb_bottom_right_out, sd);
   edje_object_signal_callback_add(o, "mouse,out", "zone.top_left", _edje_cb_top_left_out, sd);

   /* Setup event catcher */
   o = evas_object_rectangle_add(evas_object_evas_get(obj));
   evas_object_repeat_events_set(o, EINA_TRUE);
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
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_IN,
                                  _smart_cb_mouse_in, obj);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_OUT,
                                  _smart_cb_mouse_out, obj);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_WHEEL,
                                  _smart_cb_mouse_wheel, obj);

   evas_object_event_callback_add(obj, EVAS_CALLBACK_KEY_DOWN,
                                  _smart_cb_key_down, obj);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_KEY_UP,
                                  _smart_cb_key_up, obj);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_FOCUS_IN,
                                  _smart_cb_focus_in, obj);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_FOCUS_OUT,
                                  _smart_cb_focus_out, obj);

   sd->link.suspend = 1;
   
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
                  if (imf_id) sd->imf = ecore_imf_context_add(imf_id);
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
        ecore_imf_context_use_preedit_set
          (sd->imf, EINA_FALSE);
        ecore_imf_context_prediction_allow_set
          (sd->imf, EINA_FALSE);
        ecore_imf_context_autocapital_type_set
          (sd->imf, ECORE_IMF_AUTOCAPITAL_TYPE_NONE);
        ecore_imf_context_input_panel_layout_set
          (sd->imf, ECORE_IMF_INPUT_PANEL_LAYOUT_TERMINAL);
        ecore_imf_context_input_mode_set
          (sd->imf, ECORE_IMF_INPUT_MODE_FULL);
        ecore_imf_context_input_panel_language_set
          (sd->imf, ECORE_IMF_INPUT_PANEL_LANG_ALPHABET);
        ecore_imf_context_input_panel_return_key_type_set
          (sd->imf, ECORE_IMF_INPUT_PANEL_RETURN_KEY_TYPE_DEFAULT);
imf_done:
        if (sd->imf) DBG("Ecore IMF Setup");
        else WRN("Ecore IMF failed");
     }
}

static void
_smart_del(Evas_Object *obj)
{
   Evas_Object *o;
   Termio *sd = evas_object_smart_data_get(obj);
   
   if (!sd) return;
   if (sd->imf)
     {
        ecore_imf_context_event_callback_del
          (sd->imf, ECORE_IMF_CALLBACK_COMMIT, _imf_event_commit_cb);
        ecore_imf_context_del(sd->imf);
     }
   if (sd->cur.obj) evas_object_del(sd->cur.obj);
   if (sd->event) evas_object_del(sd->event);
   if (sd->cur.selo_top) evas_object_del(sd->cur.selo_top);
   if (sd->cur.selo_bottom) evas_object_del(sd->cur.selo_bottom);
   if (sd->cur.selo_theme) evas_object_del(sd->cur.selo_theme);
   if (sd->anim) ecore_animator_del(sd->anim);
   if (sd->delayed_size_timer) ecore_timer_del(sd->delayed_size_timer);
   if (sd->link_do_timer) ecore_timer_del(sd->link_do_timer);
   if (sd->mouse_move_job) ecore_job_del(sd->mouse_move_job);
   if (sd->font.name) eina_stringshare_del(sd->font.name);
   if (sd->pty) termpty_free(sd->pty);
   if (sd->link.string) free(sd->link.string);
   EINA_LIST_FREE(sd->link.objs, o) evas_object_del(o);
   _compose_seq_reset(sd);
   sd->cur.obj = NULL;
   sd->event = NULL;
   sd->cur.selo_top = NULL;
   sd->cur.selo_bottom = NULL;
   sd->cur.selo_theme = NULL;
   sd->anim = NULL;
   sd->delayed_size_timer = NULL;
   sd->font.name = NULL;
   sd->pty = NULL;
   sd->imf = NULL;
   ecore_imf_shutdown();

   termpty_shutdown();

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

static void
_smart_pty_exited(void *data)
{
   evas_object_smart_callback_call(data, "exited", NULL);
}

static void
_smart_pty_bell(void *data)
{
   Termio *sd = evas_object_smart_data_get(data);
   if (!sd) return;
   evas_object_smart_callback_call(data, "bell", NULL);
   edje_object_signal_emit(sd->cur.obj, "bell", "terminology");
}

static void
_smart_pty_command(void *data)
{
   Evas_Object *obj = data;
   Termio *sd;
   sd = evas_object_smart_data_get(obj);
   if (!sd) return;
}

Evas_Object *
termio_add(Evas_Object *parent, Config *config, const char *cmd, const char *cd, int w, int h)
{
   Evas *e;
   Evas_Object *obj, *g;
   Termio *sd;

   EINA_SAFETY_ON_NULL_RETURN_VAL(parent, NULL);
   e = evas_object_evas_get(parent);
   if (!e) return NULL;

   if (!_smart) _smart_init();
   obj = evas_object_smart_add(e, _smart);
   sd = evas_object_smart_data_get(obj);
   if (!sd) return obj;

   _termio_config_set(obj, config);

   g = elm_gesture_layer_add(parent);
   elm_gesture_layer_attach(g, sd->event);

   elm_gesture_layer_cb_set(g, ELM_GESTURE_N_LONG_TAPS,
                            ELM_GESTURE_STATE_MOVE, _smart_cb_gest_long_move,
                            obj);
   
   elm_gesture_layer_cb_set(g, ELM_GESTURE_ZOOM,
                            ELM_GESTURE_STATE_START, _smart_cb_gest_zoom_start,
                            obj);
   elm_gesture_layer_cb_set(g, ELM_GESTURE_ZOOM,
                            ELM_GESTURE_STATE_MOVE, _smart_cb_gest_zoom_move,
                            obj);
   elm_gesture_layer_cb_set(g, ELM_GESTURE_ZOOM,
                            ELM_GESTURE_STATE_END, _smart_cb_gest_zoom_end,
                            obj);
   elm_gesture_layer_cb_set(g, ELM_GESTURE_ZOOM,
                            ELM_GESTURE_STATE_ABORT, _smart_cb_gest_zoom_abort,
                            obj);
   
   termpty_init();

   sd->pty = termpty_new(cmd, cd, w, h, config->scrollback);
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
   sd->pty->cb.exited.func = _smart_pty_exited;
   sd->pty->cb.exited.data = obj;
   sd->pty->cb.bell.func = _smart_pty_bell;
   sd->pty->cb.bell.data = obj;
   sd->pty->cb.command.func = _smart_pty_command;
   sd->pty->cb.command.data = obj;
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
   char *s;
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
        if (!cells) continue;
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
             if ((cells[x].codepoint == 0) && (cells[x].att.dblwidth))
               {
                  if (x < end_x) x++;
                  else break;
               }
             if (x >= w) break;
             if ((cells[x].codepoint == 0) || (cells[x].codepoint == ' '))
               {
                  if (last0 < 0) last0 = x;
               }
             else if (cells[x].att.newline)
               {
                  last0 = -1;
                  eina_strbuf_append_char(sb, '\n');
                  break;
               }
             else if (cells[x].att.tab)
               {
                  eina_strbuf_append_char(sb, '\t');
                  x = ((x + 8) / 8) * 8;
                  x--;
               }
             else
               {
                  char txt[8];
                  int txtlen;

                  if (last0 >= 0)
                    {
                       v = x - last0 - 1;
                       last0 = -1;
                       while (v >= 0)
                         {
                            eina_strbuf_append_char(sb, ' ');
                            v--;
                         }
                    }
                  txtlen = codepoint_to_utf8(cells[x].codepoint, txt);
                  if (txtlen > 0)
                    eina_strbuf_append_length(sb, txt, txtlen);
                  if (x == (w - 1))
                    {
                       if (!cells[x].att.autowrapped)
                         eina_strbuf_append_char(sb, '\n');
                    }
               }
          }
        if (last0 >= 0)
          {
             if (y == c2y)
               {
                  Eina_Bool have_more = EINA_FALSE;
                  
                  for (x = end_x + 1; x < w; x++)
                    {
                       if ((cells[x].codepoint == 0) &&
                           (cells[x].att.dblwidth))
                         {
                            if (x < (w - 1)) x++;
                            else break;
                         }
                       if (((cells[x].codepoint != 0) &&
                            (cells[x].codepoint != ' ')) ||
                           (cells[x].att.newline) ||
                           (cells[x].att.tab))
                         {
                            have_more = EINA_TRUE;
                            break;
                         }
                    }
                  if (!have_more) eina_strbuf_append_char(sb, '\n');
                  else
                    {
                       for (x = last0; x <= end_x; x++)
                         {
                            if ((cells[x].codepoint == 0) &&
                                (cells[x].att.dblwidth))
                              {
                                 if (x < (w - 1)) x++;
                                 else break;
                              }
                            if (x >= w) break;
                            eina_strbuf_append_char(sb, ' ');
                         }
                    }
               }
             else eina_strbuf_append_char(sb, '\n');
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
   sd->jump_on_keypress = sd->config->jump_on_keypress;

   termpty_backscroll_set(sd->pty, sd->config->scrollback);
   sd->scroll = 0;

   edje_object_signal_emit(sd->cur.obj, "focus,out", "terminology");
   if (sd->config->disable_cursor_blink)
     edje_object_signal_emit(sd->cur.obj, "focus,in,noblink", "terminology");
   else
     edje_object_signal_emit(sd->cur.obj, "focus,in", "terminology");
   
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

void
termio_copy_clipboard(Evas_Object *obj)
{
   _take_selection(obj, ELM_SEL_TYPE_CLIPBOARD);
}

void
termio_paste_clipboard(Evas_Object *obj)
{
   _paste_selection(obj, ELM_SEL_TYPE_CLIPBOARD);
}

const char  *
termio_link_get(const Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);
   return sd->link.string;
}

void
termio_mouseover_suspend_pushpop(Evas_Object *obj, int dir)
{
   Termio *sd = evas_object_smart_data_get(obj);
   if (!sd) return;
   sd->link.suspend += dir;
   if (sd->link.suspend < 0) sd->link.suspend = 0;
   _smart_update_queue(obj, sd);
}

void
termio_size_get(Evas_Object *obj, int *w, int *h)
{
   Termio *sd = evas_object_smart_data_get(obj);
   if (!sd)
     {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
     }
   if (w) *w = sd->grid.w;
   if (h) *h = sd->grid.h;
}

int
termio_scroll_get(Evas_Object *obj)
{
   Termio *sd = evas_object_smart_data_get(obj);
   if (!sd) return 0;
   return sd->scroll;
}
