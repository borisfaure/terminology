#if defined(BINARY_TYTEST)

#include "private.h"

#include <Elementary.h>
#include <Ecore_Input.h>
#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "termpty.h"
#include "termptyops.h"
#include "keyin_encode.h"
#include "keyin.h"
#include "unit_tests.h"

/* keyin_encode_key() writes through termpty_write(), which appends to
 * ty->write_buffer and, with no fd handler attached, does nothing else. */

static Termpty *_ty;
static Config _cfg;

static void
_init(void)
{
   eina_init();

   _ty = calloc(1, sizeof(*_ty));
   assert(_ty);
   memset(&_cfg, 0, sizeof(_cfg));
   _ty->config = &_cfg;
   _ty->w = 80;
   _ty->h = 24;
   _ty->fd = -1;
   _ty->slavefd = -1;
   _ty->pid = -1;
}

static void
_shutdown(void)
{
   ty_sb_free(&_ty->write_buffer);
   free(_ty);
   _ty = NULL;
   eina_shutdown();
}

static void
_show(const char *s, size_t len)
{
   size_t i;

   for (i = 0; i < len; i++)
     {
        unsigned char c = s[i];

        if (c == 0x1b) fprintf(stderr, "ESC");
        else if ((c < 0x20) || (c >= 0x7f)) fprintf(stderr, "\\x%02x", c);
        else fprintf(stderr, "%c", c);
     }
}

/* Press a key and compare every byte it put on the pty. */
static int
_press_is(const char *key, Eina_Bool numlock, const char *string,
          int alt, int shift, int ctrl,
          const char *expected, size_t expected_len)
{
   struct ty_sb *sb = &_ty->write_buffer;
   Keyin_Event kev = {
      .key = key,
      .string = string,
      .mods = (alt ? KEYIN_MOD_ALT : 0) |
              (shift ? KEYIN_MOD_SHIFT : 0) |
              (ctrl ? KEYIN_MOD_CTRL : 0) |
              (numlock ? KEYIN_MOD_NUM_LOCK : 0),
      .action = KEYIN_ACTION_PRESS,
   };
   int bad = 0;

   ty_sb_free(sb);
   keyin_encode_event(_ty, &kev);

   if ((sb->len != expected_len) ||
       memcmp(sb->buf + sb->gap, expected, expected_len))
     {
        fprintf(stderr, "\n  %s%s%s%s: got '",
                ctrl ? "Ctrl+" : "", alt ? "Alt+" : "",
                shift ? "Shift+" : "", key);
        _show(sb->buf + sb->gap, sb->len);
        fprintf(stderr, "' want '");
        _show(expected, expected_len);
        fprintf(stderr, "'");
        bad = 1;
     }
   ty_sb_free(sb);
   return bad;
}

/* For a named key, where ev->string is never reached. */
#define PRESS(Key, Alt, Shift, Ctrl, Expected) \
   bad += _press_is(Key, EINA_FALSE, NULL, Alt, Shift, Ctrl, Expected, sizeof(Expected) - 1)

/* For an ordinary character, with the ev->string evas would deliver. */
#define PRESS_S(Key, String, Alt, Shift, Ctrl, Expected) \
   bad += _press_is(Key, EINA_FALSE, String, Alt, Shift, Ctrl, Expected, sizeof(Expected) - 1)

/* For a keypad key, where Num_Lock is a dimension of the case. */
#define PRESS_KP(Key, Numlock, String, Alt, Shift, Ctrl, Expected) \
   bad += _press_is(Key, Numlock, String, Alt, Shift, Ctrl, Expected, sizeof(Expected) - 1)

