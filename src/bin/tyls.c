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
int tw = 0, th = 0;

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
   const char *match, *icon;
} Cmatch;

// for regular files
const Cmatch fmatch[] =
{
   { 5, 5, 5,  9, 9, 9, "*.jpg", NULL},
   { 5, 5, 5,  9, 9, 9, "*.JPG", NULL},
   { 5, 5, 5,  9, 9, 9, "*.jpeg", NULL},
   { 5, 5, 5,  9, 9, 9, "*.JPEG", NULL},
   { 5, 5, 5,  9, 9, 9, "*.jfif", NULL},
   { 5, 5, 5,  9, 9, 9, "*.JFIF", NULL},
   { 5, 5, 5,  9, 9, 9, "*.jfi", NULL},
   { 5, 5, 5,  9, 9, 9, "*.JFI", NULL},
   { 5, 5, 5,  9, 9, 9, "*.png", NULL},
   { 5, 5, 5,  9, 9, 9, "*.PNG", NULL},
   { 5, 5, 5,  9, 9, 9, "*.bmp", NULL},
   { 5, 5, 5,  9, 9, 9, "*.BMP", NULL},
   { 5, 5, 5,  9, 9, 9, "*.gif", NULL},
   { 5, 5, 5,  9, 9, 9, "*.GIF", NULL},
   { 5, 5, 5,  9, 9, 9, "*.xcf", NULL},
   { 5, 5, 5,  9, 9, 9, "*.XCF", NULL},
   { 5, 5, 5,  9, 9, 9, "*.xcf.gz", NULL},
   { 5, 5, 5,  9, 9, 9, "*.XCF.gz", NULL},
   { 5, 5, 5,  9, 9, 9, "*.svg", NULL},
   { 5, 5, 5,  9, 9, 9, "*.SVG", NULL},
   { 5, 5, 5,  9, 9, 9, "*.svgz", NULL},
   { 5, 5, 5,  9, 9, 9, "*.SVGZ", NULL},
   { 5, 5, 5,  9, 9, 9, "*.svg.gz", NULL},
   { 5, 5, 5,  9, 9, 9, "*.SVG.GZ", NULL},
   { 5, 5, 5,  9, 9, 9, "*.ppm", NULL},
   { 5, 5, 5,  9, 9, 9, "*.PPM", NULL},
   { 5, 5, 5,  9, 9, 9, "*.tif", NULL},
   { 5, 5, 5,  9, 9, 9, "*.TIF", NULL},
   { 5, 5, 5,  9, 9, 9, "*.tiff", NULL},
   { 5, 5, 5,  9, 9, 9, "*.TIFF", NULL},
   { 5, 5, 5,  9, 9, 9, "*.ico", NULL},
   { 5, 5, 5,  9, 9, 9, "*.ICO", NULL},
   { 5, 5, 5,  9, 9, 9, "*.pgm", NULL},
   { 5, 5, 5,  9, 9, 9, "*.PGM", NULL},
   { 5, 5, 5,  9, 9, 9, "*.pbm", NULL},
   { 5, 5, 5,  9, 9, 9, "*.PBM", NULL},
   { 5, 5, 5,  9, 9, 9, "*.psd", NULL},
   { 5, 5, 5,  9, 9, 9, "*.PSD", NULL},
   { 5, 5, 5,  9, 9, 9, "*.wbmp", NULL},
   { 5, 5, 5,  9, 9, 9, "*.WBMP", NULL},
   { 5, 5, 5,  9, 9, 9, "*.cur", NULL},
   { 5, 5, 5,  9, 9, 9, "*.CUR", NULL},
   { 5, 5, 5,  9, 9, 9, "*.arw", NULL},
   { 5, 5, 5,  9, 9, 9, "*.ARW", NULL},
   { 5, 5, 5,  9, 9, 9, "*.webp", NULL},
   { 5, 5, 5,  9, 9, 9, "*.WEBP", NULL},
   { 5, 5, 5,  9, 9, 9, "*.cr2", NULL},
   { 5, 5, 5,  9, 9, 9, "*.CR2", NULL},
   { 5, 5, 5,  9, 9, 9, "*.crw", NULL},
   { 5, 5, 5,  9, 9, 9, "*.CRW", NULL},
   { 5, 5, 5,  9, 9, 9, "*.dcr", NULL},
   { 5, 5, 5,  9, 9, 9, "*.DCR", NULL},
   { 5, 5, 5,  9, 9, 9, "*.dng", NULL},
   { 5, 5, 5,  9, 9, 9, "*.DNG", NULL},
   { 5, 5, 5,  9, 9, 9, "*.k25", NULL},
   { 5, 5, 5,  9, 9, 9, "*.K25", NULL},
   { 5, 5, 5,  9, 9, 9, "*.kdc", NULL},
   { 5, 5, 5,  9, 9, 9, "*.KDC", NULL},
   { 5, 5, 5,  9, 9, 9, "*.thm", NULL},
   { 5, 5, 5,  9, 9, 9, "*.THM", NULL},
   { 5, 5, 5,  9, 9, 9, "*.erf", NULL},
   { 5, 5, 5,  9, 9, 9, "*.ERF", NULL},
   { 5, 5, 5,  9, 9, 9, "*.mrw", NULL},
   { 5, 5, 5,  9, 9, 9, "*.MRW", NULL},
   { 5, 5, 5,  9, 9, 9, "*.nef", NULL},
   { 5, 5, 5,  9, 9, 9, "*.NEF", NULL},
   { 5, 5, 5,  9, 9, 9, "*.nrf", NULL},
   { 5, 5, 5,  9, 9, 9, "*.NRF", NULL},
   { 5, 5, 5,  9, 9, 9, "*.nrw", NULL},
   { 5, 5, 5,  9, 9, 9, "*.NRW", NULL},
   { 5, 5, 5,  9, 9, 9, "*.orf", NULL},
   { 5, 5, 5,  9, 9, 9, "*.ORF", NULL},
   { 5, 5, 5,  9, 9, 9, "*.raw", NULL},
   { 5, 5, 5,  9, 9, 9, "*.RAW", NULL},
   { 5, 5, 5,  9, 9, 9, "*.rw2", NULL},
   { 5, 5, 5,  9, 9, 9, "*.RW2", NULL},
   { 5, 5, 5,  9, 9, 9, "*.pef", NULL},
   { 5, 5, 5,  9, 9, 9, "*.PEF", NULL},
   { 5, 5, 5,  9, 9, 9, "*.raf", NULL},
   { 5, 5, 5,  9, 9, 9, "*.RAF", NULL},
   { 5, 5, 5,  9, 9, 9, "*.sr2", NULL},
   { 5, 5, 5,  9, 9, 9, "*.SR2", NULL},
   { 5, 5, 5,  9, 9, 9, "*.srf", NULL},
   { 5, 5, 5,  9, 9, 9, "*.SRF", NULL},
   { 5, 5, 5,  9, 9, 9, "*.x3f", NULL},
   { 5, 5, 5,  9, 9, 9, "*.X3F", NULL},
   
   { 5, 5, 2,  9, 9, 9, "*.pdf", NULL},
   { 5, 5, 2,  9, 9, 9, "*.PDF", NULL},
   { 5, 5, 2,  9, 9, 9, "*.pdf", NULL},
   { 5, 5, 2,  9, 9, 9, "*.ps", NULL},
   { 5, 5, 2,  9, 9, 9, "*.PS", NULL},
   { 5, 5, 2,  9, 9, 9, "*.ps.gz", NULL},
   { 5, 5, 2,  9, 9, 9, "*.PS.GZ", NULL},

   { 3, 4, 5,  9, 9, 9, "*.asf", NULL},
   { 3, 4, 5,  9, 9, 9, "*.avi", NULL},
   { 3, 4, 5,  9, 9, 9, "*.bdm", NULL},
   { 3, 4, 5,  9, 9, 9, "*.bdmv", NULL},
   { 3, 4, 5,  9, 9, 9, "*.clpi", NULL},
   { 3, 4, 5,  9, 9, 9, "*.cpi", NULL},
   { 3, 4, 5,  9, 9, 9, "*.dv", NULL},
   { 3, 4, 5,  9, 9, 9, "*.fla", NULL},
   { 3, 4, 5,  9, 9, 9, "*.flv", NULL},
   { 3, 4, 5,  9, 9, 9, "*.m1v", NULL},
   { 3, 4, 5,  9, 9, 9, "*.m2t", NULL},
   { 3, 4, 5,  9, 9, 9, "*.m4v", NULL},
   { 3, 4, 5,  9, 9, 9, "*.mkv", NULL},
   { 3, 4, 5,  9, 9, 9, "*.mov", NULL},
   { 3, 4, 5,  9, 9, 9, "*.mp2", NULL},
   { 3, 4, 5,  9, 9, 9, "*.mp2ts", NULL},
   { 3, 4, 5,  9, 9, 9, "*.mp4", NULL},
   { 3, 4, 5,  9, 9, 9, "*.mpe", NULL},
   { 3, 4, 5,  9, 9, 9, "*.mpeg", NULL},
   { 3, 4, 5,  9, 9, 9, "*.mpg", NULL},
   { 3, 4, 5,  9, 9, 9, "*.mpl", NULL},
   { 3, 4, 5,  9, 9, 9, "*.mpls", NULL},
   { 3, 4, 5,  9, 9, 9, "*.mts", NULL},
   { 3, 4, 5,  9, 9, 9, "*.mxf", NULL},
   { 3, 4, 5,  9, 9, 9, "*.nut", NULL},
   { 3, 4, 5,  9, 9, 9, "*.nuv", NULL},
   { 3, 4, 5,  9, 9, 9, "*.ogg", NULL},
   { 3, 4, 5,  9, 9, 9, "*.ogv", NULL},
   { 3, 4, 5,  9, 9, 9, "*.ogm", NULL},
   { 3, 4, 5,  9, 9, 9, "*.qt", NULL},
   { 3, 4, 5,  9, 9, 9, "*.rm", NULL},
   { 3, 4, 5,  9, 9, 9, "*.rmj", NULL},
   { 3, 4, 5,  9, 9, 9, "*.rmm", NULL},
   { 3, 4, 5,  9, 9, 9, "*.rms", NULL},
   { 3, 4, 5,  9, 9, 9, "*.rmvb", NULL},
   { 3, 4, 5,  9, 9, 9, "*.rmx", NULL},
   { 3, 4, 5,  9, 9, 9, "*.rv", NULL},
   { 3, 4, 5,  9, 9, 9, "*.swf", NULL},
   { 3, 4, 5,  9, 9, 9, "*.ts", NULL},
   { 3, 4, 5,  9, 9, 9, "*.weba", NULL},
   { 3, 4, 5,  9, 9, 9, "*.webm", NULL},
   { 3, 4, 5,  9, 9, 9, "*.wmv", NULL},
   { 3, 4, 5,  9, 9, 9, "*.3g2", NULL},
   { 3, 4, 5,  9, 9, 9, "*.3gp2", NULL},
   { 3, 4, 5,  9, 9, 9, "*.3gpp", NULL},
   { 3, 4, 5,  9, 9, 9, "*.3gpp2", NULL},
   { 3, 4, 5,  9, 9, 9, "*.3p2", NULL},
   { 3, 4, 5,  9, 9, 9, "*.264", NULL},
   { 3, 4, 5,  9, 9, 9, "*.3gp", NULL},
   
   { 2, 3, 5,  9, 9, 9, "*.mp3", "audio-x-mpeg.svg"},
   { 2, 3, 5,  9, 9, 9, "*.MP3", "audio-x-mpeg.svg"},
   { 2, 3, 5,  9, 9, 9, "*.aac", "audio-x-generic.svg"},
   { 2, 3, 5,  9, 9, 9, "*.AAC", "audio-x-generic.svg"},
   { 2, 3, 5,  9, 9, 9, "*.wav", "audio-x-wav.svg"},
   { 2, 3, 5,  9, 9, 9, "*.WAV", "audio-x-wav.svg"},
   { 2, 3, 5,  9, 9, 9, "*.m3u", "audio-x-mp3-playlist"},
   { 2, 3, 5,  9, 9, 9, "*.M3U", "audio-x-mp3-playlist"},
   
   { 3, 5, 2,  9, 9, 9, "*.patch", "text-x-generic"},
   { 3, 5, 2,  9, 9, 9, "*.PATCH", "text-x-generic"},
   { 3, 5, 2,  9, 9, 9, "*.diff", "text-x-generic"},
   { 3, 5, 2,  9, 9, 9, "*.DIFF", "text-x-generic"},

   { 5, 3, 0,  9, 9, 9, "*.rpm", "package-x-generic"},
   { 5, 3, 0,  9, 9, 9, "*.RPM", "package-x-generic"},
   { 5, 3, 0,  9, 9, 9, "*.srpm", "package-x-generic"},
   { 5, 3, 0,  9, 9, 9, "*.SRPM", "package-x-generic"},
   { 5, 3, 0,  9, 9, 9, "*.deb", "package-x-generic"},
   { 5, 3, 0,  9, 9, 9, "*.DEB", "package-x-generic"},
   { 5, 3, 0,  9, 9, 9, "*.pkg.tar.xz", "package-x-generic"},
   { 5, 3, 0,  9, 9, 9, "*.PKG.TAR.XZ", "package-x-generic"},

   { 5, 1, 1,  9, 9, 9, "*.tar.*", "application-x-tar"},
   { 5, 1, 1,  9, 9, 9, "*.TAR.*", "application-x-tar"},
   { 5, 1, 1,  9, 9, 9, "*.tar", "application-x-tar"},
   { 5, 1, 1,  9, 9, 9, "*.TAR", "application-x-tar"},
   { 5, 1, 1,  9, 9, 9, "*.tgz", "application-x-tar"},
   { 5, 1, 1,  9, 9, 9, "*.TGZ", "application-x-tar"},
   { 5, 1, 1,  9, 9, 9, "*.tbz", "application-x-tar"},
   { 5, 1, 1,  9, 9, 9, "*.TBZ", "application-x-tar"},
   { 5, 1, 1,  9, 9, 9, "*.zip", "application-x-zip"},
   { 5, 1, 1,  9, 9, 9, "*.ZIP", "application-x-zip"},
   { 5, 1, 1,  9, 9, 9, "*.rar", "text-x-generic"},
   { 5, 1, 1,  9, 9, 9, "*.RAR", "text-x-generic"},
   { 5, 1, 1,  9, 9, 9, "*.cpio", "text-x-generic"},
   { 5, 1, 1,  9, 9, 9, "*.CPIO", "text-x-generic"},

   { 0, 5, 2,  9, 9, 9, "*.iso", "application-x-cd-image"},
   { 0, 5, 2,  9, 9, 9, "*.ISO", "application-x-cd-image"},
   { 0, 5, 2,  9, 9, 9, "*.img", "text-x-generic"},
   { 0, 5, 2,  9, 9, 9, "*.IMG", "text-x-generic"},
   
   { 3, 5, 1,  9, 9, 9, "*.ttf", "font-x-generic"},
   { 3, 5, 1,  9, 9, 9, "*.TTF", "font-x-generic"},
   { 3, 5, 1,  9, 9, 9, "*.bdf", "font-x-generic"},
   { 3, 5, 1,  9, 9, 9, "*.BDF", "font-x-generic"},
   { 3, 5, 1,  9, 9, 9, "*.pcf", "font-x-generic"},
   { 3, 5, 1,  9, 9, 9, "*.PCF", "font-x-generic"},

   { 1, 1, 1,  9, 9, 9, "*~", "text-x-generic"},
   
   { 1, 2, 2,  9, 9, 9, "stamp-h1", "text-x-generic"},

   { 5, 3, 1,  9, 9, 9, "Makefile", "text-x-generic"},
   { 4, 2, 0,  9, 9, 9, "Makefile.in", "text-x-generic"},
   { 3, 1, 0,  9, 9, 9, "*.am", "text-x-generic"},

   { 4, 2, 5,  9, 9, 9, "*.spec", "text-x-generic"},
   
   { 4, 1, 5,  9, 9, 9, "*.m4", "application-x-m4"},
   
   { 5, 2, 0,  9, 9, 9, "*.sh", "text-x-generic"},
   { 5, 2, 0,  9, 9, 9, "*.SH", "text-x-generic"},
   { 5, 2, 0,  9, 9, 9, "*.bin", "text-x-generic"},
   { 5, 2, 0,  9, 9, 9, "*.BIN", "text-x-generic"},
   { 5, 2, 0,  9, 9, 9, "*.run", "text-x-generic"},
   { 5, 2, 0,  9, 9, 9, "*.RUN", "text-x-generic"},

   { 5, 4, 0,  9, 9, 9, "configure", "text-x-generic"},
   { 5, 3, 0,  9, 9, 9, "configure.in", "text-x-generic"},
   { 5, 2, 0,  9, 9, 9, "configure.ac", "text-x-generic"},

   { 2, 2, 3,  9, 9, 9, "*.in", "text-x-generic"},
   
   { 0, 5, 5,  9, 9, 9, "*.c", "text-x-c"},
   { 0, 5, 5,  9, 9, 9, "*.C", "text-x-c"},
   { 1, 4, 5,  9, 9, 9, "*.x", "text-x-c"},
   { 1, 4, 5,  9, 9, 9, "*.X", "text-x-c"},
   { 1, 3, 5,  9, 9, 9, "*.h", "text-x-chdr"},
   { 1, 3, 5,  9, 9, 9, "*.H", "text-x-chdr"},

   { 5, 5, 2,  9, 9, 9, "*.edc", "text-x-generic"},
   { 5, 5, 2,  9, 9, 9, "*.EDC", "text-x-generic"},

   { 5, 3, 0,  9, 9, 9, "*.edj", "text-x-generic"},
   { 5, 3, 0,  9, 9, 9, "*.EDJ", "text-x-generic"},
   
   { 5, 0, 5,  9, 9, 9, "*.cc", "text-x-c++"},
   { 5, 0, 5,  9, 9, 9, "*.CC", "text-x-c++"},
   { 3, 1, 5,  9, 9, 9, "*.hh", "text-x-c++hdr"},
   { 3, 1, 5,  9, 9, 9, "*.HH", "text-x-c++hdr"},

   { 5, 5, 2,  9, 9, 9, "*.php", "application-x-php"},
   { 5, 5, 2,  9, 9, 9, "*.PHP", "application-x-php"},

   { 2, 0, 5,  9, 9, 9, "*.desktop", "application-x-desktop"},
   { 2, 0, 5,  9, 9, 9, "*.directory", "application-x-desktop"},
   
   { 0, 1, 3,  9, 9, 9, "*.o", "text-x-generic"},
   { 0, 1, 3,  9, 9, 9, "*.O", "text-x-generic"},
   { 0, 1, 3,  9, 9, 9, "*.lo", "text-x-generic"},
   { 0, 1, 3,  9, 9, 9, "*.la", "text-x-generic"},

   { 5, 1, 3,  9, 9, 9, "*.log", "text-x-generic"},
   { 5, 1, 3,  9, 9, 9, "*.LOG", "text-x-generic"},

   { 3, 1, 5,  9, 9, 9, "*.txt", "text-x-generic"},
   { 3, 1, 5,  9, 9, 9, "*.TXT", "text-x-generic"},
   { 3, 1, 5,  9, 9, 9, "*.xml", "text-xml"},
   { 3, 1, 5,  9, 9, 9, "*.XML", "text-xml"},
   { 3, 1, 5,  9, 9, 9, "README", "text-x-generic"},
   { 3, 1, 5,  9, 9, 9, "Readme", "text-x-generic"},
   { 3, 1, 5,  9, 9, 9, "readme", "text-x-generic"},
   { 3, 1, 5,  9, 9, 9, "INSTALL", "text-x-generic"},
   { 3, 1, 5,  9, 9, 9, "COPYING", "text-x-generic"},
   { 3, 1, 5,  9, 9, 9, "NEWS", "text-x-generic"},
   { 3, 1, 5,  9, 9, 9, "ChangeLog", "text-x-changelog"},
   { 3, 1, 5,  9, 9, 9, "AUTHORS", "text-x-generic"},
   { 3, 1, 5,  9, 9, 9, "TODO", "text-x-generic"},
   { 3, 1, 5,  9, 9, 9, "*.doc", "x-office-document"},
   { 3, 1, 5,  9, 9, 9, "*.DOC", "x-office-document"},
   { 3, 1, 5,  9, 9, 9, "*.docx", "x-office-document"},
   { 3, 1, 5,  9, 9, 9, "*.DOCX", "x-office-document"},
   { 3, 1, 5,  9, 9, 9, "*.html", "text-x-html"},
   { 3, 1, 5,  9, 9, 9, "*.HTML", "text-x-html"},
   { 3, 1, 5,  9, 9, 9, "*.htm", "text-x-html"},
   { 3, 1, 5,  9, 9, 9, "*.HTM", "text-x-html"},
   { 3, 1, 5,  9, 9, 9, "*.css", "text-x-css"},
   { 3, 1, 5,  9, 9, 9, "*.CSS", "text-x-css"},
   
   { 0, 0, 0,  0, 0, 0, NULL, NULL}
};

