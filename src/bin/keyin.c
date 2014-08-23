#include <Elementary.h>
#include <Ecore_Input.h>
#include <Ecore_IMF.h>
#include <Ecore_IMF_Evas.h>
#include "private.h"
#include "termpty.h"
#include "termio.h"
#include "termcmd.h"
#include "keyin.h"

typedef struct _Keyout Keyout;

struct _Keyout
{
   const char *in;
   const char *out;
   int         inlen;
   int         outlen;
};

static Eina_Hash *_key_bindings = NULL;



#define KEY(in, out) {in, out, sizeof(in) - 1, sizeof(out) - 1}

static const Keyout crlf_keyout[] =
{
   KEY("Return",       "\r\n"),
   
   KEY(NULL, "END")
};

static const Keyout nocrlf_keyout[] =
{
   KEY("Return",       "\r"),
   
   KEY(NULL, "END")
};

static const Keyout appcur_keyout[] =
{
   KEY("Left",         "\033OD"),
   KEY("Right",        "\033OC"),
   KEY("Up",           "\033OA"),
   KEY("Down",         "\033OB"),
   KEY("Home",         "\033OH"),
   KEY("End",          "\033OF"),
//// why did i change these?   
//   KEY("Home",         "\033[7~"),
//   KEY("End",          "\033[8~"),
   KEY("F1",           "\033OP"),
   KEY("F2",           "\033OQ"),
   KEY("F3",           "\033OR"),
   KEY("F4",           "\033OS"),
   
   KEY(NULL, "END")
};

static const Keyout ctrl_keyout[] =
{
   KEY("Left",         "\033[1;5D"),
   KEY("Right",        "\033[1;5C"),
   KEY("Up",           "\033[1;5A"),
   KEY("Down",         "\033[1;5B"),
   KEY("Home",         "\033[1;5H"),
   KEY("End",          "\033[1;5F"),
   KEY("Insert",       "\033[2;5~"),
   KEY("Delete",       "\033[3;5~"),
   KEY("Prior",        "\033[5;5~"),
   KEY("Next",         "\033[6;5~"),
   KEY("F1",           "\033[O5P"),
   KEY("F2",           "\033[O5Q"),
   KEY("F3",           "\033[O5R"),
   KEY("F4",           "\033[O5S"),
   KEY("F5",           "\033[15;5~"),
   KEY("F6",           "\033[17;5~"),
   KEY("F7",           "\033[18;5~"),
   KEY("F8",           "\033[19;5~"),
   KEY("F9",           "\033[20;5~"),
   KEY("F10",          "\033[21;5~"),
   KEY("F11",          "\033[23;5~"),
   KEY("F12",          "\033[24;5~"),
   KEY("F13",          "\033[25;5~"),
   KEY("F14",          "\033[26;5~"),
   KEY("F15",          "\033[28;5~"),
   KEY("F16",          "\033[29;5~"),
   KEY("F17",          "\033[31;5~"),
   KEY("F18",          "\033[32;5~"),
   KEY("F19",          "\033[33;5~"),
   KEY("F20",          "\033[34;5~"),
   KEY("F21",          "\033[35;5~"),
   KEY("F22",          "\033[36;5~"),
   KEY("F23",          "\033[37;5~"),
   KEY("F24",          "\033[38;5~"),
   KEY("F25",          "\033[39;5~"),
   KEY("F26",          "\033[40;5~"),
   KEY("F27",          "\033[41;5~"),
   KEY("F28",          "\033[42;5~"),
   KEY("F29",          "\033[43;5~"),
   KEY("F30",          "\033[44;5~"),
   KEY("F31",          "\033[45;5~"),
   KEY("F32",          "\033[46;5~"),
   KEY("F33",          "\033[47;5~"),
   KEY("F34",          "\033[48;5~"),
   KEY("F35",          "\033[49;5~"),

   KEY(NULL, "END")
};