/* Legacy encoding, i.e. modifyOtherKeys off. */
int
tytest_keyin_legacy(void)
{
   int bad = 0;

   _init();

   /*      key          alt shift ctrl   expected */
   PRESS("Escape",        0,  0,   0,   "\033");
   PRESS("Escape",        1,  0,   0,   "\033\033");   /* not \x9b */
   PRESS("Return",        0,  0,   0,   "\r");
   PRESS("Return",        1,  0,   0,   "\033\r");
   PRESS("BackSpace",     0,  0,   0,   "\b");
   PRESS("BackSpace",     1,  0,   0,   "\033\b");
   PRESS("Tab",           0,  0,   0,   "\t");
   PRESS("Tab",           1,  0,   0,   "\033\t");    /* not \x89 */
   PRESS("Tab",           0,  1,   0,   "\033[Z");
   PRESS("Tab",           1,  1,   0,   "\033\033[Z");
   PRESS("space",         1,  0,   0,   "\033 ");     /* not \xa0 */
   PRESS("space",         1,  0,   1,   "\033\0");    /* not \x80 */
   PRESS("minus",         1,  1,   0,   "\033_");     /* not \xdf */
   PRESS("minus",         1,  0,   1,   "\033\x1f");
   PRESS("underscore",    1,  0,   1,   "\033\x1f");  /* not \x9f */
   PRESS("F1",            0,  0,   0,   "\033OP");
   PRESS("F1",            1,  0,   0,   "\033[1;3P"); /* not \033[O3P */
   PRESS("F1",            0,  0,   1,   "\033[1;5P");
   PRESS("Left",          0,  0,   0,   "\033[D");
   PRESS("Left",          1,  0,   0,   "\033[1;3D");

   /* ordinary characters come through ev->string, Alt prefixing an ESC */
   PRESS_S("a", "a",      0,  0,   0,   "a");
   PRESS_S("a", "a",      1,  0,   0,   "\033a");
   PRESS_S("a", "\001",   0,  0,   1,   "\001");
   PRESS_S("a", "\001",   1,  0,   1,   "\033\001");
   /* the Ctrl+digit table, which evas does not encode itself */
   PRESS_S("3", "3",      0,  0,   1,   "\033");
   PRESS_S("8", "8",      0,  0,   1,   "\177");

   /* erase_is_del turns an unmodified BackSpace into DEL */
   _cfg.erase_is_del = EINA_TRUE;
   PRESS("BackSpace",     0,  0,   0,   "\177");
   PRESS("BackSpace",     0,  0,   1,   "\b");
   _cfg.erase_is_del = EINA_FALSE;

   /* DECSET 1 -- application cursor keys */
   _ty->termstate.appcursor = EINA_TRUE;
   PRESS("Left",          0,  0,   0,   "\033OD");
   _ty->termstate.appcursor = EINA_FALSE;

   if (bad)
     fprintf(stderr, "\n  %d wrong encoding%s", bad, bad == 1 ? "" : "s");
   _shutdown();
   return bad;
}

/* modifyOtherKeys=2: CSI 27 ; Pm ; Pc ~ for the keys xterm's
 * ModifyOtherKeys() says qualify at that level. */
int
tytest_keyin_modify_other_keys(void)
{
   int bad = 0;

   _init();
   _ty->termstate.xmod[XMOD_OTHER] = 2;

   /* Escape, Return and Tab: any modifier */
   PRESS("Escape",        1,  0,   0,   "\033[27;3;27~");
   PRESS("Escape",        0,  1,   0,   "\033[27;2;27~");
   PRESS("Escape",        0,  0,   1,   "\033[27;5;27~");
   PRESS("Escape",        1,  1,   1,   "\033[27;8;27~");
   PRESS("Return",        1,  0,   0,   "\033[27;3;13~");
   PRESS("Tab",           1,  0,   0,   "\033[27;3;9~");
   PRESS("Tab",           0,  1,   0,   "\033[27;2;9~");

   /* unmodified keys keep the legacy encoding */
   PRESS("Escape",        0,  0,   0,   "\033");
   PRESS("Return",        0,  0,   0,   "\r");
   PRESS("Tab",           0,  0,   0,   "\t");

   /* BackSpace: any modifier but Control alone */
   PRESS("BackSpace",     1,  0,   0,   "\033[27;3;8~");
   PRESS("BackSpace",     0,  0,   1,   "\b");
   PRESS("BackSpace",     0,  1,   1,   "\033[27;6;8~");

   /* Ordinary keys, with the ev->string evas would deliver, so that these
    * also show the CSI 27 form winning over the legacy encoding.
    * Keysym 0x40..0x7f always qualifies, ... */
   PRESS_S("a", "\001",   0,  0,   1,   "\033[27;5;97~");
   PRESS_S("A", "A",      0,  1,   0,   "\033[27;2;65~");
   PRESS_S("bracketleft", "\033", 0, 0, 1, "\033[27;5;91~");
   /* ... so does anything with a modifier other than Shift, ... */
   PRESS_S("1", "1",      1,  0,   0,   "\033[27;3;49~");
   /* ... and Shift+space, but no other Shift-only combination. */
   PRESS_S("space", " ",  0,  1,   0,   "\033[27;2;32~");
   PRESS_S("1", "!",      0,  1,   0,   "!");

   /* function and cursor keys keep their parameterized forms */
   PRESS("F1",            0,  0,   1,   "\033[1;5P");
   PRESS("Left",          1,  0,   0,   "\033[1;3D");

   /* level 0 is unchanged: nothing is re-encoded */
   _ty->termstate.xmod[XMOD_OTHER] = 0;
   PRESS("Escape",        1,  0,   0,   "\033\033");
   PRESS_S("a", "\001",   0,  0,   1,   "\001");

   if (bad)
     fprintf(stderr, "\n  %d wrong encoding%s", bad, bad == 1 ? "" : "s");
   _shutdown();
   return bad;
}

