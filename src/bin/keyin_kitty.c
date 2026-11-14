#include "private.h"

#include <Elementary.h>
#include <stdio.h>
#include <string.h>
#include <wctype.h>

#include "termpty.h"
#include "keyin_encode.h"
#include "keyin_kitty.h"

/* https://sw.kovidgoyal.net/kitty/keyboard-protocol/#functional */

typedef struct tag_Kitty_Functional_Key
{
   const char *name;
   unsigned short number;
   char final;
   unsigned char is_modifier : 1;
} Kitty_Functional_Key;

/* sorted by name for bsearch */
static const Kitty_Functional_Key kitty_functional_keys[] = {
     { "Alt_L", 57443, 'u', 1 },
     { "Alt_R", 57449, 'u', 1 },
     { "BackSpace", 127, 'u', 0 },
     { "Caps_Lock", 57358, 'u', 1 },
     { "Control_L", 57442, 'u', 1 },
     { "Control_R", 57448, 'u', 1 },
     { "Delete", 3, '~', 0 },
     { "Down", 1, 'B', 0 },
     { "End", 1, 'F', 0 },
     { "Escape", 27, 'u', 0 },
     { "F1", 1, 'P', 0 },
     { "F10", 21, '~', 0 },
     { "F11", 23, '~', 0 },
     { "F12", 24, '~', 0 },
     { "F13", 57376, 'u', 0 },
     { "F14", 57377, 'u', 0 },
     { "F15", 57378, 'u', 0 },
     { "F16", 57379, 'u', 0 },
     { "F17", 57380, 'u', 0 },
     { "F18", 57381, 'u', 0 },
     { "F19", 57382, 'u', 0 },
     { "F2", 1, 'Q', 0 },
     { "F20", 57383, 'u', 0 },
     { "F21", 57384, 'u', 0 },
     { "F22", 57385, 'u', 0 },
     { "F23", 57386, 'u', 0 },
     { "F24", 57387, 'u', 0 },
     { "F25", 57388, 'u', 0 },
     { "F26", 57389, 'u', 0 },
     { "F27", 57390, 'u', 0 },
     { "F28", 57391, 'u', 0 },
     { "F29", 57392, 'u', 0 },
     { "F3", 13, '~', 0 },
     { "F30", 57393, 'u', 0 },
     { "F31", 57394, 'u', 0 },
     { "F32", 57395, 'u', 0 },
     { "F33", 57396, 'u', 0 },
     { "F34", 57397, 'u', 0 },
     { "F35", 57398, 'u', 0 },
     { "F4", 1, 'S', 0 },
     { "F5", 15, '~', 0 },
     { "F6", 17, '~', 0 },
     { "F7", 18, '~', 0 },
     { "F8", 19, '~', 0 },
     { "F9", 20, '~', 0 },
     { "Home", 1, 'H', 0 },
     { "Hyper_L", 57445, 'u', 1 },
     { "Hyper_R", 57451, 'u', 1 },
     { "ISO_Left_Tab", 9, 'u', 0 },
     { "ISO_Level3_Shift", 57453, 'u', 1 },
     { "ISO_Level5_Shift", 57454, 'u', 1 },
     { "Insert", 2, '~', 0 },
     { "KP_0", 57399, 'u', 0 },
     { "KP_1", 57400, 'u', 0 },
     { "KP_2", 57401, 'u', 0 },
     { "KP_3", 57402, 'u', 0 },
     { "KP_4", 57403, 'u', 0 },
     { "KP_5", 57404, 'u', 0 },
     { "KP_6", 57405, 'u', 0 },
     { "KP_7", 57406, 'u', 0 },
     { "KP_8", 57407, 'u', 0 },
     { "KP_9", 57408, 'u', 0 },
     { "KP_Add", 57413, 'u', 0 },
     { "KP_Begin", 1, 'E', 0 },
     { "KP_Decimal", 57409, 'u', 0 },
     { "KP_Delete", 57426, 'u', 0 },
     { "KP_Divide", 57410, 'u', 0 },
     { "KP_Down", 57420, 'u', 0 },
     { "KP_End", 57424, 'u', 0 },
     { "KP_Enter", 57414, 'u', 0 },
     { "KP_Equal", 57415, 'u', 0 },
     { "KP_Home", 57423, 'u', 0 },
     { "KP_Insert", 57425, 'u', 0 },
     { "KP_Left", 57417, 'u', 0 },
     { "KP_Multiply", 57411, 'u', 0 },
     { "KP_Next", 57422, 'u', 0 },
     { "KP_Prior", 57421, 'u', 0 },
     { "KP_Right", 57418, 'u', 0 },
     { "KP_Separator", 57416, 'u', 0 },
     { "KP_Subtract", 57412, 'u', 0 },
     { "KP_Up", 57419, 'u', 0 },
     { "Left", 1, 'D', 0 },
     { "Menu", 57363, 'u', 0 },
     { "Meta_L", 57446, 'u', 1 },
     { "Meta_R", 57452, 'u', 1 },
     { "Next", 6, '~', 0 },
     { "Num_Lock", 57360, 'u', 1 },
     { "Pause", 57362, 'u', 0 },
     { "Print", 57361, 'u', 0 },
     { "Prior", 5, '~', 0 },
     { "Return", 13, 'u', 0 },
     { "Right", 1, 'C', 0 },
     { "Scroll_Lock", 57359, 'u', 1 },
     { "Shift_L", 57441, 'u', 1 },
     { "Shift_R", 57447, 'u', 1 },
     { "Super_L", 57444, 'u', 1 },
     { "Super_R", 57450, 'u', 1 },
     { "Tab", 9, 'u', 0 },
     { "Up", 1, 'A', 0 },
     { "XF86AudioForward", 57433, 'u', 0 },
     { "XF86AudioLowerVolume", 57438, 'u', 0 },
     { "XF86AudioMute", 57440, 'u', 0 },
     { "XF86AudioNext", 57435, 'u', 0 },
     { "XF86AudioPause", 57429, 'u', 0 },
     { "XF86AudioPlay", 57428, 'u', 0 },
     { "XF86AudioPrev", 57436, 'u', 0 },
     { "XF86AudioRaiseVolume", 57439, 'u', 0 },
     { "XF86AudioRecord", 57437, 'u', 0 },
     { "XF86AudioRewind", 57434, 'u', 0 },
     { "XF86AudioStop", 57432, 'u', 0 },
};

