#ifndef _TERMPTY_SAVE_H__
#define _TERMPTY_SAVE_H__ 1

void termpty_save_freeze(void);
void termpty_save_thaw(void);
void termpty_save_register(Termpty *ty);
void termpty_save_unregister(Termpty *ty);
Termsave *termpty_save_extract(Termsave *ts);
Termsave *termpty_save_new(int w);
void termpty_save_free(Termsave *ts);

#endif
