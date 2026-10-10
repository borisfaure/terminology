#include "private.h"
#include <assert.h>
#include <limits.h>
#include <Elementary.h>
#include "config.h"
#include "colors.h"
#include "termpty.h" /* COL_DEF */
#include "tytest.h"

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
   .bg = CS(32, 32, 32), /* #202020 */
   .main = CS(53, 153, 255), /* #3599ff */
   .hl = CS(255,255,255), /* #ffffff */
   .end_sel = CS(53, 153, 255), /* #3599ff */
   .tab_missed_1 = CS(255,153,51), /* #ff9933 */
   .tab_missed_2 = CS(255,51,0), /* #ff3300 */
   .tab_missed_3 = CS(255,0,0), /* #ff0000 */
   .tab_missed_over_1 = CS(255,255,64), /* #ffff40 */
   .tab_missed_over_2 = CS(255,153,51), /* #ff9933 */
   .tab_missed_over_3 = CS(255,0,0), /* #ff0000 */
   .tab_title_2 = CS(0,0,0), /* #000000 */
   .normal = {
        .def        = CS(170, 170, 170), /* #aaaaaa */
        .black      = CS(  0,   0,   0), /* #000000 */
        .red        = CS(204,  51,  51), /* #cc3333 */
        .green      = CS( 51, 204,  51), /* #33cc33 */
        .yellow     = CS(204, 136,  51), /* #cc8833 */
        .blue       = CS( 51,  51, 204), /* #3333cc */
        .magenta    = CS(204,  51, 204), /* #cc33cc */
        .cyan       = CS( 51, 204, 204), /* #33cccc */
        .white      = CS(204, 204, 204), /* #cccccc */
        .inverse_fg = CS( 34,  34,  34), /* #222222 */
        .inverse_bg = CS(170, 170, 170), /* #aaaaaa */
   },
   .bright = {
        .def        = CS(238, 238, 238), /* #eeeeee */
        .black      = CS(102, 102, 102), /* #666666 */
        .red        = CS(255, 102, 102), /* #ff6666 */
        .green      = CS(102, 255, 102), /* #66ff66 */
        .yellow     = CS(255, 255, 102), /* #ffff66 */
        .blue       = CS(102, 102, 255), /* #6666ff */
        .magenta    = CS(255, 102, 255), /* #ff66ff */
        .cyan       = CS(102, 255, 255), /* #66ffff */
        .white      = CS(255, 255, 255), /* #ffffff */
        .inverse_fg = CS( 17,  17,  17), /* #111111 */
        .inverse_bg = CS(238, 238, 238), /* #eeeeee */
   },
   .faint = {
        .def        = CS(135, 135, 135), /* #878787 */
        .black      = CS(  8,   8,   8), /* #080808 */
        .red        = CS(152,   8,   8), /* #980808 */
        .green      = CS(  8, 152,   8), /* #089808 */
        .yellow     = CS(152, 152,   8), /* #989808 */
        .blue       = CS(  8,   8, 152), /* #080898 */
        .magenta    = CS(152,   8, 152), /* #980898 */
        .cyan       = CS(  8, 152, 152), /* #089898 */
        .white      = CS(152, 152, 152), /* #989898 */
        .inverse_fg = CS( 33,  33,  33), /* #212121 */
        .inverse_bg = CS(135, 135, 135), /* #878787 */
   },
   .brightfaint = {
        .def        = CS(186, 186, 186), /* #bababa */
        .black      = CS( 84,  84,  84), /* #545454 */
        .red        = CS(199,  84,  84), /* #c75454 */
        .green      = CS( 84, 199,  84), /* #54c754 */
        .yellow     = CS(199, 199,  84), /* #c7c754 */
        .blue       = CS( 84,  84, 199), /* #5454c7 */
        .magenta    = CS(199,  84, 199), /* #c754c7 */
        .cyan       = CS( 84, 199, 199), /* #54c7c7 */
        .white      = CS(199, 199, 199), /* #c7c7c7 */
        .inverse_fg = CS( 20,  20,  20), /* #141414 */
        .inverse_bg = CS(186, 186, 186), /* #bababa */
   }
};
#undef CS

