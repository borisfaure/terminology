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
   .bg = CS(32, 32, 32), /* #202020 */
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

#define CS_SET_INVISIBLE(_c)  do {                         \
        evas_object_textgrid_palette_set(                  \
           textgrid, EVAS_TEXTGRID_PALETTE_STANDARD, _c,   \
           0, 0, 0, 0);                                    \
} while(0)
#define CS_SET(_c, _F) do {                                \
        r = cs->_F.r;                                      \
        g = cs->_F.g;                                      \
        b = cs->_F.b;                                      \
        a = cs->_F.a;                                      \
        evas_object_textgrid_palette_set(                  \
           textgrid, EVAS_TEXTGRID_PALETTE_STANDARD, _c,   \
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

#define CS_SET(_c, _F) do {                                \
        r = cs->_F.r;                                      \
        g = cs->_F.g;                                      \
        b = cs->_F.b;                                      \
        a = cs->_F.a;                                      \
        evas_object_textgrid_palette_set(                  \
           textgrid, EVAS_TEXTGRID_PALETTE_EXTENDED, _c,   \
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

#define CS_SET(_K, _F) do {\
   if (edje_object_color_class_set(edje, _K,                            \
                               cs->_F.r, cs->_F.g, cs->_F.b, cs->_F.a,  \
                               cs->_F.r, cs->_F.g, cs->_F.b, cs->_F.a,  \
                               cs->_F.r, cs->_F.g, cs->_F.b, cs->_F.a)  \
       != EINA_TRUE)                                                    \
       {                                                                \
          ERR("error setting color class '%s' on object %p", _K, edje); \
       }                                                                \
} while (0)
#define CS_SET_MANY(_K, _F1, _F2, _F3) do {                                \
   if (edje_object_color_class_set(edje, _K,                               \
                               cs->_F1.r, cs->_F1.g, cs->_F1.b, cs->_F1.a, \
                               cs->_F2.r, cs->_F2.g, cs->_F2.b, cs->_F2.a, \
                               cs->_F3.r, cs->_F3.g, cs->_F3.b, cs->_F3.a) \
       != EINA_TRUE)                                                       \
       {                                                                   \
          ERR("error setting color class '%s' on object %p", _K, edje);    \
       }                                                                   \
} while (0)

   CS_SET("BG", bg);
   CS_SET("FG", normal.def);
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
   CS_SET_MANY("TAB_TITLE", normal.def, tab_title_2, bg);

#define CS_DARK      64,  64,  64, 255
edje_object_color_class_set(edje, "BG_SENDFILE", CS_DARK, CS_DARK, CS_DARK);
#undef CS_DARK

   CS_SET("SHINE", hl);

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

void
config_compute_color_scheme(Config *cfg)
{
   EINA_SAFETY_ON_NULL_RETURN(cfg);

   free((void*)cfg->color_scheme);
   cfg->color_scheme = _color_scheme_get(cfg->color_scheme_name);
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
