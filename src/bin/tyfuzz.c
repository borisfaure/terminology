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
/* }}} */



static void
_termpty_init(Termpty *ty)
{
   memset(ty, '\0', sizeof(*ty));
   ty->w = 80;
   ty->h = 25;
   ty->backsize = 50;
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

   _log_domain = eina_log_domain_register("tyfuzz", NULL);

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

   _termpty_shutdown(&ty);

   eina_shutdown();
   free(_config);

   return 0;
}
