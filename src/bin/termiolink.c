
#if !defined(BINARY_TYFUZZ_COLOR_PARSER)
#include "private.h"
#include <Elementary.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include "termpty.h"
#include "backlog.h"
#include "termiolink.h"
#include "termio.h"
#include "sb.h"
#include "utf8.h"
#include "theme.h"
#include "utils.h"
#else
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sb.h"
#define eina_convert_strtod_c strtod
#ifndef MIN
# define MIN(x, y) (((x) > (y)) ? (y) : (x))
#endif
#ifndef MAX
# define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif
typedef uint32_t Eina_Unicode;
typedef bool Eina_Bool;
#define EINA_TRUE true
#define EINA_FALSE false
#endif

#if !defined(BINARY_TYFUZZ_COLOR_PARSER)
__attribute__((const))
static Eina_Bool
_isspace_unicode(const int codepoint)
{
   switch (codepoint)
     {
      case 9: // character tabulation
         return EINA_TRUE;
      case 10: // line feed
         return EINA_TRUE;
      case 11: // line tabulation
         return EINA_TRUE;
      case 12: // form feed
         return EINA_TRUE;
      case 13: // carriage return
         return EINA_TRUE;
      case 32: // space
         return EINA_TRUE;
      case 133: // next line
         return EINA_TRUE;
      case 160: // no-break space
         return EINA_TRUE;
      case 5760: // ogham space mark
         return EINA_TRUE;
      case 6158: // mongolian vowel separator
         return EINA_TRUE;
      case 8192: // en quad
         return EINA_TRUE;
      case 8193: // em quad
         return EINA_TRUE;
      case 8194: // en space
         return EINA_TRUE;
      case 8195: // em space
         return EINA_TRUE;
      case 8196: // three-per-em space
         return EINA_TRUE;
      case 8197: // four-per-em space
         return EINA_TRUE;
      case 8198: // six-per-em space
         return EINA_TRUE;
      case 8199: // figure space
         return EINA_TRUE;
      case 8200: // puncturation space
         return EINA_TRUE;
      case 8201: // thin space
         return EINA_TRUE;
      case 8202: // hair space
         return EINA_TRUE;
      case 8203: // zero width space
         return EINA_TRUE;
      case 8204: // zero width non-joiner
         return EINA_TRUE;
      case 8205: // zero width joiner
         return EINA_TRUE;
      case 8232: // line separator
         return EINA_TRUE;
      case 8233: // paragraph separator
         return EINA_TRUE;
      case 8239: // narrow no-break space
         return EINA_TRUE;
      case 8287: // medium mathematical space
         return EINA_TRUE;
      case 8288: // word joiner
         return EINA_TRUE;
      case 12288: // ideographic space
         return EINA_TRUE;
      case 65279: // zero width non-breaking space
         return EINA_TRUE;
     }
   return EINA_FALSE;
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

/* isalpha() may produce unsigned-integer-overflow
 * runtime error: unsigned integer overflow: 32 - 97 cannot be represented in
 * type 'unsigned int'
 * int isalpha(int c)
 * {
 *     return ((unsigned)c|32)-'a' < 26;
 * }
 */
#if defined(__clang__)
__attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
Eina_Bool
link_is_protocol(const char *str)
{
   const char *p = str;
   int c;

   if (!str)
     return EINA_FALSE;

   c = *p;
   if (!isalpha(c))
     return EINA_FALSE;

   /* Try to follow RFC3986 a bit
    * URI         = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
    * hier-part   = "//" authority path-abempty
    *             [...] other stuff not taken into account
    * scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    */

   do
     {
        p++;
        c = *p;
     }
   while (isalpha(c) || (c == '.') || (c == '-') || (c == '+'));

   return (p[0] == ':') && (p[1] == '/') && (p[2] == '/');
}

Eina_Bool
link_is_url(const char *str)
{
   if (!str)
     return EINA_FALSE;

   if (link_is_protocol(str) ||
       casestartswith(str, "www.") ||
       casestartswith(str, "ftp."))
     return EINA_TRUE;
   return EINA_FALSE;
}

Eina_Bool
link_is_email(const char *str)
{
   const char *at;

   if (!str)
     return EINA_FALSE;

   at = strchr(str, '@');
   if (at && strchr(at + 1, '.'))
     return EINA_TRUE;
   if (casestartswith(str, "mailto:"))
     return EINA_TRUE;
   return EINA_FALSE;
}

Eina_Bool
link_is_file(const char *str)
{
   if (!str)
     return EINA_FALSE;

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
_txt_at(Termpty *ty, int *x, int *y, char *txt, int *txtlenp, int *codepointp)
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

   if (cell.att.tab_inserted)
     {
        *txtlenp = 1;
        *codepointp = '\t';
        txt[0] = '\t';
        return 0;
     }
   if ((cell.codepoint == 0) || (cell.att.link_id))
     goto empty;

   *txtlenp = codepoint_to_utf8(cell.codepoint, txt);
   *codepointp = cell.codepoint;

   return 0;

empty:
        txt[0] = '\0';
        *txtlenp = 0;
        *codepointp = 0;
   return 0;

bad:
   *txtlenp = 0;
   txt[0] = '\0';
   return -1;
}

static int
_txt_prev_at(Termpty *ty, int *x, int *y, char *txt, int *txtlenp,
             int *codepointp)
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

   if (cell.att.tab_last)
     {
        while (*x >= 0 && !cells[*x].att.tab_inserted)
          (*x)--;
        if (*x < 0)
          goto bad;
        *txtlenp = 1;
        *codepointp = '\t';
        txt[0] = '\t';
        return 0;
     }
   if ((cell.codepoint == 0) || (cell.att.link_id))
     goto empty;

   *txtlenp = codepoint_to_utf8(cell.codepoint, txt);
   *codepointp = cell.codepoint;

   return 0;

empty:
   txt[0] = '\0';
   *txtlenp = 0;
   *codepointp = 0;
   return 0;

bad:
   txt[0] = '\0';
   *txtlenp = 0;
   return -1;
}

static int
_txt_next_at(Termpty *ty, int *x, int *y, char *txt, int *txtlenp,
             int *codepointp)
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
   if (cell.att.tab_inserted)
     {
        while (*x < w && !cells[*x].att.tab_last)
          (*x)++;
        if (*x >= w)
          goto bad;
        *txtlenp = 1;
        *codepointp = '\t';
        txt[0] = '\t';
        return 0;
     }
   if ((cell.codepoint == 0) || (cell.att.link_id))
     goto empty;

   *txtlenp = codepoint_to_utf8(cell.codepoint, txt);
   *codepointp = cell.codepoint;

   return 0;

empty:
   txt[0] = '\0';
   *txtlenp = 0;
   *codepointp = 0;
   return 0;

bad:
   *txtlenp = 0;
   txt[0] = '\0';
   return -1;
}

