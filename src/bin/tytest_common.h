#ifndef TERMINOLOGY_TYTEST_COMMON_H_
#define TERMINOLOGY_TYTEST_COMMON_H_ 1

#define TY_H 24
#define TY_W 80
#define TY_CH_H 15
#define TY_CH_W  7
#define TY_OFFSET_X 1
#define TY_OFFSET_Y 1
#define TY_BACKSIZE 50

/* The fuzzers' test case format: a 3-byte header expands into the stream
 * that is fed, capped at TY_FUZZ_STREAM_MAX bytes. See tytest_common.c. */
#define TY_FUZZ_HEADER_SIZE 3
#define TY_FUZZ_STREAM_MAX (1024 * 1024)

void tytest_common_main_loop(void);
void tytest_common_feed(const char *data, int datalen);
void tytest_common_feed_chunked(const char *data, int len, int chunk);
void tytest_common_feed_input(const unsigned char *data, int len);
void tytest_common_reset(void);
void tytest_common_set_chunk(int chunk);
void tytest_common_init(void);
void tytest_common_shutdown(void);
void tytest_common_set_fd(int fd);
#if defined(BINARY_TYTEST)
const char *tytest_cursor_shape_get(void);
void tytest_notifications_clear(void);
const char *tytest_notifications_get(void);
Termpty *tytest_termpty_get(void);
const char *tytest_common_stream_get(int *len);
#endif

#endif
