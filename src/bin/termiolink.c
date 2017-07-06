#include "private.h"
#include <Elementary.h>
#include "termio.h"
#include "sb.h"
#include "utf8.h"
#include "utils.h"

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
_home_path_get(const Evas_Object *_obj EINA_UNUSED,
               const char *relpath)
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

static int
_txt_at(Termpty *ty, int *x, int *y, char *txt, int *txtlenp)
{
   Termcell *cells = NULL;
   Termcell cell;
   ssize_t w;

   cells = termpty_cellrow_get(ty, *y, &w);
   if (!cells || !w)
     goto bad;
   if ((*x >= w))
     goto empty;
   cell = cells[*x];
   if ((cell.codepoint == 0) && (cell.att.dblwidth))
     {
        (*x)--;
        if (*x < 0)
          goto bad;
        cell = cells[*x];
     }

   if (cell.codepoint == 0)
     goto empty;

   *txtlenp = codepoint_to_utf8(cell.codepoint, txt);

   return 0;

empty:
        txt[0] = '\0';
        *txtlenp = 0;
   return 0;

bad:
   *txtlenp = 0;
   txt[0] = '\0';
   return -1;
}

static int
_txt_prev_at(Termpty *ty, int *x, int *y, char *txt, int *txtlenp)
{
   Termcell *cells = NULL;
   Termcell cell;
   ssize_t w;

   (*x)--;
   if ((*x) < 0)
     {
        (*y)--;
        *x = ty->w-1;
        cells = termpty_cellrow_get(ty, *y, &w);
        if (!cells || !w)
          goto bad;
        if ((*x) >= w)
          goto empty;
        cell = cells[*x];
        /* Either the cell is in the normal screen and needs to have
         * autowrapped flag or is in the backlog and its length is larger than
         * the screen, spanning multiple lines */
        if (((!cell.att.autowrapped) && (*y) >= 0)
            || (w < ty->w))
          goto empty;
     }
   else
     {
        cells = termpty_cellrow_get(ty, *y, &w);
        if (!cells || !w)
          goto bad;
        if ((*x) >= w)
          goto empty;

        cell = cells[*x];
     }
   if ((cell.codepoint == 0) && (cell.att.dblwidth))
     {
        (*x)--;
        if (*x < 0)
          goto bad;
        cell = cells[*x];
     }

   if (cell.codepoint == 0)
     goto empty;

   *txtlenp = codepoint_to_utf8(cell.codepoint, txt);

   return 0;

empty:
   txt[0] = '\0';
   *txtlenp = 0;
   return 0;

bad:
   *txtlenp = 0;
   txt[0] = '\0';
   return -1;
}

static int
_txt_next_at(Termpty *ty, int *x, int *y, char *txt, int *txtlenp)
{
   Termcell *cells = NULL;
   Termcell cell;
   ssize_t w;

   (*x)++;
   cells = termpty_cellrow_get(ty, *y, &w);
   if (!cells || !w)
     goto bad;
   if ((*x) >= w)
     {
        (*y)++;
        if ((*x) <= ty->w)
          {
             cell = cells[w-1];
             if (!cell.att.autowrapped)
               goto empty;
          }

        *x = 0;
        cells = termpty_cellrow_get(ty, *y, &w);
        if (!cells || !w)
          goto bad;
     }

   cell = cells[*x];
   if ((cell.codepoint == 0) && (cell.att.dblwidth))
     {
        (*x)++;
        if (*x >= w)
          {
             cell = cells[w-1];
             if (!cell.att.autowrapped && w == ty->w)
               goto empty;
             (*y)++;
             *x = 0;
             cells = termpty_cellrow_get(ty, *y, &w);
             if (!cells || !w)
               goto bad;
          }
          goto bad;
     }

   cell = cells[*x];
   if (cell.codepoint == 0)
     goto empty;

   *txtlenp = codepoint_to_utf8(cell.codepoint, txt);

   return 0;

empty:
   txt[0] = '\0';
   *txtlenp = 0;
   return 0;

bad:
   *txtlenp = 0;
   txt[0] = '\0';
   return -1;
}

