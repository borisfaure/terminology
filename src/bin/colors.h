#ifndef _COLORS_H__
#define _COLORS_H__ 1

#include <Evas.h>
#include "config.h"

typedef struct _Color_Scheme Color_Scheme;

struct _Color_Scheme
{
   int version;
   struct {
        int         version;
        const char *name;
        const char *author;
        const char *website;
        const char *license;
   } md;

   Color def;
   Color bg;
   Color fg;
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

   Color ansi[16];
};


void
colors_term_init(Evas_Object *textgrid,
                 const Evas_Object *bg,
                 const Config *config);
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


void
color_scheme_apply_from_config(Evas_Object *edje,
                               const Config *config);

Eina_List *
color_scheme_list(void);

void
colors_init(void);

void
colors_shutdown(void);


#endif