// for directories
const Cmatch dmatch[] =
{
   { 5, 3, 0,  9, 9, 9, "Desktop", "user-desktop"},
   { 5, 3, 0,  9, 9, 9, "Desktop-*", "user-desktop"},
   { 4, 1, 5,  9, 9, 9, "public_html", "folder"},
   { 1, 3, 5,  9, 9, 9, "*", "folder"},
   { 0, 0, 0,  0, 0, 0, NULL, NULL}
};

// for exectuable files
const Cmatch xmatch[] =
{
   { 5, 2, 0,  9, 9, 9, "*.sh", "text-x-script"},
   { 5, 2, 0,  9, 9, 9, "*.bin", "application-x-executable"},
   { 5, 2, 0,  9, 9, 9, "*.BIN", "application-x-executable"},

   { 4, 0, 4,  9, 9, 9, "*.exe", "application-x-executable"},
   { 4, 0, 4,  9, 9, 9, "*.EXE", "application-x-executable"},

   { 5, 0, 5,  9, 9, 9, "*", "application-x-executable"},
   
   { 0, 0, 0,  0, 0, 0, NULL, NULL}
};

static const char *
filematch(const char *name, const Cmatch *m)
{
   int i;
   
   for (i = 0; m[i].match; i++)
     {
        if (!fnmatch(m[i].match, name, 0))
          return m[i].icon;
     }
   return NULL;
}

