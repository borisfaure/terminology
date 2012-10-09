#include "private.h"
#include <Elementary.h>
#include "termio.h"
#include "utils.h"

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

static char *
_cwd_path_get(const Evas_Object *obj, const char *relpath)
{
   char procpath[PATH_MAX], cwdpath[PATH_MAX], tmppath[PATH_MAX];
   pid_t pid = termio_pid_get(obj);

   snprintf(procpath, sizeof(procpath), "/proc/%d/cwd", pid);
   if (readlink(procpath, cwdpath, sizeof(cwdpath)) < 1)
     {
        ERR("Could not load working directory %s: %s",
            procpath, strerror(errno));
        return NULL;
     }

   eina_str_join(tmppath, sizeof(tmppath), '/', cwdpath, relpath);
   return strdup(tmppath);
}

static char *
_home_path_get(const Evas_Object *obj __UNUSED__, const char *relpath)
{
   char tmppath[PATH_MAX];
   const char *home = getenv("HOME");
   if (!home)
     {
        uid_t uid = getuid();
        struct passwd *pw = getpwuid(uid);
        if (pw) home = pw->pw_dir;
     }
   if (!home)
     {
        ERR("Could not get $HOME");
        return NULL;
     }

   eina_str_join(tmppath, sizeof(tmppath), '/', home, relpath);
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
             if (link_is_protocol(s))
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
                  if ((casestartswith((s + 1), "www.")) ||
                      (casestartswith((s + 1), "ftp.")) ||
                      (_is_file(s + 1)))
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