static const Color default_colors[4][12] =
{
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
     { // faint
          { 0x80, 0x80, 0x80, 0xff }, // COL_DEF
          { 0x08, 0x08, 0x08, 0xff }, // COL_BLACK
          { 0x98, 0x08, 0x08, 0xff }, // COL_RED
          { 0x08, 0x98, 0x08, 0x98 }, // COL_GREEN
          { 0x98, 0x98, 0x08, 0x98 }, // COL_YELLOW
          { 0x08, 0x08, 0x98, 0x98 }, // COL_BLUE
          { 0x98, 0x08, 0x98, 0x98 }, // COL_MAGENTA
          { 0x08, 0x98, 0x98, 0x98 }, // COL_CYAN
          { 0x98, 0x98, 0x98, 0x98 }, // COL_WHITE
          { 0x00, 0x00, 0x00, 0x00 }, // COL_INVIS
          { 0x21, 0x21, 0x21, 0xff }, // COL_INVERSE
          { 0x87, 0x87, 0x87, 0xff }, // COL_INVERSEBG
     },
     { // bright + faint
          { 0xba, 0xba, 0xba, 0xff }, // COL_DEF
          { 0x54, 0x54, 0x54, 0xff }, // COL_BLACK
          { 0xc7, 0x54, 0x54, 0xff }, // COL_RED
          { 0x54, 0xc7, 0x54, 0xc7 }, // COL_GREEN
          { 0xc7, 0xc7, 0x54, 0xc7 }, // COL_YELLOW
          { 0x54, 0x54, 0xc7, 0xc7 }, // COL_BLUE
          { 0xc7, 0x54, 0xc7, 0xc7 }, // COL_MAGENTA
          { 0x54, 0xc7, 0xc7, 0xc7 }, // COL_CYAN
          { 0xc7, 0xc7, 0xc7, 0xc7 }, // COL_WHITE
          { 0x00, 0x00, 0x00, 0x00 }, // COL_INVIS
          { 0x14, 0x14, 0x14, 0xff }, // COL_INVERSE
          { 0xba, 0xba, 0xba, 0xff }, // COL_INVERSEBG
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
static Eet_Data_Descriptor *edd_cb = NULL;
static Eet_Data_Descriptor *edd_color = NULL;

void
colors_term_init(Evas_Object *textgrid,
                 const Color_Scheme *cs)
{
   int c;
   int r, g , b, a;

   if (!cs)
     cs = &default_colorscheme;

#define CS_SET_INVISIBLE(c_)  do {                         \
        evas_object_textgrid_palette_set(                  \
           textgrid, EVAS_TEXTGRID_PALETTE_STANDARD, c_,   \
           0, 0, 0, 0);                                    \
} while(0)
#define CS_SET(c_, F_) do {                                \
        r = cs->F_.r;                                      \
        g = cs->F_.g;                                      \
        b = cs->F_.b;                                      \
        a = cs->F_.a;                                      \
        evas_object_textgrid_palette_set(                  \
           textgrid, EVAS_TEXTGRID_PALETTE_STANDARD, c_,   \
           r, g, b, a);                                    \
} while (0)

   /* Normal */
   CS_SET(0 /* def */,     normal.def);
   CS_SET(1 /* black */,   normal.black);
   CS_SET(2 /* red */,     normal.red);
   CS_SET(3 /* green */,   normal.green);
   CS_SET(4 /* yellow */,  normal.yellow);
   CS_SET(5 /* blue */,    normal.blue);
   CS_SET(6 /* magenta */, normal.magenta);
   CS_SET(7 /* cyan */,    normal.cyan);
   CS_SET(8 /* white */,   normal.white);
   CS_SET_INVISIBLE(9);
   CS_SET(10 /* inverse fg */, normal.inverse_fg);
   CS_SET(11 /* inverse bg */, normal.inverse_bg);

   /* Bold */
   CS_SET(12 /* def */,     bright.def);
   CS_SET(13 /* black */,   bright.black);
   CS_SET(14 /* red */,     bright.red);
   CS_SET(15 /* green */,   bright.green);
   CS_SET(16 /* yellow */,  bright.yellow);
   CS_SET(17 /* blue */,    bright.blue);
   CS_SET(18 /* magenta */, bright.magenta);
   CS_SET(19 /* cyan */,    bright.cyan);
   CS_SET(20 /* white */,   bright.white);
   CS_SET_INVISIBLE(21);
   CS_SET(22 /* inverse fg */, bright.inverse_fg);
   CS_SET(23 /* inverse bg */, bright.inverse_bg);

   /* Faint */
   CS_SET(24 /* def */,     faint.def);
   CS_SET(25 /* black */,   faint.black);
   CS_SET(26 /* red */,     faint.red);
   CS_SET(27 /* green */,   faint.green);
   CS_SET(28 /* yellow */,  faint.yellow);
   CS_SET(29 /* blue */,    faint.blue);
   CS_SET(30 /* magenta */, faint.magenta);
   CS_SET(31 /* cyan */,    faint.cyan);
   CS_SET(32 /* white */,   faint.white);
   CS_SET_INVISIBLE(33);
   CS_SET(34 /* inverse fg */, faint.inverse_fg);
   CS_SET(35 /* inverse bg */, faint.inverse_bg);

   /* BrightFaint */
   CS_SET(36 /* def */,     faint.def);
   CS_SET(37 /* black */,   faint.black);
   CS_SET(38 /* red */,     faint.red);
   CS_SET(39 /* green */,   faint.green);
   CS_SET(40 /* yellow */,  faint.yellow);
   CS_SET(41 /* blue */,    faint.blue);
   CS_SET(42 /* magenta */, faint.magenta);
   CS_SET(43 /* cyan */,    faint.cyan);
   CS_SET(44 /* white */,   faint.white);
   CS_SET_INVISIBLE(45);
   CS_SET(46 /* inverse fg */, faint.inverse_fg);
   CS_SET(47 /* inverse bg */, faint.inverse_bg);
#undef CS_SET

#define CS_SET(c_, F_) do {                                \
        r = cs->F_.r;                                      \
        g = cs->F_.g;                                      \
        b = cs->F_.b;                                      \
        a = cs->F_.a;                                      \
        evas_object_textgrid_palette_set(                  \
           textgrid, EVAS_TEXTGRID_PALETTE_EXTENDED, c_,   \
           r, g, b, a);                                    \
   } while (0)

   CS_SET(0 /* black */,   normal.black);
   CS_SET(1 /* red */,     normal.red);
   CS_SET(2 /* green */,   normal.green);
   CS_SET(3 /* yellow */,  normal.yellow);
   CS_SET(4 /* blue */,    normal.blue);
   CS_SET(5 /* magenta */, normal.magenta);
   CS_SET(6 /* cyan */,    normal.cyan);
   CS_SET(7 /* white */,   normal.white);

   CS_SET(8 /* black */,    bright.black);
   CS_SET(9 /* red */,      bright.red);
   CS_SET(10 /* green */,   bright.green);
   CS_SET(11 /* yellow */,  bright.yellow);
   CS_SET(12 /* blue */,    bright.blue);
   CS_SET(13 /* magenta */, bright.magenta);
   CS_SET(14 /* cyan */,    bright.cyan);
   CS_SET(15 /* white */,   bright.white);

   for (c = 16; c < 256; c++)
     {
        r = default_colors256[c].r;
        g = default_colors256[c].g;
        b = default_colors256[c].b;
        a = default_colors256[c].a;
        evas_object_textgrid_palette_set(
           textgrid, EVAS_TEXTGRID_PALETTE_EXTENDED, c,
           r, g, b, a);
     }