/* modifyOtherKeys=1, foot and ghostty's tables for Return, Escape, Tab */
int
tytest_keyin_modify_other_keys_level1(void)
{
   int bad = 0;

   _init();
   _ty->termstate.xmod[XMOD_OTHER] = 1;

   /*      key          alt shift ctrl   expected */
   PRESS("Return",        0,  0,   0,   "\r");
   PRESS("Return",        1,  0,   0,   "\033\r");
   PRESS("Return",        0,  1,   0,   "\033[27;2;13~");
   PRESS("Return",        0,  0,   1,   "\033[27;5;13~");
   PRESS("Return",        0,  1,   1,   "\033[27;6;13~");
   PRESS("Return",        1,  1,   0,   "\033[27;4;13~");
   PRESS("Return",        1,  0,   1,   "\033[27;7;13~");
   PRESS("Return",        1,  1,   1,   "\033[27;8;13~");

   PRESS("Escape",        0,  0,   0,   "\033");
   PRESS("Escape",        1,  0,   0,   "\033\033");
   PRESS("Escape",        0,  1,   0,   "\033[27;2;27~");
   PRESS("Escape",        0,  0,   1,   "\033[27;5;27~");
   PRESS("Escape",        1,  0,   1,   "\033[27;7;27~");

   PRESS("Tab",           0,  0,   0,   "\t");
   PRESS("Tab",           1,  0,   0,   "\033\t");
   PRESS("Tab",           0,  0,   1,   "\033[27;5;9~");
   PRESS("Tab",           1,  0,   1,   "\033[27;7;9~");
   PRESS("ISO_Left_Tab",  0,  1,   0,   "\033[Z");
   PRESS("ISO_Left_Tab",  0,  1,   1,   "\033[27;6;9~");
   PRESS("ISO_Left_Tab",  1,  1,   0,   "\033[27;4;9~");

   /* BackSpace is never reported at level 1 */
   PRESS("BackSpace",     0,  1,   0,   "\b");
   PRESS("BackSpace",     1,  0,   0,   "\033\b");
   PRESS("BackSpace",     0,  0,   1,   "\b");

   /* Ctrl on a key with no C0 meaning */
   PRESS_S("1", "1",      0,  0,   1,   "\033[27;5;49~");
   PRESS_S("1", "1",      1,  0,   1,   "\033[27;7;49~");
   PRESS_S("semicolon", ";", 0, 0, 1,   "\033[27;5;59~");
   PRESS_S("exclam", "!", 0,  1,   1,   "\033[27;6;33~");
   /* ... but Ctrl+letter, Ctrl+2..8 and Alt or Shift alone stay legacy */
   PRESS_S("a", "\001",   0,  0,   1,   "\001");
   PRESS_S("A", "\001",   0,  1,   1,   "\001");
   PRESS_S("3", "3",      0,  0,   1,   "\033");
   PRESS_S("1", "1",      1,  0,   0,   "\033" "1");
   PRESS_S("exclam", "!", 0,  1,   0,   "!");

   /* function and cursor keys keep their own modifier encoding */
   PRESS("F1",            0,  0,   1,   "\033[1;5P");
   PRESS("Left",          0,  1,   0,   "\033[1;2D");

   /* application escape (7727) still wins */
   _ty->termstate.esc_keycode = 1;
   PRESS("Escape",        0,  0,   1,   "\033O[");
   _ty->termstate.esc_keycode = 0;

   /* level 0 is unchanged */
   _ty->termstate.xmod[XMOD_OTHER] = 0;
   PRESS("Return",        0,  1,   0,   "\r");
   PRESS("Tab",           0,  0,   1,   "\t");
   PRESS_S("1", "1",      0,  0,   1,   "1");

   if (bad)
     fprintf(stderr, "\n  %d wrong encoding%s", bad, bad == 1 ? "" : "s");
   _shutdown();
   return bad;
}

