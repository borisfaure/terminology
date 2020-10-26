#include "private.h"
#include <assert.h>
#include <Elementary.h>
#include "config.h"
#include "colors.h"

#define COLORSCHEMES_FILENAME "colorschemes.eet"
#define COLORSCHEMES_VERSION  1

#define CS(R,G,B) {.r = R, .g = G, .b = B, .a = 255}
static Color_Scheme default_colorscheme =
{
   .version = COLORSCHEMES_VERSION,
   .md = {
        .version = 1,
        .name = "Default",
        .author = gettext_noop("Terminology's developers"),
        .website = "https://www.enlightenment.org/about-terminology",
        .license = "BSD-2-Clause",
   },
   .def = CS(170, 170, 170), /* #aaaaaa */
   .bg = CS(32, 32, 32), /* #202020 */
   .fg = CS(170, 170, 170), /* #aaaaaa */
   .main = CS(53, 153, 255), /* #3599ff */
   .hl = CS(255,255,255), /* #ffffff */
   .end_sel = CS(255,51,0), /* #ff3300 */
   .tab_missed_1 = CS(255,153,51), /* #ff9933 */
   .tab_missed_2 = CS(255,51,0), /* #ff3300 */
   .tab_missed_3 = CS(255,0,0), /* #ff0000 */
   .tab_missed_over_1 = CS(255,255,64), /* #ffff40 */
   .tab_missed_over_2 = CS(255,153,51), /* #ff9933 */
   .tab_missed_over_3 = CS(255,0,0), /* #ff0000 */
   .tab_title_2 = CS(0,0,0), /* #000000 */
   .ansi[0]  = CS(  0,   0,   0), /* #000000 */
   .ansi[1]  = CS(204,  51,  51), /* #cc3333 */
   .ansi[2]  = CS( 51, 204,  51), /* #33cc33 */
   .ansi[3]  = CS(204, 136,  51), /* #cc8833 */
   .ansi[4]  = CS( 51,  51, 204), /* #3333cc */
   .ansi[5]  = CS(204,  51, 204), /* #cc33cc */
   .ansi[6]  = CS( 51, 204, 204), /* #33cccc */
   .ansi[7]  = CS(204, 204, 204), /* #cccccc */
   .ansi[8]  = CS(102, 102, 102), /* #666666 */
   .ansi[9]  = CS(255, 102, 102), /* #ff6666 */
   .ansi[10] = CS(102, 255, 102), /* #66ff66 */
   .ansi[11] = CS(255, 255, 102), /* #ffff66 */
   .ansi[12] = CS(102, 102, 255), /* #6666ff */
   .ansi[13] = CS(255, 102, 255), /* #ff66ff */
   .ansi[14] = CS(102, 255, 255), /* #66ffff */
   .ansi[15] = CS(255, 255, 255), /* #ffffff */
};
#undef CS