#undef CS_SET
}

void
colors_standard_get(int set, int col,
                    unsigned char *r,
                    unsigned char *g,
                    unsigned char *b,
                    unsigned char *a)
{
   assert((set >= 0) && (set < 4));

   *r = default_colors[set][col].r;
   *g = default_colors[set][col].g;
   *b = default_colors[set][col].b;
   *a = default_colors[set][col].a;
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
   EINA_SAFETY_ON_NULL_RETURN(cs);

#define CS_SET(K_, F_) do {\
   if (edje_object_color_class_set(edje, K_,                            \
                               cs->F_.r, cs->F_.g, cs->F_.b, cs->F_.a,  \
                               cs->F_.r, cs->F_.g, cs->F_.b, cs->F_.a,  \
                               cs->F_.r, cs->F_.g, cs->F_.b, cs->F_.a)  \
       != EINA_TRUE)                                                    \
       {                                                                \
          ERR("error setting color class '%s' on object %p", K_, edje); \
       }                                                                \
} while (0)
#define CS_SET_MANY(K_, F1_, F2_, F3_) do {                                \
   if (edje_object_color_class_set(edje, K_,                               \
                               cs->F1_.r, cs->F1_.g, cs->F1_.b, cs->F1_.a, \
                               cs->F2_.r, cs->F2_.g, cs->F2_.b, cs->F2_.a, \
                               cs->F3_.r, cs->F3_.g, cs->F3_.b, cs->F3_.a) \
       != EINA_TRUE)                                                       \
       {                                                                   \
          ERR("error setting color class '%s' on object %p", K_, edje);    \
       }                                                                   \
} while (0)

   CS_SET("BG", bg);
   CS_SET("FG", normal.def);
   CS_SET("CURSOR", hl);
   CS_SET("GLOW", main);
   CS_SET("HIGHLIGHT", hl);
   CS_SET("GLOW_TXT", main);
   CS_SET_MANY("GLOW_TXT_HIGHLIGHT", hl, main, main);

   CS_SET("END_SELECTION", end_sel);
   CS_SET("/fg/normal/term/selection/arrow/left", end_sel);
   CS_SET("/fg/normal/term/selection/arrow/down", end_sel);
   CS_SET("/fg/normal/term/selection/arrow/up", end_sel);
   CS_SET("/fg/normal/term/selection/arrow/right", end_sel);

   CS_SET_MANY("TAB_MISSED", tab_missed_1, tab_missed_2, tab_missed_3);
   CS_SET_MANY("TAB_MISSED_OVER",
               tab_missed_over_1, tab_missed_over_2, tab_missed_over_3);
   CS_SET_MANY("TAB_TITLE", normal.def, tab_title_2, bg);

