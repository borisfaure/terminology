#ifndef TERMINOLOGY_KEYIN_H_
#define TERMINOLOGY_KEYIN_H_ 1

typedef struct tag_Keys_Handler Keys_Handler;

struct tag_Keys_Handler
{
   Ecore_IMF_Context *imf;
   unsigned int last_keyup;
   Eina_List *seq;
   unsigned char composing : 1;
};

void
keyin_handle_key_to_pty(Termpty *ty, const Evas_Event_Key_Down *ev,
                        const int alt, const int shift, const int ctrl);
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