static const Color default_colors[2][2][12] =
{
   { // normal
        { // normal
             { 0xaa, 0xaa, 0xaa, 0xff }, // COL_DEF
             { 0x00, 0x00, 0x00, 0xff }, // COL_BLACK
             { 0xc0, 0x00, 0x00, 0xff }, // COL_RED
             { 0x00, 0xc0, 0x00, 0xff }, // COL_GREEN
             { 0xc0, 0xc0, 0x00, 0xff }, // COL_YELLOW
             { 0x00, 0x00, 0xc0, 0xff }, // COL_BLUE
             { 0xc0, 0x00, 0xc0, 0xff }, // COL_MAGENTA
             { 0x00, 0xc0, 0xc0, 0xff }, // COL_CYAN
             { 0xc0, 0xc0, 0xc0, 0xff }, // COL_WHITE
             { 0x00, 0x00, 0x00, 0x00 }, // COL_INVIS
             { 0x22, 0x22, 0x22, 0xff }, // COL_INVERSE
             { 0xaa, 0xaa, 0xaa, 0xff }, // COL_INVERSEBG
        },
        { // bright/bold
             { 0xee, 0xee, 0xee, 0xff }, // COL_DEF
             { 0xcc, 0xcc, 0xcc, 0xff }, // COL_BLACK
             { 0xcc, 0x88, 0x88, 0xff }, // COL_RED
             { 0x88, 0xcc, 0x88, 0xff }, // COL_GREEN
             { 0xcc, 0xaa, 0x88, 0xff }, // COL_YELLOW
             { 0x88, 0x88, 0xcc, 0xff }, // COL_BLUE
             { 0xcc, 0x88, 0xcc, 0xff }, // COL_MAGENTA
             { 0x88, 0xcc, 0xcc, 0xff }, // COL_CYAN
             { 0xcc, 0xcc, 0xcc, 0xff }, // COL_WHITE
             { 0x00, 0x00, 0x00, 0x00 }, // COL_INVIS
             { 0x11, 0x11, 0x11, 0xff }, // COL_INVERSE
             { 0xee, 0xee, 0xee, 0xff }, // COL_INVERSEBG
        },
   },
   { // intense
        { // normal
             { 0xdd, 0xdd, 0xdd, 0xff }, // COL_DEF
             { 0x80, 0x80, 0x80, 0xff }, // COL_BLACK
             { 0xff, 0x80, 0x80, 0xff }, // COL_RED
             { 0x80, 0xff, 0x80, 0xff }, // COL_GREEN
             { 0xff, 0xff, 0x80, 0xff }, // COL_YELLOW
             { 0x80, 0x80, 0xff, 0xff }, // COL_BLUE
             { 0xff, 0x80, 0xff, 0xff }, // COL_MAGENTA
             { 0x80, 0xff, 0xff, 0xff }, // COL_CYAN
             { 0xff, 0xff, 0xff, 0xff }, // COL_WHITE
             { 0x00, 0x00, 0x00, 0x00 }, // COL_INVIS
             { 0x11, 0x11, 0x11, 0xff }, // COL_INVERSE
             { 0xcc, 0xcc, 0xcc, 0xff }, // COL_INVERSEBG
        },
        { // bright/bold
             { 0xff, 0xff, 0xff, 0xff }, // COL_DEF
             { 0xcc, 0xcc, 0xcc, 0xff }, // COL_BLACK
             { 0xff, 0xcc, 0xcc, 0xff }, // COL_RED
             { 0xcc, 0xff, 0xcc, 0xff }, // COL_GREEN
             { 0xff, 0xff, 0xcc, 0xff }, // COL_YELLOW
             { 0xcc, 0xcc, 0xff, 0xff }, // COL_BLUE
             { 0xff, 0xcc, 0xff, 0xff }, // COL_MAGENTA
             { 0xcc, 0xff, 0xff, 0xff }, // COL_CYAN
             { 0xff, 0xff, 0xff, 0xff }, // COL_WHITE
             { 0x00, 0x00, 0x00, 0x00 }, // COL_INVIS
             { 0x00, 0x00, 0x00, 0xff }, // COL_INVERSE
             { 0xff, 0xff, 0xff, 0xff }, // COL_INVERSEBG
        }
   }
};

