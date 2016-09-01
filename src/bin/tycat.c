#include <Eina.h>
#include <Ecore.h>
#include <Evas.h>
#include <Ecore_Evas.h>
#include <Ecore_File.h>
#include <Edje.h>
#include <Emotion.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "tycommon.h"

enum {
  CENTER,
  FILL,
  STRETCH,
  NOIMG
};

#define VIDEO_DECODE_TIMEOUT 1.0

static Evas *evas = NULL;
static struct termios told, tnew;
static int tw = 0, th = 0, cw = 0, ch = 0, maxw = 0, maxh = 0, _mode = CENTER;
static Ecore_Timer *timeout_t = NULL;

#include "extns.h"

static int
echo_off(void)
{
   if (tcgetattr(0, &told) != 0) return -1;
   tnew = told;
   tnew.c_lflag &= ~ECHO;
   if (tcsetattr(0, TCSAFLUSH, &tnew) != 0) return -1;
   return 0;
}

static int
echo_on(void)
{
   return tcsetattr(0, TCSAFLUSH, &told);
}

static void
scaleterm(int w, int h, int *iw, int *ih)
{
   int width = maxw ? maxw : tw;
   if (w > (width * cw))
     {
        *iw = width;
        *ih = ((h * (width * cw) / w) + (ch - 1)) / ch;
     }
   else
     {
        *iw = (w + (cw - 1)) / cw;
        *ih = (h + (ch - 1)) / ch;
     }
   if (maxh && *ih > maxh)
     {
        *ih = maxh;
        *iw = ((w * (maxh * ch) / h) + (cw - 1)) / cw;
     }
}

static const char *
is_fmt(const char *f, const char **extn)
{
   int i, len, l;

   len = strlen(f);
   for (i = 0; extn[i]; i++)
     {
        l = strlen(extn[i]);
        if (len < l) continue;
        if (!strcasecmp(extn[i], f + len - l)) return extn[i];
     }
   return NULL;
}

static void
prnt(const char *path, int w, int h, int mode)
{
   int x, y, i;
   char *line, buf[4096];

   if ((w >= 512) || (h >= 512)) return;
   line = malloc(w + 100);
   if (!line) return;
   if (mode == CENTER)
     snprintf(buf, sizeof(buf), "%c}ic#%i;%i;%s", 0x1b, w, h, path);
   else if (mode == FILL)
     snprintf(buf, sizeof(buf), "%c}if#%i;%i;%s", 0x1b, w, h, path);
   else
     snprintf(buf, sizeof(buf), "%c}is#%i;%i;%s", 0x1b, w, h, path);
   if (write(0, buf, strlen(buf) + 1) < 0) perror("write");
   i = 0;
   line[i++] = 0x1b;
   line[i++] = '}';
   line[i++] = 'i';
   line[i++] = 'b';
   line[i++] = 0;
   for (x = 0; x < w; x++) line[i++] = '#';
   line[i++] = 0x1b;
   line[i++] = '}';
   line[i++] = 'i';
   line[i++] = 'e';
   line[i++] = 0;
   line[i++] = '\n';
   for (y = 0; y < h; y++)
     {
        if (write(0, line, i) < 0) perror("write");
     }
   free(line);
}

static void
print_usage(const char *argv0)
{
   printf("Usage: %s "HELP_ARGUMENT_SHORT" [-s|-f|-c] [-g <width>x<height>] FILE1 [FILE2 ...]\n"
          "\n"
          "  -s  Stretch file to fill nearest character cell size\n"
          "  -f  Fill file to totally cover character cells with no gaps\n"
          "  -c  Center file in nearest character cells but only scale down (default)\n"
          "  -g <width>x<height>  Set maximum geometry for the image (cell count)\n"
          HELP_ARGUMENT_DOC"\n",
         argv0);
}

static Eina_Bool
timeout_cb(void *data)
{
   evas_object_del(data);
   timeout_t = NULL;

   return ECORE_CALLBACK_CANCEL;
}

static int
handle_image(char *rp)
{
   Evas_Object *o;
   int w = 0, h = 0;
   int iw = 0, ih = 0;
   int r = -1;

   if (!is_fmt(rp, extn_img) &&
       !is_fmt(rp, extn_scale) &&
       !is_fmt(rp, extn_mov))
     return -1;

   o = evas_object_image_add(evas);
   evas_object_image_file_set(o, rp, NULL);
   evas_object_image_size_get(o, &w, &h);
   if ((w >= 0) && (h > 0))
     {
        scaleterm(w, h, &iw, &ih);
        prnt(rp, iw, ih, _mode);
        r = 0;
     }

   evas_object_del(o);

   return r;
}

static int
handle_edje(char *rp)
{
   Evas_Object *o;
   int iw = 0, ih = 0;
   int r = -1;

   if (!is_fmt(rp, extn_edj)) return -1;

   o = edje_object_add(evas);
   if (edje_object_file_set
       (o, rp, "terminology/backgroud") == EINA_TRUE ||
       !edje_object_file_set
       (o, rp, "e/desktop/background"))
     {
        Evas_Coord mw = 0, mh = 0;
        edje_object_size_min_get(o, &mw, &mh);
        if ((mw <= 0) || (mh <= 0))
          edje_object_size_min_calc(o, &mw, &mh);
        if ((mw <= 0) || (mh <= 0))
          {
             mw = (tw) * cw;
             mh = (th - 1) * ch;
          }
        scaleterm(mw, mh, &iw, &ih);
        prnt(rp, iw, ih, _mode);
        r = 0;
     }

   evas_object_del(o);

   return r;
}

