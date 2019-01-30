#ifndef _TERMPTY_H__
#define _TERMPTY_H__ 1

#include "config.h"
#include "media.h"
#include "sb.h"

typedef struct _Termcell      Termcell;
typedef struct _Termatt       Termatt;
typedef struct _Termsave      Termsave;
typedef struct _Termsavecomp  Termsavecomp;
typedef struct _Termblock     Termblock;
typedef struct _Termexp       Termexp;
typedef struct _Termpty       Termpty;
typedef struct _Termlink      Term_Link;

#define COL_DEF        0
#define COL_BLACK      1
#define COL_RED        2
#define COL_GREEN      3
#define COL_YELLOW     4
#define COL_BLUE       5
#define COL_MAGENTA    6
#define COL_CYAN       7
#define COL_WHITE      8
#define COL_INVIS      9

#define COL_INVERSE   10
#define COL_INVERSEBG 11

#define MOUSE_OFF              0
#define MOUSE_X10              1 // Press only
#define MOUSE_NORMAL           2 // Press+release only
#define MOUSE_NORMAL_BTN_MOVE  3 // Press+release+motion while pressed
#define MOUSE_NORMAL_ALL_MOVE  4 // Press+release+all motion

#define MOUSE_EXT_NONE         0
#define MOUSE_EXT_UTF8         1
#define MOUSE_EXT_SGR          2
#define MOUSE_EXT_URXVT        3

// Only for testing purpose
//#define SUPPORT_80_132_COLUMNS 1

#define MOVIE_STATE_PLAY   0
#define MOVIE_STATE_PAUSE  1
#define MOVIE_STATE_STOP   2

#define HL_LINKS_MAX  (1 << 16)

struct _Termlink
{
    const char *key;
    const char *url;
    unsigned int refcount;
};




struct _Termatt
{
   unsigned char fg, bg;
   unsigned short bold : 1;
   unsigned short faint : 1;
   unsigned short italic : 1;
   unsigned short dblwidth : 1;
   unsigned short underline : 1;
   unsigned short blink : 1; // don't intend to support this currently
   unsigned short blink2 : 1; // don't intend to support this currently
   unsigned short inverse : 1;
   unsigned short invisible : 1;
   unsigned short strike : 1;
   unsigned short fg256 : 1;
   unsigned short bg256 : 1;
   unsigned short fgintense : 1;
   unsigned short bgintense : 1;
   // below used for working out text from selections
   unsigned short autowrapped : 1;
   unsigned short newline : 1;
   unsigned short fraktur : 1;
   unsigned short framed : 1;
   unsigned short encircled : 1;
   unsigned short overlined : 1;
#if defined(SUPPORT_80_132_COLUMNS)
   unsigned short is_80_132_mode_allowed : 1;
   unsigned short bit_padding : 11;
#else
   unsigned short bit_padding : 12;
#endif
   uint16_t       link_id;
};

typedef struct _Backlog_Beacon{
    int screen_y;
    int backlog_y;
} Backlog_Beacon;

typedef struct _Term_State {
    Termatt       att;
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
    unsigned int  wrapnext : 1;
    unsigned int  crlf : 1;
    unsigned int  send_bs : 1;
    unsigned int  kbd_lock : 1;
    unsigned int  reverse : 1;
    unsigned int  no_autorepeat : 1;
    unsigned int  cjk_ambiguous_wide : 1;
    unsigned int  hide_cursor : 1;
    unsigned int  combining_strike : 1;
    unsigned int  sace_rectangular : 1;
} Term_State;

typedef struct _Term_Cursor {
    int cx;
    int cy;
} Term_Cursor;

