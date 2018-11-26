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
#include <assert.h>

#ifdef TYTEST
#include "md5/md5.h"
#endif

#define TY_H 25
#define TY_W 80
#define TY_BACKSIZE 50


/* {{{ stub */
int _log_domain = -1;
static Config *_config = NULL;

const char *
theme_path_get(void)
{
   return NULL;
}

void
main_config_sync(const Config *config EINA_UNUSED)
{
}

void
termio_content_change(Evas_Object *obj EINA_UNUSED,
                      Evas_Coord x EINA_UNUSED, Evas_Coord y EINA_UNUSED,
                      int n EINA_UNUSED)
{
}


Config *
termio_config_get(const Evas_Object *obj EINA_UNUSED)
{
   return _config;
}

void
termio_scroll(Evas_Object *obj EINA_UNUSED,
              int direction EINA_UNUSED,
              int start_y EINA_UNUSED,
              int end_y EINA_UNUSED)
{
}

void
termio_font_size_set(Evas_Object *obj EINA_UNUSED,
                     int size EINA_UNUSED)
{
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
   MD5Update(&ctx, (unsigned char const*)_cursor_shape,
             strlen(_cursor_shape));

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
   ty->fd_dev_null = open("/dev/null", O_WRONLY|O_APPEND);
   assert(ty->fd_dev_null >= 0);
   ty->hl.bitmap = calloc(1, HL_LINKS_MAX / 8); /* bit map for 1 << 16 elements */
   assert(ty->hl.bitmap);
   /* Mark id 0 as set */
   ty->hl.bitmap[0] = 1;
}

static void
_termpty_shutdown(Termpty *ty)
{
   close(ty->fd_dev_null);
}

int
main(int argc EINA_UNUSED, char **argv EINA_UNUSED)
{
   Termpty ty;
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

   _termpty_init(&ty);

   if (argc > 1)
     {
       ty.fd = open(argv[1], O_RDONLY);
       assert(ty.fd >= 0);
     }

   do
     {
        char *rbuf = buf;
        len = sizeof(buf) - 1;

        for (i = 0; i < (int)sizeof(ty.oldbuf) && ty.oldbuf[i] & 0x80; i++)
          {
             *rbuf = ty.oldbuf[i];
             rbuf++;
             len--;
          }
        len = read(ty.fd, rbuf, len);
        if (len < 0 && errno != EAGAIN)
          {
             ERR("error while reading from tty slave fd");
             break;
          }
        if (len <= 0) break;

        for (i = 0; i < (int)sizeof(ty.oldbuf); i++)
          ty.oldbuf[i] = 0;

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
                      (len - prev_i) <= (int)sizeof(ty.oldbuf))
                    {
                       for (k = 0;
                            (k < (int)sizeof(ty.oldbuf)) &&
                            (k < (len - prev_i));
                            k++)
                         {
                            ty.oldbuf[k] = buf[prev_i+k];
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
        termpty_handle_buf(&ty, codepoint, j);
     }
   while (1);

#ifdef TYTEST
   _tytest_checksum(&ty);
#endif

   _termpty_shutdown(&ty);

   eina_shutdown();
   free(_config);

   return 0;
}
