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
#include <fnmatch.h>

// this code sucks. just letting you know... in advance... in case you
// might be tempted to think otherwise... :)

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
size_print(char *buf, int bufsz, char *sz, unsigned long long size)
{
   if (size < 1024LL)
     {
        snprintf(buf, bufsz, "%4lld", size);
        *sz = ' ';
     }
   else if (size < (1024LL * 1024LL))
     {
        snprintf(buf, bufsz, "%4lld", size / (1024LL));
        *sz = 'K';
     }
   else if (size < (1024LL * 1024LL * 1024LL))
     {
        snprintf(buf, bufsz, "%4lld", size / (1024LL * 1024LL));
        *sz = 'M';
     }
   else if (size < (1024LL * 1024LL * 1024LL * 1024LL))
     {
        snprintf(buf, bufsz, "%4lld", size / (1024LL * 1024 * 1024LL));
        *sz = 'G';
     }
   else if (size < (1024LL * 1024LL * 1024LL * 1024LL * 1024LL))
     {
        snprintf(buf, bufsz, "%4lld", size / (1024LL * 1024LL * 1024LL * 1024LL));
        *sz = 'T';
     }
   else if (size < (1024LL * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL))
     {
        snprintf(buf, bufsz, "%4lld", size / (1024LL * 1024LL * 1024LL * 1024LL * 1024LL));
        *sz = 'P';
     }
   else
     {
        snprintf(buf, bufsz, "%4lld", size / (1024LL * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL));
        *sz = 'E';
     }
}

#define RESET 0
#define CORE  1
#define CUBE  2
#define GRAY  3

#define BG 0
#define FG 1

#define BLACK   0
#define RED     1
#define GREEN   2
#define YELLOW  3
#define BLUE    4
#define PURPLE  5
#define CYAN    6
#define WHITE   7
#define BRIGHT  8

// #3399ff
// 51 153 355
// as 6x6x6
// 1.0 3.0 5.0

static void
colorprint(int mode, int fgbg, int r, int g, int b)
{
   if (mode == RESET)
     {
        printf("%c[0m", 0x1b);
     }
   else if (mode == CORE) // 8 bg and 8 fg colors
     {
        int v = 0;
        
        if (r & BRIGHT) printf("%c[01m", 0x1b);
        else printf("%c[0m", 0x1b);
        if (fgbg == FG) v = 30 + (r & 0x7);
        else v = 40 + (r & 0x7);
        printf("%c[%im", 0x1b, v);
     }
   else if (mode == CUBE) // 6x6x6 cube
     {
        int v = 0, c = 0;
        
        if (fgbg == FG) v = 38;
        else v = 48;
        c = 16 + ((r * 6 * 6) + (g * 6) + b);
        printf("%c[%i;5;%im", 0x1b, v, c);
     }
   else if (mode == GRAY) // 26 levels of grey
     {
     }
}

static void
sizeprint(char *sz, char szch)
{
   colorprint(CUBE, FG, 1, 3, 5);
   printf("%s", sz);
   if (szch == 'K')
     colorprint(CUBE, FG, 3, 4, 5);
   else if (szch == 'M')
     colorprint(CUBE, FG, 5, 5, 5);
   else if (szch == 'G')
     colorprint(CUBE, FG, 5, 3, 0);
   else if (szch == 'T')
     colorprint(CUBE, FG, 5, 1, 1);
   else if (szch == 'P')
     colorprint(CUBE, FG, 5, 0, 0);
   else if (szch == 'E')
     colorprint(CUBE, FG, 5, 5, 0);
   printf("%c", szch);
   colorprint(RESET, 0, 0, 0, 0);
}

typedef struct _Cmatch
{
   short fr, fg, fb;
   short br, bg, bb;
   const char *match;
} Cmatch;