static const Color default_colors256[256] =
{
   // basic 16 repeated
   { 0x00, 0x00, 0x00, 0xff }, // COL_BLACK
   { 0xc0, 0x00, 0x00, 0xff }, // COL_RED
   { 0x00, 0xc0, 0x00, 0xff }, // COL_GREEN
   { 0xc0, 0xc0, 0x00, 0xff }, // COL_YELLOW
   { 0x00, 0x00, 0xc0, 0xff }, // COL_BLUE
   { 0xc0, 0x00, 0xc0, 0xff }, // COL_MAGENTA
   { 0x00, 0xc0, 0xc0, 0xff }, // COL_CYAN
   { 0xc0, 0xc0, 0xc0, 0xff }, // COL_WHITE

   { 0x80, 0x80, 0x80, 0xff }, // COL_BLACK
   { 0xff, 0x80, 0x80, 0xff }, // COL_RED
   { 0x80, 0xff, 0x80, 0xff }, // COL_GREEN
   { 0xff, 0xff, 0x80, 0xff }, // COL_YELLOW
   { 0x80, 0x80, 0xff, 0xff }, // COL_BLUE
   { 0xff, 0x80, 0xff, 0xff }, // COL_MAGENTA
   { 0x80, 0xff, 0xff, 0xff }, // COL_CYAN
   { 0xff, 0xff, 0xff, 0xff }, // COL_WHITE

   // pure 6x6x6 colorcube
   { 0x00, 0x00, 0x00, 0xff },
   { 0x00, 0x00, 0x5f, 0xff },
   { 0x00, 0x00, 0x87, 0xff },
   { 0x00, 0x00, 0xaf, 0xff },
   { 0x00, 0x00, 0xd7, 0xff },
   { 0x00, 0x00, 0xff, 0xff },

   { 0x00, 0x5f, 0x00, 0xff },
   { 0x00, 0x5f, 0x5f, 0xff },
   { 0x00, 0x5f, 0x87, 0xff },
   { 0x00, 0x5f, 0xaf, 0xff },
   { 0x00, 0x5f, 0xd7, 0xff },
   { 0x00, 0x5f, 0xff, 0xff },

   { 0x00, 0x87, 0x00, 0xff },
   { 0x00, 0x87, 0x5f, 0xff },
   { 0x00, 0x87, 0x87, 0xff },
   { 0x00, 0x87, 0xaf, 0xff },
   { 0x00, 0x87, 0xd7, 0xff },
   { 0x00, 0x87, 0xff, 0xff },

   { 0x00, 0xaf, 0x00, 0xff },
   { 0x00, 0xaf, 0x5f, 0xff },
   { 0x00, 0xaf, 0x87, 0xff },
   { 0x00, 0xaf, 0xaf, 0xff },
   { 0x00, 0xaf, 0xd7, 0xff },
   { 0x00, 0xaf, 0xff, 0xff },

   { 0x00, 0xd7, 0x00, 0xff },
   { 0x00, 0xd7, 0x5f, 0xff },
   { 0x00, 0xd7, 0x87, 0xff },
   { 0x00, 0xd7, 0xaf, 0xff },
   { 0x00, 0xd7, 0xd7, 0xff },
   { 0x00, 0xd7, 0xff, 0xff },

   { 0x00, 0xff, 0x00, 0xff },
   { 0x00, 0xff, 0x5f, 0xff },
   { 0x00, 0xff, 0x87, 0xff },
   { 0x00, 0xff, 0xaf, 0xff },
   { 0x00, 0xff, 0xd7, 0xff },
   { 0x00, 0xff, 0xff, 0xff },

   { 0x5f, 0x00, 0x00, 0xff },
   { 0x5f, 0x00, 0x5f, 0xff },
   { 0x5f, 0x00, 0x87, 0xff },
   { 0x5f, 0x00, 0xaf, 0xff },
   { 0x5f, 0x00, 0xd7, 0xff },
   { 0x5f, 0x00, 0xff, 0xff },

   { 0x5f, 0x5f, 0x00, 0xff },
   { 0x5f, 0x5f, 0x5f, 0xff },
   { 0x5f, 0x5f, 0x87, 0xff },
   { 0x5f, 0x5f, 0xaf, 0xff },
   { 0x5f, 0x5f, 0xd7, 0xff },
   { 0x5f, 0x5f, 0xff, 0xff },

   { 0x5f, 0x87, 0x00, 0xff },
   { 0x5f, 0x87, 0x5f, 0xff },
   { 0x5f, 0x87, 0x87, 0xff },
   { 0x5f, 0x87, 0xaf, 0xff },
   { 0x5f, 0x87, 0xd7, 0xff },
   { 0x5f, 0x87, 0xff, 0xff },

   { 0x5f, 0xaf, 0x00, 0xff },
   { 0x5f, 0xaf, 0x5f, 0xff },
   { 0x5f, 0xaf, 0x87, 0xff },
   { 0x5f, 0xaf, 0xaf, 0xff },
   { 0x5f, 0xaf, 0xd7, 0xff },
   { 0x5f, 0xaf, 0xff, 0xff },

   { 0x5f, 0xd7, 0x00, 0xff },
   { 0x5f, 0xd7, 0x5f, 0xff },
   { 0x5f, 0xd7, 0x87, 0xff },
   { 0x5f, 0xd7, 0xaf, 0xff },
   { 0x5f, 0xd7, 0xd7, 0xff },
   { 0x5f, 0xd7, 0xff, 0xff },

   { 0x5f, 0xff, 0x00, 0xff },
   { 0x5f, 0xff, 0x5f, 0xff },
   { 0x5f, 0xff, 0x87, 0xff },
   { 0x5f, 0xff, 0xaf, 0xff },
   { 0x5f, 0xff, 0xd7, 0xff },
   { 0x5f, 0xff, 0xff, 0xff },

   { 0x87, 0x00, 0x00, 0xff },
   { 0x87, 0x00, 0x5f, 0xff },
   { 0x87, 0x00, 0x87, 0xff },
   { 0x87, 0x00, 0xaf, 0xff },
   { 0x87, 0x00, 0xd7, 0xff },
   { 0x87, 0x00, 0xff, 0xff },

   { 0x87, 0x5f, 0x00, 0xff },
   { 0x87, 0x5f, 0x5f, 0xff },
   { 0x87, 0x5f, 0x87, 0xff },
   { 0x87, 0x5f, 0xaf, 0xff },
   { 0x87, 0x5f, 0xd7, 0xff },
   { 0x87, 0x5f, 0xff, 0xff },

   { 0x87, 0x87, 0x00, 0xff },
   { 0x87, 0x87, 0x5f, 0xff },
   { 0x87, 0x87, 0x87, 0xff },
   { 0x87, 0x87, 0xaf, 0xff },
   { 0x87, 0x87, 0xd7, 0xff },
   { 0x87, 0x87, 0xff, 0xff },

   { 0x87, 0xaf, 0x00, 0xff },
   { 0x87, 0xaf, 0x5f, 0xff },
   { 0x87, 0xaf, 0x87, 0xff },
   { 0x87, 0xaf, 0xaf, 0xff },
   { 0x87, 0xaf, 0xd7, 0xff },
   { 0x87, 0xaf, 0xff, 0xff },

   { 0x87, 0xd7, 0x00, 0xff },
   { 0x87, 0xd7, 0x5f, 0xff },
   { 0x87, 0xd7, 0x87, 0xff },
   { 0x87, 0xd7, 0xaf, 0xff },
   { 0x87, 0xd7, 0xd7, 0xff },
   { 0x87, 0xd7, 0xff, 0xff },

   { 0x87, 0xff, 0x00, 0xff },
   { 0x87, 0xff, 0x5f, 0xff },
   { 0x87, 0xff, 0x87, 0xff },
   { 0x87, 0xff, 0xaf, 0xff },
   { 0x87, 0xff, 0xd7, 0xff },
   { 0x87, 0xff, 0xff, 0xff },

   { 0xaf, 0x00, 0x00, 0xff },
   { 0xaf, 0x00, 0x5f, 0xff },
   { 0xaf, 0x00, 0x87, 0xff },
   { 0xaf, 0x00, 0xaf, 0xff },
   { 0xaf, 0x00, 0xd7, 0xff },
   { 0xaf, 0x00, 0xff, 0xff },

   { 0xaf, 0x5f, 0x00, 0xff },
   { 0xaf, 0x5f, 0x5f, 0xff },
   { 0xaf, 0x5f, 0x87, 0xff },
   { 0xaf, 0x5f, 0xaf, 0xff },
   { 0xaf, 0x5f, 0xd7, 0xff },
   { 0xaf, 0x5f, 0xff, 0xff },

   { 0xaf, 0x87, 0x00, 0xff },
   { 0xaf, 0x87, 0x5f, 0xff },
   { 0xaf, 0x87, 0x87, 0xff },
   { 0xaf, 0x87, 0xaf, 0xff },
   { 0xaf, 0x87, 0xd7, 0xff },
   { 0xaf, 0x87, 0xff, 0xff },

   { 0xaf, 0xaf, 0x00, 0xff },
   { 0xaf, 0xaf, 0x5f, 0xff },
   { 0xaf, 0xaf, 0x87, 0xff },
   { 0xaf, 0xaf, 0xaf, 0xff },
   { 0xaf, 0xaf, 0xd7, 0xff },
   { 0xaf, 0xaf, 0xff, 0xff },

   { 0xaf, 0xd7, 0x00, 0xff },
   { 0xaf, 0xd7, 0x5f, 0xff },
   { 0xaf, 0xd7, 0x87, 0xff },
   { 0xaf, 0xd7, 0xaf, 0xff },
   { 0xaf, 0xd7, 0xd7, 0xff },
   { 0xaf, 0xd7, 0xff, 0xff },

   { 0xaf, 0xff, 0x00, 0xff },
   { 0xaf, 0xff, 0x5f, 0xff },
   { 0xaf, 0xff, 0x87, 0xff },
   { 0xaf, 0xff, 0xaf, 0xff },
   { 0xaf, 0xff, 0xd7, 0xff },
   { 0xaf, 0xff, 0xff, 0xff },

   { 0xd7, 0x00, 0x00, 0xff },
   { 0xd7, 0x00, 0x5f, 0xff },
   { 0xd7, 0x00, 0x87, 0xff },
   { 0xd7, 0x00, 0xaf, 0xff },
   { 0xd7, 0x00, 0xd7, 0xff },
   { 0xd7, 0x00, 0xff, 0xff },

   { 0xd7, 0x5f, 0x00, 0xff },
   { 0xd7, 0x5f, 0x5f, 0xff },
   { 0xd7, 0x5f, 0x87, 0xff },
   { 0xd7, 0x5f, 0xaf, 0xff },
   { 0xd7, 0x5f, 0xd7, 0xff },
   { 0xd7, 0x5f, 0xff, 0xff },

   { 0xd7, 0x87, 0x00, 0xff },
   { 0xd7, 0x87, 0x5f, 0xff },
   { 0xd7, 0x87, 0x87, 0xff },
   { 0xd7, 0x87, 0xaf, 0xff },
   { 0xd7, 0x87, 0xd7, 0xff },
   { 0xd7, 0x87, 0xff, 0xff },

   { 0xd7, 0xaf, 0x00, 0xff },
   { 0xd7, 0xaf, 0x5f, 0xff },
   { 0xd7, 0xaf, 0x87, 0xff },
   { 0xd7, 0xaf, 0xaf, 0xff },
   { 0xd7, 0xaf, 0xd7, 0xff },
   { 0xd7, 0xaf, 0xff, 0xff },

   { 0xd7, 0xd7, 0x00, 0xff },
   { 0xd7, 0xd7, 0x5f, 0xff },
   { 0xd7, 0xd7, 0x87, 0xff },
   { 0xd7, 0xd7, 0xaf, 0xff },
   { 0xd7, 0xd7, 0xd7, 0xff },
   { 0xd7, 0xd7, 0xff, 0xff },

   { 0xd7, 0xff, 0x00, 0xff },
   { 0xd7, 0xff, 0x5f, 0xff },
   { 0xd7, 0xff, 0x87, 0xff },
   { 0xd7, 0xff, 0xaf, 0xff },
   { 0xd7, 0xff, 0xd7, 0xff },
   { 0xd7, 0xff, 0xff, 0xff },


   { 0xff, 0x00, 0x00, 0xff },
   { 0xff, 0x00, 0x5f, 0xff },
   { 0xff, 0x00, 0x87, 0xff },
   { 0xff, 0x00, 0xaf, 0xff },
   { 0xff, 0x00, 0xd7, 0xff },
   { 0xff, 0x00, 0xff, 0xff },

   { 0xff, 0x5f, 0x00, 0xff },
   { 0xff, 0x5f, 0x5f, 0xff },
   { 0xff, 0x5f, 0x87, 0xff },
   { 0xff, 0x5f, 0xaf, 0xff },
   { 0xff, 0x5f, 0xd7, 0xff },
   { 0xff, 0x5f, 0xff, 0xff },

   { 0xff, 0x87, 0x00, 0xff },
   { 0xff, 0x87, 0x5f, 0xff },
   { 0xff, 0x87, 0x87, 0xff },
   { 0xff, 0x87, 0xaf, 0xff },
   { 0xff, 0x87, 0xd7, 0xff },
   { 0xff, 0x87, 0xff, 0xff },

   { 0xff, 0xaf, 0x00, 0xff },
   { 0xff, 0xaf, 0x5f, 0xff },
   { 0xff, 0xaf, 0x87, 0xff },
   { 0xff, 0xaf, 0xaf, 0xff },
   { 0xff, 0xaf, 0xd7, 0xff },
   { 0xff, 0xaf, 0xff, 0xff },

   { 0xff, 0xd7, 0x00, 0xff },
   { 0xff, 0xd7, 0x5f, 0xff },
   { 0xff, 0xd7, 0x87, 0xff },
   { 0xff, 0xd7, 0xaf, 0xff },
   { 0xff, 0xd7, 0xd7, 0xff },
   { 0xff, 0xd7, 0xff, 0xff },

   { 0xff, 0xff, 0x00, 0xff },
   { 0xff, 0xff, 0x5f, 0xff },
   { 0xff, 0xff, 0x87, 0xff },
   { 0xff, 0xff, 0xaf, 0xff },
   { 0xff, 0xff, 0xd7, 0xff },
   { 0xff, 0xff, 0xff, 0xff },

   // greyscale ramp (24 not including black and white, so 26 if included)
   { 0x08, 0x08, 0x08, 0xff },
   { 0x12, 0x12, 0x12, 0xff },
   { 0x1c, 0x1c, 0x1c, 0xff },
   { 0x26, 0x26, 0x26, 0xff },
   { 0x30, 0x30, 0x30, 0xff },
   { 0x3a, 0x3a, 0x3a, 0xff },
   { 0x44, 0x44, 0x44, 0xff },
   { 0x4e, 0x4e, 0x4e, 0xff },
   { 0x58, 0x58, 0x58, 0xff },
   { 0x62, 0x62, 0x62, 0xff },
   { 0x6c, 0x6c, 0x6c, 0xff },
   { 0x76, 0x76, 0x76, 0xff },
   { 0x80, 0x80, 0x80, 0xff },
   { 0x8a, 0x8a, 0x8a, 0xff },
   { 0x94, 0x94, 0x94, 0xff },
   { 0x9e, 0x9e, 0x9e, 0xff },
   { 0xa8, 0xa8, 0xa8, 0xff },
   { 0xb2, 0xb2, 0xb2, 0xff },
   { 0xbc, 0xbc, 0xbc, 0xff },
   { 0xc6, 0xc6, 0xc6, 0xff },
   { 0xd0, 0xd0, 0xd0, 0xff },
   { 0xda, 0xda, 0xda, 0xff },
   { 0xe4, 0xe4, 0xe4, 0xff },
   { 0xee, 0xee, 0xee, 0xff },
};