char *
termio_link_find(const Evas_Object *obj, int cx, int cy,
                 int *x1r, int *y1r, int *x2r, int *y2r)
{
   char *s = NULL;
   char endmatch = 0;
   int x1, x2, y1, y2, w = 0, h = 0, sc;
   Eina_Bool goback = EINA_TRUE, goforward = EINA_FALSE, escaped = EINA_FALSE;
   struct ty_sb sb = {.buf = NULL, .gap = 0, .len = 0, .alloc = 0};
   Termpty *ty = termio_pty_get(obj);
   int res;
   char txt[8];
   int txtlen = 0;
   Eina_Bool was_protocol = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ty, NULL);

   x1 = x2 = cx;
   y1 = y2 = cy;
   termio_size_get(obj, &w, &h);
   if ((w <= 0) || (h <= 0)) goto end;

   sc = termio_scroll_get(obj);

   termpty_backlog_lock();

   y1 -= sc;
   y2 -= sc;

   res = _txt_at(ty, &x1, &y1, txt, &txtlen);
   if ((res != 0) || (txtlen == 0)) goto end;
   res = ty_sb_add(&sb, txt, txtlen);
   if (res < 0) goto end;

   while (goback)
     {
        int new_x1 = x1, new_y1 = y1;

        res = _txt_prev_at(ty, &new_x1, &new_y1, txt, &txtlen);
        if ((res != 0) || (txtlen == 0))
          {
             goback = EINA_FALSE;
             goforward = EINA_TRUE;
             break;
          }
        res = ty_sb_prepend(&sb, txt, txtlen);
        if (res < 0) goto end;
        if (isspace(sb.buf[0]))
          {
             int old_txtlen = txtlen;
             res = _txt_prev_at(ty, &new_x1, &new_y1, txt, &txtlen);
             if ((res != 0) || (txtlen == 0))
               {
                  goback = EINA_FALSE;
                  goforward = EINA_TRUE;
                  break;
               }
             if (txt[0] != '\\')
               {
                  ty_sb_lskip(&sb, old_txtlen);
                  goback = EINA_FALSE;
                  goforward = EINA_TRUE;
                  break;
               }
          }
        switch (sb.buf[0])
          {
           case '"': endmatch = '"'; break;
           case '\'': endmatch = '\''; break;
           case '`': endmatch = '`'; break;
           case '<': endmatch = '>'; break;
           case '[': endmatch = ']'; break;
           case '{': endmatch = '}'; break;
           case '(': endmatch = ')'; break;
           case '|': endmatch = '|'; break;
          }
        if (endmatch)
          {
             ty_sb_lskip(&sb, txtlen);
             goback = EINA_FALSE;
             goforward = EINA_TRUE;
             break;
          }

        if (!link_is_protocol(sb.buf))
          {
             if (was_protocol)
               {
                  if (!isspace(sb.buf[0]))
                    endmatch = sb.buf[0];
                  ty_sb_lskip(&sb, txtlen);
                  goback = EINA_FALSE;
                  goforward = EINA_TRUE;
                  break;
               }
          }
        else
          {
             was_protocol = EINA_TRUE;
          }
        x1 = new_x1;
        y1 = new_y1;
     }

   while (goforward)
     {
        int new_x2 = x2, new_y2 = y2;
        /* Check if the previous char is a delimiter */
        res = _txt_next_at(ty, &new_x2, &new_y2, txt, &txtlen);
        if ((res != 0) || (txtlen == 0))
          {
             goforward = EINA_FALSE;
             break;
          }
        /* escaping */
        if (txt[0] == '\\')
          {
             x2 = new_x2;
             y2 = new_y2;
             escaped = EINA_TRUE;
             continue;
          }
        if (escaped)
          {
             x2 = new_x2;
             y2 = new_y2;
             escaped = EINA_FALSE;
          }

        if (isspace(txt[0]) || txt[0] == endmatch)
          {
             goforward = EINA_FALSE;
             break;
          }
        switch (txt[0])
          {
           case '"':
           case '\'':
           case '`':
           case '<':
           case '>':
           case '[':
           case ']':
           case '{':
           case '}':
           case '|':
             goto out;
          }

        res = ty_sb_add(&sb, txt, txtlen);
        if (res < 0) goto end;

        if (!link_is_protocol(sb.buf))
          {
             if (was_protocol)
               {
                  ty_sb_rskip(&sb, txtlen);
                  goback = EINA_FALSE;
               }
          }
        else
          {
             was_protocol = EINA_TRUE;
          }
        x2 = new_x2;
        y2 = new_y2;
     }

out:
   if (sb.len)
     {
        Eina_Bool is_file = _is_file(sb.buf);

        if (is_file ||
            link_is_email(sb.buf) ||
            link_is_url(sb.buf))
          {
             if (x1r) *x1r = x1;
             if (y1r) *y1r = y1 + sc;
             if (x2r) *x2r = x2;
             if (y2r) *y2r = y2 + sc;

             if (is_file && (sb.buf[0] != '/'))
               {
                  char *local= _local_path_get(obj, (const char*)sb.buf);
                  ty_sb_free(&sb);
                  s = local;
                  goto end;
               }
             else
               {
                  s = ty_sb_steal_buf(&sb);
               }

             goto end;
          }
     }
end:
   termpty_backlog_unlock();
   ty_sb_free(&sb);
   return s;
}