static const Keyout ctrl_shift_keyout[] =
{
   KEY("Left",         "\033[1;6D"),
   KEY("Right",        "\033[1;6C"),
   KEY("Up",           "\033[1;6A"),
   KEY("Down",         "\033[1;6B"),
   KEY("Home",         "\033[1;6H"),
   KEY("End",          "\033[1;6F"),
   KEY("Insert",       "\033[2;6~"),
   KEY("Delete",       "\033[3;6~"),
   KEY("Prior",        "\033[5;6~"),
   KEY("Next",         "\033[6;6~"),

   KEY("F1",           "\033[O6P"),
   KEY("F2",           "\033[O6Q"),
   KEY("F3",           "\033[O6R"),
   KEY("F4",           "\033[O6S"),
   KEY("F5",           "\033[15;6~"),
   KEY("F6",           "\033[17;6~"),
   KEY("F7",           "\033[18;6~"),
   KEY("F8",           "\033[19;6~"),
   KEY("F9",           "\033[20;6~"),
   KEY("F10",          "\033[21;6~"),
   KEY("F11",          "\033[23;6~"),
   KEY("F12",          "\033[24;6~"),
   KEY("F13",          "\033[25;6~"),
   KEY("F14",          "\033[26;6~"),
   KEY("F15",          "\033[28;6~"),
   KEY("F16",          "\033[29;6~"),
   KEY("F17",          "\033[31;6~"),
   KEY("F18",          "\033[32;6~"),
   KEY("F19",          "\033[33;6~"),
   KEY("F20",          "\033[34;6~"),
   KEY("F21",          "\033[35;6~"),
   KEY("F22",          "\033[36;6~"),
   KEY("F23",          "\033[37;6~"),
   KEY("F24",          "\033[38;6~"),
   KEY("F25",          "\033[39;6~"),
   KEY("F26",          "\033[40;6~"),
   KEY("F27",          "\033[41;6~"),
   KEY("F28",          "\033[42;6~"),
   KEY("F29",          "\033[43;6~"),
   KEY("F30",          "\033[44;6~"),
   KEY("F31",          "\033[45;6~"),
   KEY("F32",          "\033[46;6~"),
   KEY("F33",          "\033[47;6~"),
   KEY("F34",          "\033[48;6~"),
   KEY("F35",          "\033[49;6~"),

   KEY(NULL, "END")
};

static const Keyout shift_keyout[] =
{
   KEY("Left",         "\033[1;2D"),
   KEY("Right",        "\033[1;2C"),
   KEY("Up",           "\033[1;2A"),
   KEY("Down",         "\033[1;2B"),
   KEY("Home",         "\033[1;2H"),
   KEY("End",          "\033[1;2F"),
   KEY("Insert",       "\033[2;2~"),
   KEY("Delete",       "\033[3;2~"),
   KEY("Prior",        "\033[5;2~"),
   KEY("Next",         "\033[6;2~"),

   KEY("Tab",          "\033[Z"),
   KEY("ISO_Left_Tab", "\033[Z"),
   KEY("F1",           "\033[1;2P"),
   KEY("F2",           "\033[1;2Q"),
   KEY("F3",           "\033[1;2R"),
   KEY("F4",           "\033[1;2S"),
   KEY("F5",           "\033[15;2~"),
   KEY("F6",           "\033[17;2~"),
   KEY("F7",           "\033[18;2~"),
   KEY("F8",           "\033[19;2~"),
   KEY("F9",           "\033[20;2~"),
   KEY("F10",          "\033[21;2~"),
   KEY("F11",          "\033[23;2~"),
   KEY("F12",          "\033[24;2~"),
   KEY("F13",          "\033[25;2~"),
   KEY("F14",          "\033[26;2~"),
   KEY("F15",          "\033[28;2~"),
   KEY("F16",          "\033[29;2~"),
   KEY("F17",          "\033[31;2~"),
   KEY("F18",          "\033[32;2~"),
   KEY("F19",          "\033[33;2~"),
   KEY("F20",          "\033[34;2~"),
   KEY("F21",          "\033[35;2~"),
   KEY("F22",          "\033[36;2~"),
   KEY("F23",          "\033[37;2~"),
   KEY("F24",          "\033[38;2~"),
   KEY("F25",          "\033[39;2~"),
   KEY("F26",          "\033[40;2~"),
   KEY("F27",          "\033[41;2~"),
   KEY("F28",          "\033[42;2~"),
   KEY("F29",          "\033[43;2~"),
   KEY("F30",          "\033[44;2~"),
   KEY("F31",          "\033[45;2~"),
   KEY("F32",          "\033[46;2~"),
   KEY("F33",          "\033[47;2~"),
   KEY("F34",          "\033[48;2~"),
   KEY("F35",          "\033[49;2~"),

   KEY(NULL, "END")
};

