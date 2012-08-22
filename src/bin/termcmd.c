#include "private.h"

#include <Elementary.h>
#include "main.h"
#include "win.h"
#include "termio.h"
#include "config.h"
#include "controls.h"
#include "media.h"
#include "utils.h"
#include "termcmd.h"

// called as u type
Eina_Bool
termcmd_watch(Evas_Object *obj, Evas_Object *win, Evas_Object *bg, const char *cmd)
{
   if (!cmd) return EINA_FALSE;
   if ((cmd[0] == '/') || (cmd[0] == 's')) // search
     {
        if (cmd[1] == 0) // clear search
          {
             printf("search clear\n");
             return EINA_TRUE;
          }
        printf("search '%s'\n", cmd + 1);
        return EINA_TRUE;
     }
   return EINA_FALSE;
   obj = win = bg = NULL;
}

// called when you hit enter
Eina_Bool
termcmd_do(Evas_Object *obj, Evas_Object *win, Evas_Object *bg, const char *cmd)
{
   if (!cmd) return EINA_FALSE;
   if ((cmd[0] == '/') || (cmd[0] == 's')) // search
     {
        if (cmd[1] == 0) // clear search
          {
             printf("search clear\n");
             return EINA_TRUE;
          }
        printf("search '%s'\n", cmd + 1);
        return EINA_TRUE;
     }
   if ((cmd[0] == 'f') || (cmd[0] == 'F')) // font size
     {
        Config *config = termio_config_get(obj);
        
        if (config)
          {
             if (cmd[1] == 0) // back to default
               {
                  config->font.bitmap = config->font.orig_bitmap;
                  if (config->font.orig_name)
                    {
                       eina_stringshare_del(config->font.name);
                       config->font.name = eina_stringshare_add(config->font.orig_name);
                    }
                  termio_font_size_set(obj, config->font.orig_size);
                  return EINA_TRUE;
               }
             else if (cmd[1] == 'b') // big font size
               {
                  if (config->font.orig_bitmap)
                    {
                       config->font.bitmap = 1;
                       eina_stringshare_del(config->font.name);
                       config->font.name = eina_stringshare_add("10x20.pcf");
                       termio_font_size_set(obj, 20);
                    }
                  else
                    {
                       termio_font_size_set(obj, 20);
                    }
               }
             else if (cmd[1] == '+') // size up
               {
                  termio_font_size_set(obj, config->font.size + 1);
               }
             else if (cmd[1] == '-') // size down
               {
                  termio_font_size_set(obj, config->font.size - 1);
               }
          }
        return EINA_TRUE;
     }
   if ((cmd[0] == 'g') || (cmd[0] == 'G')) // grid size
     {
        int w = -1, h = -1;
        int r = sscanf(cmd, "g%ix%i", &w, &h);

        if (r == 1)
          {
             switch (w)
               {
                case 0:
                   w = 80;
                   h = 24;
                   break;
                case 1:
                   w = 80;
                   h = 40;
                   break;
                case 2:
                   w = 80;
                   h = 60;
                   break;
                case 3:
                   w = 80;
                   h = 80;
                   break;
                case 4:
                   w = 120;
                   h = 24;
                   break;
                case 5:
                   w = 120;
                   h = 40;
                   break;
                case 6:
                   w = 120;
                   h = 60;
                   break;
                case 7:
                   w = 120;
                   h = 80;
                   break;
                case 8:
                   w = 120;
                   h = 120;
                   break;
               }
          }
        if ((w > 0) && (h > 0))
          termio_grid_size_set(obj, w, h);
        return EINA_TRUE;
     }
   return EINA_FALSE;
   obj = win = bg = NULL;
}