/* returned string must be freed */
char *
termio_link_find(const Evas_Object *obj, int cx, int cy,
                 int *x1r, int *y1r, int *x2r, int *y2r)
{
   char *s = NULL;
   int endmatch1 = 0, endmatch2 = 0;
   int x1, x2, y1, y2, w = 0, h = 0, sc;
   Eina_Bool goback = EINA_TRUE,
             goforward = EINA_FALSE,
             escaped = EINA_FALSE;
   struct ty_sb sb = {.buf = NULL, .gap = 0, .len = 0, .alloc = 0};
   Termpty *ty = termio_pty_get(obj);
   int res;
   char txt[8];
   int txtlen = 0;
   int codepoint = 0;
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

   res = _txt_at(ty, &x1, &y1, txt, &txtlen, &codepoint);
   if ((res != 0) || (txtlen == 0)) goto end;
   if (_isspace_unicode(codepoint))
     goto end;
   res = ty_sb_add(&sb, txt, txtlen);
   if (res < 0) goto end;

   while (goback)
     {
        int new_x1 = x1, new_y1 = y1;

        res = _txt_prev_at(ty, &new_x1, &new_y1, txt, &txtlen, &codepoint);
        if ((res != 0) || (txtlen == 0))
          {
             goback = EINA_FALSE;
             goforward = EINA_TRUE;
             break;
          }
        res = ty_sb_prepend(&sb, txt, txtlen);
        if (res < 0) goto end;
        if (_isspace_unicode(codepoint))
          {
             int old_txtlen = txtlen;
             res = _txt_prev_at(ty, &new_x1, &new_y1, txt, &txtlen, &codepoint);
             if ((res != 0) || (txtlen == 0) || (codepoint != '\\'))
               {
                  ty_sb_lskip(&sb, old_txtlen);
                  goback = EINA_FALSE;
                  goforward = EINA_TRUE;
                  break;
               }
          }
        switch (codepoint)
          {
           case '"':  endmatch1 = endmatch2 = '"'; break;
           case '\'': endmatch1 = endmatch2 = '\''; break;
           case '`':  endmatch1 = endmatch2 = '`'; break;
           case '<':  endmatch1 = endmatch2 = '>'; break;
           case '[':  endmatch1 = endmatch2 = ']'; break;
           case ']':  endmatch1 = endmatch2 = '['; break;
           case '{':  endmatch1 = endmatch2 = '}'; break;
           case '(':  endmatch1 = endmatch2 = ')'; break;
           case '|':  endmatch1 = endmatch2 = '|'; break;
           case 0xab:  endmatch1 = endmatch2 = 0xbb; break; // french « »
           case 0xbb:  endmatch1 = endmatch2 = 0xab; break; // swedish » «
           case 0x2018:  endmatch1 = endmatch2 = 0x2019; break;  // ‘ ’
           case 0x201b:  endmatch1 = endmatch2 = 0x2019; break;  // ‛ ’
           case 0x201c:  endmatch1 = endmatch2 = 0x201d; break;  // “ ”
           case 0x201e:  endmatch1 = 0x201c; endmatch2 = 0x201d; break;  // „ “”
           case 0x2039: endmatch1 = endmatch2 = 0x203a; break; // ‹›
           case 0x27e6:  endmatch1 = endmatch2 = 0x27e7; break; // ⟦ ⟧
           case 0x27e8:  endmatch1 = endmatch2 = 0x27e9; break; // ⟨ ⟩
           case 0x2329:  endmatch1 = endmatch2 = 0x232a; break; // 〈 〉
           case 0x231c:  endmatch1 = 0x231d; endmatch2 = 0x231f; break;  // ⌜⌝⌟
           case 0x231e:  endmatch1 = 0x231d; endmatch2 = 0x231f; break;  // ⌞⌝⌟
           case 0x2308:  endmatch1 = 0x2309; endmatch2 = 0x230b; break;  // ⌈⌉⌋
           case 0x230a:  endmatch1 = 0x2309; endmatch2 = 0x230b; break;  // ⌊⌉⌋
          }
        if (endmatch1)
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
                  if (!_isspace_unicode(codepoint))
                    endmatch1 = endmatch2 = codepoint;
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
        res = _txt_next_at(ty, &new_x2, &new_y2, txt, &txtlen, &codepoint);
        if ((res != 0) || (txtlen == 0))
          {
             goforward = EINA_FALSE;
             break;
          }
        /* escaping */
        if (codepoint == '\\')
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

        if (_isspace_unicode(codepoint) || (codepoint == endmatch1)
            || (codepoint == endmatch2))
          {
             goforward = EINA_FALSE;
             break;
          }
        switch (codepoint)
          {
           case '"': goto out;
           case '\'': goto out;
           case '`': goto out;
           case '<': goto out;
           case '>': goto out;
           case '[': goto out;
           case ']': goto out;
           case '{': goto out;
           case '}': goto out;
           case '|': goto out;
           case 0xab: goto out;
           case 0xbb: goto out;
           case 0x2018: goto out;
           case 0x2019: goto out;
           case 0x201b: goto out;
           case 0x201c: goto out;
           case 0x201d: goto out;
           case 0x201e: goto out;
           case 0x2039: goto out;
           case 0x203a: goto out;
           case 0x2308: goto out;
           case 0x2309: goto out;
           case 0x230a: goto out;
           case 0x230b: goto out;
           case 0x231c: goto out;
           case 0x231d: goto out;
           case 0x231e: goto out;
           case 0x231f: goto out;
           case 0x2329: goto out;
           case 0x232a: goto out;
           case 0x27e6: goto out;
           case 0x27e7: goto out;
           case 0x27e8: goto out;
           case 0x27e9: goto out;
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
        Eina_Bool is_file = link_is_file(sb.buf);

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
#endif

__attribute__((const))
static Eina_Bool
_is_authorized_in_color(const int codepoint)
{
   switch (codepoint)
     {
      case '\t': return EINA_TRUE;
      case ' ': return EINA_TRUE;
      case '#': return EINA_TRUE;
      case '%': return EINA_TRUE;
      case '(': return EINA_TRUE;
      case '+': return EINA_TRUE;
      case ',': return EINA_TRUE;
      case '.': return EINA_TRUE;
      case '/': return EINA_TRUE;
      case '0': return EINA_TRUE;
      case '1': return EINA_TRUE;
      case '2': return EINA_TRUE;
      case '3': return EINA_TRUE;
      case '4': return EINA_TRUE;
      case '5': return EINA_TRUE;
      case '6': return EINA_TRUE;
      case '7': return EINA_TRUE;
      case '8': return EINA_TRUE;
      case '9': return EINA_TRUE;
      case ':': return EINA_TRUE;
      case 'A': return EINA_TRUE;
      case 'B': return EINA_TRUE;
      case 'C': return EINA_TRUE;
      case 'D': return EINA_TRUE;
      case 'E': return EINA_TRUE;
      case 'F': return EINA_TRUE;
      case 'a': return EINA_TRUE;
      case 'b': return EINA_TRUE;
      case 'c': return EINA_TRUE;
      case 'd': return EINA_TRUE;
      case 'e': return EINA_TRUE;
      case 'f': return EINA_TRUE;
      case 'g': return EINA_TRUE;
      case 'h': return EINA_TRUE;
      case 'l': return EINA_TRUE;
      case 'n': return EINA_TRUE;
      case 'o': return EINA_TRUE;
      case 'r': return EINA_TRUE;
      case 's': return EINA_TRUE;
      case 't': return EINA_TRUE;
      case 'u': return EINA_TRUE;
     }
   return EINA_FALSE;
}

static Eina_Bool
_parse_hex(const char c, uint8_t *v)
{
   if (c < '0')
     return EINA_FALSE;
   if (c <= '9')
     {
        *v = c - '0';
        return EINA_TRUE;
     }
   if (c < 'A')
     return EINA_FALSE;
   if (c <= 'F')
     {
        *v = 10 + c - 'A';
        return EINA_TRUE;
     }
   if (c < 'a')
     return EINA_FALSE;
   if (c <= 'f')
     {
        *v = 10 + c - 'a';
        return EINA_TRUE;
     }
   return EINA_FALSE;
}

static Eina_Bool
_parse_2hex(const char *s, uint8_t *v)
{
   uint8_t v0, v1;
   if (!_parse_hex(s[0], &v0))
     return EINA_FALSE;
   if (!_parse_hex(s[1], &v1))
     return EINA_FALSE;
   *v = v0 << 4 | v1;
   return EINA_TRUE;
}

static Eina_Bool
_parse_sharp_color(struct ty_sb *sb,
                   uint8_t *rp, uint8_t *gp, uint8_t *bp, uint8_t *ap)
{
   uint8_t r, g, b, a = 255;

   /* sharp */
   if (sb->buf[0] != '#')
     return EINA_FALSE;
   ty_sb_lskip(sb, 1);
   switch (sb->len)
     {
      case 3:
         if ((!_parse_hex(sb->buf[0], &r)) ||
             (!_parse_hex(sb->buf[1], &g)) ||
             (!_parse_hex(sb->buf[2], &b)))
           return EINA_FALSE;
         r <<= 4;
         g <<= 4;
         b <<= 4;
         break;
      case 4:
         if ((!_parse_hex(sb->buf[0], &r)) ||
             (!_parse_hex(sb->buf[1], &g)) ||
             (!_parse_hex(sb->buf[2], &b)) ||
             (!_parse_hex(sb->buf[3], &a)))
           return EINA_FALSE;
         r <<= 4;
         g <<= 4;
         b <<= 4;
         a <<= 4;
         break;
      case 6:
         if ((!_parse_2hex(&sb->buf[0], &r)) ||
             (!_parse_2hex(&sb->buf[2], &g)) ||
             (!_parse_2hex(&sb->buf[4], &b)))
           return EINA_FALSE;
         break;
      case 8:
         if ((!_parse_2hex(&sb->buf[0], &r)) ||
             (!_parse_2hex(&sb->buf[2], &g)) ||
             (!_parse_2hex(&sb->buf[4], &b)) ||
             (!_parse_2hex(&sb->buf[6], &a)))
           return EINA_FALSE;
         break;
      default:
         return EINA_FALSE;
     }
   *rp = r;
   *gp = g;
   *bp = b;
   *ap = a;
   return EINA_TRUE;
}

static Eina_Bool
_parse_uint8(struct ty_sb *sb,
             uint8_t *vp)
{
   uint16_t v = 0;
   Eina_Bool ret = EINA_FALSE;

   if (!sb->len) return EINA_FALSE;

   while (sb->len && v < 255 && sb->buf[0] >= '0' && sb->buf[0] <= '9')
     {
        v = (v * 10) + sb->buf[0] - '0';
        ty_sb_lskip(sb, 1);
        ret = EINA_TRUE;
     }
   if (v > 255)
     return EINA_FALSE;

   *vp = v;
   return ret;
}

/* isnan() in musl generates  ' runtime error: negation of 1 cannot be
 * represented in type 'unsigned long long'
 * under ubsan
 */
#if defined(__clang__) && !defined(__GLIBC__)
__attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
static Eina_Bool
_parse_one_css_rgb_color(struct ty_sb *sb,
                         uint8_t *vp,
                         Eina_Bool *is_percentp)
{
   char *endptr_double = sb->buf, *endptr_long;
   double d;
   long int l;

   if (!sb->len)
     return EINA_FALSE;

   d = eina_convert_strtod_c(sb->buf, &endptr_double);
   l = (long int)strtol(sb->buf, &endptr_long, 0);
   if (endptr_double == sb->buf || d < 0 || l < 0 || isnan(d))
       return EINA_FALSE;
   if (endptr_double > endptr_long)
     {
        ty_sb_lskip(sb, endptr_double - sb->buf);
        *is_percentp = sb->len && sb->buf[0] == '%';
        if (*is_percentp)
          {
             ty_sb_lskip(sb, 1);
             if (d > 100.0)
               return EINA_FALSE;
             d = (255.0 * d) / 100;
          }
        if (d > 255)
          return EINA_FALSE;
        *vp = round(d);
     }
   else
     {
        ty_sb_lskip(sb, endptr_long - sb->buf);
        *is_percentp = sb->len && sb->buf[0] == '%';
        if (*is_percentp)
          {
             ty_sb_lskip(sb, 1);
             if (l > 100)
               return EINA_FALSE;
             l = (255 * l) / 100;
          }
        if (l > 255)
          return EINA_FALSE;
        *vp = (uint8_t)l;
     }
   return EINA_TRUE;
}

/* isnan() in musl generates  ' runtime error: negation of 1 cannot be
 * represented in type 'unsigned long long'
 * under ubsan
 */
#if defined(__clang__) && !defined(__GLIBC__)
__attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
static Eina_Bool
_parse_one_css_alpha(struct ty_sb *sb,
                     uint8_t *ap)
{
   char *endptr_double = sb->buf;
   double d;

   if (!sb->len)
     return EINA_FALSE;

   d = eina_convert_strtod_c(sb->buf, &endptr_double);
   if (endptr_double == sb->buf || d < 0 || isnan(d))
       return EINA_FALSE;
   ty_sb_lskip(sb, endptr_double - sb->buf);
   if (sb->len && sb->buf[0] == '%')
     {
        ty_sb_lskip(sb, 1);
        if (d > 100.0)
          return EINA_FALSE;
        d = (255.0 * d) / 100;
     }
   else
     {
        d *= 255.0;
     }
   if (d > 255)
     return EINA_FALSE;
   *ap = round(d);
   return EINA_TRUE;
}

/* isnan() in musl generates  ' runtime error: negation of 1 cannot be
 * represented in type 'unsigned long long'
 * under ubsan
 */
#if defined(__clang__) && !defined(__GLIBC__)
__attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
/* returns hue between 0 and 1 */
static Eina_Bool
_parse_one_hue(struct ty_sb *sb,
               double *dp)
{
   char *endptr_double = sb->buf;
   double d;

   if (!sb->len)
     return EINA_FALSE;

   d = eina_convert_strtod_c(sb->buf, &endptr_double);
   if (endptr_double == sb->buf || d < 0 || isnan(d))
       return EINA_FALSE;
   ty_sb_lskip(sb, endptr_double - sb->buf);
   if (sbstartswith(sb, "turn"))
     {
        ty_sb_lskip(sb, strlen("turn"));
     }
   else if (sbstartswith(sb, "rad"))
     {
          ty_sb_lskip(sb, strlen("rad"));
          d /= 2 * M_PI;
     }
   else if (sbstartswith(sb, "grad"))
     {
          ty_sb_lskip(sb, strlen("grad"));
          d /= 400;
     }
   else if (sbstartswith(sb, "deg") || sb->buf[0] == ',' || sb->buf[0] == ' ')
     {
        if (sbstartswith(sb, "deg"))
          ty_sb_lskip(sb, strlen("deg"));
        d /= 360;
     }
   else
     return EINA_FALSE;
   if (d >= (double)LONG_MAX)
     return EINA_FALSE;
   d = d - (long) d;
   if (d < 0)
     d = 1 + d;
   *dp = d;
   return EINA_TRUE;
}

/* isnan() in musl generates  ' runtime error: negation of 1 cannot be
 * represented in type 'unsigned long long'
 * under ubsan
 */
#if defined(__clang__) && !defined(__GLIBC__)
__attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
static Eina_Bool
_parse_one_percent(struct ty_sb *sb,
                   double *dp)
{
   char *endptr_double = sb->buf;
   double d;

   if (!sb->len)
     return EINA_FALSE;

   d = eina_convert_strtod_c(sb->buf, &endptr_double);
   if (isnan(d) || endptr_double == sb->buf || d < 0)
       return EINA_FALSE;
   ty_sb_lskip(sb, endptr_double - sb->buf);
   if (!sb->len || sb->buf[0] != '%') return EINA_FALSE;
   ty_sb_lskip(sb, 1);
   if (d > 100.0)
     return EINA_FALSE;
   d /= 100.0;
   *dp = d;
   return EINA_TRUE;
}


/* From http://en.wikipedia.org/wiki/HSL_color_space */
static Eina_Bool
_hsl_to_rgb(double hue, double saturation, double lightness,
            uint8_t *rp, uint8_t *gp, uint8_t *bp)
{
   double a = saturation * MIN(lightness, 1.0 - lightness);
   double n[3] = {0., 8., 4.};
   double res[3] = {};
   int i;

   for (i = 0; i < 3; i++)
     {
        double k = fmod(n[i] + 12.0 * hue, 12.);
        double f = lightness - a * MAX(-1, MIN(MIN(k - 3, 9 - k), 1));
        if (f > 1 || f < 0)
          return EINA_FALSE;
        res[i] = f * 255.0;
     }

   *rp = res[0];
   *gp = res[1];
   *bp = res[2];
   return EINA_TRUE;
}

static Eina_Bool
_parse_css_hsl_color(struct ty_sb *sb,
                     uint8_t *rp, uint8_t *gp, uint8_t *bp, uint8_t *ap)
{
   uint8_t r = 0, g = 0, b = 0, a = 255;
   double hue, saturation, lightness;
   Eina_Bool is_functional;

   ty_sb_spaces_ltrim(sb);

   if (!sbstartswith(sb, "hsl"))
       return EINA_FALSE;
   ty_sb_lskip(sb, 3);
   if (sb->buf[0] == 'a')
       ty_sb_lskip(sb, 1);
   if (!sb->len) return EINA_FALSE;
   ty_sb_spaces_ltrim(sb);
   if (!sb->len || sb->buf[0] != '(')
       return EINA_FALSE;
   ty_sb_lskip(sb, 1);
   if (!sb->len) return EINA_FALSE;
   ty_sb_spaces_ltrim(sb);

   if (!_parse_one_hue(sb, &hue))
     return EINA_FALSE;
   is_functional = (sb->len && sb->buf[0] == ',');
   if (is_functional)
     {
        ty_sb_lskip(sb, 1);
     }
   ty_sb_spaces_ltrim(sb);
   if (!sb->len) return EINA_FALSE;

   if (!_parse_one_percent(sb, &saturation))
     return EINA_FALSE;
   ty_sb_spaces_ltrim(sb);
   if (!sb->len) return EINA_FALSE;
   if (is_functional != (sb->buf[0] == ','))
     return EINA_FALSE;
   if (is_functional)
     ty_sb_lskip(sb, 1);
   ty_sb_spaces_ltrim(sb);
   if (!sb->len) return EINA_FALSE;

   if (!_parse_one_percent(sb, &lightness))
     return EINA_FALSE;
   ty_sb_spaces_ltrim(sb);
   if (!sb->len) return EINA_FALSE;
   if (sb->buf[0] == ')')
     {
        ty_sb_lskip(sb, 1);
        a = 255;
     }
   else
     {
        if ((is_functional && (sb->buf[0] != ',')) ||
            (!is_functional && (sb->buf[0] != '/')))
          return EINA_FALSE;
        if (!sb->len) return EINA_FALSE;
        ty_sb_lskip(sb, 1);
        ty_sb_spaces_ltrim(sb);
        if (!sb->len) return EINA_FALSE;
        if (!_parse_one_css_alpha(sb, &a))
          return EINA_FALSE;
        ty_sb_spaces_ltrim(sb);
        if (sb->buf[0] != ')')
          return EINA_FALSE;
     }
   if (!_hsl_to_rgb(hue, saturation, lightness, &r, &g, &b))
     return EINA_FALSE;

   *rp = r;
   *gp = g;
   *bp = b;
   *ap = a;
   return EINA_TRUE;
}


static Eina_Bool
_parse_css_rgb_color(struct ty_sb *sb,
                     uint8_t *rp, uint8_t *gp, uint8_t *bp, uint8_t *ap)
{
   uint8_t r = 0, g = 0, b = 0, a = 255;
   Eina_Bool must_be_percent, is_percent, is_functional;

   ty_sb_spaces_ltrim(sb);

   if (!sbstartswith(sb, "rgb"))
       return EINA_FALSE;
   ty_sb_lskip(sb, 3);
   if (sb->buf[0] == 'a')
       ty_sb_lskip(sb, 1);
   if (!sb->len) return EINA_FALSE;
   ty_sb_spaces_ltrim(sb);
   if (!sb->len || sb->buf[0] != '(')
       return EINA_FALSE;
   ty_sb_lskip(sb, 1);
   if (!sb->len) return EINA_FALSE;
   ty_sb_spaces_ltrim(sb);

   if (!_parse_one_css_rgb_color(sb, &r, &must_be_percent))
     return EINA_FALSE;
   ty_sb_spaces_ltrim(sb);
   is_functional = (sb->buf[0] == ',');
   if (is_functional)
     ty_sb_lskip(sb, 1);
   ty_sb_spaces_ltrim(sb);
   if (!sb->len) return EINA_FALSE;

   if (!_parse_one_css_rgb_color(sb, &g, &is_percent))
     return EINA_FALSE;
   if (is_percent != must_be_percent)
     return EINA_FALSE;
   ty_sb_spaces_ltrim(sb);
   if (is_functional != (sb->buf[0] == ','))
     return EINA_FALSE;
   if (is_functional)
     ty_sb_lskip(sb, 1);
   ty_sb_spaces_ltrim(sb);
   if (!sb->len) return EINA_FALSE;

   if (!_parse_one_css_rgb_color(sb, &b, &is_percent))
     return EINA_FALSE;
   if (is_percent != must_be_percent)
     return EINA_FALSE;
   ty_sb_spaces_ltrim(sb);
   if (sb->buf[0] == ')')
     {
        ty_sb_lskip(sb, 1);
        a = 255;
     }
   else
     {
        if ((is_functional && (sb->buf[0] != ',')) ||
            (!is_functional && (sb->buf[0] != '/')))
          return EINA_FALSE;
        if (!sb->len) return EINA_FALSE;
        ty_sb_lskip(sb, 1);
        ty_sb_spaces_ltrim(sb);
        if (!sb->len) return EINA_FALSE;
        if (!_parse_one_css_alpha(sb, &a))
          return EINA_FALSE;
        ty_sb_spaces_ltrim(sb);
        if (sb->buf[0] != ')')
          return EINA_FALSE;
     }

   *rp = r;
   *gp = g;
   *bp = b;
   *ap = a;
   return EINA_TRUE;
}

static Eina_Bool
_parse_edc_color(struct ty_sb *sb,
                 uint8_t *rp, uint8_t *gp, uint8_t *bp, uint8_t *ap)
{
   uint8_t r = 0, g = 0, b = 0, a = 255;

   ty_sb_spaces_ltrim(sb);
   /* skip color */
   if (sb->len <= 6)
     return EINA_FALSE;
   if (strncmp(sb->buf, "color", 5) != 0)
     return EINA_FALSE;
   ty_sb_lskip(sb, 5);
   if (sb->buf[0] == '2' || sb->buf[0] == '3')
     ty_sb_lskip(sb, 1);
   ty_sb_spaces_ltrim(sb);
   if (sb->buf[0] != ':')
     return EINA_FALSE;
   ty_sb_lskip(sb, 1); /* skip ':' */
   ty_sb_spaces_ltrim(sb);

   if (!sb->len) return EINA_FALSE;
   if (sb->buf[0] == '#')
     return _parse_sharp_color(sb, rp, gp, bp, ap);
   if (sbstartswith(sb, "rgb"))
       return _parse_css_rgb_color(sb, rp, gp, bp, ap);
   if (sbstartswith(sb, "hsl"))
       return _parse_css_hsl_color(sb, rp, gp, bp, ap);

   if (!_parse_uint8(sb, &r)) return EINA_FALSE;
   ty_sb_spaces_ltrim(sb);
   if (!_parse_uint8(sb, &g)) return EINA_FALSE;
   ty_sb_spaces_ltrim(sb);
   if (!_parse_uint8(sb, &b)) return EINA_FALSE;
   ty_sb_spaces_ltrim(sb);
   if (!_parse_uint8(sb, &a)) return EINA_FALSE;

   *rp = r;
   *gp = g;
   *bp = b;
   *ap = a;
   return EINA_TRUE;
}

__attribute__((const))
static Eina_Bool
_is_authorized_in_color_sharp(const Eina_Unicode g)
{
   if (g == '#' ||
       (g >= '0' && g <= '9') ||
       (g >= 'A' && g <= 'F') ||
       (g >= 'a' && g <= 'f'))
     return EINA_TRUE;
   return EINA_FALSE;
}

static Eina_Bool
_parse_color(struct ty_sb *sb,
             uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
   if (!sb->len)
     goto end;

   if (sb->buf[0] == '#')
     {
        if (!_parse_sharp_color(sb, r, g, b, a))
          goto end;
     }
   else if (sbstartswith(sb, "color"))
     {
        if (!_parse_edc_color(sb, r, g, b, a))
          goto end;
     }
   else if (sbstartswith(sb, "rgb"))
     {
        if (!_parse_css_rgb_color(sb, r, g, b, a))
          goto end;
     }
   else if (sbstartswith(sb, "hsl"))
     {
        if (!_parse_css_hsl_color(sb, r, g, b, a))
          goto end;
     }
   else
     goto end;

   return EINA_TRUE;

end:
   return EINA_FALSE;
}

#if !defined(BINARY_TYFUZZ_COLOR_PARSER)
static Eina_Bool
_sharp_color_find(Termpty *ty, int sc,
                  int *x1r, int *y1r, int *x2r, int *y2r,
                  uint8_t *rp, uint8_t *gp, uint8_t *bp, uint8_t *ap)
{
   int x1 = *x1r, y1 = *y1r, x2 = *x2r, y2 = *y2r;
   Eina_Bool goback = EINA_TRUE,
             goforward = EINA_FALSE;
   struct ty_sb sb = {.buf = NULL, .gap = 0, .len = 0, .alloc = 0};
   int res;
   char txt[8];
   int txtlen = 0;
   Eina_Bool found = EINA_FALSE;
   int codepoint;
   uint8_t r, g, b, a = 255;

   res = _txt_at(ty, &x1, &y1, txt, &txtlen, &codepoint);
   if ((res != 0) || (txtlen == 0))
     goto end;
   if (!_is_authorized_in_color_sharp(codepoint))
     goto end;
   res = ty_sb_add(&sb, txt, txtlen);
   if (res < 0) goto end;

   while (goback && sb.len <= 9)
     {
        int new_x1 = x1, new_y1 = y1;

        res = _txt_prev_at(ty, &new_x1, &new_y1, txt, &txtlen, &codepoint);
        if ((res != 0) || (txtlen == 0))
          {
             goback = EINA_FALSE;
             goforward = EINA_TRUE;
             break;
          }
        res = ty_sb_prepend(&sb, txt, txtlen);
        if (res < 0) goto end;
        if (!_is_authorized_in_color_sharp(codepoint))
          {
             ty_sb_lskip(&sb, txtlen);
             goback = EINA_FALSE;
             goforward = EINA_TRUE;
             break;
          }

        x1 = new_x1;
        y1 = new_y1;
     }

   while (goforward && sb.len <= 9)
     {
        int new_x2 = x2, new_y2 = y2;

        /* Check if the previous char is a delimiter */
        res = _txt_next_at(ty, &new_x2, &new_y2, txt, &txtlen, &codepoint);
        if ((res != 0) || (txtlen == 0))
          {
             goforward = EINA_FALSE;
             break;
          }

        if (!_is_authorized_in_color_sharp(codepoint))
          {
             goforward = EINA_FALSE;
             break;
          }

        res = ty_sb_add(&sb, txt, txtlen);
        if (res < 0) goto end;

        x2 = new_x2;
        y2 = new_y2;
     }

   if (sb.buf[0] == '#')
     {
        if (!_parse_sharp_color(&sb, &r, &g, &b, &a))
          goto end;
        found = EINA_TRUE;
     }
   end:
   ty_sb_free(&sb);
   if (found)
     {
        *rp = r;
        *gp = g;
        *bp = b;
        *ap = a;
        *x1r = x1;
        *y1r = y1 + sc;
        *x2r = x2;
        *y2r = y2 + sc;
     }
   return found;
}


Eina_Bool
termio_color_find(const Evas_Object *obj, int cx, int cy,
                  int *x1r, int *y1r, int *x2r, int *y2r,
                  uint8_t *rp, uint8_t *gp, uint8_t *bp, uint8_t *ap)
{
   int x1, x2, y1, y2, w = 0, h = 0, sc;
   Eina_Bool goback = EINA_TRUE,
             goforward = EINA_FALSE;
   struct ty_sb sb = {.buf = NULL, .gap = 0, .len = 0, .alloc = 0};
   Termpty *ty = termio_pty_get(obj);
   int res;
   char txt[8];
   int txtlen = 0;
   Eina_Bool found = EINA_FALSE;
   int codepoint;
   uint8_t r, g, b, a = 255;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ty, EINA_FALSE);

   x1 = x2 = cx;
   y1 = y2 = cy;
   termio_size_get(obj, &w, &h);
   if ((w <= 0) || (h <= 0)) goto end;

   sc = termio_scroll_get(obj);

   termpty_backlog_lock();

   y1 -= sc;
   y2 -= sc;

   found = _sharp_color_find(ty, sc, &x1, &y1, &x2, &y2,
                             &r, &g, &b, &a);
   if (found)
     goto end;

   res = _txt_at(ty, &x1, &y1, txt, &txtlen, &codepoint);
   if ((res != 0) || (txtlen == 0))
     goto end;
   if (!_is_authorized_in_color(codepoint))
     goto end;
   res = ty_sb_add(&sb, txt, txtlen);
   if (res < 0) goto end;

   while (goback)
     {
        int new_x1 = x1, new_y1 = y1;

        res = _txt_prev_at(ty, &new_x1, &new_y1, txt, &txtlen, &codepoint);
        if ((res != 0) || (txtlen == 0))
          {
             goback = EINA_FALSE;
             goforward = EINA_TRUE;
             break;
          }
        res = ty_sb_prepend(&sb, txt, txtlen);
        if (res < 0) goto end;
        if (!_is_authorized_in_color(codepoint))
          {
             ty_sb_lskip(&sb, txtlen);
             goback = EINA_FALSE;
             goforward = EINA_TRUE;
             break;
          }

        x1 = new_x1;
        y1 = new_y1;
        if (sbstartswith(&sb, "rgb") ||
            sbstartswith(&sb, "hsl") ||
            sbstartswith(&sb, "color"))
          {
             goback = EINA_FALSE;
             goforward = EINA_TRUE;
             break;
          }
     }

   while (goforward)
     {
        int new_x2 = x2, new_y2 = y2;

        /* Check if the previous char is a delimiter */
        res = _txt_next_at(ty, &new_x2, &new_y2, txt, &txtlen, &codepoint);
        if ((res != 0) || (txtlen == 0) ||
            (!_is_authorized_in_color(codepoint) && (codepoint != ')')))
          break;

        res = ty_sb_add(&sb, txt, txtlen);
        if (res < 0) goto end;

        x2 = new_x2;
        y2 = new_y2;
        if (codepoint == ')')
          break;
     }

   /* colors do not span multiple lines (for the moment) */
   if (y1 != y2)
     goto end;

   /* Trim */
   while (sb.len && (sb.buf[0] == ' ' || sb.buf[0] == '\t'))
     {
        sb.len--;
        sb.buf++;
        sb.gap++;
        x1++;
     }
   while (sb.len && (sb.buf[sb.len-1] == ' ' || sb.buf[sb.len-1] == '\t'))
     {
        sb.len--;
        x2--;
     }

   if (!_parse_color(&sb, &r, &g, &b, &a))
     goto end;


   found = EINA_TRUE;

end:
   termpty_backlog_unlock();
   ty_sb_free(&sb);
   if (found)
     {
        if (rp) *rp = r;
        if (gp) *gp = g;
        if (bp) *bp = b;
        if (ap) *ap = a;
        if (x1r) *x1r = x1;
        if (y1r) *y1r = y1 + sc;
        if (x2r) *x2r = x2;
        if (y2r) *y2r = y2 + sc;
     }
   return found;
}

#endif
#if defined(BINARY_TYTEST)
int
tytest_color_parse_hex(void)
{
   uint8_t v = 42;

   assert(_parse_hex('3', &v) == EINA_TRUE);
   assert(v == 3);

   assert(_parse_hex('b', &v) == EINA_TRUE);
   assert(v == 11);

   assert(_parse_hex('F', &v) == EINA_TRUE);
   assert(v == 15);

   assert(_parse_hex('@', &v) == EINA_FALSE);
   assert(_parse_hex('#', &v) == EINA_FALSE);
   assert(_parse_hex('G', &v) == EINA_FALSE);
   assert(_parse_hex('_', &v) == EINA_FALSE);
   assert(_parse_hex('g', &v) == EINA_FALSE);
   assert(_parse_hex('~', &v) == EINA_FALSE);

   return 0;
}

int
tytest_color_parse_2hex(void)
{
   uint8_t v = 42;

   assert(_parse_2hex("03", &v) == EINA_TRUE);
   assert(v == 3);

   assert(_parse_2hex("aF", &v) == EINA_TRUE);
   assert(v == 175);

   assert(_parse_2hex("44", &v) == EINA_TRUE);
   assert(v == 68);

   assert(_parse_2hex("gg", &v) == EINA_FALSE);
   assert(_parse_2hex("@3", &v) == EINA_FALSE);
   assert(_parse_2hex("#3", &v) == EINA_FALSE);
   assert(_parse_2hex("G3", &v) == EINA_FALSE);
   assert(_parse_2hex("_3", &v) == EINA_FALSE);
   assert(_parse_2hex("~3", &v) == EINA_FALSE);
   return 0;
}

int
tytest_color_parse_sharp(void)
{
   struct ty_sb sb = {};
   uint8_t r = 0, g = 0, b = 0, a = 0;

   /* 3 letters */
   assert(TY_SB_ADD(&sb, "#fab") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 240 && g == 160 && b == 176 && a == 255);
   ty_sb_free(&sb);

   /* 4 letters */
   assert(TY_SB_ADD(&sb, "#2345") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 32 && g == 48 && b == 64 && a == 80);
   ty_sb_free(&sb);

   /* 6 letters */
   assert(TY_SB_ADD(&sb, "#decfab") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 222 && g == 207 && b == 171 && a == 255);
   ty_sb_free(&sb);

   /* 8 letters */
   assert(TY_SB_ADD(&sb, "#fabdec86") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 250 && g == 189 && b == 236 && a == 134);
   ty_sb_free(&sb);

   /* upper case */
   assert(TY_SB_ADD(&sb, "#DECFAB") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 222 && g == 207 && b == 171 && a == 255);
   ty_sb_free(&sb);

   /* mixed case */
   assert(TY_SB_ADD(&sb, "#fAbDeC") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 250 && g == 189 && b == 236 && a == 255);
   ty_sb_free(&sb);

   /* Invalid*/
   /* improper start */
   assert(TY_SB_ADD(&sb, "~decfab") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);
   /* improper ending */
   assert(TY_SB_ADD(&sb, "#decfab;") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);
   /* invalid hex */
   assert(TY_SB_ADD(&sb, "#dgc") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "#dgca") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "#dgcaca") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "#dgcacaca") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "#dgcacaca") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);
   /* invalid length */
   assert(TY_SB_ADD(&sb, "#a") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "#da") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "#dadad") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "#dadadad") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "#dadadadad") == 0);
   assert(_parse_sharp_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);
   return 0;
}

