#include "private.h"
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "private.h"
#include <Elementary.h>
#include "config.h"
#include "termpty.h"
#include "termptyops.h"
#include "backlog.h"
#include "utf8.h"
#include "simd/simd.h"
#include "termiointernals.h"
#include "tytest.h"
#include "unit_tests.h"
#include "tytest_common.h"

#include "md5.h"

int _log_domain = -1;

/* {{{ Unit tests */

static struct {
     const char *name;
     tytest_func func;
} _tytests[] = {
       { "dummy", tytest_dummy },
       { "simd_parity", tytest_simd_parity},
       { "rgb_to_palette", tytest_rgb_to_palette},
       { "sb_skip", tytest_sb_skip},
       { "sb_trim", tytest_sb_trim},
       { "sb_gap", tytest_sb_gap},
       { "sb_steal", tytest_sb_steal},
       { "color_parse_hex", tytest_color_parse_hex},
       { "color_parse_2hex", tytest_color_parse_2hex},
       { "color_parse_sharp", tytest_color_parse_sharp},
       { "color_parse_uint8", tytest_color_parse_uint8},
       { "color_parse_edc", tytest_color_parse_edc},
       { "color_parse_css_rgb", tytest_color_parse_css_rgb},
       { "color_parse_css_hsl", tytest_color_parse_css_hsl},
       { "extn_matching", tytest_extn_matching},
       { "base64", tytest_base64},
       { "shell_quote", tytest_shell_quote},
       { "sync_frame_coherence", tytest_sync_frame_coherence},
       { "sync_watchdog_teardown", tytest_sync_watchdog_teardown},
       { "sync_nested", tytest_sync_nested},
       { "sync_resize", tytest_sync_resize},
       { "altscreen_resize_keeps_content", tytest_altscreen_resize_keeps_content},
       { "sync_soft_reset", tytest_sync_soft_reset},
       { "sync_change_cb_coalesced", tytest_sync_change_cb_coalesced},
       { "sync_change_cb_watchdog", tytest_sync_change_cb_watchdog},
       { "sync_change_cb_altscreen", tytest_sync_change_cb_altscreen},
       { "percent_decode", tytest_percent_decode},
       { "xmodkeys_set", tytest_xmodkeys_set},
       { "xmodkeys_query", tytest_xmodkeys_query},
       { "kitty_keyboard_ignored", tytest_kitty_keyboard_ignored},
       { "vs16_regression_no_scroll", tytest_vs16_regression_no_scroll},
       { "vs16_bare_narrow", tytest_vs16_bare_narrow},
       { "vs16_widens_one_call", tytest_vs16_widens_one_call},
       { "vs16_widens_split_calls", tytest_vs16_widens_split_calls},
       { "vs16_no_room_no_wrap", tytest_vs16_no_room_no_wrap},
       { "vs16_base_table_unaffected", tytest_vs16_base_table_unaffected},
       { "vs16_guard_invalidated_by_cursor_move", tytest_vs16_guard_invalidated_by_cursor_move},
       { "vs16_cjk_ambiguous_wide", tytest_vs16_cjk_ambiguous_wide},
       { "vs16_guard_invalidated_by_ht", tytest_vs16_guard_invalidated_by_ht},
       { "vs16_guard_invalidated_by_decom", tytest_vs16_guard_invalidated_by_decom},
       { "vs16_guard_invalidated_by_decstbm", tytest_vs16_guard_invalidated_by_decstbm},
       { "vs16_guard_invalidated_by_decrc", tytest_vs16_guard_invalidated_by_decrc},
       { "osc52_read_gate", tytest_osc52_read_gate},
       { "osc52_read_ask", tytest_osc52_read_ask},
       { "osc52_long_max", tytest_osc52_long_max},
       { NULL, NULL},
};

static int
_run_this_tytest(const char *name, tytest_func func)
{
   int res;
   fprintf(stderr, "\033[0m%s...", name);
   res = func();
   fprintf(stderr, " %s\033[0m\n", res == 0 ? "\033[32m✔" : "\033[31;1m×");
   return res;
}