static const Keyout alt_keyout[] =
{
   KEY("Left",         "\033[1;3D"),
   KEY("Right",        "\033[1;3C"),
   KEY("Up",           "\033[1;3A"),
   KEY("Down",         "\033[1;3B"),
   KEY("End",          "\033[1;3F"),
   KEY("BackSpace",    "\033\b"),
   KEY("Return",       "\033\015"),
   KEY("space",        "\033\040"),
   KEY("Home",         "\033[1;3H"),
   KEY("End",          "\033[1;3F"),
   KEY("Prior",        "\033[5;3~"),
   KEY("Next",         "\033[6;3~"),
   KEY("Insert",       "\033[2;3~"),
   KEY("Delete",       "\033[3;3~"),
   KEY("Menu",         "\033[29;3~"),
   KEY("Find",         "\033[1;3~"),
   KEY("Help",         "\033[28;3~"),
   KEY("Execute",      "\033[3;3~"),
   KEY("Select",       "\033[4;3~"),
   KEY("F1",           "\033[O3P"),
   KEY("F2",           "\033[O3Q"),
   KEY("F3",           "\033[O3R"),
   KEY("F4",           "\033[O3S"),
   KEY("F5",           "\033[15;3~"),
   KEY("F6",           "\033[17;3~"),
   KEY("F7",           "\033[18;3~"),
   KEY("F8",           "\033[19;3~"),
   KEY("F9",           "\033[20;3~"),
   KEY("F10",          "\033[21;3~"),
   KEY("F11",          "\033[23;3~"),
   KEY("F12",          "\033[24;3~"),
   KEY("F13",          "\033[25;3~"),
   KEY("F14",          "\033[26;3~"),
   KEY("F15",          "\033[28;3~"),
   KEY("F16",          "\033[29;3~"),
   KEY("F17",          "\033[31;3~"),
   KEY("F18",          "\033[32;3~"),
   KEY("F19",          "\033[33;3~"),
   KEY("F20",          "\033[34;3~"),
   KEY("F21",          "\033[35;3~"),
   KEY("F22",          "\033[36;3~"),
   KEY("F23",          "\033[37;3~"),
   KEY("F24",          "\033[38;3~"),
   KEY("F25",          "\033[39;3~"),
   KEY("F26",          "\033[40;3~"),
   KEY("F27",          "\033[41;3~"),
   KEY("F28",          "\033[42;3~"),
   KEY("F29",          "\033[43;3~"),
   KEY("F30",          "\033[44;3~"),
   KEY("F31",          "\033[45;3~"),
   KEY("F32",          "\033[46;3~"),
   KEY("F33",          "\033[47;3~"),
   KEY("F34",          "\033[48;3~"),
   KEY("F35",          "\033[49;3~"),

   KEY(NULL, "END")
};

