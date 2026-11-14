#include "private.h"

#include <Elementary.h>
#include <Ecore_IMF.h>
#include "termpty.h"
#include "termptyops.h"
#include "keyin_encode.h"
#include "keyin_kitty.h"
#include "keyin.h"

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

static int
_xmod_other_keys_level(const Termpty *ty)
{
   return ty->termstate.xmod[XMOD_OTHER];
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

/* modifyOtherKeys=1, foot and ghostty's tables: Return and Escape are
 * reported whenever Shift or Ctrl is held, Alt included in Pm */
static Eina_Bool
_xmod_level1_wants_return_or_escape(int shift, int ctrl)
{
   return shift || ctrl;
}

/* modifyOtherKeys=1: Ctrl on a key that has no C0 meaning, like foot's
 * !ctrl_seq. Keysym 0x40..0x7f and xterm's control aliases (Ctrl+Space,
 * Ctrl+2..8, Ctrl+'/') stay C0, and so does anything evas already turned
 * into a single control byte. */
static Eina_Bool
_xmod_level1_wants_other(int code, const char *string, int ctrl)
{
   if (!ctrl)
     return EINA_FALSE;
   if ((code >= 0x40) && (code <= 0x7f))
     return EINA_FALSE;
   if ((code == ' ') || (code == '/') || ((code >= '2') && (code <= '8')))
     return EINA_FALSE;
   if (string && string[0] && !string[1] &&
       (((unsigned char)string[0] < 0x20) || (string[0] == 0x7f)))
     return EINA_FALSE;
   return EINA_TRUE;
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

static void
_encode_legacy(Termpty *ty, const Keyin_Event *kev)
{
   const char *key = kev->key;
   const char *string = kev->string;
   const int alt = (kev->mods & KEYIN_MOD_ALT) != 0;
   const int shift = (kev->mods & KEYIN_MOD_SHIFT) != 0;
   const int ctrl = (kev->mods & KEYIN_MOD_CTRL) != 0;
   const Eina_Bool numlock = (kev->mods & KEYIN_MOD_NUM_LOCK) != 0;
   int level = _xmod_other_keys_level(ty);

   if (kev->action == KEYIN_ACTION_RELEASE)
     return;

   if (!strcmp(key, "BackSpace"))
     {
        const Config *cfg = ty->config;
        char code = '\b';

        if (!ty->termstate.send_bs && cfg->erase_is_del && !ctrl)
          code = '\177';

        /* xterm strips Control here, as per IsBackarrowToggle() */
        if ((level == 2) && (alt || shift))
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
        else if (((level == 2) && (alt || shift || ctrl)) ||
                 ((level == 1) &&
                  _xmod_level1_wants_return_or_escape(shift, ctrl)))
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
        if (((level == 2) && (alt || shift || ctrl)) ||
            ((level == 1) &&
             _xmod_level1_wants_return_or_escape(shift, ctrl)))
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
   if (level == 2)
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
   if ((level == 1) &&
       (!strcmp(key, "Tab") || !strcmp(key, "ISO_Left_Tab")))
     {
        /* evas reports Shift+Tab as ISO_Left_Tab, sometimes without the
         * Shift bit, so the keysym carries it */
        int tab_shift = shift || !strcmp(key, "ISO_Left_Tab");

        if (ctrl || (tab_shift && alt))
          {
             _xmod_write(ty, '\t', alt, tab_shift, ctrl);
             return;
          }
     }
   if (level == 1)
     {
        int code = _xmod_key_code(key);

        if ((code >= 0) && _xmod_level1_wants_other(code, string, ctrl))
          {
             _xmod_write(ty, code, alt, shift, ctrl);
             return;
          }
     }
   if (!strncmp(key, "KP_", 3))
     {
        /* ?1035, numLock, default set: Num_Lock overrides DECPAM */
        if (ty->termstate.alt_kp && !(ty->termstate.numlock_mode && numlock))
          {
             if (_key_try(ty, tty_keys_kp_app,
                          sizeof(tty_keys_kp_app)/sizeof(tty_keys_kp_app[0]),
                          key, alt, shift, ctrl))
               return;
          }
        else if (!numlock)
          {
             if (_key_try(ty, tty_keys_kp_plain,
                          sizeof(tty_keys_kp_plain)/sizeof(tty_keys_kp_plain[0]),
                          key, alt, shift, ctrl))
               return;
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

void
keyin_encode_event(Termpty *ty, const Keyin_Event *kev)
{
   unsigned int flags;

   if (!kev->key)
     return;

   flags = termpty_kitty_kbd_flags_get(ty);
   if (flags && keyin_kitty_encode(ty, kev, flags))
     return;

   _encode_legacy(ty, kev);
}

/* {{{ Pressed key tracking */

/* the spec reports a release only for a key whose press was reported */

/* mods with the spec's bit for a modifier key's own press or release */
unsigned int
keyin_mods_with_modifier_key(unsigned int mods, const Keys_Handler *khdl,
                             const char *key, Keyin_Action action)
{
   static const struct {
      const char *left, *right;
      unsigned int mod;
   } modifiers[] = {
      { "Shift_L",   "Shift_R",   KEYIN_MOD_SHIFT },
      { "Control_L", "Control_R", KEYIN_MOD_CTRL },
      { "Alt_L",     "Alt_R",     KEYIN_MOD_ALT },
      { "Super_L",   "Super_R",   KEYIN_MOD_SUPER },
      { "Meta_L",    "Meta_R",    KEYIN_MOD_META },
   };
   unsigned int i, j, lock;

   for (i = 0; i < sizeof(modifiers) / sizeof(modifiers[0]); i++)
     {
        const char *other_side = NULL;

        if (!strcmp(key, modifiers[i].left))
          other_side = modifiers[i].right;
        else if (!strcmp(key, modifiers[i].right))
          other_side = modifiers[i].left;
        if (!other_side)
          continue;
        if (action != KEYIN_ACTION_RELEASE)
          return mods | modifiers[i].mod;
        for (j = 0; j < khdl->pressed_count; j++)
          if (khdl->pressed[j].key &&
              !strcmp(khdl->pressed[j].key, other_side))
            return mods;
        return mods & ~modifiers[i].mod;
     }

   if (!strcmp(key, "Caps_Lock"))
     lock = KEYIN_MOD_CAPS_LOCK;
   else if (!strcmp(key, "Num_Lock"))
     lock = KEYIN_MOD_NUM_LOCK;
   else
     return mods;
   /* a lock key's new state shows in the next event only: toggle it on
    * press, and keep on release what the press sent, whenever the
    * platform applies the change */
   if (action != KEYIN_ACTION_RELEASE)
     return mods ^ lock;
   for (j = 0; j < khdl->pressed_count; j++)
     if (khdl->pressed[j].key && !strcmp(khdl->pressed[j].key, key))
       return (mods & ~lock) | (khdl->pressed[j].locks & lock);
   return mods;
}

static Keyin_Pressed_Key *
_pressed_key_get(Keys_Handler *khdl, unsigned int keycode)
{
   unsigned int i;

   for (i = 0; i < khdl->pressed_count; i++)
     if (khdl->pressed[i].keycode == keycode)
       return &khdl->pressed[i];
   return NULL;
}

Keyin_Action
keyin_key_down_track(Keys_Handler *khdl, const Evas_Event_Key_Down *ev)
{
   Keyin_Pressed_Key *pk;

   pk = _pressed_key_get(khdl, ev->keycode);
   if (pk)
     return KEYIN_ACTION_REPEAT;
   /* a key past the table gets no release: 16 held keys is plenty */
   if (khdl->pressed_count >= KEYIN_PRESSED_MAX)
     return KEYIN_ACTION_PRESS;
   pk = &khdl->pressed[khdl->pressed_count++];
   pk->keycode = ev->keycode;
   pk->key = eina_stringshare_add(ev->key);
   pk->locks = 0;
   pk->reported = 0;
   return KEYIN_ACTION_PRESS;
}

void
keyin_key_reported_set(Keys_Handler *khdl, unsigned int keycode,
                       unsigned int mods)
{
   Keyin_Pressed_Key *pk = _pressed_key_get(khdl, keycode);

   if (!pk)
     return;
   pk->reported = 1;
   pk->locks = mods & KEYIN_MOD_LOCKS;
}

Eina_Bool
keyin_key_reported_get(const Keys_Handler *khdl, unsigned int keycode)
{
   unsigned int i;

   for (i = 0; i < khdl->pressed_count; i++)
     if (khdl->pressed[i].keycode == keycode)
       return khdl->pressed[i].reported;
   return EINA_FALSE;
}

Eina_Bool
keyin_key_up_track(Keys_Handler *khdl, const Evas_Event_Key_Up *ev)
{
   Keyin_Pressed_Key *pk = _pressed_key_get(khdl, ev->keycode);
   Eina_Bool reported = EINA_FALSE;

   if (!pk)
     return EINA_FALSE;
   reported = pk->reported;
   eina_stringshare_del(pk->key);
   *pk = khdl->pressed[--khdl->pressed_count];
   return reported;
}

void
keyin_pressed_reset(Keys_Handler *khdl)
{
   unsigned int i;

   for (i = 0; i < khdl->pressed_count; i++)
     eina_stringshare_del(khdl->pressed[i].key);
   khdl->pressed_count = 0;
}

/* }}} */