int
tytest_color_parse_uint8(void)
{
   struct ty_sb sb = {};
   uint8_t v = 0;

   /* 1 digit (with/without ending) */
   assert(TY_SB_ADD(&sb, "3 ") == 0);
   assert(_parse_uint8(&sb, &v) == EINA_TRUE);
   assert(v == 3);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "4") == 0);
   assert(_parse_uint8(&sb, &v) == EINA_TRUE);
   assert(v == 4);
   ty_sb_free(&sb);

   /* 2 digit (with/without ending) */
   assert(TY_SB_ADD(&sb, "12 ") == 0);
   assert(_parse_uint8(&sb, &v) == EINA_TRUE);
   assert(v == 12);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "34") == 0);
   assert(_parse_uint8(&sb, &v) == EINA_TRUE);
   assert(v == 34);
   ty_sb_free(&sb);

   /* 3 digit (with/without ending) */
   assert(TY_SB_ADD(&sb, "123 ") == 0);
   assert(_parse_uint8(&sb, &v) == EINA_TRUE);
   assert(v == 123);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "234") == 0);
   assert(_parse_uint8(&sb, &v) == EINA_TRUE);
   assert(v == 234);
   ty_sb_free(&sb);

   /* ending with semicolon */
   assert(TY_SB_ADD(&sb, "42;") == 0);
   assert(_parse_uint8(&sb, &v) == EINA_TRUE);
   assert(v == 42);
   ty_sb_free(&sb);

   /* 0 padded */
   assert(TY_SB_ADD(&sb, "012") == 0);
   assert(_parse_uint8(&sb, &v) == EINA_TRUE);
   assert(v == 12);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "007") == 0);
   assert(_parse_uint8(&sb, &v) == EINA_TRUE);
   assert(v == 7);
   ty_sb_free(&sb);

   /* too large */
   assert(TY_SB_ADD(&sb, "256") == 0);
   assert(_parse_uint8(&sb, &v) == EINA_FALSE);
   ty_sb_free(&sb);
   /* too long */
   assert(TY_SB_ADD(&sb, "1234") == 0);
   assert(_parse_uint8(&sb, &v) == EINA_FALSE);
   ty_sb_free(&sb);
   /* not a digit */
   assert(TY_SB_ADD(&sb, "a") == 0);
   assert(_parse_uint8(&sb, &v) == EINA_FALSE);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, ".") == 0);
   assert(_parse_uint8(&sb, &v) == EINA_FALSE);
   ty_sb_free(&sb);

   return 0;
}

