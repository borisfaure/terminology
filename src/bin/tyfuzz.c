#include "coverity.h"

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
#endif

#define TY_H 25
#define TY_W 80
#define TY_CH_H 15
#define TY_CH_W  7
#define TY_BACKSIZE 50


/* {{{ stub */
int _log_domain = -1;

static Config *_config = NULL;
static Termpty _ty;
static Termio _sd = {
     .font = {
          .name = "",
          .size = 12,
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
       *x = 0;
     if (y)
       *y = 0;
     if (w)
       *w = sd->font.chw * sd->grid.w;
     if (h)
       *h = sd->font.chh * sd->grid.h;
}

void
termio_font_size_set(Evas_Object *obj EINA_UNUSED,
                     int size)
{
   _sd.font.size = size;
}

#ifndef TYTEST
void
termio_set_cursor_shape(Evas_Object *obj EINA_UNUSED,
                        Cursor_Shape shape EINA_UNUSED)
{
}
#endif
/* }}} */
/* {{{ TYTEST */
#ifdef TYTEST
const char *_cursor_shape = "undefined";
typedef struct _Termpty_Tests
{
   size_t backsize, backpos;
   Backlog_Beacon backlog_beacon;
   Term_State termstate;
   Term_Cursor cursor_state;
   Term_Cursor cursor_save[2];
   int w, h;
   unsigned int altbuf     : 1;
   unsigned int mouse_mode : 3;
   unsigned int mouse_ext  : 2;
   unsigned int bracketed_paste : 1;
} Termpty_Tests;

Evas_Object *
termio_textgrid_get(const Evas_Object *obj EINA_UNUSED)
{
   return NULL;
}

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

static void
_termpty_to_termpty_tests(Termpty *ty, Termpty_Tests *tt)
{
   memset(tt, '\0', sizeof(*tt));
   tt->backsize = ty->backsize;
   tt->backpos = ty->backpos;
   tt->backlog_beacon = ty->backlog_beacon;
   tt->termstate = ty->termstate;
   tt->cursor_state = ty->cursor_state;
   tt->cursor_save[0] = ty->cursor_save[0];
   tt->cursor_save[1] = ty->cursor_save[1];
   tt->w = ty->w;
   tt->h = ty->h;
   tt->altbuf = ty->altbuf;
   tt->mouse_mode = ty->mouse_mode;
   tt->mouse_ext = ty->mouse_ext;
   tt->bracketed_paste = ty->bracketed_paste;
}

static void
_tytest_checksum(Termpty *ty)
{
   MD5_CTX ctx;
   Termpty_Tests tests;
   char md5out[(2 * MD5_HASHBYTES) + 1];
   unsigned char hash[MD5_HASHBYTES];
   static const char hex[] = "0123456789abcdef";
   int n;

   _termpty_to_termpty_tests(ty, &tests);

   MD5Init(&ctx);
   /* Termpty */
   MD5Update(&ctx,
             (unsigned char const*)&tests,
             sizeof(tests));
   /* The screens */
   MD5Update(&ctx,
             (unsigned char const*)ty->screen,
             sizeof(Termcell) * ty->w * ty->h);
   MD5Update(&ctx,
             (unsigned char const*)ty->screen2,
             sizeof(Termcell) * ty->w * ty->h);
   /* Icon/Title */
   if (ty->prop.icon)
     {
        MD5Update(&ctx,
                  (unsigned char const*)ty->prop.icon,
                  strlen(ty->prop.icon));
     }
   else
     {
        MD5Update(&ctx, (unsigned char const*)"(NULL)", 6);
     }
   if (ty->prop.title)
     {
        MD5Update(&ctx,
                  (unsigned char const*)ty->prop.title,
                  strlen(ty->prop.title));
     }
   else
     {
        MD5Update(&ctx, (unsigned char const*)"(NULL)", 6);
     }
   /* Cursor shape */
   MD5Update(&ctx, (unsigned char const*)_cursor_shape,
             strlen(_cursor_shape));
   /* Write buffer */
   MD5Update(&ctx, (unsigned char const*)ty->write_buffer.buf,
             ty->write_buffer.len);

   MD5Final(hash, &ctx);

   for (n = 0; n < MD5_HASHBYTES; n++)
     {
        md5out[2 * n] = hex[hash[n] >> 4];
        md5out[2 * n + 1] = hex[hash[n] & 0x0f];
     }
   md5out[2 * MD5_HASHBYTES] = '\0';
   printf("%s", md5out);
}
#endif
/* }}} */



static void
_termpty_init(Termpty *ty)
{
   memset(ty, '\0', sizeof(*ty));
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
#if defined(ENABLE_FUZZING)
   ty->fd_dev_null = open("/dev/null", O_WRONLY|O_APPEND);
   assert(ty->fd_dev_null >= 0);
#endif
   ty->hl.bitmap = calloc(1, HL_LINKS_MAX / 8); /* bit map for 1 << 16 elements */
   assert(ty->hl.bitmap);
   /* Mark id 0 as set */
   ty->hl.bitmap[0] = 1;
}

static void
_termpty_shutdown(Termpty *ty)
{
#if defined(ENABLE_TESTS)
   ty_sb_free(&ty->write_buffer);
#endif
#if defined(ENABLE_FUZZING)
   close(ty->fd_dev_null);
#endif
}

int
main(int argc EINA_UNUSED, char **argv EINA_UNUSED)
{
   char buf[4097];
   Eina_Unicode codepoint[4097];
   int len, i, j, k;

   eina_init();

   _log_domain = eina_log_domain_register(
#ifdef TYTEST
       "tytest",
#else
       "tyfuzz",
#endif
       NULL);

   _config = config_new();
   _sd.config = _config;

   _termpty_init(&_ty);

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

   _termpty_shutdown(&_ty);

   eina_shutdown();
   free(_config);

   return 0;
}