#define CS_DARK      64,  64,  64, 255
   if (edje_object_color_class_set(edje, "BG_SENDFILE", CS_DARK, CS_DARK, CS_DARK)
       != EINA_TRUE)
     {
        ERR("error setting color class '%s' on object %p",
              "BG_SENDFILE", edje);
     }
#undef CS_DARK

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
_color_scheme_get(const char *name)
{
   static char path_user[PATH_MAX] = "";
   static char path_app[PATH_MAX] = "";
   Color_Scheme *cs_user;
   Color_Scheme *cs_app;

   snprintf(path_user, sizeof(path_user) - 1,
            "%s/terminology/colorschemes/%s.eet",
            efreet_config_home_get(), name);

   snprintf(path_app, sizeof(path_app) - 1,
            "%s/colorschemes/%s.eet",
            elm_app_data_dir_get(), name);

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
     {
        ERR("failed find colorscheme '%s'", name);
        return NULL;
     }
}

void
config_compute_color_scheme(Config *cfg)
{
   EINA_SAFETY_ON_NULL_RETURN(cfg);

   free((void*)cfg->color_scheme);
   cfg->color_scheme = (cfg->color_scheme_name) ?
      _color_scheme_get(cfg->color_scheme_name) : NULL;
   if (!cfg->color_scheme)
     {
        eina_stringshare_del(cfg->color_scheme_name);
        cfg->color_scheme_name = eina_stringshare_add("Default");
        cfg->color_scheme = color_scheme_dup(&default_colorscheme);
     }
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
   Eina_List *dir = NULL;
   Eina_List *name_list = NULL;
   Eina_List *search_paths = NULL;
   static char buf[PATH_MAX] = "";
   char *file;
   char *sp;
   Eet_File *ef = NULL;
   Eina_Iterator *it = NULL;
   Eet_Entry *entry;
   Color_Scheme *cs;
   const char *current_name;
   Eina_Bool default_found = EINA_FALSE;

   /* Search homedir first, so color classes there get used */
   snprintf(buf, sizeof(buf) - 1,
            "%s/terminology/colorschemes",
            efreet_config_home_get());
   search_paths = eina_list_append(search_paths, eina_stringshare_add(buf));
   snprintf(buf, sizeof(buf) - 1,
            "%s/colorschemes",
            elm_app_data_dir_get());

  search_paths = eina_list_append(search_paths, eina_stringshare_add(buf));

   /* Add default theme */
   cs = malloc(sizeof(*cs));
   if (!cs)
     return NULL;
   memcpy(cs, &default_colorscheme, sizeof(*cs));
   l = eina_list_sorted_insert(l, color_scheme_cmp, cs);

   EINA_LIST_FREE(search_paths, sp)
      {
         dir = ecore_file_ls(sp);

         EINA_LIST_FREE(dir, file)
           {
              snprintf(buf, sizeof(buf), "%s/%s", sp, file);
              if ((!ecore_file_is_dir(buf)) && (ecore_file_size(buf) > 0))
                {
                   if (eina_str_has_extension(file, ".eet"))
                     {
                        ef = eet_open(buf, EET_FILE_MODE_READ);
                        if (!ef)
                           {
                              ERR("failed to open '%s'", buf);
                              continue;
                           }
                        it = eet_list_entries(ef);
                        if (!it)
                           {
                              ERR("failed to list entries in '%s'", buf);
                              continue;
                           }
                        EINA_ITERATOR_FOREACH(it, entry)
                           {
                              /* If we already have a cs with this name skip it */
                              current_name = eina_stringshare_add(entry->name);
                              if (eina_list_data_find_list(name_list, current_name))
                                 {
                                    WRN("Skipping loading '%s' from '%s' color scheme exists already",
                                       entry->name, buf);
                                    eina_stringshare_del(current_name);
                                    continue;
                                 }
                              cs = eet_data_read(ef, edd_cs, entry->name);
                              if (!cs)
                                {
                                   ERR("failed to load color scheme '%s' from '%s'",
                                       entry->name, buf);
                                   eina_stringshare_del(current_name);
                                   continue;
                                }
                              l = eina_list_sorted_insert(l, color_scheme_cmp, cs);
                              name_list = eina_list_append(name_list, current_name);
                              if (strcmp(current_name, "Default") == 0)
                                default_found = EINA_TRUE;
                           }
                        eet_close(ef);
                     }
                }
              free(file);
           }
     }
   /* Make sure default theme is there */
   if (!default_found)
     name_list = eina_list_prepend(name_list, eina_stringshare_add("Default"));

   eina_iterator_free(it);

   return l;
}