// for regular files
const Cmatch fmatch[] =
{
   { 5, 5, 5,  9, 9, 9, "*.jpg"},
   { 5, 5, 5,  9, 9, 9, "*.JPG"},
   { 5, 5, 5,  9, 9, 9, "*.jpeg"},
   { 5, 5, 5,  9, 9, 9, "*.JPEG"},
   { 5, 5, 5,  9, 9, 9, "*.jfif"},
   { 5, 5, 5,  9, 9, 9, "*.JFIF"},
   { 5, 5, 5,  9, 9, 9, "*.jfi"},
   { 5, 5, 5,  9, 9, 9, "*.JFI"},
   { 5, 5, 5,  9, 9, 9, "*.png"},
   { 5, 5, 5,  9, 9, 9, "*.PNG"},
   { 5, 5, 5,  9, 9, 9, "*.bmp"},
   { 5, 5, 5,  9, 9, 9, "*.BMP"},
   { 5, 5, 5,  9, 9, 9, "*.gif"},
   { 5, 5, 5,  9, 9, 9, "*.GIF"},
   { 5, 5, 5,  9, 9, 9, "*.xcf"},
   { 5, 5, 5,  9, 9, 9, "*.XCF"},
   { 5, 5, 5,  9, 9, 9, "*.xcf.gz"},
   { 5, 5, 5,  9, 9, 9, "*.XCF.gz"},
   { 5, 5, 5,  9, 9, 9, "*.svg"},
   { 5, 5, 5,  9, 9, 9, "*.SVG"},
   { 5, 5, 5,  9, 9, 9, "*.svgz"},
   { 5, 5, 5,  9, 9, 9, "*.SVGZ"},
   { 5, 5, 5,  9, 9, 9, "*.svg.gz"},
   { 5, 5, 5,  9, 9, 9, "*.SVG.GZ"},
   { 5, 5, 5,  9, 9, 9, "*.ppm"},
   { 5, 5, 5,  9, 9, 9, "*.PPM"},
   { 5, 5, 5,  9, 9, 9, "*.tif"},
   { 5, 5, 5,  9, 9, 9, "*.TIF"},
   { 5, 5, 5,  9, 9, 9, "*.tiff"},
   { 5, 5, 5,  9, 9, 9, "*.TIFF"},
   { 5, 5, 5,  9, 9, 9, "*.ico"},
   { 5, 5, 5,  9, 9, 9, "*.ICO"},
   { 5, 5, 5,  9, 9, 9, "*.pgm"},
   { 5, 5, 5,  9, 9, 9, "*.PGM"},
   { 5, 5, 5,  9, 9, 9, "*.pbm"},
   { 5, 5, 5,  9, 9, 9, "*.PBM"},
   { 5, 5, 5,  9, 9, 9, "*.psd"},
   { 5, 5, 5,  9, 9, 9, "*.PSD"},
   { 5, 5, 5,  9, 9, 9, "*.wbmp"},
   { 5, 5, 5,  9, 9, 9, "*.WBMP"},
   { 5, 5, 5,  9, 9, 9, "*.cur"},
   { 5, 5, 5,  9, 9, 9, "*.CUR"},
   { 5, 5, 5,  9, 9, 9, "*.arw"},
   { 5, 5, 5,  9, 9, 9, "*.ARW"},
   { 5, 5, 5,  9, 9, 9, "*.webp"},
   { 5, 5, 5,  9, 9, 9, "*.WEBP"},
   { 5, 5, 5,  9, 9, 9, "*.cr2"},
   { 5, 5, 5,  9, 9, 9, "*.CR2"},
   { 5, 5, 5,  9, 9, 9, "*.crw"},
   { 5, 5, 5,  9, 9, 9, "*.CRW"},
   { 5, 5, 5,  9, 9, 9, "*.dcr"},
   { 5, 5, 5,  9, 9, 9, "*.DCR"},
   { 5, 5, 5,  9, 9, 9, "*.dng"},
   { 5, 5, 5,  9, 9, 9, "*.DNG"},
   { 5, 5, 5,  9, 9, 9, "*.k25"},
   { 5, 5, 5,  9, 9, 9, "*.K25"},
   { 5, 5, 5,  9, 9, 9, "*.kdc"},
   { 5, 5, 5,  9, 9, 9, "*.KDC"},
   { 5, 5, 5,  9, 9, 9, "*.thm"},
   { 5, 5, 5,  9, 9, 9, "*.THM"},
   { 5, 5, 5,  9, 9, 9, "*.erf"},
   { 5, 5, 5,  9, 9, 9, "*.ERF"},
   { 5, 5, 5,  9, 9, 9, "*.mrw"},
   { 5, 5, 5,  9, 9, 9, "*.MRW"},
   { 5, 5, 5,  9, 9, 9, "*.nef"},
   { 5, 5, 5,  9, 9, 9, "*.NEF"},
   { 5, 5, 5,  9, 9, 9, "*.nrf"},
   { 5, 5, 5,  9, 9, 9, "*.NRF"},
   { 5, 5, 5,  9, 9, 9, "*.nrw"},
   { 5, 5, 5,  9, 9, 9, "*.NRW"},
   { 5, 5, 5,  9, 9, 9, "*.orf"},
   { 5, 5, 5,  9, 9, 9, "*.ORF"},
   { 5, 5, 5,  9, 9, 9, "*.raw"},
   { 5, 5, 5,  9, 9, 9, "*.RAW"},
   { 5, 5, 5,  9, 9, 9, "*.rw2"},
   { 5, 5, 5,  9, 9, 9, "*.RW2"},
   { 5, 5, 5,  9, 9, 9, "*.pef"},
   { 5, 5, 5,  9, 9, 9, "*.PEF"},
   { 5, 5, 5,  9, 9, 9, "*.raf"},
   { 5, 5, 5,  9, 9, 9, "*.RAF"},
   { 5, 5, 5,  9, 9, 9, "*.sr2"},
   { 5, 5, 5,  9, 9, 9, "*.SR2"},
   { 5, 5, 5,  9, 9, 9, "*.srf"},
   { 5, 5, 5,  9, 9, 9, "*.SRF"},
   { 5, 5, 5,  9, 9, 9, "*.x3f"},
   { 5, 5, 5,  9, 9, 9, "*.X3F"},
   
   { 5, 5, 2,  9, 9, 9, "*.pdf"},
   { 5, 5, 2,  9, 9, 9, "*.PDF"},
   { 5, 5, 2,  9, 9, 9, "*.pdf"},
   { 5, 5, 2,  9, 9, 9, "*.ps"},
   { 5, 5, 2,  9, 9, 9, "*.PS"},
   { 5, 5, 2,  9, 9, 9, "*.ps.gz"},
   { 5, 5, 2,  9, 9, 9, "*.PS.GZ"},

   { 3, 4, 5,  9, 9, 9, "*.asf"},
   { 3, 4, 5,  9, 9, 9, "*.avi"},
   { 3, 4, 5,  9, 9, 9, "*.bdm"},
   { 3, 4, 5,  9, 9, 9, "*.bdmv"},
   { 3, 4, 5,  9, 9, 9, "*.clpi"},
   { 3, 4, 5,  9, 9, 9, "*.cpi"},
   { 3, 4, 5,  9, 9, 9, "*.dv"},
   { 3, 4, 5,  9, 9, 9, "*.fla"},
   { 3, 4, 5,  9, 9, 9, "*.flv"},
   { 3, 4, 5,  9, 9, 9, "*.m1v"},
   { 3, 4, 5,  9, 9, 9, "*.m2t"},
   { 3, 4, 5,  9, 9, 9, "*.m4v"},
   { 3, 4, 5,  9, 9, 9, "*.mkv"},
   { 3, 4, 5,  9, 9, 9, "*.mov"},
   { 3, 4, 5,  9, 9, 9, "*.mp2"},
   { 3, 4, 5,  9, 9, 9, "*.mp2ts"},
   { 3, 4, 5,  9, 9, 9, "*.mp4"},
   { 3, 4, 5,  9, 9, 9, "*.mpe"},
   { 3, 4, 5,  9, 9, 9, "*.mpeg"},
   { 3, 4, 5,  9, 9, 9, "*.mpg"},
   { 3, 4, 5,  9, 9, 9, "*.mpl"},
   { 3, 4, 5,  9, 9, 9, "*.mpls"},
   { 3, 4, 5,  9, 9, 9, "*.mts"},
   { 3, 4, 5,  9, 9, 9, "*.mxf"},
   { 3, 4, 5,  9, 9, 9, "*.nut"},
   { 3, 4, 5,  9, 9, 9, "*.nuv"},
   { 3, 4, 5,  9, 9, 9, "*.ogg"},
   { 3, 4, 5,  9, 9, 9, "*.ogv"},
   { 3, 4, 5,  9, 9, 9, "*.ogm"},
   { 3, 4, 5,  9, 9, 9, "*.qt"},
   { 3, 4, 5,  9, 9, 9, "*.rm"},
   { 3, 4, 5,  9, 9, 9, "*.rmj"},
   { 3, 4, 5,  9, 9, 9, "*.rmm"},
   { 3, 4, 5,  9, 9, 9, "*.rms"},
   { 3, 4, 5,  9, 9, 9, "*.rmvb"},
   { 3, 4, 5,  9, 9, 9, "*.rmx"},
   { 3, 4, 5,  9, 9, 9, "*.rv"},
   { 3, 4, 5,  9, 9, 9, "*.swf"},
   { 3, 4, 5,  9, 9, 9, "*.ts"},
   { 3, 4, 5,  9, 9, 9, "*.weba"},
   { 3, 4, 5,  9, 9, 9, "*.webm"},
   { 3, 4, 5,  9, 9, 9, "*.wmv"},
   { 3, 4, 5,  9, 9, 9, "*.3g2"},
   { 3, 4, 5,  9, 9, 9, "*.3gp2"},
   { 3, 4, 5,  9, 9, 9, "*.3gpp"},
   { 3, 4, 5,  9, 9, 9, "*.3gpp2"},
   { 3, 4, 5,  9, 9, 9, "*.3p2"},
   { 3, 4, 5,  9, 9, 9, "*.264"},
   { 3, 4, 5,  9, 9, 9, "*.3gp"},
   
   { 2, 3, 5,  9, 9, 9, "*.mp3"},
   { 2, 3, 5,  9, 9, 9, "*.MP3"},
   { 2, 3, 5,  9, 9, 9, "*.aac"},
   { 2, 3, 5,  9, 9, 9, "*.AAC"},
   { 2, 3, 5,  9, 9, 9, "*.wav"},
   { 2, 3, 5,  9, 9, 9, "*.WAV"},
   
   { 3, 5, 2,  9, 9, 9, "*.patch"},
   { 3, 5, 2,  9, 9, 9, "*.PATCH"},
   { 3, 5, 2,  9, 9, 9, "*.diff"},
   { 3, 5, 2,  9, 9, 9, "*.DIFF"},

   { 5, 1, 1,  9, 9, 9, "*.tar.*"},
   { 5, 1, 1,  9, 9, 9, "*.TAR.*"},
   { 5, 1, 1,  9, 9, 9, "*.tar"},
   { 5, 1, 1,  9, 9, 9, "*.TAR"},
   { 5, 1, 1,  9, 9, 9, "*.tgz"},
   { 5, 1, 1,  9, 9, 9, "*.TGZ"},
   { 5, 1, 1,  9, 9, 9, "*.tbz"},
   { 5, 1, 1,  9, 9, 9, "*.TBZ"},
   { 5, 1, 1,  9, 9, 9, "*.zip"},
   { 5, 1, 1,  9, 9, 9, "*.ZIP"},
   { 5, 1, 1,  9, 9, 9, "*.rar"},
   { 5, 1, 1,  9, 9, 9, "*.RAR"},
   { 5, 1, 1,  9, 9, 9, "*.cpio"},
   { 5, 1, 1,  9, 9, 9, "*.CPIO"},

   { 0, 5, 2,  9, 9, 9, "*.iso"},
   { 0, 5, 2,  9, 9, 9, "*.ISO"},
   { 0, 5, 2,  9, 9, 9, "*.img"},
   { 0, 5, 2,  9, 9, 9, "*.IMG"},
   
   { 1, 1, 1,  9, 9, 9, "*~"},

   { 5, 3, 1,  9, 9, 9, "Makefile"},
   { 4, 2, 0,  9, 9, 9, "Makefile.in"},
   { 3, 1, 0,  9, 9, 9, "*.am"},

   { 4, 1, 5,  9, 9, 9, "*.m4"},

   { 5, 2, 0,  9, 9, 9, "*.sh"},
   { 5, 2, 0,  9, 9, 9, "*.bin"},
   { 5, 2, 0,  9, 9, 9, "*.BIN"},

   { 2, 2, 3,  9, 9, 9, "*.in"},
   
   { 5, 4, 0,  9, 9, 9, "configure"},
   { 5, 3, 0,  9, 9, 9, "configure.in"},
   { 5, 2, 0,  9, 9, 9, "configure.ac"},

   { 0, 5, 5,  9, 9, 9, "*.c"},
   { 0, 5, 5,  9, 9, 9, "*.C"},
   { 1, 4, 5,  9, 9, 9, "*.x"},
   { 1, 4, 5,  9, 9, 9, "*.X"},
   { 1, 3, 5,  9, 9, 9, "*.h"},
   { 1, 3, 5,  9, 9, 9, "*.H"},

   { 5, 5, 2,  9, 9, 9, "*.edc"},
   { 5, 5, 2,  9, 9, 9, "*.EDC"},

   { 5, 3, 0,  9, 9, 9, "*.edj"},
   { 5, 3, 0,  9, 9, 9, "*.EDJ"},
   
   { 5, 0, 5,  9, 9, 9, "*.cc"},
   { 5, 0, 5,  9, 9, 9, "*.CC"},
   { 3, 1, 5,  9, 9, 9, "*.hh"},
   { 3, 1, 5,  9, 9, 9, "*.HH"},

   { 2, 0, 5,  9, 9, 9, "*.desktop"},
   { 2, 0, 5,  9, 9, 9, "*.directory"},
   
   { 0, 1, 3,  9, 9, 9, "*.o"},
   { 0, 1, 3,  9, 9, 9, "*.O"},
   { 0, 1, 3,  9, 9, 9, "*.lo"},

   { 5, 1, 3,  9, 9, 9, "*.log"},
   { 5, 1, 3,  9, 9, 9, "*.LOG"},

   { 3, 1, 5,  9, 9, 9, "*.txt"},
   { 3, 1, 5,  9, 9, 9, "*.TXT"},
   { 3, 1, 5,  9, 9, 9, "README"},
   { 3, 1, 5,  9, 9, 9, "Readme"},
   { 3, 1, 5,  9, 9, 9, "readme"},
   { 3, 1, 5,  9, 9, 9, "INSTALL"},
   { 3, 1, 5,  9, 9, 9, "COPYING"},
   { 3, 1, 5,  9, 9, 9, "NEWS"},
   { 3, 1, 5,  9, 9, 9, "ChangeLog"},
   { 3, 1, 5,  9, 9, 9, "AUTHORS"},
   { 3, 1, 5,  9, 9, 9, "TODO"},
   { 3, 1, 5,  9, 9, 9, "*.doc"},
   { 3, 1, 5,  9, 9, 9, "*.DOC"},
   { 3, 1, 5,  9, 9, 9, "*.docx"},
   { 3, 1, 5,  9, 9, 9, "*.DOCX"},
   { 3, 1, 5,  9, 9, 9, "*.html"},
   { 3, 1, 5,  9, 9, 9, "*.HTML"},
   { 3, 1, 5,  9, 9, 9, "*.htm"},
   { 3, 1, 5,  9, 9, 9, "*.HTM"},
   
   { 0, 0, 0,  0, 0, 0, NULL}
};