static Eet_Data_Descriptor *edd_cs = NULL;
static Eet_Data_Descriptor *edd_color = NULL;

void
colors_term_init(Evas_Object *textgrid,
                 const Evas_Object *bg,
                 const Config *config)
{
   int c;
   char buf[32];
   int r, g , b, a;
   const Color *color;

   for (c = 0; c < (4 * 12); c++)
     {
        int n = c + (24 * (c / 24));
        if (config->colors_use)
          {
             r = config->colors[c].r;
             g = config->colors[c].g;
             b = config->colors[c].b;
             a = config->colors[c].a;
          }
        else
          {
             snprintf(buf, sizeof(buf) - 1, "c%i", c);
             if (!edje_object_color_class_get(bg, buf,
                                              &r,
                                              &g,
                                              &b,
                                              &a,
                                              NULL, NULL, NULL, NULL,
                                              NULL, NULL, NULL, NULL))
               {
                  color = &default_colors[c / 24][(c % 24) / 12][c % 12];
                  r = color->r;
                  g = color->g;
                  b = color->b;
                  a = color->a;
               }
          }
        /* normal */
        evas_object_textgrid_palette_set(
           textgrid, EVAS_TEXTGRID_PALETTE_STANDARD, n,
           r, g, b, a);

        /* faint */
        evas_object_textgrid_palette_set(
           textgrid, EVAS_TEXTGRID_PALETTE_STANDARD, n + 24,
           r / 2, g / 2, b / 2, a / 2);
     }
   for (c = 48; c < 72; c++)
     {
        if (!config->colors_use)
          {
             snprintf(buf, sizeof(buf) - 1, "c%i", c);
             if (edje_object_color_class_get(bg, buf,
                                             &r,
                                             &g,
                                             &b,
                                             &a,
                                             NULL, NULL, NULL, NULL,
                                             NULL, NULL, NULL, NULL))
               {
                   /* faint */
                   evas_object_textgrid_palette_set(
                       textgrid, EVAS_TEXTGRID_PALETTE_STANDARD, c,
                       r, g, b, a);
               }
          }
     }
   for (c = 72; c < 96; c++)
     {
        if (!config->colors_use)
          {
             snprintf(buf, sizeof(buf) - 1, "c%i", c);
             if (edje_object_color_class_get(bg, buf,
                                             &r,
                                             &g,
                                             &b,
                                             &a,
                                             NULL, NULL, NULL, NULL,
                                             NULL, NULL, NULL, NULL))
               {
                   /* faint */
                   evas_object_textgrid_palette_set(
                       textgrid, EVAS_TEXTGRID_PALETTE_STANDARD, c - 48,
                       r, g, b, a);
               }
          }
     }
   for (c = 0; c < 256; c++)
     {
        snprintf(buf, sizeof(buf) - 1, "C%i", c);

        if (!edje_object_color_class_get(bg, buf,
                                         &r,
                                         &g,
                                         &b,
                                         &a,
                                         NULL, NULL, NULL, NULL,
                                         NULL, NULL, NULL, NULL))
          {
             color = &default_colors256[c];
             r = color->r;
             g = color->g;
             b = color->b;
             a = color->a;
          }
        evas_object_textgrid_palette_set(
           textgrid, EVAS_TEXTGRID_PALETTE_EXTENDED, c,
           r, g, b, a);
     }
}

