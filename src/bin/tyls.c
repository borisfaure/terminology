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
   SMALL,
   MEDIUM,
   LARGE
};

Ecore_Evas *ee = NULL;
Evas *evas = NULL;
Evas_Object *o = NULL;
struct termios told, tnew;
int tw = 0, th = 0, cw = 0, ch = 0;

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
   snprintf(buf, sizeof(buf), "%c}ic#%i;%i;%s", 0x1b, w, h, path);
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
list_dir(const char *dir, int mode)
{
   Eina_List *files, *l;
   char *s, **names;
   int maxlen = 0, cols, c, i, j, num, cw, stuff;
   
   files = ecore_file_ls(dir);
   if (!files) return;
   names = calloc(eina_list_count(files), sizeof(char *));
   if (!names) return;
   i = 0;
   EINA_LIST_FOREACH(files, l, s)
     {
        int len = eina_unicode_utf8_get_len(s);
        
        if (s[0] == '.') continue;
        if (len > maxlen) maxlen = len;
        names[i] = s;
        i++;
     }
   num = i;
   stuff = 0;
   if (mode == SMALL) stuff += 2;
   stuff += 1; // spacer at start
   stuff += 5; // xxxx[ /K/M/G/T/P...]
   stuff += 1; // type [@/*/|/=...]
   stuff += 1; // spacer
   maxlen += stuff;
   if (maxlen > 0)
     {
        cols = tw / maxlen;
        if (cols < 1) cols = 1;
        for (i = 0; i < ((num + (cols - 1)) / cols); i++)
          {
             for (c = 0; c < cols; c++)
               {
                  char buf[4096], sz[6];
                  long long size;
                  
                  s = names[((c * (num + (cols - 1))) / cols) + i];
                  if (!s) continue;
                  snprintf(buf, sizeof(buf), "%s/%s", dir, s);
                  int len = eina_unicode_utf8_get_len(s);
                  cw = tw / cols;
                  size = ecore_file_size(buf);
                  if (size < 1024)
                    snprintf(sz, sizeof(sz), " %4lld", size);
                  else if (size < (1024 * 1024))
                    snprintf(sz, sizeof(sz), "%4lldK", size / (1024));
                  else if (size < (1024 * 1024 * 1024))
                    snprintf(sz, sizeof(sz), "%4lldM", size / (1024 * 1024));
                  else if (size < (1024 * 1024 * 1024 * 1024))
                    snprintf(sz, sizeof(sz), "%4lldG", size / (1024 * 1024 * 1024));
                  else if (size < (1024 * 1024 * 1024 * 1024 * 1024))
                    snprintf(sz, sizeof(sz), "%4lldT", size / (1024 * 1024 * 1024 * 1024));
                  else if (size < (1024 * 1024 * 1024 * 1024 * 1024 * 1024))
                    snprintf(sz, sizeof(sz), "%4lldP", size / (1024 * 1024 * 1024 * 1024 * 1024));
                  len += stuff;
                  printf("%c}ic#%i;%i;%s%c", 0x1b, 2, 1, buf, 0);
                  printf("%c}ib%c", 0x1b, 0);
                  printf("##");
                  printf("%c}ie%c", 0x1b, 0);
                  printf(" %s %s", sz, s);
                  if (ecore_file_is_dir(buf)) printf("/");
                  else
                    {
                       char *ts;
                       
                       ts = ecore_file_readlink(buf);
                       if (ts)
                         {
                            printf("@");
                            free(ts);
                         }
                       else
                         {
                            printf(" ");
                         }
                    }
                  for (j = 0; j < (cw - len); j++) printf(" ");
               }
             printf("\n");
          }
     }
   free(names);
   EINA_LIST_FREE(files, s) free(s);
}
/*
                  if ((is_fmt(rp, extn_img)) ||
                      (is_fmt(rp, extn_scale)) ||
                      (is_fmt(rp, extn_mov)))
                    {
                       o = evas_object_image_add(evas);
                       evas_object_image_file_set(o, rp, NULL);
                       evas_object_image_size_get(o, &w, &h);
                       if ((w >= 0) && (h > 0))
                         {
                            scaleterm(w, h, &iw, &ih);
                            prnt(rp, iw, ih, mode);
                            goto done;
                         }
                       evas_object_del(o);
                       o = NULL;
                    }
                  
                  if (is_fmt(rp, extn_edj))
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
                            goto done;
                         }
                       evas_object_del(o);
                       o = NULL;
                    }
                  
                  if ((is_fmt(rp, extn_aud)) ||
                      (is_fmt(rp, extn_mov)))
                    {
                       Eina_Bool ok = EINA_TRUE;
                       
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
                                 goto done;
                              }
                         }
                       evas_object_del(o);
                       o = NULL;
                    }
done:
                  if (o) evas_object_del(o);
                  o = NULL;
                  free(rp);
               }
             evas_norender(evas);
*/

int
main(int argc, char **argv)
{
   char buf[64];
   int w = 0, h = 0;
   int iw = 0, ih = 0;
   Eina_Bool listed = EINA_FALSE;
   
   if (!getenv("TERMINOLOGY")) return 0;
   if ((argc == 2) && (!strcmp(argv[1], "-h")))
     {
        printf("Usage: %s [-s|-m|-l] FILE1 [FILE2 ...]\n"
               "\n"
               "  -s  Small list mode\n"
               "  -m  Medium list mode\n"
               "  -l  Large list mode\n",
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
        int i, mode = SMALL;
        char *rp;
        
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
             char *path;

             if (!strcmp(argv[i], "-c"))
               {
                  mode = SMALL;
                  i++;
                  if (i >= argc) break;
               }
             else if (!strcmp(argv[i], "-m"))
               {
                  mode = MEDIUM;
                  i++;
                  if (i >= argc) break;
               }
             else if (!strcmp(argv[i], "-l"))
               {
                  mode = LARGE;
                  i++;
                  if (i >= argc) break;;
               }
             path = argv[i];
             rp = ecore_file_realpath(path);
             if (rp)
               {
                  if (ecore_file_is_dir(rp))
                    {
                       list_dir(rp, mode);
                       listed = EINA_TRUE;
                    }
                  free(rp);
               }
          }
        if (!listed)
          {
             rp = ecore_file_realpath("./");
             if (rp)
               {
                  list_dir(rp, mode);
                  free(rp);
               }
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
