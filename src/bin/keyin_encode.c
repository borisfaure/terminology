#include "private.h"

#include <Elementary.h>
#include "termpty.h"
#include "keyin_encode.h"

typedef struct tag_Tty_Key Tty_Key;
typedef struct tag_Key_Values Key_Values;

struct _s {
    char *s;
    ssize_t len;
};

struct tag_Key_Values {
    struct _s plain;
    struct _s alt;
    struct _s ctrl;
    struct _s ctrl_alt;
    struct _s shift;
    struct _s shift_alt;
    struct _s shift_ctrl;
    struct _s shift_ctrl_alt;
};
struct tag_Tty_Key
{
    char *key;
    int key_len;
    Key_Values default_mode;
    Key_Values cursor;
};

/* {{{ Keys to TTY */

/* {{{ modifyOtherKeys */

static Eina_Bool
_xmod_other_keys(const Termpty *ty)
{
   return ty->termstate.xmod[XMOD_OTHER] == 2;
}

static void
_xmod_write(Termpty *ty, int code, int alt, int shift, int ctrl)
{
   char buf[32];
   int param = 1 + (shift ? 1 : 0) + (alt ? 2 : 0) + (ctrl ? 4 : 0);
   int len = snprintf(buf, sizeof(buf), "\033[27;%d;%d~", param, code);

   termpty_write(ty, buf, len);
}

/* xterm's ModifyOtherKeys(), modifyOtherKeys=2, for an ordinary key */
static Eina_Bool
_xmod_wants_other(int code, int alt, int shift, int ctrl)
{
   if (!alt && !shift && !ctrl)
     return EINA_FALSE;
   if ((code >= 0x40) && (code <= 0x7f))
     return EINA_TRUE;
   if (alt || ctrl)
     return EINA_TRUE;
   return (code == ' ');
}

/* Keysym name -> the character xterm would report for it, or -1 */
static int
_xmod_key_code(const char *key)
{
   static const struct {
      const char *name;
      char code;
   } named[] = {
      { "space", ' ' },        { "exclam", '!' },       { "quotedbl", '"' },
      { "numbersign", '#' },   { "dollar", '$' },       { "percent", '%' },
      { "ampersand", '&' },    { "apostrophe", '\'' },  { "parenleft", '(' },
      { "parenright", ')' },   { "asterisk", '*' },     { "plus", '+' },
      { "comma", ',' },        { "minus", '-' },        { "period", '.' },
      { "slash", '/' },        { "colon", ':' },        { "semicolon", ';' },
      { "less", '<' },         { "equal", '=' },        { "greater", '>' },
      { "question", '?' },     { "at", '@' },           { "bracketleft", '[' },
      { "backslash", '\\' },   { "bracketright", ']' }, { "asciicircum", '^' },
      { "underscore", '_' },   { "grave", '`' },        { "braceleft", '{' },
      { "bar", '|' },          { "braceright", '}' },   { "asciitilde", '~' },
   };
   unsigned int i;

   if (key[0] && !key[1] && (key[0] >= 0x20) && (key[0] < 0x7f))
     return key[0];
   for (i = 0; i < sizeof(named)/sizeof(named[0]); i++)
     if (!strcmp(key, named[i].name))
       return named[i].code;
   return -1;
}

/* }}} */

static Eina_Bool
_key_try(Termpty *ty, const Tty_Key *map, int len, const char *key,
         int alt, int shift, int ctrl)
{
   int i, inlen;

   inlen = strlen(key);
   for (i = 0; i < len; i++)
     {
        if ((inlen == map[i].key_len) && (!memcmp(key, map[i].key, inlen)))
          {
             const struct _s *s = NULL;
             const Key_Values *kv;

             if (!ty->termstate.appcursor) kv = &map[i].default_mode;
             else                          kv = &map[i].cursor;
             if (!alt && !ctrl && !shift)     s = &kv->plain;
             else if (alt && !ctrl && !shift) s = &kv->alt;
             else if (!alt && ctrl && !shift) s = &kv->ctrl;
             else if (alt && ctrl && !shift)  s = &kv->ctrl_alt;
             else if (!alt && !ctrl && shift) s = &kv->shift;
             else if (alt && !ctrl && shift)  s = &kv->shift_alt;
             else if (!alt && ctrl && shift)  s = &kv->shift_ctrl;
             else if (alt && ctrl && shift)   s = &kv->shift_ctrl_alt;

             if (s) termpty_write(ty, s->s, s->len);
             return EINA_TRUE;
          }
     }
   return EINA_FALSE;
}