/* BackSpace: DEL by default, BS for Ctrl, under DECBKM, and when
 * erase_is_del is off. */
int
tytest_keyin_backspace(void)
{
   Config *cfg;
   int bad = 0;

   _init();

   /* the harness starts from a zeroed config; take the default from
    * config_new(), which is what a fresh Terminology runs with */
   cfg = config_new();
   _cfg.erase_is_del = cfg->erase_is_del;
   config_del(cfg);

   /*      key          alt shift ctrl   expected */
   PRESS("BackSpace",     0,  0,   0,   "\177");
   PRESS("BackSpace",     0,  0,   1,   "\b");

   /* DECBKM set (CSI ? 67 h): the backarrow key sends BS */
   _ty->termstate.send_bs = EINA_TRUE;
   PRESS("BackSpace",     0,  0,   0,   "\b");
   _ty->termstate.send_bs = EINA_FALSE;

   _cfg.erase_is_del = EINA_FALSE;
   PRESS("BackSpace",     0,  0,   0,   "\b");

   if (bad)
     fprintf(stderr, "\n  %d wrong encoding%s", bad, bad == 1 ? "" : "s");
   _shutdown();
   return bad;
}

/* Keypad keys under DECPAM (\033=) and Num_Lock: the numLock mode
 * (?1035) decides which of the two wins. */
int
tytest_keyin_keypad(void)
{
   int bad = 0;

   _init();

   /* ?1035 set, the default: Num_Lock wins, the digits come from
    * ev->string, whatever DECPAM says */
   _ty->termstate.numlock_mode = EINA_TRUE;
   _ty->termstate.alt_kp = EINA_TRUE;
   /*      key            nl string alt shift ctrl  expected */
   PRESS_KP("KP_7",       1, "7",   0,  0,    0,    "7");
   PRESS_KP("KP_Add",     1, "+",   0,  0,    0,    "+");
   PRESS_KP("KP_Enter",   1, "\r",  0,  0,    0,    "\r");
   _ty->termstate.alt_kp = EINA_FALSE;
   PRESS_KP("KP_7",       1, "7",   0,  0,    0,    "7");

   /* ?1035 reset, DECPAM set: DECPAM wins over Num_Lock, with the SS3
    * forms the terminfo entry advertises (ka1=\EOw, kb2=\EOu, kc3=\EOs,
    * kent=\EOM) */
   _ty->termstate.numlock_mode = EINA_FALSE;
   _ty->termstate.alt_kp = EINA_TRUE;
   PRESS_KP("KP_7",       1, "7",   0,  0,    0,    "\033Ow");
   PRESS_KP("KP_5",       1, "5",   0,  0,    0,    "\033Ou");
   PRESS_KP("KP_3",       1, "3",   0,  0,    0,    "\033Os");
   PRESS_KP("KP_1",       1, "1",   0,  0,    0,    "\033Oq");
   PRESS_KP("KP_Enter",   1, "\r",  0,  0,    0,    "\033OM");
   PRESS_KP("KP_Add",     1, "+",   0,  0,    0,    "\033Ok");
   PRESS_KP("KP_Decimal", 1, ".",   0,  0,    0,    "\033On");
   PRESS_KP("KP_7",       1, "7",   0,  1,    0,    "\033O2w");
   PRESS_KP("KP_5",       1, "5",   0,  0,    1,    "\033O5u");

   /* ?1035 reset, no DECPAM: Num_Lock keeps sending the digits */
   _ty->termstate.alt_kp = EINA_FALSE;
   PRESS_KP("KP_7",       1, "7",   0,  0,    0,    "7");

   /* Num_Lock off: unchanged for either mode and either DECPAM state */
   _ty->termstate.alt_kp = EINA_TRUE;
   _ty->termstate.appcursor = EINA_TRUE;
   PRESS_KP("KP_Home",    0, NULL,  0,  0,    0,    "\033OH");
   PRESS_KP("KP_Begin",   0, NULL,  0,  0,    0,    "\033OE");
   _ty->termstate.appcursor = EINA_FALSE;
   PRESS_KP("KP_Next",    0, NULL,  0,  0,    0,    "\033[6~");
   PRESS_KP("KP_Enter",   0, NULL,  0,  0,    0,    "\033OM");
   PRESS_KP("KP_Add",     0, NULL,  0,  0,    0,    "\033Ok");

   _ty->termstate.alt_kp = EINA_FALSE;
   PRESS_KP("KP_Up",      0, NULL,  0,  0,    0,    "\033[A");
   PRESS_KP("KP_Up",      0, NULL,  0,  1,    0,    "8");
   _ty->termstate.appcursor = EINA_TRUE;
   PRESS_KP("KP_Up",      0, NULL,  0,  0,    0,    "\033OA");
   _ty->termstate.appcursor = EINA_FALSE;

   /* ?1035 set again: Num_Lock off, DECPAM still wins */
   _ty->termstate.numlock_mode = EINA_TRUE;
   _ty->termstate.alt_kp = EINA_TRUE;
   PRESS_KP("KP_7",       0, NULL,  0,  0,    0,    "\033Ow");

   if (bad)
     fprintf(stderr, "\n  %d wrong encoding%s", bad, bad == 1 ? "" : "s");
   _shutdown();
   return bad;
}