Color_Scheme *
color_scheme_dup(const Color_Scheme *src)
{
   Color_Scheme *cs;
   if (!src) src = &default_colorscheme;
   size_t len_name = strlen(src->md.name) + 1;
   size_t len_author = strlen(src->md.author) + 1;
   size_t len_website = strlen(src->md.website) + 1;
   size_t len_license = strlen(src->md.license) + 1;
   size_t len = sizeof(*cs) + len_name + len_author + len_website
      + len_license;
   char *s;

   cs = malloc(len);
   if (!cs)
     return NULL;
   memcpy(cs, src, sizeof(*cs));
   s = ((char*)cs) + sizeof(*cs);

   cs->md.name = s;
   memcpy(s, src->md.name, len_name);
   s += len_name;

   cs->md.author = s;
   memcpy(s, src->md.author, len_author);
   s += len_author;

   cs->md.website = s;
   memcpy(s, src->md.website, len_website);
   s += len_website;

   cs->md.license = s;
   memcpy(s, src->md.license, len_license);

   return cs;
}

void
color_scheme_apply_from_config(Evas_Object *edje,
                               const Config *config)
{
   const Color_Scheme *cs;

   EINA_SAFETY_ON_NULL_RETURN(config);
   EINA_SAFETY_ON_NULL_RETURN(edje);

   cs = config->color_scheme;
   if (!cs)
     {
        ERR("Could not find color scheme \"%s\"", config->color_scheme_name);
        return;
     }
   color_scheme_apply(edje, cs);
}

