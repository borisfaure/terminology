#include <Elementary.h>
#include "termpty.h"
#include "keyin.h"

typedef struct _Keyout Keyout;

struct _Keyout
{
   const char *in;
   const char *out;
   int         inlen;
   int         outlen;
};

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
   
   KEY(NULL, "END")
};

static const Keyout shift_keyout[] =
{
   KEY("Left",         "\033[1;2D"),
   KEY("Right",        "\033[1;2C"),
   KEY("Up",           "\033[1;2A"),
   KEY("Down",         "\033[1;2B"),
   KEY("Tab",          "\033[Z"),
   KEY("ISO_Left_Tab", "\033[Z"),
   
   KEY(NULL, "END")
};

static const Keyout alt_keyout[] =
{
   KEY("Left",         "\033[1;3D"),
   KEY("Right",        "\033[1;3C"),
   KEY("Up",           "\033[1;3A"),
   KEY("Down",         "\033[1;3B"),
   KEY("End",          "\033[1;3F"),
   KEY("BackSpace",    "\033\177"),
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
   KEY("F1",           "\033[11;3~"), // \033OP
   KEY("F2",           "\033[12;3~"), // \033OQ
   KEY("F3",           "\033[13;3~"), // \033OR
   KEY("F4",           "\033[14;3~"), // \033OR
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
   KEY("F1",           "\033[11~"), // \033OP
   KEY("F2",           "\033[12~"), // \033OQ
   KEY("F3",           "\033[13~"), // \033OR
   KEY("F4",           "\033[14~"), // \033OR
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
_key_try(Termpty *ty, const Keyout *map, Evas_Event_Key_Down *ev)
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
keyin_handle(Termpty *ty, Evas_Event_Key_Down *ev,
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