int
tytest_color_parse_edc(void)
{
   struct ty_sb sb = {};
   uint8_t r = 0, g = 0, b = 0, a = 0;

   /* color */
   assert(TY_SB_ADD(&sb, "  color:  51 153 255  32;") == 0);
   assert(_parse_edc_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 51 && g == 153 && b == 255 && a == 32);
   ty_sb_free(&sb);

   /* color2 */
   assert(TY_SB_ADD(&sb, "  color2:  244  99 93 127;") == 0);
   assert(_parse_edc_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 244 && g == 99 && b == 93 && a == 127);
   ty_sb_free(&sb);

   /* color3 */
   assert(TY_SB_ADD(&sb, "  color3:  149 181 16 234;") == 0);
   assert(_parse_edc_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 149 && g == 181 && b == 16 && a == 234);
   ty_sb_free(&sb);

   /* color: #color */
   assert(TY_SB_ADD(&sb, "  color:  #3399ff20") == 0);
   assert(_parse_edc_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 51 && g == 153 && b == 255 && a == 32);
   ty_sb_free(&sb);

   /* with tabs */
   assert(TY_SB_ADD(&sb, "  color\t:\t244\t99\t93\t127\t;") == 0);
   assert(_parse_edc_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 244 && g == 99 && b == 93 && a == 127);
   ty_sb_free(&sb);

   /* invalid */
   assert(TY_SB_ADD(&sb, "  color:  COL;") == 0);
   assert(_parse_edc_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);

   return 0;
}

