#ifndef _TERMPTY_OPS_H__
#define _TERMPTY_OPS_H__ 1

typedef enum _Termpty_Clear
{
   TERMPTY_CLR_END,
   TERMPTY_CLR_BEGIN,
   TERMPTY_CLR_ALL
} Termpty_Clear;

void termpty_text_save_top(Termpty *ty, Termcell *cells, ssize_t w_max);
void termpty_cells_copy(Termpty *ty, Termcell *cells, Termcell *dest, int count);
void termpty_cells_fill(Termpty *ty, Eina_Unicode codepoint, Termcell *cells, int count);
void termpty_cells_clear(Termpty *ty, Termcell *cells, int count);
void termpty_cells_att_fill_preserve_colors(Termpty *ty, Termcell *cells,
                                       Eina_Unicode codepoint, int count);
void termpty_text_scroll(Termpty *ty, Eina_Bool clear);
void termpty_text_scroll_rev(Termpty *ty, Eina_Bool clear);
void termpty_text_scroll_test(Termpty *ty, Eina_Bool clear);
void termpty_text_scroll_rev_test(Termpty *ty, Eina_Bool clear);
void termpty_text_append(Termpty *ty, const Eina_Unicode *codepoints, int len);
void termpty_clear_line(Termpty *ty, Termpty_Clear mode, int limit);
void termpty_clear_screen(Termpty *ty, Termpty_Clear mode);
void termpty_clear_all(Termpty *ty);
void termpty_reset_att(Termatt *att);
void termpty_reset_state(Termpty *ty);
void termpty_cursor_copy(Termpty *ty, Eina_Bool save);
void termpty_clear_tabs_on_screen(Termpty *ty);
void termpty_clear_backlog(Termpty *ty);

void termpty_move_cursor(Termpty *ty, int cx, int cy);

#define _term_txt_write(ty, txt) termpty_write(ty, txt, sizeof(txt) - 1)

#define TAB_WIDTH 8u

#define  TAB_SET(ty, col) \
   do { \
        ty->tabs[col / sizeof(unsigned int) / 8] |= \
            1u << (col % (sizeof(unsigned int) * 8)); \
   } while (0)

#define  TAB_UNSET(ty, col) \
   do { \
        ty->tabs[col / sizeof(unsigned int) / 8] &= \
            ~(1u << (col % (sizeof(unsigned int) * 8))); \
   } while (0)

#define  TAB_TEST(ty, col) \
   (ty->tabs[col / sizeof(unsigned int) / 8] & \
    (1u << (col % (sizeof(unsigned int) * 8))))

#endif
