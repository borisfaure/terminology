#ifndef TERMINOLOGY_COLORS_H_
#define TERMINOLOGY_COLORS_H_ 1

#include <Evas.h>
#include "config.h"


struct tag_Color_Block
{
   Color def;
   Color black;
   Color red;
   Color green;
   Color yellow;
   Color blue;
   Color magenta;
   Color cyan;
   Color white;
   Color inverse_fg;
   Color inverse_bg;
   uint32_t _padding;
};

struct tag_Color_Scheme
{
   int version;
   struct {
        int         version;
        const char *name;
        const char *author;
        const char *website;
        const char *license;
   } md;

   Color bg;
   Color main;
   Color hl;
   Color end_sel;

   Color tab_missed_1;
   Color tab_missed_2;
   Color tab_missed_3;
   Color tab_missed_over_1;
   Color tab_missed_over_2;
   Color tab_missed_over_3;

   Color tab_title_2;

   Color_Block normal;
   Color_Block bright;
   Color_Block faint;
   Color_Block brightfaint;
};

void
colors_term_init(Evas_Object *textgrid,
                 const Color_Scheme *cs);

/* write @idx's colour into the textgrid, and its faint slots for 0-15 */
void
colors_palette_color_set(Evas_Object *textgrid,
                         const Color_Scheme *cs,
                         int idx,
                         const Color *c);

struct tag_Termpty;

/* closest extended-palette entry for an RGB, cached per @ty */
uint8_t
colors_rgb_to_palette(Evas_Object *textgrid,
                      const struct tag_Termpty *ty,
                      uint8_t r, uint8_t g, uint8_t b);
void
colors_standard_get(int set,
                    int col,
                    unsigned char *r,
                    unsigned char *g,
                    unsigned char *b,
                    unsigned char *a);

void
colors_256_get(int col,
               unsigned char *r,
               unsigned char *g,
               unsigned char *b,
               unsigned char *a);

Eina_Bool
colors_is_dark(int r, int g, int b);


void
color_scheme_apply_from_config(Evas_Object *edje,
                               const Config *config);
void
color_scheme_apply(Evas_Object *edje,
                   const Color_Scheme *cs);

Eina_List *
color_scheme_list(void);

Color_Scheme *
color_scheme_dup(const Color_Scheme *src);

void
config_compute_color_scheme(Config *cfg);

void
colors_init(void);

void
colors_shutdown(void);


#endif