static tytest_func
_find_tytest(const char *name)
{
   int ntests = (sizeof(_tytests) / sizeof(_tytests[0])) - 1;
   int i;

   for (i = 0; i < ntests; i++)
     {
        if (strcmp(name, _tytests[i].name) == 0)
          return _tytests[i].func;
     }
   return NULL;
}

static int
_run_all_tytests(void)
{
   int ntests = (sizeof(_tytests) / sizeof(_tytests[0])) - 1;
   int i, res = 0;

   for (i = 0; res == 0 && i < ntests; i++)
     res = _run_this_tytest(_tytests[i].name, _tytests[i].func);
   return res;
}

static int
_run_tytests(int argc, char **argv)
{
   int i, res = 0;

   for (i = 1; res == 0 && i < argc; i++)
     {
        if (strncmp(argv[i], "all", strlen("all")) == 0)
          res = _run_all_tytests();
        else
          {
             tytest_func func = _find_tytest(argv[i]);
             if (!func)
               {
                  fprintf(stderr, "can not find test named '%s'\n", argv[i]);
                  return -1;
               }
             res = _run_this_tytest(argv[i], func);
          }
     }
   return res;
}

/* }}} */

/* Frozen copy of the Termatt layout used when these checksums were
 * recorded.
 *
 * _tytest_checksum() hashes terminal state as raw bytes, so any change to
 * the size or layout of Termatt, Termcell or Term_State would invalidate
 * every line of tests/tests.results at once. Hashing through this frozen
 * image instead keeps the byte stream stable: cells are converted to it
 * field by field, so a layout change only shows up in the checksum when it
 * actually changes the terminal state. New fields with no equivalent here
 * are hashed separately, after the image, only when they are set -- the
 * same trick used for the working directory below. */
typedef struct tag_tytest_termatt_v1
{
   uint8_t fg, bg;
   unsigned short bold : 1;
   unsigned short faint : 1;
   unsigned short italic : 1;
   unsigned short dblwidth : 1;
   unsigned short underline : 1;
   unsigned short blink : 1;
   unsigned short blink2 : 1;
   unsigned short inverse : 1;
   unsigned short invisible : 1;
   unsigned short strike : 1;
   unsigned short fg256 : 1;
   unsigned short bg256 : 1;
   unsigned short fgintense : 1;
   unsigned short bgintense : 1;
   unsigned short autowrapped : 1;
   unsigned short newline : 1;
   unsigned short fraktur : 1;
   unsigned short framed : 1;
   unsigned short encircled : 1;
   unsigned short overlined : 1;
   unsigned short tab_inserted : 1;
   unsigned short tab_last : 1;
#if defined(SUPPORT_80_132_COLUMNS)
   unsigned short is_80_132_mode_allowed : 1;
   unsigned short bit_padding :  9;
#else
   unsigned short bit_padding : 10;
#endif
   uint16_t       link_id;
} __attribute((__packed__)) tytest_termatt_v1;

typedef struct tag_Term_State_V1 {
    tytest_termatt_v1 att;
    unsigned char charset;
    unsigned char charsetch;
    unsigned char chset[4];
    int           top_margin, bottom_margin;
    int           left_margin, right_margin;
    int           had_cr_x, had_cr_y;
    unsigned int  lr_margins : 1;
    unsigned int  restrict_cursor : 1;
    unsigned int  multibyte : 1;
    unsigned int  alt_kp : 1;
    unsigned int  insert : 1;
    unsigned int  appcursor : 1;
    unsigned int  wrap : 1;
    unsigned int  crlf : 1;
    unsigned int  send_bs : 1;
    unsigned int  kbd_lock : 1;
    unsigned int  reverse : 1;
    unsigned int  no_autorepeat : 1;
    unsigned int  cjk_ambiguous_wide : 1;
    unsigned int  hide_cursor : 1;
    unsigned int  combining_strike : 1;
    unsigned int  sace_rectangular : 1;
    unsigned int  esc_keycode : 1;
    unsigned int  alternate_esc : 1;
    int xmod[XMOD_LAST];
} Term_State_V1;

typedef struct tag_tytest_termcell_v1
{
   Eina_Unicode codepoint;
   tytest_termatt_v1 att;
} tytest_termcell_v1;

/* The V1 images have no implicit padding: every field is copied, and the
 * padding bits are memset to zero, which is how they are always left. */
