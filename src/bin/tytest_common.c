#include "private.h"
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "private.h"
#include <Elementary.h>
#include "termpty.h"
#include "termptyops.h"
#include "termiointernals.h"
#include "tytest_common.h"
#if defined(BINARY_TYTEST)
#include "colors.h"
#include "tytest.h"
#endif
#include <assert.h>

static Config *_config = NULL;
static Termpty _ty;
static Termio _sd = {
     .font = {
          .name = "",
          .size = 10,
          .chw = TY_CH_W,
          .chh = TY_CH_H,
     },
     .grid = {
          .w = TY_W,
          .h = TY_H,
     },
     .sel = {},
     .cursor = {
          .obj = NULL,
          .x = 0,
          .y = 0,
          .shape = CURSOR_SHAPE_BLOCK,
     },
     .mouse = {},
     .link = {},
     .pty = &_ty,
     .config = NULL,
};
static Color _bg = { .r = 131, .g = 132, .b = 133, .a = 134 };
static Color _cursor = { .r = 135, .g = 136, .b = 137, .a = 138 };
static const char *_cursor_shape = "undefined";
#if defined(BINARY_TYTEST)
static Evas_Textgrid_Cell *_cells;

const char *
tytest_cursor_shape_get(void)
{
   return _cursor_shape;
}

Evas_Textgrid_Cell *
tytest_textgrid_cellrow_get(Evas_Object *obj EINA_UNUSED, int y)
{
   assert (y >= 0 && y < _sd.pty->h);
   return &_cells[y * _sd.pty->w];
}

void
tytest_termio_resize(int w, int h)
{
   Evas_Textgrid_Cell *cells = realloc(_cells,
                                       w * h * sizeof(Evas_Textgrid_Cell));
   assert (cells);
   _sd.grid.w = w;
   _sd.grid.h = h;
   _cells = cells;
}

Termpty *
tytest_termpty_get(void)
{
   return &_ty;
}
#endif


void
main_config_sync(const Config *config EINA_UNUSED)
{
}

Evas_Object *
termio_get_cursor(const Evas_Object *obj EINA_UNUSED)
{
   return NULL;
}

Termio *
termio_get_from_obj(Evas_Object *obj EINA_UNUSED)
{
   return &_sd;
}

Config *
termio_config_get(const Evas_Object *obj EINA_UNUSED)
{
   return _config;
}

void
termio_take_selection_text(Termio *sd EINA_UNUSED,
                           Elm_Sel_Type type EINA_UNUSED,
                           const char *text EINA_UNUSED)
{
}

void
termio_paste_selection(Evas_Object *obj EINA_UNUSED,
                       Elm_Sel_Type type EINA_UNUSED)
{
}

void
termio_smart_update_queue(Termio *sd EINA_UNUSED)
{
}

void
termio_handle_right_click(Evas_Event_Mouse_Down *ev EINA_UNUSED,
                          Termio *sd EINA_UNUSED,
                          int cx EINA_UNUSED, int cy EINA_UNUSED)
{
}

void
termio_smart_cb_mouse_move_job(void *data EINA_UNUSED)
{
}

void
miniview_position_offset(const Evas_Object *obj EINA_UNUSED,
                         int by EINA_UNUSED,
                         Eina_Bool sanitize EINA_UNUSED)
{
}

Evas_Object *
term_miniview_get(const Term *term EINA_UNUSED)
{
   return NULL;
}

int
termio_scroll_get(const Evas_Object *obj EINA_UNUSED)
{
   return _sd.scroll;
}

void
termio_size_get(const Evas_Object *obj EINA_UNUSED,
                int *w, int *h)
{
   if (w) *w = _sd.grid.w;
   if (h) *h = _sd.grid.h;
}

Termpty *
termio_pty_get(const Evas_Object *obj EINA_UNUSED)
{
   return &_ty;
}

Eina_Bool
termio_cwd_get(const Evas_Object *obj EINA_UNUSED,
               char *buf,
               size_t size)
{
   assert (size >= 2);

   buf[0] = '\0';
   buf[1] = '\0';

   return EINA_TRUE;
}

void
termio_sel_set(Termio *sd, Eina_Bool enable)
{
   sd->pty->selection.is_active = enable;
   if (!enable)
     {
        sd->pty->selection.by_word = EINA_FALSE;
        sd->pty->selection.by_line = EINA_FALSE;
        free(sd->pty->selection.codepoints);
        sd->pty->selection.codepoints = NULL;
     }
}
void
termio_object_geometry_get(Termio *sd,
                           Evas_Coord *x, Evas_Coord *y,
                           Evas_Coord *w, Evas_Coord *h)
{
     if (x)
       *x = TY_OFFSET_X;
     if (y)
       *y = TY_OFFSET_Y;
     if (w)
       *w = TY_OFFSET_X + sd->font.chw * sd->grid.w;
     if (h)
       *h = TY_OFFSET_Y + sd->font.chh * sd->grid.h;
}

void
termio_font_size_set(Evas_Object *obj EINA_UNUSED,
                     int size)
{
   _sd.font.size = size;
}