void
colors_standard_get(int set, int col,
                    unsigned char *r,
                    unsigned char *g,
                    unsigned char *b,
                    unsigned char *a)
{
   int s1, s2;
   assert((set >= 0) && (set < 4));

   s1 = set / 2;
   s2 = set % 2;
   *r = default_colors[s1][s2][col].r;
   *g = default_colors[s1][s2][col].g;
   *b = default_colors[s1][s2][col].b;
   *a = default_colors[s1][s2][col].a;
}

void
colors_256_get(int col,
               unsigned char *r,
               unsigned char *g,
               unsigned char *b,
               unsigned char *a)
{
   assert(col < 256);
   *r = default_colors256[col].r;
   *g = default_colors256[col].g;
   *b = default_colors256[col].b;
   *a = default_colors256[col].a;
}

void
color_scheme_apply(Evas_Object *edje,
                   const Color_Scheme *cs)
{
   if (!cs)
     return;

#define CS_SET(_K, _F) do {\
   edje_object_color_class_set(edje, _K, \
                               cs->_F.r, cs->_F.g, cs->_F.b, cs->_F.a, \
                               cs->_F.r, cs->_F.g, cs->_F.b, cs->_F.a, \
                               cs->_F.r, cs->_F.g, cs->_F.b, cs->_F.a); \
} while (0)
#define CS_SET_MANY(_K, _F1, _F2, _F3) do {\
   edje_object_color_class_set(edje, _K, \
                               cs->_F1.r, cs->_F1.g, cs->_F1.b, cs->_F1.a, \
                               cs->_F2.r, cs->_F2.g, cs->_F2.b, cs->_F2.a, \
                               cs->_F3.r, cs->_F3.g, cs->_F3.b, cs->_F3.a); \
} while (0)

   CS_SET("DEF", def);
   CS_SET("BG", bg);
   CS_SET("FG", fg);
   CS_SET("CURSOR", main);
   CS_SET("CURSOR_HIGHLIGHT", hl);
   CS_SET("GLOW", main);
   CS_SET("GLOW_HIGHLIGHT", hl);
   CS_SET("GLOW_TXT", main);
   CS_SET_MANY("GLOW_TXT_HIGHLIGHT", hl, main, main);

   CS_SET("END_SELECTION", end_sel);
   CS_SET_MANY("TAB_MISSED", tab_missed_1, tab_missed_2, tab_missed_3);
   CS_SET_MANY("TAB_MISSED_OVER",
               tab_missed_over_1, tab_missed_over_2, tab_missed_over_3);
   CS_SET_MANY("TAB_TITLE", fg, tab_title_2, bg);