_Static_assert(sizeof(tytest_termatt_v1) == 8, "frozen Termatt layout changed");
_Static_assert(sizeof(tytest_termcell_v1) == 12, "frozen Termcell layout changed");

static void
_termatt_to_v1(const Termatt *att, tytest_termatt_v1 *v1)
{
   memset(v1, '\0', sizeof(*v1));
   v1->fg = att->fg;
   v1->bg = att->bg;
   v1->bold = att->bold;
   v1->faint = att->faint;
   v1->italic = att->italic;
   v1->dblwidth = att->dblwidth;
   v1->underline = att->underline;
   v1->blink = att->blink;
   v1->blink2 = att->blink2;
   v1->inverse = att->inverse;
   v1->invisible = att->invisible;
   v1->strike = att->strike;
   v1->fg256 = att->fg256;
   v1->bg256 = att->bg256;
   v1->fgintense = att->fgintense;
   v1->bgintense = att->bgintense;
   v1->autowrapped = att->autowrapped;
   v1->newline = att->newline;
   v1->fraktur = att->fraktur;
   v1->framed = att->framed;
   v1->encircled = att->encircled;
   v1->overlined = att->overlined;
   v1->tab_inserted = att->tab_inserted;
   v1->tab_last = att->tab_last;
#if defined(SUPPORT_80_132_COLUMNS)
   v1->is_80_132_mode_allowed = att->is_80_132_mode_allowed;
#endif
   v1->link_id = att->link_id;
}

static void
_termstate_to_v1(const Term_State *ts, Term_State_V1 *v1)
{
   memset(v1, '\0', sizeof(*v1));
   _termatt_to_v1(&ts->att, &v1->att);
   v1->charset = ts->charset;
   v1->charsetch = ts->charsetch;
   memcpy(v1->chset, ts->chset, sizeof(v1->chset));
   v1->top_margin = ts->top_margin;
   v1->bottom_margin = ts->bottom_margin;
   v1->left_margin = ts->left_margin;
   v1->right_margin = ts->right_margin;
   v1->had_cr_x = ts->had_cr_x;
   v1->had_cr_y = ts->had_cr_y;
   v1->lr_margins = ts->lr_margins;
   v1->restrict_cursor = ts->restrict_cursor;
   v1->multibyte = ts->multibyte;
   v1->alt_kp = ts->alt_kp;
   v1->insert = ts->insert;
   v1->appcursor = ts->appcursor;
   v1->wrap = ts->wrap;
   v1->crlf = ts->crlf;
   v1->send_bs = ts->send_bs;
   v1->kbd_lock = ts->kbd_lock;
   v1->reverse = ts->reverse;
   v1->no_autorepeat = ts->no_autorepeat;
   v1->cjk_ambiguous_wide = ts->cjk_ambiguous_wide;
   v1->hide_cursor = ts->hide_cursor;
   v1->combining_strike = ts->combining_strike;
   v1->sace_rectangular = ts->sace_rectangular;
   v1->esc_keycode = ts->esc_keycode;
   v1->alternate_esc = ts->alternate_esc;
   memcpy(v1->xmod, ts->xmod, sizeof(v1->xmod));
}

static void
_termcells_to_v1(const Termcell *cells, ssize_t w, tytest_termcell_v1 *out)
{
   ssize_t i;

   for (i = 0; i < w; i++)
     {
        out[i].codepoint = cells[i].codepoint;
        _termatt_to_v1(&cells[i].att, &out[i].att);
     }
}

typedef struct tag_Termpty_Tests
{
   uint64_t backsize, backpos;
   Backlog_Beacon backlog_beacon;
   Term_State_V1 termstate;
   Term_Cursor cursor_state;
   Term_Cursor cursor_save[2];
   int w, h;
   uint64_t altbuf     : 1;
   uint64_t mouse_mode : 3;
   uint64_t mouse_ext  : 2;
   uint64_t bracketed_paste : 1;
} __attribute((__packed__)) Termpty_Tests;