static const Keyout keyout[] =
{
//   KEY("BackSpace",    "\177"),
   KEY("BackSpace",    "\b"),
   KEY("Left",         "\033[D"),
   KEY("Right",        "\033[C"),
   KEY("Up",           "\033[A"),
   KEY("Down",         "\033[B"),
//   KEY("Tab",          "\t"),
//   KEY("ISO_Left_Tab", "\t"),
   KEY("Home",         "\033[H"),
   KEY("End",          "\033[F"),
   KEY("Prior",        "\033[5~"),
   KEY("Next",         "\033[6~"),
   KEY("Insert",       "\033[2~"),
   KEY("Delete",       "\033[3~"),
   KEY("Menu",         "\033[29~"),
   KEY("Find",         "\033[1~"),
   KEY("Help",         "\033[28~"),
   KEY("Execute",      "\033[3~"),
   KEY("Select",       "\033[4~"),
   KEY("F1",           "\033OP"),
   KEY("F2",           "\033OQ"),
   KEY("F3",           "\033OR"),
   KEY("F4",           "\033OS"),
   KEY("F5",           "\033[15~"),
   KEY("F6",           "\033[17~"),
   KEY("F7",           "\033[18~"),
   KEY("F8",           "\033[19~"),
   KEY("F9",           "\033[20~"),
   KEY("F10",          "\033[21~"),
   KEY("F11",          "\033[23~"),
   KEY("F12",          "\033[24~"),
   KEY("F13",          "\033[25~"),
   KEY("F14",          "\033[26~"),
   KEY("F15",          "\033[28~"),
   KEY("F16",          "\033[29~"),
   KEY("F17",          "\033[31~"),
   KEY("F18",          "\033[32~"),
   KEY("F19",          "\033[33~"),
   KEY("F20",          "\033[34~"),
   KEY("F21",          "\033[35~"),
   KEY("F22",          "\033[36~"),
   KEY("F23",          "\033[37~"),
   KEY("F24",          "\033[38~"),
   KEY("F25",          "\033[39~"),
   KEY("F26",          "\033[40~"),
   KEY("F27",          "\033[41~"),
   KEY("F28",          "\033[42~"),
   KEY("F29",          "\033[43~"),
   KEY("F30",          "\033[44~"),
   KEY("F31",          "\033[45~"),
   KEY("F32",          "\033[46~"),
   KEY("F33",          "\033[47~"),
   KEY("F34",          "\033[48~"),
   KEY("F35",          "\033[49~"),
/*   
   KEY("KP_F1",        "\033OP"),
   KEY("KP_F2",        "\033OQ"),
   KEY("KP_F3",        "\033OR"),
   KEY("KP_F4",        "\033OS"),
   KEY("KP_Begin",     "\033Ou"),
   KEY("KP_Multiply",  "\033Oj"),
   KEY("KP_Add",       "\033Ok"),
   KEY("KP_Separator", "\033Ol"),
   KEY("KP_Subtract",  "\033Om"),
   KEY("KP_Decimal",   "\033On"),
   KEY("KP_Divide",    "\033Oo"),
   KEY("KP_0",         "\033Op"),
   KEY("KP_1",         "\033Oq"),
   KEY("KP_2",         "\033Or"),
   KEY("KP_3",         "\033Os"),
   KEY("KP_4",         "\033Ot"),
   KEY("KP_5",         "\033Ou"),
   KEY("KP_6",         "\033Ov"),
   KEY("KP_7",         "\033Ow"),
   KEY("KP_8",         "\033Ox"),
   KEY("KP_9",         "\033Oy"),
 */
   KEY(NULL, "END")
};

static const Keyout kp_keyout[] =
{
   KEY("KP_Left",         "\033[D"),
   KEY("KP_Right",        "\033[C"),
   KEY("KP_Up",           "\033[A"),
   KEY("KP_Down",         "\033[B"),
   KEY("KP_Home",         "\033[H"),
   KEY("KP_End",          "\033[F"),
   KEY("KP_Prior",        "\033[5~"),
   KEY("KP_Next",         "\033[6~"),
   KEY("KP_Insert",       "\033[2~"),
   KEY("KP_Delete",       "\033[3~"),
   KEY("KP_Enter",        "\r"),
   KEY(NULL, "END")
};

static const Keyout kps_keyout[] =
{
   KEY("KP_Left",         "\033Ot"),
   KEY("KP_Right",        "\033Ov"),
   KEY("KP_Up",           "\033Ox"),
   KEY("KP_Down",         "\033Or"),
   KEY("KP_Home",         "\033Ow"),
   KEY("KP_End",          "\033Oq"),
   KEY("KP_Prior",        "\033Oy"),
   KEY("KP_Next",         "\033Os"),
   KEY("KP_Insert",       "\033Op"),
   KEY("KP_Delete",       "\033On"),
   KEY("KP_Enter",        "\033OM"),
   
   KEY(NULL, "END")
};

static Eina_Bool
_key_try(Termpty *ty, const Keyout *map, const Evas_Event_Key_Down *ev)
{
   int i, inlen;
   
   if (!ev->key) return EINA_FALSE;

   inlen = strlen(ev->key);
   for (i = 0; map[i].in; i++)
     {
        if ((inlen == map[i].inlen) && (!memcmp(ev->key, map[i].in, inlen)))
          {
             termpty_write(ty, map[i].out, map[i].outlen);
             return EINA_TRUE;
          }
     }
   return EINA_FALSE;
}

