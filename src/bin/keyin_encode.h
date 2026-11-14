#ifndef TERMINOLOGY_KEYIN_ENCODE_H_
#define TERMINOLOGY_KEYIN_ENCODE_H_ 1

/* Key encoding, split out of keyin.c so that it can be tested: keyin.c
 * itself pulls in termio, win and term through the key binding callbacks. */

typedef struct tag_Termpty Termpty;

typedef enum _Keyin_Action
{
   KEYIN_ACTION_PRESS = 1,
   KEYIN_ACTION_REPEAT = 2,
   KEYIN_ACTION_RELEASE = 3
} Keyin_Action;

/* bit values from the kitty keyboard protocol, minus 1 */
#define KEYIN_MOD_SHIFT      (1 << 0)
#define KEYIN_MOD_ALT        (1 << 1)
#define KEYIN_MOD_CTRL       (1 << 2)
#define KEYIN_MOD_SUPER      (1 << 3)
#define KEYIN_MOD_HYPER      (1 << 4)
#define KEYIN_MOD_META       (1 << 5)
#define KEYIN_MOD_CAPS_LOCK  (1 << 6)
#define KEYIN_MOD_NUM_LOCK   (1 << 7)
#define KEYIN_MOD_LOCKS      (KEYIN_MOD_CAPS_LOCK | KEYIN_MOD_NUM_LOCK)

/* plain data, so that tests can build one without a canvas */
typedef struct tag_Keyin_Event
{
   const char *key;      /* ev->key: keysym name, layout and modifiers applied */
   const char *keyname;  /* ev->keyname: level 0 of the first layout group */
   const char *string;   /* ev->string, may be NULL */
   unsigned int mods;    /* KEYIN_MOD_* */
   Keyin_Action action;
} Keyin_Event;

void
keyin_encode_event(Termpty *ty, const Keyin_Event *kev);

#endif