static int
_kitty_functional_key_cmp(const void *name, const void *elem)
{
   return strcmp(name, ((const Kitty_Functional_Key *)elem)->name);
}

static const Kitty_Functional_Key *
_kitty_functional_key_find(const char *name)
{
   return bsearch(name, kitty_functional_keys,
                  sizeof(kitty_functional_keys) / sizeof(kitty_functional_keys[0]),
                  sizeof(kitty_functional_keys[0]),
                  _kitty_functional_key_cmp);
}

/* the spec's text: no C0, DEL or C1, and none with Ctrl, Alt or Super */
static Eina_Bool
_kitty_text_ok(const Keyin_Event *kev)
{
   const char *s = kev->string;
   int idx = 0;

   if (!s || !s[0])
     return EINA_FALSE;
   if (kev->action == KEYIN_ACTION_RELEASE)
     return EINA_FALSE;
   if (kev->mods & (KEYIN_MOD_CTRL | KEYIN_MOD_ALT | KEYIN_MOD_SUPER))
     return EINA_FALSE;
   while (s[idx])
     {
        Eina_Unicode cp = eina_unicode_utf8_next_get(s, &idx);

        if (cp < 0x20)
          return EINA_FALSE;
        if (cp == 0x7f)
          return EINA_FALSE;
        if (cp >= 0x80 && cp < 0xa0)
          return EINA_FALSE;
     }
   return EINA_TRUE;
}

/* the lowercased codepoint, or the unshifted key's with Shift held */
static Eina_Unicode
_kitty_key_code(const Keyin_Event *kev)
{
   Eina_Unicode cp = keyin_keysym_codepoint(kev->key);
   Eina_Unicode lower;

   if (!cp)
     return 0;
   lower = towlower((wint_t)cp);
   if (lower != cp)
     return lower;
   if (kev->mods & KEYIN_MOD_SHIFT)
     {
        Eina_Unicode base = keyin_keysym_codepoint(kev->keyname);

        if (base)
          return base;
     }
   return cp;
}

/* CSI number[:shifted[:base]|::base][;mods+1[:action]][;text] final */
typedef struct tag_Kitty_Sequence
{
   unsigned int number;       /* omitted when 1 and nothing follows it */
   char final;
   Eina_Unicode shifted;      /* 0 when not reported */
   Eina_Unicode base;         /* 0 when not reported */
   unsigned int mods;
   int action;                /* Keyin_Action, 0 when not reported */
   const char *text;          /* UTF-8, NULL when not reported */
} Kitty_Sequence;

/* Appends the codepoints of text, ;cp:cp…; truncated before it overflows size */
static int
_kitty_text_codepoints_append(char *buf, int size, const char *text)
{
   int len = 0;
   int idx = 0;

   while (text[idx])
     {
        char tmp[16];
        Eina_Unicode cp = eina_unicode_utf8_next_get(text, &idx);
        int written = snprintf(tmp, sizeof(tmp), len == 0 ? ";%u" : ":%u", cp);

        if (written > size)
          break;
        memcpy(buf + len, tmp, written);
        len += written;
        size -= written;
     }
   return len;
}