const char *
term_preedit_str_get(Term *term EINA_UNUSED)
{
   return NULL;
}

void
termio_block_activate(Evas_Object *obj EINA_UNUSED,
                      Termblock *blk EINA_UNUSED)
{
}

Eina_Bool
termio_take_selection(Evas_Object *obj,
                      Elm_Sel_Type type EINA_UNUSED)
{
   Termio *sd;
   const char *s;
   size_t len;

   sd = termio_get_from_obj(obj);
   s = termio_internal_get_selection(sd, &len);
   if (s)
     {
        eina_stringshare_del(s);
        return EINA_TRUE;
     }
   return EINA_FALSE;
}

void
termio_remove_links(Termio *sd)
{
   sd->link.string = NULL;
   sd->link.x1 = -1;
   sd->link.y1 = -1;
   sd->link.x2 = -1;
   sd->link.y2 = -1;
   sd->link.suspend = EINA_FALSE;
   sd->link.id = 0;
   sd->link.objs = 0;
}

void
termio_set_cursor_shape(Evas_Object *obj EINA_UNUSED,
                        Cursor_Shape shape EINA_UNUSED)
{
   switch (shape)
     {
      case CURSOR_SHAPE_UNDERLINE:
         _cursor_shape = "underline";
         break;
      case CURSOR_SHAPE_BAR:
         _cursor_shape = "bar";
         break;
      default:
      case CURSOR_SHAPE_BLOCK:
         _cursor_shape = "block";
     }
}


Eina_Bool
tytest_edje_object_color_class_get(Evas_Object *termio EINA_UNUSED, const char *key,
                        int *r, int *g, int *b, int *a,
                        int *r1 EINA_UNUSED, int *g1 EINA_UNUSED, int *b1 EINA_UNUSED, int *a1 EINA_UNUSED,
                        int *r2 EINA_UNUSED, int *g2 EINA_UNUSED, int *b2 EINA_UNUSED, int *a2 EINA_UNUSED)
{
   if (strncmp(key, "BG", strlen("BG")) == 0)
     {
        if (r)
          *r = _bg.r;
        if (g)
          *g = _bg.g;
        if (b)
          *b = _bg.b;
        if (a)
          *a = _bg.a;
        return EINA_TRUE;
     }
   if (strncmp(key, "CURSOR", strlen("CURSOR")) == 0)
     {
        if (r)
          *r = _cursor.r;
        if (g)
          *g = _cursor.g;
        if (b)
          *b = _cursor.b;
        if (a)
          *a = _cursor.a;
        return EINA_TRUE;
     }
   return EINA_FALSE;
}

Eina_Bool
tytest_edje_object_color_class_set(Evas_Object *termio EINA_UNUSED, const char *key,
                                 int r, int g, int b, int a,
                                 int r1 EINA_UNUSED, int g1 EINA_UNUSED, int b1 EINA_UNUSED, int a1 EINA_UNUSED,
                                 int r2 EINA_UNUSED, int g2 EINA_UNUSED, int b2 EINA_UNUSED, int a2 EINA_UNUSED)
{
   if (strncmp(key, "BG", strlen("BG")) == 0)
     {
        _bg.r = r;
        _bg.g = g;
        _bg.b = b;
        _bg.a = a;
        return EINA_TRUE;
     }
   if (strncmp(key, "CURSOR", strlen("CURSOR")) == 0)
     {
        _cursor.r = r;
        _cursor.g = g;
        _cursor.b = b;
        _cursor.a = a;
        return EINA_TRUE;
     }
   return EINA_FALSE;
}

void
termio_reset_main_colors(Evas_Object *termio EINA_UNUSED)
{
   _bg.r = 131;
   _bg.g = 132;
   _bg.b = 133;
   _bg.a = 134;
   _cursor.r = 135;
   _cursor.g = 136;
   _cursor.b = 137;
   _cursor.a = 138;
}

Evas_Object *
termio_textgrid_get(const Evas_Object *obj EINA_UNUSED)
{
   return NULL;
}

Evas_Object *
termio_bg_get(const Evas_Object *obj EINA_UNUSED)
{
   return NULL;
}

static char *_sel_primary = NULL;
static char *_sel_clipboard = NULL;

void termio_selection_buffer_get_cb(Evas_Object *obj,
                                    Elm_Sel_Type type,
                                    Elm_Sel_Format format EINA_UNUSED,
                                    Elm_Drop_Cb cb,
                                    void *data)
{
   Elm_Selection_Data ev = {};
   switch (type)
     {
      case ELM_SEL_TYPE_PRIMARY:
         ev.data = _sel_primary;
         if (_sel_primary)
              ev.len = strlen(_sel_primary) + 1;
         else
              ev.len = 0;
         cb(data, obj, &ev);
         break;
      case ELM_SEL_TYPE_CLIPBOARD:
         ev.data = _sel_clipboard;
         if (_sel_clipboard)
              ev.len = strlen(_sel_clipboard) + 1;
         else
              ev.len = 0;
         cb(data, obj, &ev);
         break;
      default:
         break;
     }

}
void
termio_set_selection_text(Evas_Object *obj EINA_UNUSED,
                          Elm_Sel_Type type, const char *text)
{
   switch (type)
     {
      case ELM_SEL_TYPE_PRIMARY:
         free(_sel_primary);
         if (text[0])
           _sel_primary = strdup(text);
         else
           _sel_primary = NULL;
         break;
      case ELM_SEL_TYPE_CLIPBOARD:
         free(_sel_clipboard);
         if (text[0])
           _sel_clipboard = strdup(text);
         else
           _sel_clipboard = NULL;
         break;
      default:
         break;
     }
}