#define CS_DARK      64,  64,  64, 255
edje_object_color_class_set(edje, "BG_SENDFILE", CS_DARK, CS_DARK, CS_DARK);
#undef CS_DARK

   CS_SET("SHINE", hl);

   CS_SET("C0",  ansi[0]);
   CS_SET("C1",  ansi[1]);
   CS_SET("C2",  ansi[2]);
   CS_SET("C3",  ansi[3]);
   CS_SET("C4",  ansi[4]);
   CS_SET("C5",  ansi[5]);
   CS_SET("C6",  ansi[6]);
   CS_SET("C7",  ansi[7]);

   CS_SET("C8",  ansi[8]);
   CS_SET("C9",  ansi[9]);
   CS_SET("C10", ansi[10]);
   CS_SET("C11", ansi[11]);
   CS_SET("C12", ansi[12]);
   CS_SET("C13", ansi[13]);
   CS_SET("C14", ansi[14]);
   CS_SET("C15", ansi[15]);

   CS_SET("c0", def);
   CS_SET("c1", ansi[0]);
   CS_SET("c2", ansi[1]);
   CS_SET("c3", ansi[2]);
   CS_SET("c4", ansi[3]);
   CS_SET("c5", ansi[4]);
   CS_SET("c6", ansi[5]);
   CS_SET("c7", ansi[6]);
   CS_SET("c8", ansi[7]);

   CS_SET("c11", def);

   CS_SET("c12", ansi[15]);
   CS_SET("c13", ansi[8]);
   CS_SET("c14", ansi[9]);
   CS_SET("c15", ansi[10]);
   CS_SET("c16", ansi[11]);
   CS_SET("c17", ansi[12]);
   CS_SET("c18", ansi[13]);
   CS_SET("c19", ansi[14]);
   CS_SET("c20", ansi[15]);

   CS_SET("c25", ansi[8]);
   CS_SET("c26", ansi[9]);
   CS_SET("c27", ansi[10]);
   CS_SET("c28", ansi[11]);
   CS_SET("c29", ansi[12]);
   CS_SET("c30", ansi[13]);
   CS_SET("c31", ansi[14]);
   CS_SET("c32", ansi[15]);

   CS_SET("c37", ansi[8]);
   CS_SET("c38", ansi[9]);
   CS_SET("c39", ansi[10]);
   CS_SET("c40", ansi[11]);
   CS_SET("c41", ansi[12]);
   CS_SET("c42", ansi[13]);
   CS_SET("c43", ansi[14]);
   CS_SET("c44", ansi[15]);

