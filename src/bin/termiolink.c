#include "private.h"
#include <Elementary.h>
#include "termio.h"
#include "utils.h"

static Eina_Bool
coord_back(int *x, int *y, int w, int h EINA_UNUSED)
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

static char *
_cwd_path_get(const Evas_Object *obj, const char *relpath)
{
   char cwdpath[PATH_MAX], tmppath[PATH_MAX];

   if (!termio_cwd_get(obj, cwdpath, sizeof(cwdpath)))
     return NULL;

   eina_str_join(tmppath, sizeof(tmppath), '/', cwdpath, relpath);
   return strdup(tmppath);
}

static char *
_home_path_get(const Evas_Object *obj EINA_UNUSED, const char *relpath)
{
   char tmppath[PATH_MAX], homepath[PATH_MAX];

   if (!homedir_get(homepath, sizeof(homepath)))
     return NULL;

   eina_str_join(tmppath, sizeof(tmppath), '/', homepath, relpath);
   return strdup(tmppath);
}

static char *
_local_path_get(const Evas_Object *obj, const char *relpath)
{
   if (relpath[0] == '/')
     return strdup(relpath);
   else if (eina_str_has_prefix(relpath, "~/"))
     return _home_path_get(obj, relpath + 2);
   else
     return _cwd_path_get(obj, relpath);
}

static Eina_Bool
_is_file(const char *str)
{
   switch (str[0])
     {
      case '/':
        return EINA_TRUE;
      case '~':
        if (str[1] == '/')
          return EINA_TRUE;
        return EINA_FALSE;
      case '.':
        if (str[1] == '/')
          return EINA_TRUE;
        else if ((str[1] == '.') && (str[2] == '/'))
          return EINA_TRUE;
        return EINA_FALSE;
      default:
        return EINA_FALSE;
     }
}

char *
_termio_link_find(Evas_Object *obj, int cx, int cy,
                  int *x1r, int *y1r, int *x2r, int *y2r)
{
   char *s;
   char endmatch = 0;
   int x1, x2, y1, y2, w = 0, h = 0, sc;
   size_t len = 0, prev_len = 0;
   Eina_Bool goback = EINA_TRUE, goforward = EINA_FALSE, escaped = EINA_FALSE;

   x1 = x2 = cx;
   y1 = y2 = cy;
   termio_size_get(obj, &w, &h);
   if ((w <= 0) || (h <= 0)) return NULL;
   sc = termio_scroll_get(obj);
   for (;;)
     {
        prev_len = len;
        s = termio_selection_get(obj, x1, y1 - sc, x2, y2 - sc, &len);
        if (!s) break;
        if (goback)
          {
             if (link_is_protocol(s))
               {
                  goback = EINA_FALSE;
                  goforward = EINA_TRUE;

                  /* Check if the previous char is a delimiter */
                  coord_back(&x1, &y1, w, h);
                  free(s);
                  s = termio_selection_get(obj, x1, y1 - sc, x2, y2 - sc,
                                           &len);
                  if (!s) break;
                  switch (s[0])
                    {
                     case '"': endmatch = '"'; break;
                     case '\'': endmatch = '\''; break;
                     case '`': endmatch = '`'; break;
                     case '<': endmatch = '>'; break;
                     case '[': endmatch = ']'; break;
                     case '{': endmatch = '}'; break;
                     case '(': endmatch = ')'; break;
                    }
                  coord_forward(&x1, &y1, w, h);

                  free(s);
                  prev_len = len;
                  s = termio_selection_get(obj, x1, y1 - sc, x2, y2 - sc,
                                           &len);
                  if (!s) break;
               }
             else
               {
                  switch (s[0])
                    {
                     case '"': endmatch = '"'; break;
                     case '\'': endmatch = '\''; break;
                     case '`': endmatch = '`'; break;
                     case '<': endmatch = '>'; break;
                     case '[': endmatch = ']'; break;
                     case '{': endmatch = '}'; break;
                     case '(': endmatch = ')'; break;
                    }
                  if ((endmatch) || (isspace(s[0])))
                    {
                       goback = EINA_FALSE;
                       coord_forward(&x1, &y1, w, h);
                       goforward = EINA_TRUE;
                       free(s);
                       s = termio_selection_get(obj, x1, y1 - sc, x2, y2 - sc,
                                                &len);
                       if (!s) break;
                    }
               }
          }
        if ((goforward) && (len >= 1))
          {
             char end = s[len - 1];
             if (len - prev_len == 2 && len >= 2)
               end = s[len - 2];

             if ((end == endmatch) ||
                 ((!escaped) && (isspace(end)) &&
                  ((end != '\n') || (end != '\r'))))
               {
                  goforward = EINA_FALSE;
                  coord_back(&x2, &y2, w, h);
                  endmatch = 0;
               }
             escaped = (end == '\\');
          }
        if ((goback) && (!coord_back(&x1, &y1, w, h)))
          {
             goback = EINA_FALSE;
             goforward = EINA_TRUE;
          }
        if ((goforward) && (!coord_forward(&x2, &y2, w, h)))
          {
             goforward = EINA_FALSE;
          }
        if ((!goback) && (!goforward))
          {
             free(s);
             s = termio_selection_get(obj, x1, y1 - sc, x2, y2 - sc, &len);
             break;
          }
        free(s);
        s = NULL;
     }
   if (s)
     {
        if ((len > 1) && (!endmatch))
          {
             Eina_Bool is_file = _is_file(s);

             if (is_file ||
                 link_is_email(s) ||
                 link_is_url(s))
               {
                  if (x1r) *x1r = x1;
                  if (y1r) *y1r = y1;
                  if (x2r) *x2r = x2;
                  if (y2r) *y2r = y2;

                  if (is_file && (s[0] != '/'))
                    {
                       char *ret = _local_path_get(obj, s);
                       free(s);
                       return ret;
                    }

                  return s;
               }
          }
        free(s);
     }
   return NULL;
}
