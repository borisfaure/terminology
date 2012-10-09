#include "private.h"
#include <Elementary.h>
#include "termio.h"

static Eina_Bool
coord_back(int *x, int *y, int w, int h __UNUSED__)
{
   (*x)--;
   if ((*x) < 0)
     {
        if ((*y) <= 0)
          {
             (*x)++;
             return EINA_FALSE;
          }
        (*x) = w - 1;
        (*y)--;
     }
   return EINA_TRUE;
}

static Eina_Bool
coord_forward(int *x, int *y, int w, int h)
{
   (*x)++;
   if ((*x) >= w)
     {
        if ((*y) >= (h - 1))
          {
             (*x)--;
             return EINA_FALSE;
          }
        (*x) = 0;
        (*y)++;
     }
   return EINA_TRUE;
}

char *
_termio_link_find(Evas_Object *obj, int cx, int cy, int *x1r, int *y1r, int *x2r, int *y2r)
{
   char *s;
   char endmatch = 0;
   int x1, x2, y1, y2, len, w = 0, h = 0, sc;
   Eina_Bool goback = EINA_TRUE, goforward = EINA_FALSE, extend = EINA_FALSE;
   
   x1 = x2 = cx;
   y1 = y2 = cy;
   termio_size_get(obj, &w, &h);
   sc = termio_scroll_get(obj);
   if ((w <= 0) || (h <= 0)) return NULL;
   if (!coord_back(&x1, &y1, w, h)) goback = EINA_FALSE;
   for (;;)
     {
        s = termio_selection_get(obj, x1, y1 - sc, x2, y2 - sc);
        if (!s) break;
        if (goback)
          {
             if      ((!strncasecmp(s, "http://", 7))||
                      (!strncasecmp(s, "https://", 8)) ||
                      (!strncasecmp(s, "file://", 7)) ||
                      (!strncasecmp(s, "ftp://", 6)))
               {
                  goback = EINA_FALSE;
                  coord_back(&x1, &y1, w, h);
                  free(s);
                  s = termio_selection_get(obj, x1, y1 - sc, x2, y2 - sc);
                  if (!s) break;
                  if (s[0] == '"') endmatch = '"';
                  else if (s[0] == '\'') endmatch = '\'';
                  else if (s[0] == '<') endmatch = '>';
                  coord_forward(&x1, &y1, w, h);
                  free(s);
                  s = termio_selection_get(obj, x1, y1 - sc, x2, y2 - sc);
                  if (!s) break;
               }
             else if ((isspace(s[0])) ||
                      (s[0] == '"') ||
                      (s[0] == '`') ||
                      (s[0] == '\'') ||
                      (s[0] == '<') ||
                      (s[0] == '='))
               {
                  if (s[0] == '"') endmatch = '"';
                  else if (s[0] == '\'') endmatch = '\'';
                  else if (s[0] == '`') endmatch = '\'';
                  else if (s[0] == '<') endmatch = '>';
                  if ((!strncasecmp((s + 1), "www.", 4)) ||
                      (!strncasecmp((s + 1), "ftp.", 4)) ||
                      (!strncasecmp((s + 1), "/", 1)))
                    {
                       goback = EINA_FALSE;
                       coord_forward(&x1, &y1, w, h);
                    }
                  else if (strchr((s + 2), '@'))
                    {
                       goback = EINA_FALSE;
                       coord_forward(&x1, &y1, w, h);
                    }
                  else if (s[0] == '=')
                    {
                    }
                  else
                    {
                       free(s);
                       s = NULL;
                       break;
                    }
               }
          }
        if (goforward)
          {
             len = strlen(s);
             if (len > 1)
               {
                  if (((endmatch) && (s[len - 1] == endmatch)) ||
                      ((!endmatch) && 
                          ((isspace(s[len - 1])) || (s[len - 1] == '>'))
                      ))
                    {
                       goforward = EINA_FALSE;
                       coord_back(&x2, &y2, w, h);
                    }
               }
          }
        
        if (goforward)
          {
             if (!coord_forward(&x2, &y2, w, h)) goforward = EINA_FALSE;
          }
        if (goback)
          {
             if (!coord_back(&x1, &y1, w, h)) goback = EINA_FALSE;
          }
        if ((!extend) && (!goback))
          {
             goforward = EINA_TRUE;
             extend = EINA_TRUE;
          }
        if ((!goback) && (!goforward))
          {
             free(s);
             s = termio_selection_get(obj, x1, y1 - sc, x2, y2 - sc);
             break;
          }
        free(s);
        s = NULL;
     }
   if (s)
     {
        len = strlen(s);
        while (len > 1)
          {
             if (isspace(s[len - 1]))
               {
                  s[len - 1] = 0;
                  len--;
               }
             else break;
          }
        if ((!isspace(s[0])) && (len > 1))
          {
             const char *at = strchr(s, '@');

             if ((at && (strchr(at + 1, '.'))) ||
                 (!strncasecmp(s, "http://", 7))||
                 (!strncasecmp(s, "https://", 8)) ||
                 (!strncasecmp(s, "ftp://", 6)) ||
                 (!strncasecmp(s, "file://", 7)) ||
                 (!strncasecmp(s, "www.", 4)) ||
                 (!strncasecmp(s, "ftp.", 4)) ||
                 (!strncasecmp(s, "/", 1))
                )
               {
                  if (x1r) *x1r = x1;
                  if (y1r) *y1r = y1;
                  if (x2r) *x2r = x2;
                  if (y2r) *y2r = y2;
                  return s;
               }
          }
        free(s);
     }
   return NULL;
}