#if defined(BINARY_TYTEST)
void
test_textgrid_palette_get(const Evas_Object *obj EINA_UNUSED,
                          Evas_Textgrid_Palette pal,
                          int idx,
                          int *r,
                          int *g,
                          int *b,
                          int *a)
{
   if (pal == EVAS_TEXTGRID_PALETTE_EXTENDED)
     {
        colors_256_get(idx,
                       (unsigned char *)r,
                       (unsigned char *)g,
                       (unsigned char *)b,
                       (unsigned char *)a);
     }
   else
     {
        int set = idx / 12;
        int col = idx % 12;
        colors_standard_get(set, col,
                            (unsigned char*)r,
                            (unsigned char*)g,
                            (unsigned char*)b,
                            (unsigned char*)a);
     }
}
#endif

Eina_Bool
termio_is_focused(const Evas_Object *obj EINA_UNUSED)
{
   return EINA_FALSE;
}

static void
_termpty_init(Termpty *ty, Config *config)
{
   memset(ty, '\0', sizeof(*ty));
   ty->config = config;
   ty->w = TY_W;
   ty->h = TY_H;
   ty->backsize = TY_BACKSIZE;
   termpty_resize_tabs(ty, 0, ty->w);
   termpty_reset_state(ty);
   ty->screen = calloc(1, sizeof(Termcell) * ty->w * ty->h);
   ty->screen2 = calloc(1, sizeof(Termcell) * ty->w * ty->h);
   assert(ty->screen);
   assert(ty->screen2);
   ty->circular_offset = 0;
   ty->fd = STDIN_FILENO;
   ty->hl.bitmap = calloc(1, HL_LINKS_MAX / 8); /* bit map for 1 << 16 elements */
   assert(ty->hl.bitmap);
   /* Mark id 0 as set */
   ty->hl.bitmap[0] = 1;
   ty->backlog_beacon.backlog_y = 0;
   ty->backlog_beacon.screen_y = 0;
}

void
tytest_common_main_loop(void)
{
   do
     {
        char buf[4097];
        Eina_Unicode codepoint[4097];
        int i, j;
        char *rbuf = buf;
        int len = sizeof(buf) - 1;

        for (i = 0; i < (int)sizeof(_ty.oldbuf) && _ty.oldbuf[i] & 0x80; i++)
          {
             *rbuf = _ty.oldbuf[i];
             rbuf++;
             len--;
          }
        len = read(_ty.fd, rbuf, len);
        if (len < 0 && errno != EAGAIN)
          {
             ERR("error while reading from tty slave fd");
             break;
          }
        if (len <= 0) break;

        for (i = 0; i < (int)sizeof(_ty.oldbuf); i++)
          _ty.oldbuf[i] = 0;

        len += rbuf - buf;

        buf[len] = 0;
        // convert UTF8 to codepoint integers
        j = 0;
        for (i = 0; i < len;)
          {
             int g = 0, prev_i = i;

             if (buf[i])
               {
                  g = eina_unicode_utf8_next_get(buf, &i);
                  if ((0xdc80 <= g) && (g <= 0xdcff) &&
                      (len - prev_i) <= (int)sizeof(_ty.oldbuf))
                    {
                       int k;
                       for (k = 0;
                            (k < (int)sizeof(_ty.oldbuf)) &&
                            (k < (len - prev_i));
                            k++)
                         {
                            _ty.oldbuf[k] = buf[prev_i+k];
                         }
                       DBG("failure at %d/%d/%d", prev_i, i, len);
                       break;
                    }
               }
             else
               {
                  g = 0;
                  i++;
               }
             codepoint[j] = g;
             j++;
          }
        codepoint[j] = 0;
        termpty_handle_buf(&_ty, codepoint, j);
     }
   while (1);
}

void
tytest_common_init(void)
{
   _config = config_new();
   _sd.config = _config;
   _termpty_init(&_ty, _config);
#if defined(BINARY_TYTEST)
   _cells = calloc(TY_H * TY_W, sizeof(Evas_Textgrid_Cell));
   assert(_cells != NULL);
#endif
}

void
tytest_common_shutdown(void)
{
   config_del(_config);
#if defined(BINARY_TYTEST)
   free(_cells);
#endif
}

void
tytest_common_set_fd(int fd)
{
   _ty.fd = fd;
   assert(_ty.fd >= 0);
}

/* dummy unit test */
int
tytest_dummy(void)
{
   return 0;
}