#undef PRESS
#undef PRESS_S
#undef PRESS_KP

/* Flag 1, disambiguate escape codes:
 * https://sw.kovidgoyal.net/kitty/keyboard-protocol/ */
typedef struct tag_Kitty_Case
{
   unsigned int flags;
   const char *key;
   const char *keyname;
   const char *string;
   unsigned int mods;
   Keyin_Action action;
   const char *expected;   /* "" means nothing is written */
} Kitty_Case;

#define S  KEYIN_MOD_SHIFT
#define A  KEYIN_MOD_ALT
#define C  KEYIN_MOD_CTRL
#define CL KEYIN_MOD_CAPS_LOCK
#define NL KEYIN_MOD_NUM_LOCK
#define P  KEYIN_ACTION_PRESS
#define R  KEYIN_ACTION_REPEAT
#define U  KEYIN_ACTION_RELEASE

static int
_kitty_case_is(const Kitty_Case *c)
{
   struct ty_sb *sb = &_ty->write_buffer;
   Keyin_Event kev = {
      .key = c->key,
      .keyname = c->keyname,
      .string = c->string,
      .mods = c->mods,
      .action = c->action,
   };
   size_t expected_len = strlen(c->expected);
   int bad = 0;

   termpty_kitty_kbd_set(_ty, c->flags, 1);
   ty_sb_free(sb);
   keyin_encode_event(_ty, &kev);

   /* an empty buffer has no storage: memcmp() must not see its NULL */
   if ((sb->len != expected_len) ||
       ((expected_len > 0) &&
        (memcmp(sb->buf + sb->gap, c->expected, expected_len) != 0)))
     {
        fprintf(stderr, "\n  %s (mods %#x, action %d): got '",
                c->key, c->mods, c->action);
        _show(sb->buf + sb->gap, sb->len);
        fprintf(stderr, "' want '");
        _show(c->expected, expected_len);
        fprintf(stderr, "'");
        bad = 1;
     }
   ty_sb_free(sb);
   return bad;
}