int
tytest_color_parse_css_rgb(void)
{
   struct ty_sb sb = {};
   uint8_t r = 0, g = 0, b = 0, a = 0;

   /* (rgb) Functional syntax */
   assert(TY_SB_ADD(&sb, "rgb(255,1,153)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 255 && g == 1 && b == 153 && a == 255);
   ty_sb_free(&sb);

   assert(TY_SB_ADD(&sb, "rgb(254, 2, 152)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 254 && g == 2 && b == 152 && a == 255);
   ty_sb_free(&sb);

   assert(TY_SB_ADD(&sb, "rgb(253, 3, 151.0)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 253 && g == 3 && b == 151 && a == 255);
   ty_sb_free(&sb);

   /* (rgb) Percents */
   assert(TY_SB_ADD(&sb, "rgb(100%,4%,40%)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 255 && g == 10 && b == 102 && a == 255);
   ty_sb_free(&sb);

   assert(TY_SB_ADD(&sb, "rgb(50%, 0%, 60%)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 127 && g == 0 && b == 153 && a == 255);
   ty_sb_free(&sb);

   /* (rgb) ERROR! Don't mix numbers and percentages. */
   assert(TY_SB_ADD(&sb, "rgb(100%, 0, 60%)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);

   /* (rgb) Functional syntax with alpha value */
   assert(TY_SB_ADD(&sb, "rgb(254, 1, 150, 0)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 254 && g == 1 && b == 150 && a == 0);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "rgb(253, 2, 149, 1)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 253 && g == 2 && b == 149 && a == 255);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "rgb(252, 3, 148, 50%)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 252 && g == 3 && b == 148 && a == 128);
   ty_sb_free(&sb);

   /* (rgb) Whitespace syntax */
   assert(TY_SB_ADD(&sb, "rgb(255 0 153)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 255 && g == 0 && b == 153 && a == 255);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "rgb(254 1 152 / 0)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 254 && g == 1 && b == 152 && a == 0);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "rgb(253 2 151 / 100%)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 253 && g == 2 && b == 151 && a == 255);
   ty_sb_free(&sb);

   /* (rgb) Functional syntax with floats value */
   assert(TY_SB_ADD(&sb, "rgb(255, 0, 153.6, 1)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 255 && g == 0 && b == 154 && a == 255);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "rgb(1e2, .5e1, .5e0, +.25e2%)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 100 && g == 5 && b == 1 && a == 64);
   ty_sb_free(&sb);

   /* (rgba) Functional syntax */
   assert(TY_SB_ADD(&sb, "rgba(51, 170, 51, .1)") == 0); /*  10% opaque green */
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 51 && g == 170 && b == 51 && a == 26);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "rgba(50, 171, 52, .4)") == 0); /*  40% opaque green */
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 50 && g == 171 && b == 52 && a == 102);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "rgba(49, 172, 53, .7)") == 0); /*  70% opaque green */
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 49 && g == 172 && b == 53 && a == 179);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "rgba(48, 173, 54,  1)") == 0); /* full opaque green */
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 48 && g == 173 && b == 54 && a == 255);
   ty_sb_free(&sb);

   /* (rgba) Whitespace syntax */
   assert(TY_SB_ADD(&sb, "rgba(51 170 51 / 0.4)") == 0); /*  40% opaque green */
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 51 && g == 170 && b == 51 && a == 102);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "rgba(50 171 50 / 70%)") == 0); /*  40% opaque green */
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 50 && g == 171 && b == 50 && a == 179);
   ty_sb_free(&sb);
   /* (rgba) ERROR! invalid alpha */
   assert(TY_SB_ADD(&sb, "rgba(51 170 51 / 40)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_FALSE);
   ty_sb_free(&sb);

   /* (rgba) Functional syntax with floats value */
   assert(TY_SB_ADD(&sb, "rgba(255, 0, 153.6, 1)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 255 && g == 0 && b == 154 && a == 255);
   ty_sb_free(&sb);
   assert(TY_SB_ADD(&sb, "rgba(1e2, .5e1, .5e0, +.25e2%)") == 0);
   assert(_parse_css_rgb_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 100 && g == 5 && b == 1 && a == 64);
   ty_sb_free(&sb);

   return 0;
}

int
tytest_color_parse_css_hsl(void)
{
   struct ty_sb sb = {};
   uint8_t r = 0, g = 0, b = 0, a = 0;


   /* These examples all specify the same color: a lavender. */
   assert(TY_SB_ADD(&sb, "hsl(270,60%,70%)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 178 && g == 132 && b == 224 && a == 255);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   assert(TY_SB_ADD(&sb, "hsl(270, 60%, 70%)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 178 && g == 132 && b == 224 && a == 255);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   assert(TY_SB_ADD(&sb, "hsl(270 60% 70%)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 178 && g == 132 && b == 224 && a == 255);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   assert(TY_SB_ADD(&sb, "hsl(270deg, 60%, 70%)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 178 && g == 132 && b == 224 && a == 255);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   assert(TY_SB_ADD(&sb, "hsl(4.71239rad, 60%, 70%)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 178 && g == 132 && b == 224 && a == 255);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   assert(TY_SB_ADD(&sb, "hsl(300grad, 60%, 70%)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 178 && g == 132 && b == 224 && a == 255);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   assert(TY_SB_ADD(&sb, "hsl(.75turn, 60%, 70%)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 178 && g == 132 && b == 224 && a == 255);
   ty_sb_free(&sb);
   r = g = b = a = 0;


   /* These examples all specify the same color: a lavender that is 15% opaque. */
   assert(TY_SB_ADD(&sb, "hsl(270, 60%, 50%, .15)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 127 && g == 51 && b == 204 && a == 38);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   assert(TY_SB_ADD(&sb, "hsl(270, 60%, 50%, 15%)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 127 && g == 51 && b == 204 && a == 38);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   assert(TY_SB_ADD(&sb, "hsl(270 60% 50% / .15)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 127 && g == 51 && b == 204 && a == 38);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   assert(TY_SB_ADD(&sb, "hsl(270 60% 50% / 15%)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 127 && g == 51 && b == 204 && a == 38);
   ty_sb_free(&sb);
   r = g = b = a = 0;


   /* Different shades */
   assert(TY_SB_ADD(&sb, "hsla(240, 100%, 50%, .05)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 0 && g == 0 && b == 255 && a == 13);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   assert(TY_SB_ADD(&sb, "hsla(240, 100%, 50%, .4)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 0 && g == 0 && b == 255 && a == 102);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   assert(TY_SB_ADD(&sb, "hsla(240, 100%, 50%, .7)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 0 && g == 0 && b == 255 && a == 179);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   assert(TY_SB_ADD(&sb, "hsla(240, 100%, 50%, 1)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 0 && g == 0 && b == 255 && a == 255);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   /* Whitespace syntax */
   assert(TY_SB_ADD(&sb, "hsla(240 100% 50% / .05)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 0 && g == 0 && b == 255 && a == 13);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   /* Percentage value for alpha */
   assert(TY_SB_ADD(&sb, "hsla(240 100% 50% / 5%)") == 0);
   assert(_parse_css_hsl_color(&sb, &r, &g, &b, &a) == EINA_TRUE);
   assert(r == 0 && g == 0 && b == 255 && a == 13);
   ty_sb_free(&sb);
   r = g = b = a = 0;

   return 0;
}
#endif
#if defined(BINARY_TYFUZZ_COLOR_PARSER)
int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
   struct ty_sb sb = {};
   uint8_t r, g, b, a;

   assert(!ty_sb_add(&sb, (const char*)data, size));
   _parse_color(&sb, &r, &g, &b, &a);
   ty_sb_free(&sb);
   return 0;
}
#endif