struct _Termpty
{
   Evas_Object *obj;
   Ecore_Event_Handler *hand_exe_exit;
   Ecore_Fd_Handler *hand_fd;
   struct {
      struct {
         void (*func) (void *data);
         void *data;
      } change, set_title, set_icon, cancel_sel, exited, bell, command;
   } cb;
   struct {
      const char *icon;
      /* dynamic title set by xterm, keep it in case user don't want a
       * title any more by setting a empty title
       */
      const char *title;
      /* set by user */
      const char *user_title;
   } prop;
   const char *cur_cmd;
   Termcell *screen, *screen2;
   unsigned int *tabs;
   int circular_offset;
   int circular_offset2;
   Eina_Unicode *buf;
   size_t buflen;
   Eina_Unicode last_char;
   Eina_Bool buf_have_zero;
   unsigned char oldbuf[4];
   Termsave *back;
   size_t backsize, backpos;
   /* this beacon in the backlog tells about the top line in screen
    * coordinates that maps to a line in the backlog */
   Backlog_Beacon backlog_beacon;
   int w, h;
   int fd, slavefd;
#if defined(ENABLE_TESTS)
   struct ty_sb write_buffer;
#endif
#if defined(ENABLE_FUZZING)
   int fd_dev_null;
#endif
   struct {
      int curid;
      Eina_Hash *blocks;
      Eina_Hash *chid_map;
      Eina_List *active;
      Eina_List *expecting;
      unsigned char on : 1;
   } block;
   struct {
      /* start is always the start of the selection
       * so end.y can be < start.y */
      struct {
         int x, y;
      } start, end, orig;
      Eina_Unicode *codepoints;
      time_t last_click;
      unsigned char is_active : 1;
      unsigned char is_box    : 1;
      unsigned char makesel   : 1;
      unsigned char by_word   : 1;
      unsigned char by_line   : 1;
      unsigned char is_top_to_bottom : 1;
   } selection;
   Term_State termstate;
   Term_Cursor cursor_state;
   Term_Cursor cursor_save[2];
   int exit_code;
   pid_t pid;
   unsigned int altbuf     : 1;
   unsigned int mouse_mode : 3;
   unsigned int mouse_ext  : 2;
   unsigned int bracketed_paste : 1;
   unsigned int decoding_error : 1;
   struct {
       Term_Link *links;
       uint8_t *bitmap;
       uint32_t size;
   } hl;
};

struct _Termcell
{
   Eina_Unicode   codepoint;
   Termatt        att;
};

struct _Termsave
{
   unsigned int   gen  : 8;
   unsigned int   comp : 1;
   unsigned int   z    : 1;
   unsigned int   w    : 22;
   /* TODO: union ? */
   Termcell       *cells;
};

/* TODO: RESIZE rewrite Termsavecomp */
struct _Termsavecomp
{
   unsigned int   gen  : 8;
   unsigned int   comp : 1;
   unsigned int   z    : 1;
   unsigned int   w    : 22; // compressed size in bytes
   unsigned int   wout; // output width in Termcells
};

struct _Termblock
{
   Termpty     *pty;
   const char  *path, *link, *chid;
   Evas_Object *obj;
   Eina_List   *cmds;
   int          id;
   Media_Type   type;
   int          refs;
   short        w, h;
   short        x, y;
   unsigned char scale_stretch : 1;
   unsigned char scale_center : 1;
   unsigned char scale_fill : 1;
   unsigned char thumb : 1;
   unsigned char edje : 1;

   unsigned char active : 1;
   unsigned char was_active : 1;
   unsigned char was_active_before : 1;

   unsigned char mov_state : 2;  // movie state marker
};

struct _Termexp
{
   Eina_Unicode ch;
   int left, id;
   int x, y, w, h;
};


void       termpty_init(void);
void       termpty_shutdown(void);

Termpty   *termpty_new(const char *cmd, Eina_Bool login_shell, const char *cd,
                      int w, int h, int backscroll, Eina_Bool xterm_256color,
                      Eina_Bool erase_is_del, const char *emotion_mod,
                      const char *title, Ecore_Window window_id);
void       termpty_free(Termpty *ty);

void       termpty_backlog_lock(void);
void       termpty_backlog_unlock(void);

Termcell  *termpty_cellrow_get(Termpty *ty, int y, ssize_t *wret);
Termcell * termpty_cell_get(Termpty *ty, int y_requested, int x_requested);
ssize_t termpty_row_length(Termpty *ty, int y);
void       termpty_write(Termpty *ty, const char *input, int len);
void       termpty_resize(Termpty *ty, int new_w, int new_h);
void       termpty_resize_tabs(Termpty *ty, int old_w, int new_w);
void       termpty_backlog_size_set(Termpty *ty, size_t size);
ssize_t    termpty_backlog_length(Termpty *ty);
void       termpty_backscroll_adjust(Termpty *ty, int *scroll);

pid_t      termpty_pid_get(const Termpty *ty);
void       termpty_block_free(Termblock *tb);
Termblock *termpty_block_new(Termpty *ty, int w, int h, const char *path, const char *link);
void       termpty_block_insert(Termpty *ty, int ch, Termblock *blk);
int        termpty_block_id_get(const Termcell *cell, int *x, int *y);
Termblock *termpty_block_get(const Termpty *ty, int id);
void       termpty_block_chid_update(Termpty *ty, Termblock *blk);
Termblock *termpty_block_chid_get(const Termpty *ty, const char *chid);

