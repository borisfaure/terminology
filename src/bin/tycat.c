#include <Eina.h>
#include <Ecore.h>
#include <Evas.h>
#include <Ecore_Evas.h>
#include <Ecore_File.h>
#include <Edje.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

enum {
   CENTER,
   FILL,
   STRETCH
};

Ecore_Evas *ee = NULL;
Evas *evas = NULL;
Evas_Object *o = NULL;
struct termios told, tnew;
int tw = 0, th = 0, cw = 0, ch = 0;
int w = 0, h = 0;
int iw = 0, ih = 0;

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

int
main(int argc, char **argv)
{
   char buf[8192];
   
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
   ee = ecore_evas_buffer_new(1, 1);
   if (ee)
     {
        int i, mode = CENTER;
        
        evas = ecore_evas_get(ee);
        o = evas_object_image_add(evas);
        echo_off();
        snprintf(buf, sizeof(buf), "%c}qs", 0x1b);
        if (write(0, buf, strlen(buf) + 1) < 0) perror("write");
        if (scanf("%i;%i;%i;%i", &tw, &th, &cw, &ch) != 4)
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
                  evas_object_image_file_set(o, rp, NULL);
                  evas_object_image_size_get(o, &w, &h);
                  if ((w >= 0) && (h > 0))
                    {
                       int x, y, i;
                       char *line;
                       
                      if ((tw <= 0) || (th <= 0) || (cw <= 1) || (ch <= 1))
                         {
                            free(rp);
                            continue;
                         }
                       if (w > (tw * cw))
                         {
                            iw = tw;
                            ih = ((h * (tw * cw) / w) + (ch - 1)) / ch;
                         }
                       else
                         {
                            iw = (w + (cw - 1)) / cw;
                            ih = (h + (ch - 1)) / ch;
                         }
                       line = malloc(iw + 100);
                       if (!line)
                         {
                            free(rp);
                            continue;
                         }
                        if (mode == CENTER)
                         snprintf(buf, sizeof(buf), "%c}ic#%i;%i;%s",
                                  0x1b, iw, ih, rp);
                       else if (mode == FILL)
                         snprintf(buf, sizeof(buf), "%c}if#%i;%i;%s",
                                  0x1b, iw, ih, rp);
                       else
                         snprintf(buf, sizeof(buf), "%c}is#%i;%i;%s",
                                  0x1b, iw, ih, rp);
                       if (write(0, buf, strlen(buf) + 1) < 0) perror("write");
                       i = 0;
                       line[i++] = 0x1b;
                       line[i++] = '}';
                       line[i++] = 'i';
                       line[i++] = 'b';
                       line[i++] = 0;
                       for (x = 0; x < iw; x++) line[i++] = '#';
                       line[i++] = 0x1b;
                       line[i++] = '}';
                       line[i++] = 'i';
                       line[i++] = 'e';
                       line[i++] = 0;
                       line[i++] = '\n';
                       line[i++] = 0;
                       for (y = 0; y < ih; y++)
                         {
                            if (write(0, line, i) < 0) perror("write");
                         }
                       free(line);
                    }
                  free(rp);
               }
          }
//   ecore_main_loop_begin();
        ecore_evas_free(ee);
     }
   edje_shutdown();
   ecore_evas_shutdown();
   evas_shutdown();
   ecore_file_shutdown();
   ecore_shutdown();
   eina_shutdown();
   return 0;
}
