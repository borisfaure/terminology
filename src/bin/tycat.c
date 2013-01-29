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

enum {
   CENTER,
   FILL,
   STRETCH,
   NOIMG
};

Ecore_Evas *ee = NULL;
Evas *evas = NULL;
Evas_Object *o = NULL;
struct termios told, tnew;
int tw = 0, th = 0, cw = 0, ch = 0;

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
   if (w > (tw * cw))
     {
        *iw = tw;
        *ih = ((h * (tw * cw) / w) + (ch - 1)) / ch;
     }
   else
     {
        *iw = (w + (cw - 1)) / cw;
        *ih = (h + (ch - 1)) / ch;
     }
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
   line[i++] = 0;
   for (y = 0; y < h; y++)
     {
        if (write(0, line, i) < 0) perror("write");
     }
   free(line);
}

int
main(int argc, char **argv)
{
   char buf[64];
   int w = 0, h = 0;
   int iw = 0, ih = 0;
   
   if (!getenv("TERMINOLOGY")) return 0;
   if (argc <= 1)
     {
        printf("Usage: %s [-s|-f|-c] FILE1 [FILE2 ...]\n"
               "\n"
               "  -s  Stretch file to fill nearest character cell size\n"
               "  -f  Fill file to totally cover character cells with no gaps\n"
               "  -c  Center file in nearest character cells but only scale down (default)\n",
              argv[0]);
        return 0;
     }
   eina_init();
   ecore_init();
   ecore_file_init();
   evas_init();
   ecore_evas_init();
   edje_init();
   emotion_init();
   ee = ecore_evas_buffer_new(1, 1);
   if (ee)
     {
        int i, mode = CENTER;
        
        evas = ecore_evas_get(ee);
        echo_off();
        snprintf(buf, sizeof(buf), "%c}qs", 0x1b);
        if (write(0, buf, strlen(buf) + 1) < 0) perror("write");
        if (scanf("%i;%i;%i;%i", &tw, &th, &cw, &ch) != 4)
          {
             echo_on();
             return 0;
          }
        if ((tw <= 0) || (th <= 0) || (cw <= 1) || (ch <= 1))
          {
             echo_on();
             return 0;
          }
        echo_on();
        for (i = 1; i < argc; i++)
          {
             char *path, *rp;

             if (!strcmp(argv[i], "-c"))
               {
                  mode = CENTER;
                  i++;
                  if (i >= argc) return 0;
               }
             else if (!strcmp(argv[i], "-s"))
               {
                  mode = STRETCH;
                  i++;
                  if (i >= argc) return 0;
               }
             else if (!strcmp(argv[i], "-f"))
               {
                  mode = FILL;
                  i++;
                  if (i >= argc) return 0;
               }
             
             path = argv[i];
             rp = ecore_file_realpath(path);
             if (rp)
               {
                  o = evas_object_image_add(evas);
                  evas_object_image_file_set(o, rp, NULL);
                  evas_object_image_size_get(o, &w, &h);
                  if ((w >= 0) && (h > 0))
                    {
                       scaleterm(w, h, &iw, &ih);
                       prnt(rp, iw, ih, mode);
                    }
                  else
                    {
                       Eina_Bool ok = EINA_TRUE;
                       
                       evas_object_del(o);
                       o = edje_object_add(evas);
                       if (!edje_object_file_set
                           (o, rp, "terminology/backgroud"))
                         {
                            if (!edje_object_file_set
                                (o, rp, "e/desktop/background"))
                              ok = EINA_FALSE;
                         }
                       if (ok)
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
                            prnt(rp, iw, ih, mode);
                         }
                       else
                         {
                            ok = EINA_TRUE;
                            
                            evas_object_del(o);
                            
                            o = emotion_object_add(evas);
                            ok = emotion_object_init(o, NULL);
                            if (ok)
                              {
                                 if (emotion_object_file_set(o, rp))
                                   {
                                      emotion_object_audio_mute_set(o, EINA_TRUE);
                                      if (emotion_object_video_handled_get(o))
                                        {
                                           emotion_object_size_get(o, &w, &h);
                                           if ((w >= 0) && (h > 0))
                                             {
                                                scaleterm(w, h, &iw, &ih);
                                                prnt(rp, iw, ih, mode);
                                             }
                                        }
                                      else
                                        prnt(rp, tw, 3, NOIMG);
                                   }
                              }
                         }
                    }
                  evas_object_del(o);
                  free(rp);
               }
             evas_norender(evas);
          }
        exit(0);
//        ecore_main_loop_begin();
        ecore_evas_free(ee);
     }
   emotion_shutdown();
   edje_shutdown();
   ecore_evas_shutdown();
   evas_shutdown();
   ecore_file_shutdown();
   ecore_shutdown();
   eina_shutdown();
   return 0;
}
