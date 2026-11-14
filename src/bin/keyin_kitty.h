#ifndef TERMINOLOGY_KEYIN_KITTY_H_
#define TERMINOLOGY_KEYIN_KITTY_H_ 1

/* https://sw.kovidgoyal.net/kitty/keyboard-protocol/ */

#include "keyin_encode.h"

/* Returns EINA_FALSE when the caller must use the legacy encoding. */
Eina_Bool
keyin_kitty_encode(Termpty *ty, const Keyin_Event *kev, unsigned int flags);

#endif