#undef CS_SET
#undef CS_SET_MANY
}

static Color_Scheme *
_color_scheme_get_from_file(const char *path, const char *name)
{
   Eet_File *ef;
   Color_Scheme *cs;

   ef = eet_open(path, EET_FILE_MODE_READ);
   if (!ef)
     return NULL;

   cs = eet_data_read(ef, edd_cs, name);
   eet_close(ef);

   return cs;
}

static Color_Scheme *
color_scheme_get(const char *name)
{
   static char path_user[PATH_MAX] = "";
   static char path_app[PATH_MAX] = "";
   Color_Scheme *cs_user;
   Color_Scheme *cs_app;

   snprintf(path_user, sizeof(path_user) - 1,
            "%s/terminology/" COLORSCHEMES_FILENAME,
            efreet_config_home_get());

   snprintf(path_app, sizeof(path_app) - 1,
            "%s/" COLORSCHEMES_FILENAME,
            elm_app_data_dir_get());

   cs_user = _color_scheme_get_from_file(path_user, name);
   cs_app = _color_scheme_get_from_file(path_app, name);

   if (cs_user && cs_app)
     {
        /* Prefer user file */
        if (cs_user->md.version >= cs_app->md.version)
          return cs_user;
        else
          return cs_app;
     }
   else if (cs_user)
     return cs_user;
   else if (cs_app)
     return cs_app;
   else
     return NULL;
}

static int
color_scheme_cmp(const void *d1, const void *d2)
{
   const Color_Scheme *cs1 = d1;
   const Color_Scheme *cs2 = d2;

   return strcmp(cs1->md.name, cs2->md.name);
}