static void
_termpty_to_termpty_tests(Termpty *ty, Termpty_Tests *tt)
{
   memset(tt, '\0', sizeof(*tt));
   tt->backsize = ty->backsize;
   tt->backpos = ty->backpos;
   tt->backlog_beacon = ty->backlog_beacon;
   {
      /* Local copy: tt is a packed struct, so &tt->termstate is unaligned. */
      Term_State_V1 v1;

      _termstate_to_v1(&ty->termstate, &v1);
      memcpy(&tt->termstate, &v1, sizeof(v1));
   }
   tt->cursor_state = ty->cursor_state;
   tt->cursor_save[0] = ty->cursor_save[0];
   tt->cursor_save[1] = ty->cursor_save[1];
   tt->w = ty->w;
   tt->h = ty->h;
   tt->altbuf = ty->altbuf;
   tt->mouse_mode = ty->mouse_mode;
   tt->mouse_ext = ty->mouse_ext;
   tt->bracketed_paste = ty->bracketed_paste;
}

/* Fold the scrollback into the checksum.
 *
 * Only backsize/backpos/beacon used to be covered, so everything that had
 * scrolled off the screen was invisible to the tests -- which is precisely
 * where a bug in the scroll or text-append path shows up first. Rows are read
 * back through termpty_cellrow_get() rather than reached into directly, so the
 * test keeps comparing what a reader of the backlog would see even if how a
 * row is stored changes. */
static void
_checksum_backlog(Termpty *ty, MD5_CTX *ctx)
{
   ssize_t len = termpty_backlog_length(ty);
   int y;

   for (y = 1; y <= (int)len; y++)
     {
        const Termcell *cells;
        ssize_t w = 0;
        uint32_t width;

        cells = termpty_cellrow_get(ty, -y, &w);
        if (!cells || (w < 0)) w = 0;
        /* Fixed width, not sizeof(ssize_t): the checksum is compared across
         * machines and must not depend on the size of a pointer. */
        width = (uint32_t)w;
        MD5Update(ctx, (unsigned char const*)&width, sizeof(width));
        if (cells && (w > 0))
          {
             tytest_termcell_v1 *v1 = malloc(w * sizeof(*v1));

             if (v1)
               {
                  _termcells_to_v1(cells, w, v1);
                  MD5Update(ctx, (unsigned char const*)v1, sizeof(*v1) * w);
                  free(v1);
               }
          }
     }
}

static void
_checksum_screen(MD5_CTX *ctx, const Termcell *cells, int w, int h)
{
   tytest_termcell_v1 *v1 = malloc((size_t)w * h * sizeof(*v1));

   if (!v1) return;
   _termcells_to_v1(cells, (ssize_t)w * h, v1);
   MD5Update(ctx, (unsigned char const*)v1, sizeof(*v1) * (size_t)w * h);
   free(v1);
}

static void
_tytest_checksum(Termpty *ty)
{
   MD5_CTX ctx;
   Termpty_Tests tests;
   char md5out[(2 * MD5_HASHBYTES) + 1];
   unsigned char hash[MD5_HASHBYTES];
   static const char hex[] = "0123456789abcdef";
   int n;

   _termpty_to_termpty_tests(ty, &tests);

   MD5Init(&ctx);
   /* Termpty */
   MD5Update(&ctx,
             (unsigned char const*)&tests,
             sizeof(tests));
   /* The screens */
   _checksum_screen(&ctx, ty->screen, ty->w, ty->h);
   _checksum_screen(&ctx, ty->screen2, ty->w, ty->h);
   /* The scrollback */
   _checksum_backlog(ty, &ctx);
   /* Icon/Title */
   if (ty->prop.icon)
     {
        MD5Update(&ctx,
                  (unsigned char const*)ty->prop.icon,
                  strlen(ty->prop.icon));
     }
   else
     {
        MD5Update(&ctx, (unsigned char const*)"(NULL)", 6);
     }
   if (ty->prop.title)
     {
        MD5Update(&ctx,
                  (unsigned char const*)ty->prop.title,
                  strlen(ty->prop.title));
     }
   else
     {
        MD5Update(&ctx, (unsigned char const*)"(NULL)", 6);
     }
   /* Working directory. Hashed only when set, with no "(NULL)" sentinel, so
    * that the byte stream is unchanged for every test that never sends
    * OSC 7. An empty value is rejected at parse time, so "unset" and "set to
    * empty" cannot be confused. */
   if (ty->prop.cwd)
     {
        MD5Update(&ctx,
                  (unsigned char const*)ty->prop.cwd,
                  strlen(ty->prop.cwd));
     }
   /* Cursor shape */
   const char *cursor_shape = tytest_cursor_shape_get();
   MD5Update(&ctx, (unsigned char const*)cursor_shape,
             strlen(cursor_shape));
   /* Write buffer */
   if (ty->write_buffer.len)
     {
        MD5Update(&ctx, (unsigned char const*)ty->write_buffer.buf,
                  ty->write_buffer.len);
     }

   MD5Final(hash, &ctx);

   for (n = 0; n < MD5_HASHBYTES; n++)
     {
        md5out[2 * n] = hex[hash[n] >> 4];
        md5out[2 * n + 1] = hex[hash[n] & 0x0f];
     }
   md5out[2 * MD5_HASHBYTES] = '\0';
   printf("%s", md5out);
}


