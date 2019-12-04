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
#include <assert.h>

#ifdef TYTEST
#include "col.h"
#include "tytest.h"
#include "md5/md5.h"

static void
_tytest_checksum(Termpty *ty);
#endif

#define TY_H 24
#define TY_W 80
#define TY_CH_H 15
#define TY_CH_W  7
#define TY_OFFSET_X 1
#define TY_OFFSET_Y 1
#define TY_BACKSIZE 50


/* {{{ stub */
int _log_domain = -1;

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

void
main_config_sync(const Config *config EINA_UNUSED)
{
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

#ifndef TYTEST
void
termio_set_cursor_shape(Evas_Object *obj EINA_UNUSED,
                        Cursor_Shape shape EINA_UNUSED)
{
}
#endif

int
termpty_color_class_get(Termpty *ty EINA_UNUSED, const char *key,
                        int *r, int *g, int *b, int *a)
{
   if (strncmp(key, "BG", strlen("BG")) == 0)
     {
        if (r)
          *r = 131;
        if (g)
          *g = 132;
        if (b)
          *b = 133;
        if (a)
          *a = 134;
        return 0;
     }
   return -1;
}
/* }}} */



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

int
main(int argc EINA_UNUSED, char **argv EINA_UNUSED)
{
   char buf[4097];
   Eina_Unicode codepoint[4097];
   int len, i, j, k;

   eina_init();

#ifdef TYTEST
   tytest_init();
#endif


   _log_domain = eina_log_domain_register(
#ifdef TYTEST
       "tytest",
#else
       "tyfuzz",
#endif
       NULL);

   _config = config_new();
   _sd.config = _config;

   _termpty_init(&_ty, _config);

   if (argc > 1)
     {
       _ty.fd = open(argv[1], O_RDONLY);
       assert(_ty.fd >= 0);
     }

   do
     {
        char *rbuf = buf;
        len = sizeof(buf) - 1;

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

#ifdef TYTEST
   _tytest_checksum(&_ty);
#endif

#ifdef TYTEST
   tytest_shutdown();
#endif

   config_del(_config);
   eina_shutdown();

   return 0;
}