// for directories
const Cmatch dmatch[] =
{
   { 5, 3, 0,  9, 9, 9, "Desktop"},
   { 5, 3, 0,  9, 9, 9, "Desktop-*"},
   { 4, 1, 5,  9, 9, 9, "public_html"},
   { 0, 0, 0,  0, 0, 0, NULL}
};

// for exectuable files
const Cmatch xmatch[] =
{
   { 5, 2, 0,  9, 9, 9, "*.sh"},
   { 5, 2, 0,  9, 9, 9, "*.bin"},
   { 5, 2, 0,  9, 9, 9, "*.BIN"},

   { 4, 0, 4,  9, 9, 9, "*.exe"},
   { 4, 0, 4,  9, 9, 9, "*.EXE"},
   
   { 0, 0, 0,  0, 0, 0, NULL}
};

static Eina_Bool
printmatch(const char *name, const Cmatch *m)
{
   int i;
   
   for (i = 0; m[i].match; i++)
     {
        if (!fnmatch(m[i].match, name, 0))
          {
             if (m[i].fr <= 5) colorprint(CUBE, FG, m[i].fr, m[i].fg, m[i].fb);
             if (m[i].br <= 5) colorprint(CUBE, BG, m[i].br, m[i].bg, m[i].bb);
             printf("%s", name);
             return EINA_TRUE;
          }
     }
   return EINA_FALSE;
}