static void
_kitty_serialize(Termpty *ty, const Kitty_Sequence *seq)
{
   char buf[128];
   int len;
   Eina_Bool has_tail;

   has_tail = seq->mods != 0 || seq->action != 0 || seq->text != NULL;

   memcpy(buf, "\033[", 2);
   len = 2;
   if (seq->number != 1 || has_tail || seq->shifted != 0 || seq->base != 0)
     len += snprintf(buf + len, sizeof(buf) - len, "%u", seq->number);
   if (seq->shifted != 0)
     len += snprintf(buf + len, sizeof(buf) - len, ":%u", seq->shifted);
   if (seq->base != 0)
     {
        if (seq->shifted != 0)
          len += snprintf(buf + len, sizeof(buf) - len, ":%u", seq->base);
        else
          len += snprintf(buf + len, sizeof(buf) - len, "::%u", seq->base);
     }
   if (has_tail)
     {
        buf[len++] = ';';
        if (seq->mods != 0 || seq->action != 0)
          len += snprintf(buf + len, sizeof(buf) - len, "%u", seq->mods + 1);
        if (seq->action != 0)
          len += snprintf(buf + len, sizeof(buf) - len, ":%d", seq->action);
        if (seq->text != NULL)
          len += _kitty_text_codepoints_append(buf + len, sizeof(buf) - len - 1,
                                               seq->text);
     }
   buf[len] = seq->final;
   len++;
   termpty_write(ty, buf, len);
}

/* leaves these keys to erase_is_del, send_bs and crlf */
static Eina_Bool
_kitty_legacy_exception(const Keyin_Event *kev)
{
   if (kev->mods & ~KEYIN_MOD_LOCKS)
     return EINA_FALSE;
   if (strcmp(kev->key, "Return") == 0 ||
       strcmp(kev->key, "Tab") == 0 ||
       strcmp(kev->key, "BackSpace") == 0)
     return EINA_TRUE;
   /* evas may report Shift+Tab as ISO_Left_Tab without the Shift bit */
   if (strcmp(kev->key, "ISO_Left_Tab") == 0 &&
       !(kev->mods & KEYIN_MOD_SHIFT))
     return EINA_TRUE;
   return EINA_FALSE;
}

Eina_Bool
keyin_kitty_encode(Termpty *ty, const Keyin_Event *kev, unsigned int flags)
{
   const Kitty_Functional_Key *fk = _kitty_functional_key_find(kev->key);
   Kitty_Sequence seq =
     {
        .final = 'u',
     };
   Eina_Bool text_ok;

   /* modifier keys send nothing unless all keys are reported */
   if (fk && fk->is_modifier && !(flags & TERMPTY_KITTY_KBD_REPORT_ALL))
     return EINA_TRUE;

   /* releases send nothing unless event types are reported */
   if (kev->action == KEYIN_ACTION_RELEASE &&
       !(flags & TERMPTY_KITTY_KBD_REPORT_EVENTS))
     return EINA_TRUE;

   text_ok = _kitty_text_ok(kev);

   /* legacy presses keep DECCKM, DECKPAM and modifyOtherKeys */
   if (!(flags & (TERMPTY_KITTY_KBD_DISAMBIGUATE |
                  TERMPTY_KITTY_KBD_REPORT_ALL)) &&
       kev->action != KEYIN_ACTION_RELEASE)
     return EINA_FALSE;

   if (!(flags & TERMPTY_KITTY_KBD_REPORT_ALL))
     {
        if (text_ok)
          {
             termpty_write(ty, kev->string, strlen(kev->string));
             return EINA_TRUE;
          }
        if (_kitty_legacy_exception(kev))
          {
             if (kev->action == KEYIN_ACTION_RELEASE)
               return EINA_TRUE;
             return EINA_FALSE;
          }
     }

   if (fk)
     {
        seq.number = fk->number;
        seq.final = fk->final;
     }
   else
     {
        seq.number = _kitty_key_code(kev);
        if (seq.number == 0)
          {
             /* the text is all this key has to report */
             if ((flags & TERMPTY_KITTY_KBD_REPORT_ALL) && text_ok)
               termpty_write(ty, kev->string, strlen(kev->string));
             return EINA_TRUE;
          }
     }

   seq.mods = kev->mods;
   if ((flags & TERMPTY_KITTY_KBD_REPORT_EVENTS) &&
       kev->action != KEYIN_ACTION_PRESS)
     seq.action = kev->action;
   if ((flags & TERMPTY_KITTY_KBD_REPORT_TEXT) &&
       (flags & TERMPTY_KITTY_KBD_REPORT_ALL) && text_ok)
     seq.text = kev->string;

   _kitty_serialize(ty, &seq);
   return EINA_TRUE;
}