void
colors_init(void)
{
   Eet_Data_Descriptor_Class eddc_color;
   Eet_Data_Descriptor_Class eddc_color_block;
   Eet_Data_Descriptor_Class eddc_color_scheme;

   eet_eina_stream_data_descriptor_class_set
     (&eddc_color, sizeof(eddc_color), "Color", sizeof(Color));
   edd_color = eet_data_descriptor_stream_new(&eddc_color);

   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_color, Color, "r", r, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_color, Color, "g", g, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_color, Color, "b", b, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC
     (edd_color, Color, "a", a, EET_T_UCHAR);


   eet_eina_stream_data_descriptor_class_set
     (&eddc_color_block, sizeof(eddc_color_block), "Color_Block", sizeof(Color_Block));
   edd_cb = eet_data_descriptor_stream_new(&eddc_color_block);

   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cb, Color_Block, "def", def, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cb, Color_Block, "black", black, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cb, Color_Block, "red", red, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cb, Color_Block, "green", green, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cb, Color_Block, "yellow", yellow, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cb, Color_Block, "blue", blue, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cb, Color_Block, "magenta", magenta, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cb, Color_Block, "cyan", cyan, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cb, Color_Block, "white", white, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cb, Color_Block, "inverse_fg", inverse_fg, edd_color);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cb, Color_Block, "inverse_bg", inverse_bg, edd_color);


   eet_eina_stream_data_descriptor_class_set
     (&eddc_color_scheme, sizeof(eddc_color_scheme), "Color_Scheme", sizeof(Color_Scheme));
   edd_cs = eet_data_descriptor_stream_new(&eddc_color_scheme);

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
      (edd_cs, Color_Scheme, "bg", bg, edd_color);
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

   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "Normal", normal, edd_cb);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "Bright", bright, edd_cb);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "Faint", faint, edd_cb);
   EET_DATA_DESCRIPTOR_ADD_SUB_NESTED
      (edd_cs, Color_Scheme, "BrightFaint", brightfaint, edd_cb);


#if ENABLE_NLS
   default_colorscheme.md.author = gettext(default_colorscheme.md.author);
#endif
}

void
colors_shutdown(void)
{
   eet_data_descriptor_free(edd_cs);
   edd_cs = NULL;

   eet_data_descriptor_free(edd_cb);
   edd_cb = NULL;

   eet_data_descriptor_free(edd_color);
   edd_color = NULL;
}

#if !defined(BINARY_TYFUZZ)
/*********************************
 * cache true color approximations
 *********************************
 * Approximating true colors is costly since it needs to compare 256 colors.
 * The following considers that a few colors are used a lot.  For example, one
 * can consider that a text editor with syntax highlighting would only use a
 * small palette.
 * Given that, using the cache needs to be efficient: it needs to speed up the
 * approximation when the color is already in the cache but not knowing the
 * color requested is not in the cache needs to be efficient too.
 *
 * The cache is an array of @TCC_LEN uint32_t.
 * Each entry has on the MSB the color as 3 uint8_t (R,G,B) and the LSB is
 * the approximated color.
 * Searching a color is simply going through the array and testing the mask on
 * the 3 most significant bytes.
 * When a color is found, it is bubbled up towards the start of the array by
 * swapping this entry with the one above.  This way the array is ordered to
 * get most used colors as fast as possible.
 * When a color is not found, the slow process of comparing it with the 256
 * colors is used.  Then the result is inserted in the lower half of the
 * array.  This ensures that new entries can live a bit and not be removed by
 * the next new color.  Using a modulo with a prime number makes for a
 * nice-enough random generator to figure out where to insert.
 */
#define TCC_LEN 32
#define TCC_PRIME 17 /* smallest prime number larger than @TCC_LEN/2 */
static struct {
     uint32_t colors[TCC_LEN];
} _truecolor_cache;
static int _tcc_random_pos = 0;

static Eina_Bool
_tcc_find(const uint32_t color_msb, uint8_t *chosen_color)
{
   int i;
   for (i = 0; i < TCC_LEN; i++)
     {
        if ((_truecolor_cache.colors[i] & 0xffffff00) == color_msb)
          {
             *chosen_color = _truecolor_cache.colors[i] & 0xff;
             /* bubble up this result */
             if (i > 0)
               {
                  uint32_t prev = _truecolor_cache.colors[i - 1];
                  _truecolor_cache.colors[i - 1] = _truecolor_cache.colors[i];
                  _truecolor_cache.colors[i] = prev;
               }
             return EINA_TRUE;
          }
     }
   return EINA_FALSE;
}

static void
_tcc_insert(const uint32_t color_msb, const uint8_t approximated)
{
   uint32_t c = color_msb | approximated;
   int i;

   _tcc_random_pos = ((_tcc_random_pos + TCC_PRIME) % (TCC_LEN / 2));
   i = (TCC_LEN / 2) + _tcc_random_pos;

  _truecolor_cache.colors[i] = c;
}
#endif

