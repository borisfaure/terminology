#ifndef _KEYIN_H__
#define _KEYIN_H__ 1

typedef struct _Keys_Handler Keys_Handler;

struct _Keys_Handler
{
   Ecore_IMF_Context *imf;
   unsigned int last_keyup;
   Eina_List *seq;
   unsigned char composing : 1;
};

void keyin_compose_seq_reset(Keys_Handler *khdl);
Eina_Bool key_is_modifier(const char *key);
Eina_Bool keyin_handle(Keys_Handler *khdl, Termpty *ty, const Evas_Event_Key_Down *ev,
                       Eina_Bool ctrl, Eina_Bool alt, Eina_Bool shift);

void keyin_handle_up(Keys_Handler *khdl, Evas_Event_Key_Up *ev);

typedef Eina_Bool (*Key_Binding_Cb)(Evas_Object *term);

typedef struct _Shortcut_Action Shortcut_Action;

struct _Shortcut_Action
{
   const char *action;
   const char *description;
   Key_Binding_Cb cb;
};

const Shortcut_Action *shortcut_actions_get(void);

int key_bindings_load(Config *config);
void key_bindings_shutdown(void);


#endif
