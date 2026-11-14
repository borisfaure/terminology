#ifndef TERMINOLOGY_KEYIN_H_
#define TERMINOLOGY_KEYIN_H_ 1

#include "keyin_encode.h"

typedef struct tag_Keys_Handler Keys_Handler;

/* the keys currently held, for the event types of the kitty protocol */
#define KEYIN_PRESSED_MAX 16

typedef struct tag_Keyin_Pressed_Key
{
   unsigned int keycode;
   const char *key;              /* stringshare, for the modifier side check */
   unsigned char reported : 1;   /* the press reached keyin_encode_event() */
} Keyin_Pressed_Key;

struct tag_Keys_Handler
{
   Ecore_IMF_Context *imf;
   unsigned int last_keyup;
   Eina_List *seq;
   Keyin_Pressed_Key pressed[KEYIN_PRESSED_MAX];
   unsigned int pressed_count;
   unsigned char composing : 1;
};

void
keyin_handle_key_to_pty(Termpty *ty, const Evas_Event_Key_Down *ev,
                        Keyin_Action action);
void
keyin_handle_key_up_to_pty(Termpty *ty, const Keys_Handler *khdl,
                           const Evas_Event_Key_Up *ev);
Keyin_Action
keyin_key_down_track(Keys_Handler *khdl, const Evas_Event_Key_Down *ev);
void keyin_key_reported_set(Keys_Handler *khdl, unsigned int keycode);
Eina_Bool
keyin_key_up_track(Keys_Handler *khdl, const Evas_Event_Key_Up *ev);
void keyin_pressed_reset(Keys_Handler *khdl);
Eina_Bool
termpty_can_handle_key(const Termpty *ty,
                       const Keys_Handler *khdl,
                       const Evas_Event_Key_Down *ev);
void keyin_compose_seq_reset(Keys_Handler *khdl);
Eina_Bool key_is_modifier(const char *key);
Eina_Bool
keyin_handle_key_binding(Evas_Object *termio, const Evas_Event_Key_Down *ev,
                         Eina_Bool ctrl, Eina_Bool alt, Eina_Bool shift,
                         Eina_Bool win, Eina_Bool meta, Eina_Bool hyper);

void keyin_handle_up(Keys_Handler *khdl, Evas_Event_Key_Up *ev);

typedef Eina_Bool (*Key_Binding_Cb)(Evas_Object *term);

typedef struct tag_Shortcut_Action Shortcut_Action;

struct tag_Shortcut_Action
{
   const char *action;
   const char *description;
   Key_Binding_Cb cb;
};

const Shortcut_Action *shortcut_actions_get(void);

int key_bindings_load(Config *config);
int keyin_add_config(const Config_Keys *cfg_key);
int keyin_remove_config(const Config_Keys *cfg_key);
void key_bindings_shutdown(void);

#endif
