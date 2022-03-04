#ifndef TERMINOLOGY_TERMPTY_ESC_H_
#define TERMINOLOGY_TERMPTY_ESC_H_ 1

int termpty_handle_seq(Termpty *ty, const Eina_Unicode *c, const Eina_Unicode *ce);
const char * EINA_PURE termptyesc_safechar(const unsigned int c);

#endif
