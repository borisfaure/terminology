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

typedef struct _Key_Binding Key_Binding;

typedef Eina_Bool (*Key_Binding_Cb)(Evas_Object *term);
struct _Key_Binding
{
   uint16_t ctrl  : 1;
   uint16_t alt   : 1;
   uint16_t shift : 1;

   uint16_t len;

   Key_Binding_Cb cb;
   char keyname[];
};

Key_Binding *
key_binding_new(const char *keyname,
                Eina_Bool ctrl, Eina_Bool alt, Eina_Bool shift,
                Key_Binding_Cb cb);
int key_bindings_init(void);
void key_bindings_shutdown(void);


#endif