/* }}} */

#include "tty_keys.h"

void
keyin_encode_key(Termpty *ty, const char *key, const char *string,
                 Eina_Bool numlock, const int alt, const int shift,
                 const int ctrl)
{
   if (!strcmp(key, "BackSpace"))
     {
        const Config *cfg = ty->config;
        char code = '\b';

        if (!ty->termstate.send_bs && cfg->erase_is_del && !ctrl)
          code = '\177';

        /* xterm strips Control here, as per IsBackarrowToggle() */
        if (_xmod_other_keys(ty) && (alt || shift))
          {
             _xmod_write(ty, code, alt, shift, ctrl);
             return;
          }
        if (alt)
          termpty_write(ty, "\033", 1);
        termpty_write(ty, &code, 1);
        return;
     }
   if (!strcmp(key, "Escape"))
     {
        if (ty->termstate.esc_keycode)
          {
             TERMPTY_WRITE_STR("\033O[");
          }
        else if (ty->termstate.alternate_esc)
          {
             TERMPTY_WRITE_STR("\034");
          }
        else if (_xmod_other_keys(ty) && (alt || shift || ctrl))
          {
             _xmod_write(ty, '\033', alt, shift, ctrl);
          }
        else
          {
             if (alt)
               {
                  TERMPTY_WRITE_STR("\033\033");
               }
             else
               {
                  TERMPTY_WRITE_STR("\033");
               }
          }
        return;
     }
   if (!strcmp(key, "Return"))
     {
        if (_xmod_other_keys(ty) && (alt || shift || ctrl))
          {
             _xmod_write(ty, '\r', alt, shift, ctrl);
             return;
          }
        if (alt)
          termpty_write(ty, "\033", 1);
        if (ty->termstate.crlf)
          {
             termpty_write(ty, "\r\n", sizeof("\r\n") - 1);
             return;
          }
        else
          {
             termpty_write(ty, "\r", sizeof("\r") - 1);
             return;
          }
     }
   if (_xmod_other_keys(ty))
     {
        if (!strcmp(key, "Tab") || !strcmp(key, "ISO_Left_Tab"))
          {
             if (alt || shift || ctrl)
               {
                  _xmod_write(ty, '\t', alt, shift, ctrl);
                  return;
               }
          }
        else
          {
             int code = _xmod_key_code(key);

             if ((code >= 0) && _xmod_wants_other(code, alt, shift, ctrl))
               {
                  _xmod_write(ty, code, alt, shift, ctrl);
                  return;
               }
          }
     }
   if (!strncmp(key, "KP_", 3))
     {
        if (!numlock)
          {
             if (ty->termstate.alt_kp)
               {
                  if (_key_try(ty, tty_keys_kp_app,
                               sizeof(tty_keys_kp_app)/sizeof(tty_keys_kp_app[0]),
                               key, alt, shift, ctrl))
                    return;
               }
             else
               {
                  if (_key_try(ty, tty_keys_kp_plain,
                               sizeof(tty_keys_kp_plain)/sizeof(tty_keys_kp_plain[0]),
                               key, alt, shift, ctrl))
                    return;
               }
          }
     }
   else
     if (_key_try(ty, tty_keys, sizeof(tty_keys)/sizeof(tty_keys[0]), key,
                  alt, shift, ctrl))
       return;

   if (ctrl)
     {
#define CTRL_NUM(Num, Code)                        \
        if (!strcmp(key, Num))                 \
          {                                        \
             if (alt)                              \
               termpty_write(ty, "\033"Code, 2);   \
             else                                  \
               termpty_write(ty, Code, 1);         \
             return;                               \
          }
        CTRL_NUM("2", "\0")
        CTRL_NUM("3", "\x1b")
        CTRL_NUM("4", "\x1c")
        CTRL_NUM("5", "\x1d")
        CTRL_NUM("6", "\x1e")
        CTRL_NUM("7", "\x1f")
        CTRL_NUM("8", "\x7f")

#undef CTRL_NUM
     }

   if (string)
     {
        if (alt)
          termpty_write(ty, "\033", 1);
        termpty_write(ty, string, strlen(string));
     }
}