int
tytest_keyin_kitty_disambiguate(void)
{
   static const Kitty_Case cases[] = {
        /* flags key             keyname  string   mods    action expected */
        { 1, "a",                "a",     "a",     0,      P,     "a" },
        { 1, "A",                "a",     "A",     S,      P,     "A" },
        { 1, "A",                "a",     "A",     CL,     P,     "A" },
        { 1, "a",                "a",     "\001",  C,      P,     "\033[97;5u" },
        { 1, "a",                "a",     "a",     A,      P,     "\033[97;3u" },
        { 1, "A",                "a",     "A",     C|S,    P,     "\033[97;6u" },
        { 1, "A",                "a",     "A",     A|S,    P,     "\033[97;4u" },
        { 1, "a",                "a",     "\001",  C|CL,   P,     "\033[97;69u" },
        { 1, "i",                "i",     "\t",    C,      P,     "\033[105;5u" },
        { 1, "Escape",           "Escape","\033",   0,      P,     "\033[27u" },
        { 1, "Escape",           "Escape","\033",   S,      P,     "\033[27;2u" },
        { 1, "Return",           "Return","\r",    0,      P,     "\r" },
        { 1, "Return",           "Return","\r",    NL,     P,     "\r" },
        { 1, "Return",           "Return","\r",    S,      P,     "\033[13;2u" },
        { 1, "Return",           "Return","\r",    C,      P,     "\033[13;5u" },
        { 1, "Tab",              "Tab",   "\t",    0,      P,     "\t" },
        { 1, "ISO_Left_Tab",     "Tab",   NULL,    S,      P,     "\033[9;2u" },
        { 1, "BackSpace",        "BackSpace", "\b", 0,     P,     "\b" },
        { 1, "BackSpace",        "BackSpace", "\b", C,     P,     "\033[127;5u" },
        { 1, "BackSpace",        "BackSpace", "\b", A,     P,     "\033[127;3u" },
        { 1, "Left",             "Left",  NULL,    0,      P,     "\033[D" },
        { 1, "Left",             "Left",  NULL,    C,      P,     "\033[1;5D" },
        { 1, "F1",               "F1",    NULL,    0,      P,     "\033[P" },
        { 1, "F3",               "F3",    NULL,    S,      P,     "\033[13;2~" },
        { 1, "F5",               "F5",    NULL,    0,      P,     "\033[15~" },
        { 1, "Delete",           "Delete","\177",  0,      P,     "\033[3~" },
        { 1, "Prior",            "Prior", NULL,    A,      P,     "\033[5;3~" },
        { 1, "F13",              "F13",   NULL,    0,      P,     "\033[57376u" },
        { 1, "KP_1",             "KP_End","1",     NL,     P,     "1" },
        { 1, "KP_End",           "KP_End", NULL,   0,      P,     "\033[57424u" },
        { 1, "KP_Enter",         "KP_Enter", "\r", NL,     P,     "\033[57414;129u" },
        { 1, "KP_Begin",         "KP_Begin", NULL, 0,      P,     "\033[E" },
        { 1, "Shift_L",          "Shift_L", NULL,  0,      P,     "" },
        { 1, "a",                "a",     NULL,    0,      U,     "" },
        { 1, "space",            "space", NULL,    C,      P,     "\033[32;5u" },
        { 1, "Cyrillic_es",      "c",     "\003",  C,      P,     "\033[1089;5u" },
        { 1, "exclam",           "1",     "!",     S,      P,     "!" },
        { 1, "exclam",           "1",     "!",     C|S,    P,     "\033[49;6u" },
        { 1, "at",               "q",     "@",     0,      P,     "@" },
        { 1, "XF86AudioMute",    "XF86AudioMute", NULL, 0, P,     "\033[57440u" },
        { 1, "a",                "a",     "a",     0,      R,     "a" },
        { 1, "eacute",           "eacute","\xc3\xa9", C,   P,     "\033[233;5u" },
   };
   char *saved_locale;
   size_t i;
   int bad = 0;

   /* towlower() needs a UTF-8 locale; the real terminal gets one from
    * elm_init(), the harness does not */
   saved_locale = strdup(setlocale(LC_CTYPE, NULL));
   assert(saved_locale);
   if (!setlocale(LC_CTYPE, "C.UTF-8"))
     {
        fprintf(stderr, "\n  no C.UTF-8 locale, skipping");
        free(saved_locale);
        return 0;
     }

   _init();
   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
     bad += _kitty_case_is(&cases[i]);

   /* DECCKM stays legacy: the protocol has no application cursor keys */
   {
      const Kitty_Case c = { 1, "Left", "Left", NULL, 0, P, "\033[D" };

      _ty->termstate.appcursor = EINA_TRUE;
      bad += _kitty_case_is(&c);
      _ty->termstate.appcursor = EINA_FALSE;
   }

   /* erase_is_del still decides what a legacy BackSpace sends */
   {
      const Kitty_Case c = { 1, "BackSpace", "BackSpace", "\b", 0, P, "\177" };

      _cfg.erase_is_del = EINA_TRUE;
      bad += _kitty_case_is(&c);
      _cfg.erase_is_del = EINA_FALSE;
   }

   /* LNM still decides what a legacy Return sends */
   {
      const Kitty_Case c = { 1, "Return", "Return", "\r", 0, P, "\r\n" };

      _ty->termstate.crlf = 1;
      bad += _kitty_case_is(&c);
      _ty->termstate.crlf = 0;
   }

   _shutdown();
   setlocale(LC_CTYPE, saved_locale);
   free(saved_locale);

   if (bad)
     fprintf(stderr, "\n  %d wrong encoding%s", bad, bad == 1 ? "" : "s");
   return bad;
}

