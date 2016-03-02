#ifndef _TERMPTY_H__
#define _TERMPTY_H__ 1

#include "config.h"

typedef struct _Termpty       Termpty;
typedef struct _Termcell      Termcell;
typedef struct _Termatt       Termatt;
typedef struct _Termsave      Termsave;
typedef struct _Termsavecomp  Termsavecomp;
typedef struct _Termblock     Termblock;
typedef struct _Termexp       Termexp;

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
   unsigned short tab : 1;
   unsigned short fraktur : 1;
#if defined(SUPPORT_80_132_COLUMNS)
   unsigned short is_80_132_mode_allowed : 1;
   unsigned short bit_padding : 13;
#else
   unsigned short bit_padding : 14;
#endif
};

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
      const char *title, *icon, *user_title;
   } prop;
   const char *cur_cmd;
   Termcell *screen, *screen2;
   int circular_offset;
   int circular_offset2;
   Eina_Unicode *buf;
   size_t buflen;
   unsigned char oldbuf[4];
   Termsave *back;
   size_t backsize, backpos;
   struct {
        int screen_y;
        int backlog_y;
   } backlog_beacon;
   int w, h;
   int fd, slavefd;
#ifdef ENABLE_FUZZING
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
      time_t last_click;
      unsigned char is_active : 1;
      unsigned char is_box    : 1;
      unsigned char makesel   : 1;
      unsigned char by_word   : 1;
      unsigned char by_line   : 1;
      unsigned char is_top_to_bottom : 1;
   } selection;
   struct {
        Termatt       att;
        unsigned char charset;
        unsigned char charsetch;
        unsigned char chset[4];
        int           scroll_y1, scroll_y2;
        int           had_cr_x, had_cr_y;
        int           margin_top; // soon, more to come...
        unsigned int  multibyte : 1;
        unsigned int  alt_kp : 1;
        unsigned int  insert : 1;
        unsigned int  appcursor : 1;
        unsigned int  wrap : 1;
        unsigned int  wrapnext : 1;
        unsigned int  crlf : 1;
        unsigned int  had_cr : 1;
        unsigned int  send_bs : 1;
        unsigned int  kbd_lock : 1;
        unsigned int  reverse : 1;
        unsigned int  no_autorepeat : 1;
        unsigned int  cjk_ambiguous_wide : 1;
        unsigned int  hide_cursor : 1;
   } termstate;
   struct {
        int           cx, cy;
   } cursor_state, cursor_save;
   int exit_code;
   pid_t pid;
   unsigned int altbuf     : 1;
   unsigned int mouse_mode : 3;
   unsigned int mouse_ext  : 2;
   unsigned int bracketed_paste : 1;
};

struct _Termcell
{
   Eina_Unicode   codepoint;
   Termatt        att;
   unsigned char padding[2];
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
   int          type;
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
   int ch, left, id;
   int x, y, w, h;
};

void       termpty_init(void);
void       termpty_shutdown(void);

Termpty   *termpty_new(const char *cmd, Eina_Bool login_shell, const char *cd,
                      int w, int h, int backscroll, Eina_Bool xterm_256color,
                      Eina_Bool erase_is_del, const char *emotion_mod);
void       termpty_free(Termpty *ty);

void       termpty_backlog_lock(void);
void       termpty_backlog_unlock(void);

Termcell  *termpty_cellrow_get(Termpty *ty, int y, ssize_t *wret);
ssize_t termpty_row_length(Termpty *ty, int y);
void       termpty_write(Termpty *ty, const char *input, int len);
void       termpty_resize(Termpty *ty, int new_w, int new_h);
void       termpty_backlog_size_set(Termpty *ty, size_t size);
ssize_t    termpty_backlog_length(Termpty *ty);
void       termpty_backscroll_adjust(Termpty *ty, int *scroll);

pid_t      termpty_pid_get(const Termpty *ty);
void       termpty_block_free(Termblock *tb);
Termblock *termpty_block_new(Termpty *ty, int w, int h, const char *path, const char *link);
void       termpty_block_insert(Termpty *ty, int ch, Termblock *blk);
int        termpty_block_id_get(Termcell *cell, int *x, int *y);
Termblock *termpty_block_get(Termpty *ty, int id);
void       termpty_block_chid_update(Termpty *ty, Termblock *blk);
Termblock *termpty_block_chid_get(Termpty *ty, const char *chid);

void       termpty_cell_copy(Termpty *ty, Termcell *src, Termcell *dst, int n);
void       termpty_cell_fill(Termpty *ty, Termcell *src, Termcell *dst, int n);
void       termpty_cell_codepoint_att_fill(Termpty *ty, Eina_Unicode codepoint, Termatt att, Termcell *dst, int n);
void       termpty_screen_swap(Termpty *ty);

ssize_t termpty_line_length(const Termcell *cells, ssize_t nb_cells);

Config *termpty_config_get(const Termpty *ty);
void termpty_handle_buf(Termpty *ty, const Eina_Unicode *codepoints, int len);

extern int _termpty_log_dom;

#define TERMPTY_SCREEN(Tpty, X, Y) \
  Tpty->screen[X + (((Y + Tpty->circular_offset) % Tpty->h) * Tpty->w)]
#define TERMPTY_FMTCLR(Tatt) \
   (Tatt).autowrapped = (Tatt).newline = (Tatt).tab = 0

#define TERMPTY_RESTRICT_FIELD(Field, Min, Max) \
   do {                                         \
   if (Field >= Max)                            \
     Field = Max - 1;                           \
   else if (Field < Min)                        \
     Field = Min;                               \
   } while (0)

#endif