void
keyin_compose_seq_reset(Keys_Handler *khdl)
{
   char *str;

   EINA_LIST_FREE(khdl->seq, str) eina_stringshare_del(str);
   khdl->composing = EINA_FALSE;
}

static Eina_Bool
_handle_alt_ctrl(const char *keyname, Evas_Object *term)
{
   if (!strcmp(keyname, "equal"))
     termcmd_do(term, NULL, NULL, "f+");
   else if (!strcmp(keyname, "minus"))
     termcmd_do(term, NULL, NULL, "f-");
   else if (!strcmp(keyname, "0"))
     termcmd_do(term, NULL, NULL, "f");
   else if (!strcmp(keyname, "9"))
     termcmd_do(term, NULL, NULL, "fb");
   else
     return EINA_FALSE;

   return EINA_TRUE;
}

static Eina_Bool
_handle_shift(const Evas_Event_Key_Down *ev, Termpty *ty)
{
   if (!strcmp(ev->key, "Prior"))
     {
        if (!ty->altbuf)
          {
             termio_scroll_delta(ty->obj, 1, 1);
             return EINA_TRUE;
          }
        return EINA_FALSE;
     }
   else if (!strcmp(ev->key, "Next"))
     {
        if (!ty->altbuf)
          {
             termio_scroll_delta(ty->obj, -1, 1);
             return EINA_TRUE;
          }
        return EINA_FALSE;
     }
   else if (!strcmp(ev->key, "Up"))
     {
        if (!ty->altbuf)
          {
             termio_scroll_delta(ty->obj, 1, 0);
             return EINA_TRUE;
          }
        return EINA_FALSE;
     }
   else if (!strcmp(ev->key, "Down"))
     {
        if (!ty->altbuf)
          {
             termio_scroll_delta(ty->obj, -1, 0);
             return EINA_TRUE;
          }
        return EINA_FALSE;
     }
   else if (!strcmp(ev->key, "Insert"))
     {
        /*XXX CTRL*/
        if (evas_key_modifier_is_set(ev->modifiers, "Control"))
          termio_paste_selection(ty->obj, ELM_SEL_TYPE_CLIPBOARD);
        else
          termio_paste_selection(ty->obj, ELM_SEL_TYPE_PRIMARY);
     }
   else if (!strcmp(ev->key, "KP_Add"))
     {
        Config *config = termpty_config_get(ty);

        if (config) termio_font_size_set(ty->obj, config->font.size + 1);
     }
   else if (!strcmp(ev->key, "KP_Subtract"))
     {
        Config *config = termpty_config_get(ty);

        if (config) termio_font_size_set(ty->obj, config->font.size - 1);
     }
   else if (!strcmp(ev->key, "KP_Multiply"))
     {
        Config *config = termpty_config_get(ty);

        if (config) termio_font_size_set(ty->obj, 10);
     }
   else if (!strcmp(ev->key, "KP_Divide"))
     termio_take_selection(ty->obj, ELM_SEL_TYPE_CLIPBOARD);
   else
     return EINA_FALSE;

   return EINA_TRUE;
}