/* Render a cell's codepoint into a dump-safe form.
 *
 * Anything that would move the cursor or otherwise talk back to the terminal
 * displaying the dump has to be escaped, or reading a dump would garble the
 * reader's own screen. */
static void
_dump_codepoint(Eina_Unicode g)
{
   char utf8[8];
   int n;

   if (g == 0)
     {
        putchar(' ');
        return;
     }
   /* Media blocks are encoded with bit 31 set and are not text at all. */
   if (g & 0x80000000)
     {
        printf("\\B");
        return;
     }
   if (g < 0x20 || g == 0x7f)
     {
        printf("\\x%02x", (unsigned int)g);
        return;
     }
   n = codepoint_to_utf8(g, utf8);
   if (n <= 0)
     {
        printf("\\u%04x", (unsigned int)g);
        return;
     }
   fwrite(utf8, 1, n, stdout);
}

/* Emit one row as text plus a run-length summary of its attributes.
 *
 * Attributes are summarised rather than printed per cell because the point is
 * to make a diff between two dumps land on the cell that actually differs,
 * without burying it in eighty identical attribute records. */
static void
_dump_row(const Termcell *cells, int w, const char *label)
{
   int x, start;

   printf("%s |", label);
   for (x = 0; x < w; x++)
     _dump_codepoint(cells[x].codepoint);
   printf("|\n");

   x = 0;
   while (x < w)
     {
        const Termatt *a = &cells[x].att;

        start = x;
        while ((x < w) &&
               (memcmp(&cells[x].att, a, sizeof(Termatt)) == 0))
          x++;
        /* Skip the default run: saying nothing is clearer than saying nothing
         * verbosely, and it keeps a clean screen's dump short. */
        if ((a->fg != 0) || (a->bg != 0) || a->bold || a->faint || a->italic ||
            a->underline || a->blink || a->blink2 || a->inverse ||
            a->invisible || a->strike || a->fg256 || a->bg256 ||
            a->fgintense || a->bgintense || a->dblwidth || a->autowrapped ||
            a->newline || a->fraktur || a->framed || a->encircled ||
            a->overlined || a->link_id)
          {
             printf("%s  att %d-%d fg=%u bg=%u", label, start, x - 1,
                    (unsigned)a->fg, (unsigned)a->bg);
             if (a->fg256)      printf(" fg256");
             if (a->bg256)      printf(" bg256");
             if (a->fgintense)  printf(" fgint");
             if (a->bgintense)  printf(" bgint");
             if (a->bold)       printf(" bold");
             if (a->faint)      printf(" faint");
             if (a->italic)     printf(" italic");
             if (a->underline)  printf(" underline");
             if (a->blink)      printf(" blink");
             if (a->blink2)     printf(" blink2");
             if (a->inverse)    printf(" inverse");
             if (a->invisible)  printf(" invisible");
             if (a->strike)     printf(" strike");
             if (a->dblwidth)   printf(" dblwidth");
             if (a->autowrapped) printf(" autowrapped");
             if (a->newline)    printf(" newline");
             if (a->fraktur)    printf(" fraktur");
             if (a->framed)     printf(" framed");
             if (a->encircled)  printf(" encircled");
             if (a->overlined)  printf(" overlined");
             if (a->link_id)    printf(" link=%u", (unsigned)a->link_id);
             printf("\n");
          }
     }
}

