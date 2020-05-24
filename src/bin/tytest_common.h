#ifndef _TYTEST_COMMON_H__
#define _TYTEST_COMMON_H__ 1

#define TY_H 24
#define TY_W 80
#define TY_CH_H 15
#define TY_CH_W  7
#define TY_OFFSET_X 1
#define TY_OFFSET_Y 1
#define TY_BACKSIZE 50

void tytest_common_main_loop(void);
void tytest_common_init(void);
void tytest_common_shutdown(void);
void tytest_common_set_fd(int fd);
#if defined(BINARY_TYTEST)
const char *tytest_cursor_shape_get(void);
Termpty *tytest_termpty_get(void);
#endif

#endif