static void
video_decoded(void *data, Evas_Object *o, void *ei EINA_UNUSED)
{
   int w = 0, h = 0;
   int iw = 0, ih = 0;

   if (emotion_object_video_handled_get(o))
     {
        emotion_object_size_get(o, &w, &h);
        if ((w >= 0) && (h > 0))
          {
             scaleterm(w, h, &iw, &ih);
             prnt(data, iw, ih, _mode);
             goto done;
          }
        else
          {
             double ar = emotion_object_ratio_get(o);
             if (ar > 0.0)
               {
                  scaleterm(tw * cw, (int) ((tw * cw) / ar), &iw, &ih);
                  prnt(data, iw, ih, _mode);
                  goto done;
               }
          }
     }

   prnt(data, tw, 3, NOIMG);

done:
   ecore_timer_del(timeout_t);
   timeout_t = NULL;
   evas_object_del(o);
   free(data);
}

static int
handle_video(char *rp)
{
   Evas_Object *o;

   if (!is_fmt(rp, extn_aud) &&
       !is_fmt(rp, extn_mov))
     return -1;

   o = emotion_object_add(evas);
   if (emotion_object_init(o, NULL) == EINA_TRUE)
     {
        if (emotion_object_file_set(o, rp))
          {
             emotion_object_audio_mute_set(o, EINA_TRUE);
             emotion_object_play_set(o, EINA_TRUE);
             timeout_t = ecore_timer_add(VIDEO_DECODE_TIMEOUT,
                                         timeout_cb, o);
             evas_object_smart_callback_add(o, "frame_decode",
                                            video_decoded,
                                            strdup(rp));
             return 0;
          }

     }

   evas_object_del(o);

   return -1;
}

static Eina_Bool
handle_file(void *data)
{
   Eina_List **file_q = data;

   int (*handlers[])(char *rp) = {
     handle_image,
     handle_edje,
     handle_video,
     NULL
   };
   char *rp;
   int i;

   if (timeout_t) return ECORE_CALLBACK_RENEW;
   if (!(*file_q))
     {
        ecore_main_loop_quit();
        return ECORE_CALLBACK_CANCEL;
     }

   rp = eina_list_data_get(*file_q);
   *file_q = eina_list_remove_list(*file_q, *file_q);
   if (!rp) return ECORE_CALLBACK_RENEW;

   for (i = 0; handlers[i]; i++)
     {
        if (handlers[i](rp) == 0) break;
     }
   free(rp);

   return ECORE_CALLBACK_RENEW;
}

int
main(int argc, char **argv)
{
   Ecore_Evas *ee;
   char buf[64];
   int i;
   char *rp;
   Eina_List *file_q = NULL;

   ON_NOT_RUNNING_IN_TERMINOLOGY_EXIT_1();
   ARGUMENT_ENTRY_CHECK(argc, argv, print_usage);
   if (argc <= 1)
     {
        print_usage(argv[0]);
        return 0;
     }

   eina_init();
#if (ECORE_VERSION_MAJOR > 1) || (ECORE_VERSION_MINOR >= 8)
   ecore_app_no_system_modules();
#endif
   ecore_init();
   ecore_file_init();
   evas_init();
   ecore_evas_init();
   edje_init();
   emotion_init();

   ee = ecore_evas_buffer_new(1, 1);
   if (!ee) goto shutdown;
   evas = ecore_evas_get(ee);
   echo_off();
   snprintf(buf, sizeof(buf), "%c}qs", 0x1b);
   if (write(0, buf, strlen(buf) + 1) < 0) perror("write");
   if (scanf("%i;%i;%i;%i", &tw, &th, &cw, &ch) != 4 ||
       ((tw <= 0) || (th <= 0) || (cw <= 1) || (ch <= 1)))
     {
        echo_on();
        goto shutdown;
     }
   echo_on();

   for (i = 1; i < argc; i++)
     {
        char *path;

        if (!strcmp(argv[i], "-c"))
          {
             _mode = CENTER;
             i++;
             if (i >= argc) goto done;
          }
        else if (!strcmp(argv[i], "-s"))
          {
             _mode = STRETCH;
             i++;
             if (i >= argc) goto done;
          }
        else if (!strcmp(argv[i], "-f"))
          {
             _mode = FILL;
             i++;
             if (i >= argc) goto done;
          }

        if (!strcmp(argv[i], "-g"))
          {
             unsigned int width = 0, height = 0;
             int cnt;

             if (i + 2 >= argc)
               {
                  print_usage(argv[0]);
                  goto shutdown;
               }
             i++;
             cnt = sscanf(argv[i], "%ux%u", &width, &height);
             if (cnt != 2)
               {
                  print_usage(argv[0]);
                  goto done;
               }
             if (!width || tw > (int)width) maxw = width;
             maxh = height;
          }
        path = argv[i];
        rp = ecore_file_realpath(path);
        if (rp)
          file_q = eina_list_append(file_q, rp);
     }
   if (!file_q) goto done;

   ecore_idler_add(handle_file, &file_q);
   ecore_main_loop_begin();

done:
   EINA_LIST_FREE(file_q, rp)
     free(rp);

   ecore_evas_free(ee);

shutdown:
   emotion_shutdown();
   edje_shutdown();
   ecore_evas_shutdown();
   evas_shutdown();
   ecore_file_shutdown();
   ecore_shutdown();
   eina_shutdown();

   return 0;
}
