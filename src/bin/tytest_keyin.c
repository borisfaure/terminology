#if defined(BINARY_TYTEST)

#include "private.h"

#include <Elementary.h>
#include <Ecore_Input.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "termpty.h"
#include "keyin_encode.h"
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
_press_is(const char *key, const char *string,
          int alt, int shift, int ctrl,
          const char *expected, size_t expected_len)
{
   struct ty_sb *sb = &_ty->write_buffer;
   int bad = 0;

   ty_sb_free(sb);
   keyin_encode_key(_ty, key, string, EINA_FALSE, alt, shift, ctrl);

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
   bad += _press_is(Key, NULL, Alt, Shift, Ctrl, Expected, sizeof(Expected) - 1)

/* For an ordinary character, with the ev->string evas would deliver. */
#define PRESS_S(Key, String, Alt, Shift, Ctrl, Expected) \
   bad += _press_is(Key, String, Alt, Shift, Ctrl, Expected, sizeof(Expected) - 1)

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

   /* level 1 is not level 2: nothing is re-encoded */
   _ty->termstate.xmod[XMOD_OTHER] = 1;
   PRESS("Escape",        1,  0,   0,   "\033\033");
   PRESS_S("a", "\001",   0,  0,   1,   "\001");

   if (bad)
     fprintf(stderr, "\n  %d wrong encoding%s", bad, bad == 1 ? "" : "s");
   _shutdown();
   return bad;
}

#undef PRESS
#undef PRESS_S

#endif