/* Human-readable counterpart to _tytest_checksum().
 *
 * The checksum answers "did anything change"; this answers "what changed",
 * which is the question a scalar-versus-SIMD parity failure actually raises.
 * Two dumps piped through diff point straight at the offending cell. */
static void
_tytest_dump(Termpty *ty)
{
   ssize_t backlog_len;
   int y;

   printf("geom w=%d h=%d\n", ty->w, ty->h);
   printf("cursor x=%d y=%d wrapnext=%d shape=%s\n",
          ty->cursor_state.cx, ty->cursor_state.cy,
          (int)ty->cursor_state.wrapnext, tytest_cursor_shape_get());
   printf("mode altbuf=%d insert=%d wrap=%d bracketed_paste=%d\n",
          (int)ty->altbuf, (int)ty->termstate.insert,
          (int)ty->termstate.wrap, (int)ty->bracketed_paste);
   printf("margin top=%d bottom=%d left=%d right=%d restrict=%d\n",
          ty->termstate.top_margin, ty->termstate.bottom_margin,
          ty->termstate.left_margin, ty->termstate.right_margin,
          (int)ty->termstate.restrict_cursor);
   printf("title=%s\n", ty->prop.title ? ty->prop.title : "(NULL)");
   printf("icon=%s\n", ty->prop.icon ? ty->prop.icon : "(NULL)");
   printf("cwd=%s\n", ty->prop.cwd ? ty->prop.cwd : "(NULL)");

   backlog_len = termpty_backlog_length(ty);
   printf("backlog rows=%d\n", (int)backlog_len);
   for (y = (int)backlog_len; y >= 1; y--)
     {
        const Termcell *cells;
        ssize_t w = 0;
        char label[32];

        cells = termpty_cellrow_get(ty, -y, &w);
        snprintf(label, sizeof(label), "b%-4d", -y);
        if (cells && (w > 0)) _dump_row(cells, (int)w, label);
        else printf("%s ||\n", label);
     }

   printf("screen\n");
   for (y = 0; y < ty->h; y++)
     {
        char label[32];

        snprintf(label, sizeof(label), "s%-4d", y);
        _dump_row(&(TERMPTY_SCREEN(ty, 0, y)), ty->w, label);
     }

   if (ty->write_buffer.len)
     {
        size_t i;

        printf("reply ");
        for (i = 0; i < ty->write_buffer.len; i++)
          {
             unsigned char ch = (unsigned char)ty->write_buffer.buf[i];

             if ((ch >= 0x20) && (ch < 0x7f)) putchar(ch);
             else printf("\\x%02x", ch);
          }
        printf("\n");
     }
}

static void
_usage(const char *argv0)
{
   fprintf(stderr,
           "usage: %s                 read escape codes on stdin, print a state checksum\n"
           "       %s --dump          same, but print the state in a diffable text form\n"
           "       %s <test>|all      run the built-in unit tests\n",
           argv0, argv0, argv0);
}

int
main(int argc, char **argv)
{
   Eina_Bool dump = EINA_FALSE;
   int chunk = 0;
   int i;

   /* Before the argument loop: the unit-test path returns out of it without
    * ever reaching tytest_common_init(), and several of those tests drive the
    * real parser. Leaving the kernels un-dispatched there would make
    * TERMINOLOGY_SIMD_DISABLE silently ineffective for 'tytest all'. */
   simd_init();

   for (i = 1; i < argc; i++)
     {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
          {
             _usage(argv[0]);
             return 0;
          }
        else if (!strcmp(argv[i], "--dump")) dump = EINA_TRUE;
        else if (!strncmp(argv[i], "--chunk=", 8)) chunk = atoi(argv[i] + 8);
        /* Anything else is a unit test name, and those take over entirely. */
        else return _run_tytests(argc, argv);
     }

   eina_init();
   emile_init();

   _log_domain = eina_log_domain_register("tytest", NULL);

   tytest_common_init();
   if (chunk > 0) tytest_common_set_chunk(chunk);

   tytest_common_main_loop();

   if (dump) _tytest_dump(tytest_termpty_get());
   else _tytest_checksum(tytest_termpty_get());

   tytest_common_shutdown();

   emile_shutdown();
   eina_shutdown();

   return 0;
}