void       termpty_cell_codepoint_att_fill(Termpty *ty, Eina_Unicode codepoint, Termatt att, Termcell *dst, int n);
void       termpty_cells_set_content(Termpty *ty, Termcell *cells,
                          Eina_Unicode codepoint, int count);
void       termpty_screen_swap(Termpty *ty);

ssize_t termpty_line_length(const Termcell *cells, ssize_t nb_cells);

Config *termpty_config_get(const Termpty *ty);
void termpty_handle_buf(Termpty *ty, const Eina_Unicode *codepoints, int len);
void termpty_handle_block_codepoint_overwrite_heavy(Termpty *ty, int oldc, int newc);

Term_Link * term_link_new(Termpty *ty);
void term_link_free(Termpty *ty, Term_Link *link);

extern int _termpty_log_dom;

#define TERMPTY_SCREEN(Tpty, X, Y) \
  Tpty->screen[X + (((Y + Tpty->circular_offset) % Tpty->h) * Tpty->w)]

#define TERMPTY_RESTRICT_FIELD(Field, Min, Max) \
   do {                                         \
   if (Field >= Max)                            \
     Field = Max - 1;                           \
   else if (Field < Min)                        \
     Field = Min;                               \
   } while (0)

/* Try to trick the compiler into inlining the first test */
#define HANDLE_BLOCK_CODEPOINT_OVERWRITE(Tpty, OLDC, NEWC)                   \
do {                                                                         \
   if (EINA_UNLIKELY((OLDC | NEWC) & 0x80000000))                            \
       termpty_handle_block_codepoint_overwrite_heavy(Tpty, OLDC, NEWC);     \
} while (0)

#define TERMPTY_CELL_COPY(Tpty, Tsrc, Tdst, N)                               \
do {                                                                         \
   int __i;                                                                  \
                                                                             \
   for (__i = 0; __i < N; __i++)                                             \
     {                                                                       \
        HANDLE_BLOCK_CODEPOINT_OVERWRITE(Tpty,                               \
                                         (Tdst)[__i].codepoint,              \
                                         (Tsrc)[__i].codepoint);             \
        if (EINA_UNLIKELY((Tdst)[__i].att.link_id))                          \
          term_link_refcount_dec(ty, (Tdst)[__i].att.link_id, 1);            \
        if (EINA_UNLIKELY((Tsrc)[__i].att.link_id))                          \
          term_link_refcount_inc(ty, (Tsrc)[__i].att.link_id, 1);            \
     }                                                                       \
   memcpy(Tdst, Tsrc, N * sizeof(Termcell));                                 \
} while (0)


static inline void
term_link_refcount_inc(Termpty *ty, uint16_t link_id, uint16_t count)
{
   Term_Link *link;

   link = &ty->hl.links[link_id];
   link->refcount += count;
}

static inline void
term_link_refcount_dec(Termpty *ty, uint16_t link_id, uint16_t count)
{
   Term_Link *link;

   link = &ty->hl.links[link_id];
   link->refcount -= count;
   if (EINA_UNLIKELY(link->refcount == 0))
     term_link_free(ty, link);
}

static inline Eina_Bool
term_link_eq(Termpty *ty, Term_Link *hl, uint16_t link_id)
{
    Term_Link *hl2;
    uint16_t hl_id;

    if (link_id == 0)
        return EINA_FALSE;

    hl_id = hl - ty->hl.links;
    if (hl_id == link_id)
        return EINA_TRUE;
    hl2 = &ty->hl.links[link_id];
    if (!hl->key || !hl2->key ||
        strcmp(hl->key, hl2->key) != 0)
        return EINA_FALSE;
    return (strcmp(hl->url, hl2->url) == 0);
}

static inline void
termpty_cell_fill(Termpty *ty, Termcell *src, Termcell *dst, int n)
{
   int i;

   if (src)
     {
        for (i = 0; i < n; i++)
          {
             HANDLE_BLOCK_CODEPOINT_OVERWRITE(ty, dst[i].codepoint, src[0].codepoint);
             if (EINA_UNLIKELY(dst[i].att.link_id))
               term_link_refcount_dec(ty, dst[i].att.link_id, 1);

             dst[i] = src[0];
          }
        if (src[0].att.link_id)
          term_link_refcount_inc(ty, src[0].att.link_id, n);
     }
   else
     {
        for (i = 0; i < n; i++)
          {
             HANDLE_BLOCK_CODEPOINT_OVERWRITE(ty, dst[i].codepoint, 0);
             if (EINA_UNLIKELY(dst[i].att.link_id))
               term_link_refcount_dec(ty, dst[i].att.link_id, 1);

             memset(&(dst[i]), 0, sizeof(*dst));
          }
     }
}
#endif