static void
_handle_key_to_pty(Termpty *ty, const Evas_Event_Key_Down *ev,
             int alt, int shift, int ctrl)
{
   if (!alt)
     {
      if (ty->state.crlf)
        {
           if (_key_try(ty, crlf_keyout, ev)) return;
        }
      else
        {
           if (_key_try(ty, nocrlf_keyout, ev)) return;
        }
     }
   if (
       ((ty->state.alt_kp) && (shift))
//       || ((!ty->state.alt_kp) &&
//           (!shift))
      )
     {
        if (_key_try(ty, kps_keyout, ev)) return;
     }
   else
     {
        if (!evas_key_lock_is_set(ev->locks, "Num_Lock"))
          {
             if (_key_try(ty, kp_keyout, ev)) return;
          }
     }
   if (ctrl)
     {
        if (!strcmp(ev->key, "minus"))
          {
             termpty_write(ty, "\037", 1); // generate US (unit separator)
             return;
          }
        else if (!strcmp(ev->key, "space"))
          {
             termpty_write(ty, "\0", 1); // generate 0 byte for ctrl+space
             return;
          }
        else if (!shift)
          {
             if (_key_try(ty, ctrl_keyout, ev)) return;
          }
        else
          {
             if (_key_try(ty, ctrl_shift_keyout, ev)) return;
          }
     }
   else if (shift)
     {
        if (_key_try(ty, shift_keyout, ev)) return;
     }

   else if (alt)
     {
        if (_key_try(ty, alt_keyout, ev)) return;
        if (ev->key[0] > 0 && ev->key[1] == '\0')
          {
             char echo[2];
             /* xterm and rxvt differ here about their default options: */
             /* xterm, altSendsEscape off

             echo[0] = ev->key[0] | 0x80;
             termpty_write(ty, echo, 1);
             */

             /* rxvt, with meta8 off, chose it because of utf-8 */
             echo[0] = 033;
             echo[1] = ev->key[0];
             termpty_write(ty, echo, 2);
             return;
          }
     }

   if (ty->state.appcursor)
     {
        if (_key_try(ty, appcur_keyout, ev)) return;
     }

   if (!strcmp(ev->key, "BackSpace"))
     {
        if (ty->state.send_bs)
          {
             termpty_write(ty, "\b", 1);
             return;
          }
        else
          {
             Config *cfg = termpty_config_get(ty);

             if (cfg->erase_is_del)
               {
                  termpty_write(ty, "\177", sizeof("\177") - 1);
               }
             else
               {
                  termpty_write(ty, "\b", sizeof("\b") - 1);
               }
             return;
        }
     }
   if (_key_try(ty, keyout, ev)) return;
   if (ev->string)
     {
        if ((ev->string[0]) && (!ev->string[1]))
          {
             if (alt)
               termpty_write(ty, "\033", 1);
          }
        termpty_write(ty, ev->string, strlen(ev->string));
     }
}

static Key_Binding *
key_binding_lookup(const Evas_Event_Key_Down *ev,
                   Eina_Bool ctrl, Eina_Bool alt, Eina_Bool shift)
{
   Key_Binding *kb;
   size_t len = strlen(ev->keyname);

   if (len > UINT16_MAX) return NULL;

   kb = alloca(sizeof(Key_Binding) + len + 1);
   if (!kb) return NULL;
   kb->ctrl = ctrl;
   kb->alt = alt;
   kb->shift = shift;
   kb->len = len;
   strncpy(kb->keyname, ev->keyname, kb->len + 1);

   return eina_hash_find(_key_bindings, kb);
}

