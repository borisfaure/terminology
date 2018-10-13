#ifndef _TERMPTY_SAVE_H__
#define _TERMPTY_SAVE_H__ 1

void termpty_save_register(Termpty *ty);
void termpty_save_unregister(Termpty *ty);
Termsave *termpty_save_extract(Termsave *ts);
Termsave *termpty_save_new(Termpty *ty, Termsave *ts, int w);
void termpty_save_free(Termpty *ty, Termsave *ts);
Termsave *termpty_save_expand(Termpty *ty, Termsave *ts,
                              Termcell *cells, size_t delta);

#endif