uint8_t
colors_rgb_to_palette(Evas_Object *textgrid, uint8_t r0, uint8_t g0, uint8_t b0)
{
   uint8_t chosen_color = COL_DEF;
#if defined(BINARY_TYFUZZ)
   (void) textgrid;
   (void) r0;
   (void) g0;
   (void) b0;
#else
   int c;
   int distance_min = INT_MAX;
   const uint32_t color_msb = 0
      | (((uint32_t)r0) << 24)
      | (((uint32_t)g0) << 16)
      | (((uint32_t)b0) << 8);

   if (_tcc_find(color_msb, &chosen_color))
     return chosen_color;

   for (c = 0; c < 256; c++)
     {
        int r1 = 0, g1 = 0, b1 = 0, a1 = 0;
        int delta_red_sq, delta_green_sq, delta_blue_sq, red_mean;
        int distance;

        evas_object_textgrid_palette_get(textgrid,
                                         EVAS_TEXTGRID_PALETTE_EXTENDED,
                                         c, &r1, &g1, &b1, &a1);
        /* Compute the color distance
         * XXX: this is inacurate but should give good enough results.
         * See https://en.wikipedia.org/wiki/Color_difference
         */
        red_mean = (r0 + r1) / 2;
        delta_red_sq = (r0 - r1) * (r0 - r1);
        delta_green_sq = (g0 - g1) * (g0 - g1);
        delta_blue_sq = (b0 - b1) * (b0 - b1);

#if 1
        distance = 2 * delta_red_sq
           + 4 * delta_green_sq
           + 3 * delta_blue_sq
           + ((red_mean) * (delta_red_sq - delta_blue_sq) / 256);
#else
        /* from https://www.compuphase.com/cmetric.htm */
        distance = (((512 + red_mean) * delta_red_sq) >> 8)
                 + 4 * delta_green_sq
                 + (((767 - red_mean) * delta_blue_sq) >> 8);
        /* euclidian distance */
        distance = delta_red_sq + delta_green_sq + delta_blue_sq;
        (void)red_mean;
#endif
        if (distance < distance_min)
          {
             distance_min = distance;
             chosen_color = c;
          }
     }
   _tcc_insert(color_msb, chosen_color);
#endif
   return chosen_color;
}

#if defined(BINARY_TYTEST)
int
tytest_rgb_to_palette(void)
{
   /* Indices recorded from the pre-move approximation with the default
    * colour scheme, so the move is proven to change nothing. */
   static const struct {
      uint8_t r, g, b, idx;
   } table[] = {
      {   0,   0,   0,   0 },
      { 255, 255, 255,  15 },
      { 255,   0,   0, 196 },
      {   0, 255,   0,  46 },
      {   0,   0, 255,  21 },
      {   1,   2,   3,   0 },
      { 128, 128, 128,   8 },
      { 127, 127, 127,   8 },
      {  95, 135, 175,  67 },
      { 215, 135,  95, 173 },
      {   0,  95,   0,  22 },
      { 250, 240, 230, 255 },
      {  59,  34,  76, 237 },
      {  12,  34, 200,   4 },
      { 200, 100,  50, 167 },
      { 100, 200,  50,  77 },
      {  50, 100, 200,  62 },
      { 222, 222, 222, 253 },
      {  16,  16,  16, 233 },
      { 238, 238, 238, 255 },
      {  48,  48,  48, 236 },
      { 114, 114, 114, 243 },
      { 135, 135, 255, 105 },
      { 255, 135,   0, 208 },
      {  40,  40,  40, 235 },
      {  88,  88,  88, 240 },
      { 188, 188, 188, 250 },
      {   3, 200, 150,  42 },
      { 180,  60, 130, 132 },
      {  90,  90,  90, 240 },
      { 192, 168, 100, 143 },
      {  66,  33, 222,  56 },
   };
   size_t i;

   for (i = 0; i < sizeof(table) / sizeof(table[0]); i++)
     {
        uint8_t idx = colors_rgb_to_palette(NULL, table[i].r, table[i].g,
                                            table[i].b);
        assert(idx == table[i].idx);
     }
   /* Repeating one entry exercises the cache: the answer must not depend
    * on which path (cache hit or full scan) produced it. */
   assert(colors_rgb_to_palette(NULL, 215, 135, 95) == 173);
   assert(colors_rgb_to_palette(NULL, 215, 135, 95) == 173);

   return 0;
}
#endif
