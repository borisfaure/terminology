#ifndef _TY_UNIT_TESTS_H__
#define _TY_UNIT_TESTS_H__ 1

/* Unit tests */
typedef int (*tytest_func)(void);

/* list of tests */
int tytest_dummy(void);
int tytest_sb_skip(void);
int tytest_sb_trim(void);
int tytest_sb_gap(void);
int tytest_sb_steal(void);
int tytest_color_parse_hex(void);
int tytest_color_parse_2hex(void);
int tytest_color_parse_sharp(void);
int tytest_color_parse_uint8(void);
int tytest_color_parse_edc(void);
int tytest_color_parse_css_rgb(void);

#endif
