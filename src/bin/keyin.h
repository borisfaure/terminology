#ifndef _KEYIN_H__
#define _KEYIN_H__ 1

typedef struct _Keys_Handler Keys_Handler;

struct _Keys_Handler
{
   Ecore_IMF_Context *imf;
   unsigned int last_keyup;
   Eina_List *seq;
   Eina_Bool composing : 1;
};

void keyin_compose_seq_reset(Keys_Handler *khdl);
Eina_Bool key_is_modifier(const char *key);
Eina_Bool keyin_handle(Keys_Handler *khdl, Termpty *ty, const Evas_Event_Key_Down *ev,
                       int alt, int shift, int ctrl);

void keyin_handle_up(Keys_Handler *khdl, Evas_Event_Key_Up *ev);


#endif