static void
fileprint(char *path, char *name, Eina_Bool type)
{
   Eina_Bool isdir = EINA_FALSE;
   Eina_Bool islink = EINA_FALSE;
   Eina_Bool isexec = EINA_FALSE;
   
   if (name || type)
     {
        char *ts;
             
        if (ecore_file_is_dir(path)) isdir = EINA_TRUE;
        ts = ecore_file_readlink(path);
        if (ts)
          {
             islink = EINA_TRUE;
             free(ts);
          }
        if (ecore_file_can_exec(path)) isexec = EINA_TRUE;
     }
   if (name)
     {
        if (isdir)
          {
             if (!printmatch(name, dmatch))
               {
                  colorprint(CUBE, FG, 1, 3, 5);
                  printf("%s", name);
               }
          }
        else if (isexec)
          {
             if (!printmatch(name, xmatch))
               {
                  colorprint(CUBE, FG, 5, 1, 5);
                  printf("%s", name);
               }
          }
        else
          {
             if (!printmatch(name, fmatch))
               {
                  printf("%s", name);
               }
          }
     }
   if (type)
     {
        if (islink)
          {
             colorprint(CUBE, FG, 3, 1, 5);
             printf("@");
          }
        else if (isdir)
          {
             colorprint(CUBE, FG, 3, 4, 5);
             printf("/");
          }
        else if (isexec)
          {
             colorprint(CUBE, FG, 5, 1, 5);
             printf("*");
          }
        else
          printf(" ");
     }
   colorprint(RESET, 0, 0, 0, 0);
}