Eina_Bool
keyin_handle(Keys_Handler *khdl, Termpty *ty, const Evas_Event_Key_Down *ev,
             Eina_Bool ctrl, Eina_Bool alt, Eina_Bool shift)
{
   Key_Binding *kb;

   kb = key_binding_lookup(ev, ctrl, alt, shift);
   if (kb)
     {
        if (kb->cb(ty->obj))
          goto end;
     }


   if ((!alt) && (ctrl) && (!shift))
     {
        if (!strcmp(ev->key, "Next"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "next", NULL);
             return EINA_TRUE;
          }
        else if (!strcmp(ev->key, "1"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "tab,1", NULL);
             return EINA_TRUE;
          }
        else if (!strcmp(ev->key, "2"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "tab,2", NULL);
             return EINA_TRUE;
          }
        else if (!strcmp(ev->key, "3"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "tab,3", NULL);
             return EINA_TRUE;
          }
        else if (!strcmp(ev->key, "4"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "tab,4", NULL);
             return EINA_TRUE;
          }
        else if (!strcmp(ev->key, "5"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "tab,5", NULL);
             return EINA_TRUE;
          }
        else if (!strcmp(ev->key, "6"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "tab,6", NULL);
             return EINA_TRUE;
          }
        else if (!strcmp(ev->key, "7"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "tab,7", NULL);
             return EINA_TRUE;
          }
        else if (!strcmp(ev->key, "8"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "tab,8", NULL);
             return EINA_TRUE;
          }
        else if (!strcmp(ev->key, "9"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "tab,9", NULL);
             return EINA_TRUE;
          }
        else if (!strcmp(ev->key, "0"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "tab,0", NULL);
             return EINA_TRUE;
          }
     }
   if ((!alt) && (ctrl) && (shift))
     {
        if (!strcmp(ev->key, "Prior"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "split,h", NULL);
             return EINA_TRUE;
          }
        else if (!strcmp(ev->key, "Next"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "split,v", NULL);
             return EINA_TRUE;
          }
        else if (!strcasecmp(ev->key, "t"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "new", NULL);
             return EINA_TRUE;
          }
        else if (!strcmp(ev->key, "Home"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "select", NULL);
             return EINA_TRUE;
          }
        else if (!strcasecmp(ev->key, "c"))
          {
             keyin_compose_seq_reset(khdl);
             termio_take_selection(ty->obj, ELM_SEL_TYPE_CLIPBOARD);
             return EINA_TRUE;
          }
        else if (!strcasecmp(ev->key, "v"))
          {
             keyin_compose_seq_reset(khdl);
             termio_paste_selection(ty->obj, ELM_SEL_TYPE_CLIPBOARD);
             return EINA_TRUE;
          }
        else if (!strcasecmp(ev->keyname, "h"))
          {
             term_miniview_toggle(termio_term_get(ty->obj));
             return EINA_TRUE;
          }
     }
   if ((alt) && (!shift) && (!ctrl))
     {
        if (!strcmp(ev->key, "Home"))
          {
             keyin_compose_seq_reset(khdl);
             evas_object_smart_callback_call(ty->obj, "cmdbox", NULL);
             return EINA_TRUE;
          }
     }
   if ((alt) && (ctrl) && (!shift))
     {
        if (_handle_alt_ctrl(ev->key, ty->obj))
          {
             keyin_compose_seq_reset(khdl);
             return EINA_TRUE;
          }
     }
   if (shift)
     {
        if (_handle_shift(ev, ty))
          {
             keyin_compose_seq_reset(khdl);
             return EINA_TRUE;
          }
     }


   /* actions  => return Eina_True */

   /* composing */
   if (khdl->imf)
     {
        // EXCEPTION. Don't filter modifiers alt+shift -> breaks emacs
        // and jed (alt+shift+5 for search/replace for example)
        // Don't filter modifiers alt, is used by shells
        if ((!alt) && (!ctrl))
          {
             Ecore_IMF_Event_Key_Down imf_ev;

             ecore_imf_evas_event_key_down_wrap((Evas_Event_Key_Down*)ev, &imf_ev);
             if (!khdl->composing)
               {
                  if (ecore_imf_context_filter_event
                      (khdl->imf, ECORE_IMF_EVENT_KEY_DOWN, (Ecore_IMF_Event *)&imf_ev))
                    goto end;
               }
          }
     }

   // if term app asked for kbd lock - dont handle here
   if (ty->state.kbd_lock) return EINA_TRUE;
   // if app asked us to not do autorepeat - ignore press if is it is the same
   // timestamp as last one
   if ((ty->state.no_autorepeat) &&
       (ev->timestamp == khdl->last_keyup)) return EINA_TRUE;
   if (!khdl->composing)
     {
        Ecore_Compose_State state;
        char *compres = NULL;

        keyin_compose_seq_reset(khdl);
        khdl->seq = eina_list_append(khdl->seq, eina_stringshare_add(ev->key));
        state = ecore_compose_get(khdl->seq, &compres);
        if (state == ECORE_COMPOSE_MIDDLE) khdl->composing = EINA_TRUE;
        else khdl->composing = EINA_FALSE;
        if (!khdl->composing) keyin_compose_seq_reset(khdl);
        else goto end;
     }
   else
     {
        Ecore_Compose_State state;
        char *compres = NULL;

        if (key_is_modifier(ev->key)) goto end;
        khdl->seq = eina_list_append(khdl->seq, eina_stringshare_add(ev->key));
        state = ecore_compose_get(khdl->seq, &compres);
        if (state == ECORE_COMPOSE_NONE) keyin_compose_seq_reset(khdl);
        else if (state == ECORE_COMPOSE_DONE)
          {
             keyin_compose_seq_reset(khdl);
             if (compres)
               {
                  termpty_write(ty, compres, strlen(compres));
                  free(compres);
                  compres = NULL;
               }
             goto end;
          }
        else goto end;
     }


   _handle_key_to_pty(ty, ev, alt, shift, ctrl);


end:
   return EINA_FALSE;
}

