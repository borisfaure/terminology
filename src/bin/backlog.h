#ifndef _BACKLOG_H__
#define _BACKLOG_H__ 1

void termpty_save_register(Termpty *ty);
void termpty_save_unregister(Termpty *ty);
Termsave *termpty_save_extract(Termsave *ts);
Termsave *termpty_save_new(Termpty *ty, Termsave *ts, int w);
void termpty_save_free(Termpty *ty, Termsave *ts);
Termsave *termpty_save_expand(Termpty *ty, Termsave *ts,
                              Termcell *cells, size_t delta);

void       termpty_backlog_lock(void);
void       termpty_backlog_unlock(void);

void
termpty_clear_backlog(Termpty *ty);
void
termpty_backlog_free(Termpty *ty);
void
termpty_backlog_size_set(Termpty *ty, size_t size);
ssize_t
termpty_backlog_length(Termpty *ty);

#define BACKLOG_ROW_GET(Ty, Y) \
   (&Ty->back[(Ty->backsize + ty->backpos - ((Y) - 1 )) % Ty->backsize])

#endif
