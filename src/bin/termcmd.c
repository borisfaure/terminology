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

static Eina_Bool
_termcmd_search(Evas_Object *obj __UNUSED__, Evas_Object *win __UNUSED__, Evas_Object *bg __UNUSED__, const char *cmd)
{
   if (cmd[0] == 0) // clear search
     {
        printf("search clear\n");
        return EINA_TRUE;
     }
   printf("search '%s'\n", cmd);
   return EINA_TRUE;
}

static Eina_Bool
_termcmd_font_size(Evas_Object *obj, Evas_Object *win __UNUSED__, Evas_Object *bg __UNUSED__, const char *cmd)
{
   Config *config = termio_config_get(obj);

   if (config)
     {
        if (cmd[0] == 0) // back to default
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
        else if (cmd[0] == 'b') // big font size
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
        else if (cmd[0] == '+') // size up
          {
             termio_font_size_set(obj, config->font.size + 1);
          }
        else if (cmd[0] == '-') // size down
          {
             termio_font_size_set(obj, config->font.size - 1);
          }
        else
          ERR("Unknown font command: %s", cmd);
     }
   return EINA_TRUE;
}

static Eina_Bool
_termcmd_grid_size(Evas_Object *obj, Evas_Object *win __UNUSED__, Evas_Object *bg __UNUSED__, const char *cmd)
{
   int w = -1, h = -1;
   int r = sscanf(cmd, "%ix%i", &w, &h);

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
   else
     ERR("Unknown grid size command: %s", cmd);

   return EINA_TRUE;
}

static Eina_Bool
_termcmd_background(Evas_Object *obj, Evas_Object *win __UNUSED__, Evas_Object *bg __UNUSED__, const char *cmd)
{
   Config *config = termio_config_get(obj);

   if (!config) return EINA_TRUE;

   if (cmd[0] == 0)
     {
        config->temporary = EINA_TRUE;
        eina_stringshare_replace(&(config->background), NULL);
        main_media_update(config);
     }
   else if (ecore_file_can_read(cmd))
     {
        config->temporary = EINA_TRUE;
        eina_stringshare_replace(&(config->background), cmd);
        main_media_update(config);
     }
   else
     ERR("Background file cannot be read: %s", cmd);

   return EINA_TRUE;
}

// called as u type
Eina_Bool
termcmd_watch(Evas_Object *obj, Evas_Object *win, Evas_Object *bg, const char *cmd)
{
   if (!cmd) return EINA_FALSE;
   if ((cmd[0] == '/') || (cmd[0] == 's'))
     return _termcmd_search(obj, win, bg, cmd + 1);
   return EINA_FALSE;
}

// called when you hit enter
Eina_Bool
termcmd_do(Evas_Object *obj, Evas_Object *win, Evas_Object *bg, const char *cmd)
{
   if (!cmd) return EINA_FALSE;
   if ((cmd[0] == '/') || (cmd[0] == 's'))
     return _termcmd_search(obj, win, bg, cmd + 1);
   if ((cmd[0] == 'f') || (cmd[0] == 'F'))
     return _termcmd_font_size(obj, win, bg, cmd + 1);
   if ((cmd[0] == 'g') || (cmd[0] == 'G'))
     return _termcmd_grid_size(obj, win, bg, cmd + 1);
   if ((cmd[0] == 'b') || (cmd[0] == 'B'))
     return _termcmd_background(obj, win, bg, cmd + 1);

   ERR("Unknown command: %s", cmd);
   return EINA_FALSE;
}