int
tytest_keyin_kitty_alternates(void)
{
   static const Kitty_Case cases[] = {
        /* flags key             keyname  string     mods    action expected */
        { 5, "A",                "a",     "A",       S,      P,     "A" },
        { 5, "A",                "a",     "A",       C|S,    P,     "\033[97:65;6u" },
        { 5, "a",                "a",     "\001",    C,      P,     "\033[97;5u" },
        { 5, "exclam",           "1",     "!",       C|S,    P,     "\033[49:33;6u" },
        { 5, "Cyrillic_es",      "c",     NULL,      C,      P,     "\033[1089::99;5u" },
        { 5, "Cyrillic_ES",      "c",     "\xd0\xa1", C|S,   P,     "\033[1089:1057:99;6u" },
        { 5, "Left",             "Left",  NULL,      C|S,    P,     "\033[1;6D" },
        { 5, "KP_1",             "KP_End", NULL,     C|NL,   P,     "\033[57400;133u" },
        { 5, "plus",             "equal", "+",       C|S,    P,     "\033[61:43;6u" },
   };
   char *saved_locale;
   size_t i;
   int bad = 0;

   /* towlower() needs a UTF-8 locale; the real terminal gets one from
    * elm_init(), the harness does not */
   saved_locale = strdup(setlocale(LC_CTYPE, NULL));
   assert(saved_locale);
   if (!setlocale(LC_CTYPE, "C.UTF-8"))
     {
        fprintf(stderr, "\n  no C.UTF-8 locale, skipping");
        free(saved_locale);
        return 0;
     }

   _init();
   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
     bad += _kitty_case_is(&cases[i]);
   _shutdown();
   setlocale(LC_CTYPE, saved_locale);
   free(saved_locale);

   if (bad)
     fprintf(stderr, "\n  %d wrong encoding%s", bad, bad == 1 ? "" : "s");
   return bad;
}

int
tytest_keyin_kitty_events(void)
{
   static const Kitty_Case cases[] = {
        /* flags key             keyname  string   mods    action expected */
        { 3, "a",                "a",     "a",     0,      R,     "a" },
        { 3, "a",                "a",     NULL,    0,      U,     "\033[97;1:3u" },
        { 3, "a",                "a",     "\001",  C,      R,     "\033[97;5:2u" },
        { 3, "a",                "a",     NULL,    C,      U,     "\033[97;5:3u" },
        /* Return, Tab and BackSpace stay legacy without modifiers */
        { 3, "Return",           "Return", NULL,   0,      U,     "" },
        { 3, "Return",           "Return", "\r",   0,      R,     "\r" },
        { 3, "Tab",              "Tab",   NULL,    0,      U,     "" },
        { 3, "BackSpace",        "BackSpace", NULL, 0,     U,     "" },
        { 3, "Return",           "Return", NULL,   S,      U,     "\033[13;2:3u" },
        { 3, "Escape",           "Escape", NULL,   0,      U,     "\033[27;1:3u" },
        { 3, "Left",             "Left",  NULL,    0,      P,     "\033[D" },
        { 3, "Left",             "Left",  NULL,    0,      R,     "\033[1;1:2D" },
        { 3, "Left",             "Left",  NULL,    0,      U,     "\033[1;1:3D" },
        /* without flag 1, presses stay legacy, keeping DECCKM */
        { 2, "a",                "a",     "\001",  C,      P,     "\001" },
        { 2, "a",                "a",     NULL,    C,      U,     "\033[97;5:3u" },
        { 2, "Escape",           "Escape", NULL,   0,      U,     "\033[27;1:3u" },
        { 7, "A",                "a",     NULL,    S,      U,     "\033[97:65;2:3u" },
        /* a release keeps the base key of its press */
        { 7, "Cyrillic_es",      "c",     NULL,    C,      P,     "\033[1089::99;5u" },
        { 7, "Cyrillic_es",      "c",     NULL,    C,      U,     "\033[1089::99;5:3u" },
   };
   Kitty_Case appcursor = { 2, "Left", "Left", NULL, 0, P, "\033OD" };
   char *saved_locale;
   size_t i;
   int bad = 0;

   /* towlower() needs a UTF-8 locale; the real terminal gets one from
    * elm_init(), the harness does not */
   saved_locale = strdup(setlocale(LC_CTYPE, NULL));
   assert(saved_locale);
   if (!setlocale(LC_CTYPE, "C.UTF-8"))
     {
        fprintf(stderr, "\n  no C.UTF-8 locale, skipping");
        free(saved_locale);
        return 0;
     }

   _init();
   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
     bad += _kitty_case_is(&cases[i]);
   _ty->termstate.appcursor = EINA_TRUE;
   bad += _kitty_case_is(&appcursor);
   _ty->termstate.appcursor = EINA_FALSE;
   _shutdown();
   setlocale(LC_CTYPE, saved_locale);
   free(saved_locale);

   if (bad)
     fprintf(stderr, "\n  %d wrong encoding%s", bad, bad == 1 ? "" : "s");
   return bad;
}