static void
list_dir(const char *dir, int mode)
{
   Eina_List *files, *l;
   char *s, **names;
   int maxlen = 0, cols, c, rows, i, j, k, num, cw, stuff;
   
   files = ecore_file_ls(dir);
   if (!files) return;
   names = calloc(eina_list_count(files) * 2, sizeof(char *));
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
   else if (mode == MEDIUM) stuff += 4;
   stuff += 5; // xxxx[ /K/M/G/T/P...]
   stuff += 1; // spacer at start
   // name
   stuff += 1; // type [@/*/|/=...]
   stuff += 1; // spacer
   maxlen += stuff;
   if (maxlen > 0)
     {
        cols = tw / maxlen;
        if (cols < 1) cols = 1;
        if (cols == 1)
          {
             maxlen--;
             stuff--;
          }
        if (cols > num) cols = num;
        rows = ((num + (cols - 1)) / cols);
        for (i = 0; i < rows; i++)
          {
             if (mode == SMALL)
               {
                  for (c = 0; c < cols; c++)
                    {
                       char buf[4096], sz[6], szch = ' ';
                       long long size;
                       
                       s = names[(c * rows) + i];
                       if (!s) continue;
                       snprintf(buf, sizeof(buf), "%s/%s", dir, s);
                       int len = eina_unicode_utf8_get_len(s);
                       cw = tw / cols;
                       size = ecore_file_size(buf);
                       size_print(sz, sizeof(sz), &szch, size);
                       len += stuff;
                       printf("%c}it#%i;%i;%s%c", 0x1b, 2, 1, buf, 0);
                       printf("%c}ib%c", 0x1b, 0);
                       printf("##");
                       printf("%c}ie%c", 0x1b, 0);
                       sizeprint(sz, szch);
                       printf(" ");
                       fileprint(buf, s, EINA_TRUE);
                       for (j = 0; j < (cw - len); j++) printf(" ");
                    }
                  printf("\n");
               }
             else if (mode == MEDIUM)
               {
                  for (c = 0; c < cols; c++)
                    {
                       char buf[4096];
                       
                       s = names[(c * rows) + i];
                       if (!s) continue;
                       int len = eina_unicode_utf8_get_len(s);
                       snprintf(buf, sizeof(buf), "%s/%s", dir, s);
                       cw = tw / cols;
                       len += 4;
                       if (cols > 1) len += 1;
                       printf("%c}it%c%i;%i;%s%c", 0x1b, 33 + c, 4, 2, buf, 0);
                       printf("%c}ib%c", 0x1b, 0);
                       printf("%c%c%c%c", 33 + c, 33 + c, 33 + c, 33 + c);
                       printf("%c}ie%c", 0x1b, 0);
                       fileprint(buf, s, EINA_FALSE);
                       if (c < (cols - 1))
                         {
                            for (j = 0; j < (cw - len); j++) printf(" ");
                         }
                    }
                  printf("\n");
                  for (c = 0; c < cols; c++)
                    {
                       char buf[4096], sz[6], szch = ' ';
                       long long size;
                       int len;
                       
                       s = names[(c * rows) + i];
                       if (!s) continue;
                       snprintf(buf, sizeof(buf), "%s/%s", dir, s);
                       cw = tw / cols;
                       size = ecore_file_size(buf);
                       size_print(sz, sizeof(sz), &szch, size);
                       len = eina_unicode_utf8_get_len(sz) + 2 + 4;
                       if (cols > 1) len += 1;
                       printf("%c}ib%c", 0x1b, 0);
                       printf("%c%c%c%c", 33 + c, 33 + c, 33 + c, 33 + c);
                       printf("%c}ie%c", 0x1b, 0);
                       sizeprint(sz, szch);
                       printf(" ");
                       fileprint(buf, NULL, EINA_TRUE);
                       if (c < (cols - 1))
                         {
                            for (j = 0; j < (cw - len); j++) printf(" ");
                         }
                    }
                  printf("\n");
               }
          }
     }
   free(names);
   EINA_LIST_FREE(files, s) free(s);
}

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
        fflush(stdout);
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
