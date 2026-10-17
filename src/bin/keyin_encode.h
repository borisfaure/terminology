#ifndef TERMINOLOGY_KEYIN_ENCODE_H_
#define TERMINOLOGY_KEYIN_ENCODE_H_ 1

/* Key encoding, split out of keyin.c so that it can be tested: keyin.c
 * itself pulls in termio, win and term through the key binding callbacks. */
void
keyin_encode_key(Termpty *ty, const char *key, const char *string,
                 Eina_Bool numlock, const int alt, const int shift,
                 const int ctrl);

#endif