#undef S
#undef A
#undef C
#undef CL
#undef NL
#undef P
#undef R
#undef U

static int
_keysym_is(const char *name, Eina_Unicode want)
{
   Eina_Unicode cp = keyin_keysym_codepoint(name);

   if (cp == want)
     return 0;
   fprintf(stderr, "\n  %s: got %#x, want %#x", name, (unsigned)cp,
           (unsigned)want);
   return 1;
}

int
tytest_keyin_keysyms(void)
{
   int bad = 0;

#define IS(Name, Cp) bad += _keysym_is(Name, Cp)

   IS("a", 0x61);
   IS("exclam", 0x21);
   IS("Cyrillic_es", 0x0441);
   IS("Cyrillic_ES", 0x0421);
   IS("eacute", 0x00e9);
   IS("U1F600", 0x1f600);
   /* too short for the Uxxxx form, and not in the table either */
   IS("U20", 0);
   IS("F1", 0);
   IS("", 0);
   IS("Keycode-123", 0);

#undef IS

   return bad;
}

int
tytest_keyin_pressed(void)
{
   Keys_Handler khdl;
   Evas_Event_Key_Down down;
   Evas_Event_Key_Up up;
   unsigned int i;

   /* keyin_key_down_track() only reads keycode, key and keyname, so a
    * stack-allocated event is enough */
   memset(&khdl, 0, sizeof(khdl));
   memset(&down, 0, sizeof(down));
   down.key = "q";
   down.keyname = "q";
   down.keycode = 24;
   memset(&up, 0, sizeof(up));
   up.keycode = 24;

   _init();

   assert(keyin_key_down_track(&khdl, &down) == KEYIN_ACTION_PRESS);
   assert(keyin_key_down_track(&khdl, &down) == KEYIN_ACTION_REPEAT);
   /* the press was not reported, so no release */
   assert(keyin_key_up_track(&khdl, &up) == EINA_FALSE);
   assert(keyin_key_down_track(&khdl, &down) == KEYIN_ACTION_PRESS);
   keyin_key_reported_set(&khdl, 24);
   assert(keyin_key_up_track(&khdl, &up) == EINA_TRUE);
   assert(keyin_key_up_track(&khdl, &up) == EINA_FALSE);

   /* a 17th key is not tracked: no release for it */
   for (i = 0; i < KEYIN_PRESSED_MAX; i++)
     {
        down.keycode = 100 + i;
        assert(keyin_key_down_track(&khdl, &down) == KEYIN_ACTION_PRESS);
     }
   down.keycode = 200;
   assert(keyin_key_down_track(&khdl, &down) == KEYIN_ACTION_PRESS);
   up.keycode = 200;
   assert(keyin_key_up_track(&khdl, &up) == EINA_FALSE);

   /* a key held across a focus out is a fresh press afterwards */
   down.keycode = 100;
   assert(keyin_key_down_track(&khdl, &down) == KEYIN_ACTION_REPEAT);
   keyin_pressed_reset(&khdl);
   assert(keyin_key_down_track(&khdl, &down) == KEYIN_ACTION_PRESS);
   keyin_pressed_reset(&khdl);

   _shutdown();
   return 0;
}

#endif