Eina_Bool
key_is_modifier(const char *key)
{
#define STATIC_STR_EQUAL(STR) (!strncmp(key, STR, strlen(STR)))
   if ((key != NULL) && (
       STATIC_STR_EQUAL("Shift") ||
       STATIC_STR_EQUAL("Control") ||
       STATIC_STR_EQUAL("Alt") ||
       STATIC_STR_EQUAL("Meta") ||
       STATIC_STR_EQUAL("Super") ||
       STATIC_STR_EQUAL("Hyper") ||
       STATIC_STR_EQUAL("Scroll_Lock") ||
       STATIC_STR_EQUAL("Num_Lock") ||
       STATIC_STR_EQUAL("ISO_Level3_Shift") ||
       STATIC_STR_EQUAL("Caps_Lock")))
     return EINA_TRUE;
#undef STATIC_STR_EQUAL
   return EINA_FALSE;
}

void
keyin_handle_up(Keys_Handler *khdl, Evas_Event_Key_Up *ev)
{
   khdl->last_keyup = ev->timestamp;
   if (khdl->imf)
     {
        Ecore_IMF_Event_Key_Up imf_ev;
        ecore_imf_evas_event_key_up_wrap(ev, &imf_ev);
        if (ecore_imf_context_filter_event
            (khdl->imf, ECORE_IMF_EVENT_KEY_UP, (Ecore_IMF_Event *)&imf_ev))
          return;
     }
}

static Eina_Bool
cb_term_prev(Evas_Object *term)
{
   evas_object_smart_callback_call(term, "prev", NULL);
   return EINA_TRUE;
}

static unsigned int
key_binding_key_length(EINA_UNUSED const void *key)
{
   return 0;
}

static int
key_binding_key_cmp(const void *key1, int key1_length,
                    const void *key2, int key2_length)
{
   const Key_Binding *kb1 = key1,
                     *kb2 = key2;

   if (key1_length < key2_length)
     return -1;
   else if (key1_length > key2_length)
     return 1;
   else
     {
        unsigned int m1 = (kb1->ctrl << 2) | (kb1->alt << 1) | kb1->shift,
                     m2 = (kb2->ctrl << 2) | (kb2->alt << 1) | kb2->shift;
        if (m1 < m2)
          return -1;
        else if (m1 > m2)
          return 1;
        else
          return strncmp(kb1->keyname, kb2->keyname, kb1->len);
     }
}

static int
key_binding_key_hash(const void *key, int key_length)
{
   const Key_Binding *kb = key;
   int hash;

   hash = eina_hash_djb2(key, key_length);
   hash &= 0x1fffffff;
   hash |= (kb->ctrl << 31) | (kb->alt << 30) | (kb->shift << 29);
   return hash;
}


Key_Binding *
key_binding_new(const char *keyname,
                Eina_Bool ctrl, Eina_Bool alt, Eina_Bool shift,
                Key_Binding_Cb cb)
{
   Key_Binding *kb;
   size_t len = strlen(keyname);

   if (len > UINT16_MAX) return NULL;

   kb = malloc(sizeof(Key_Binding) + len + 1);
   if (!kb) return NULL;
   kb->ctrl = ctrl;
   kb->alt = alt;
   kb->shift = shift;
   kb->len = len;
   strncpy(kb->keyname, keyname, kb->len + 1);
   kb->cb = cb;

   return kb;
}

int key_bindings_init(void)
{
   Key_Binding *kb;

   _key_bindings = eina_hash_new(key_binding_key_length,
                                 key_binding_key_cmp,
                                 key_binding_key_hash,
                                 free,
                                 5);
   if (!_key_bindings) return -1;

   kb = key_binding_new("Prior", 1, 0, 0, cb_term_prev);
   if (!kb) return -1;
   if (!eina_hash_direct_add(_key_bindings, kb, kb)) return -1;


   return 0;
}

void key_bindings_shutdown(void)
{
   if (_key_bindings)
     eina_hash_free(_key_bindings);
   _key_bindings = NULL;
}