Eina_List *
color_scheme_list(void)
{
   Eina_List *l = NULL;
   static char path_user[PATH_MAX] = "";
   static char path_app[PATH_MAX] = "";
   Eet_File *ef_app = NULL;
   Eet_File *ef_user = NULL;
   Eina_Iterator *it = NULL;
   Eet_Entry *entry;
   Color_Scheme *cs_user;
   Color_Scheme *cs_app;

   snprintf(path_user, sizeof(path_user) - 1,
            "%s/terminology/" COLORSCHEMES_FILENAME,
            efreet_config_home_get());

   snprintf(path_app, sizeof(path_app) - 1,
            "%s/" COLORSCHEMES_FILENAME,
            elm_app_data_dir_get());

   /* Add default theme */
   cs_app = malloc(sizeof(*cs_app));
   if (!cs_app)
     return NULL;
   memcpy(cs_app, &default_colorscheme, sizeof(*cs_app));
   l = eina_list_sorted_insert(l, color_scheme_cmp, cs_app);

   ef_app = eet_open(path_app, EET_FILE_MODE_READ);
   if (!ef_app)
     {
        ERR("failed to open '%s'", path_app);
        goto end;
     }
   ef_user = eet_open(path_user, EET_FILE_MODE_READ);

   it = eet_list_entries(ef_app);
   if (!it)
     {
        ERR("failed to list entries in '%s'", path_app);
        goto end;
     }
   EINA_ITERATOR_FOREACH(it, entry)
     {
        cs_app = eet_data_read(ef_app, edd_cs, entry->name);
        if (!cs_app)
          {
             ERR("failed to load color scheme '%s' from '%s'",
                 entry->name, path_app);
             continue;
          }
        cs_user = eet_data_read(ef_user, edd_cs, entry->name);
        if (cs_user)
          {
             /* Prefer user file */
             if (cs_user->md.version >= cs_app->md.version)
               {
                  l = eina_list_sorted_insert(l, color_scheme_cmp, cs_user);
                  free(cs_app);
               }
             else
               {
                  l = eina_list_sorted_insert(l, color_scheme_cmp, cs_app);
                  free(cs_user);
               }
          }
        else
          {
             l = eina_list_sorted_insert(l, color_scheme_cmp, cs_app);
          }
     }
end:
   eina_iterator_free(it);
   if (ef_app)
     eet_close(ef_app);
   if (ef_user)
     eet_close(ef_user);

   return l;
}

void
color_scheme_apply_from_config(Evas_Object *edje,
                               const Config *config)
{
   Color_Scheme *cs;

   if (!eina_str_has_suffix(config->theme, "/nord.edj"))
     return;

   /* This should be cached in config */
   cs = color_scheme_get("Nord");
   if (!cs)
     {
        ERR("Could not find color scheme \"%s\"", "Nord");
        return;
     }
   color_scheme_apply(edje, cs);
}

void
colors_init(void)
{
   Eet_Data_Descriptor_Class eddc;

   eet_eina_stream_data_descriptor_class_set
     (&eddc, sizeof(eddc), "Color", sizeof(Color));
   edd_color = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_color, Color, "r", r, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_color, Color, "g", g, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_color, Color, "b", b, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_color, Color, "a", a, EET_T_UCHAR);

   eet_eina_stream_data_descriptor_class_set
     (&eddc, sizeof(eddc), "Color_Scheme", sizeof(Color_Scheme));
   edd_cs = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_cs, Color_Scheme, "version", version, EET_T_INT);

   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_cs, Color_Scheme, "md.version", md.version, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_cs, Color_Scheme, "md.name", md.name, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_cs, Color_Scheme, "md.author", md.author, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_cs, Color_Scheme, "md.website", md.website, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_cs, Color_Scheme, "md.license", md.license, EET_T_STRING);

   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "def", def, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "bg", bg, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "fg", fg, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "main", main, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "hl", hl, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "end_sel", end_sel, edd_color);

   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "tab_missed_1", tab_missed_1, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "tab_missed_2", tab_missed_2, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "tab_missed_3", tab_missed_3, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "tab_missed_over_1", tab_missed_over_1, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "tab_missed_over_2", tab_missed_over_2, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "tab_missed_over_3", tab_missed_over_3, edd_color);

   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "tab_title_2", tab_title_2, edd_color);

   EET_DATA_DESCRIPTOR_ADD_ARRAY
     (edd_cs, Color_Scheme, "ansi", ansi, edd_color);

#if ENABLE_NLS
   default_colorscheme.md.author = gettext(default_colorscheme.md.author);
#endif
}

void
colors_shutdown(void)
{
   eet_data_descriptor_free(edd_cs);
   edd_cs = NULL;

   eet_data_descriptor_free(edd_color);
   edd_color = NULL;
}