static const char *
fileicon(const char *path)
{
   Eina_Bool isdir = EINA_FALSE;
   Eina_Bool isexec = EINA_FALSE;
   const char *name;
   
   name = ecore_file_file_get(path);
   if (name)
     {
        char *ts;
             
        if (ecore_file_is_dir(path)) isdir = EINA_TRUE;
        ts = ecore_file_readlink(path);
        if (ts)
          {
             free(ts);
          }
        if (ecore_file_can_exec(path)) isexec = EINA_TRUE;
        
        if (isdir) return filematch(name, dmatch);
        else if (isexec) return filematch(name, xmatch);
        else return filematch(name, fmatch);
     }
   return NULL;
}

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
   int maxlen = 0, cols, c, rows, i, j, num, cw, stuff;
   
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
        if (cols == 0) cols = 1;
        rows = ((num + (cols - 1)) / cols);
        for (i = 0; i < rows; i++)
          {
             char buf[4096];
             const char *icon;
             
             if (mode == SMALL)
               {
                  for (c = 0; c < cols; c++)
                    {
                       char sz[6], szch = ' ';
                       long long size;
                       
                       s = names[(c * rows) + i];
                       if (!s) continue;
                       snprintf(buf, sizeof(buf), "%s/%s", dir, s);
                       int len = eina_unicode_utf8_get_len(s);
                       icon = fileicon(buf);
                       cw = tw / cols;
                       size = ecore_file_size(buf);
                       size_print(sz, sizeof(sz), &szch, size);
                       len += stuff;
                       if (icon)
                         printf("%c}it#%i;%i;%s\n%s%c", 0x1b, 2, 1, buf, icon, 0);
                       else
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
                       s = names[(c * rows) + i];
                       if (!s) continue;
                       int len = eina_unicode_utf8_get_len(s);
                       snprintf(buf, sizeof(buf), "%s/%s", dir, s);
                       icon = fileicon(buf);
                       cw = tw / cols;
                       len += 3;
                       if (cols > 1) len += 1;
                       if (icon)
                         printf("%c}it%c%i;%i;%s\n%s%c", 0x1b, 33 + c, 4, 2, buf, icon, 0);
                       else
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
                       char sz[6], szch = ' ';
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
   Eina_Bool listed = EINA_FALSE;
   
   if (!getenv("TERMINOLOGY")) return 0;
   if ((argc == 2) && (!strcmp(argv[1], "-h")))
     {
        printf("Usage: %s [-s|-m] FILE1 [FILE2 ...]\n"
               "\n"
               "  -s  Small list mode\n"
               "  -m  Medium list mode\n",
               /*"  -l  Large list mode\n", Enable again once we support it */
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
        int i, cw, ch, mode = SMALL;
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
                  if (i >= argc) break;
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
