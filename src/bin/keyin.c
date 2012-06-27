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
   KEY("Home",         "\033[7~"),
   KEY("End",          "\033[8~"),
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
   
   KEY(NULL, "END")
};

static const Keyout shift_keyout[] =
{
   KEY("Left",         "\033[1;2D"),
   KEY("Right",        "\033[1;2C"),
   KEY("Up",           "\033[1;2A"),
   KEY("Down",         "\033[1;2B"),
   KEY("Tab",          "\033[Z"),
   
   KEY(NULL, "END")
};

static const Keyout alt_keyout[] =
{
   KEY("Left",         "\033[1;3D"),
   KEY("Right",        "\033[1;3C"),
   KEY("Up",           "\033[1;3A"),
   KEY("Down",         "\033[1;3B"),
   
   KEY(NULL, "END")
};

static const Keyout keyout[] =
{
   KEY("BackSpace",    "\177"),
//   KEY("BackSpace",    "\b"),
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
   
   if (!ev->keyname) return EINA_FALSE;

   inlen = strlen(ev->keyname);
   for (i = 0; map[i].in; i++)
     {
        if ((inlen == map[i].inlen) && (!memcmp(ev->keyname, map[i].in, inlen)))
          {
             termpty_write(ty, map[i].out, map[i].outlen);
             return EINA_TRUE;
          }
     }
   return EINA_FALSE;
}

void
keyin_handle(Termpty *ty, Evas_Event_Key_Down *ev)
{
   if (ty->state.crlf)
     {
        if (_key_try(ty, crlf_keyout, ev)) return;
     }
   else
     {
        if (_key_try(ty, nocrlf_keyout, ev)) return;
     }
   if (
       ((ty->state.alt_kp) &&
           (evas_key_modifier_is_set(ev->modifiers, "Shift")))
//       || ((!ty->state.alt_kp) &&
//           (!evas_key_modifier_is_set(ev->modifiers, "Shift")))
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
   if (evas_key_modifier_is_set(ev->modifiers, "Control"))
     {
        if (!strcmp(ev->keyname, "Minus"))
          {
             termpty_write(ty, "\037", 1); // generate US (unit separator)
             return;
          }
        else if (!strcmp(ev->keyname, "space"))
          {
             termpty_write(ty, "\0", 1); // generate 0 byte for ctrl+space
             return;
          }
        else if (!evas_key_modifier_is_set(ev->modifiers, "Shift"))
          {
             if (_key_try(ty, ctrl_keyout, ev)) return;
          }
     }
   else if (evas_key_modifier_is_set(ev->modifiers, "Shift"))
     {
        if (_key_try(ty, shift_keyout, ev)) return;
     }

   else if (evas_key_modifier_is_set(ev->modifiers, "Alt"))
     {
        if (_key_try(ty, alt_keyout, ev)) return;
     }

   if (ty->state.appcursor)
     {
        if (_key_try(ty, appcur_keyout, ev)) return;
     }

   if ((ty->state.send_bs) && (!strcmp(ev->keyname, "BackSpace")))
     {
        termpty_write(ty, "\b", 1);
        return;
     }
   if (_key_try(ty, keyout, ev)) return;
   if (ev->string)
     {
        if ((ev->string[0]) && (!ev->string[1]))
          {
             if (evas_key_modifier_is_set(ev->modifiers, "Alt"))
               termpty_write(ty, "\033", 1);
          }
        termpty_write(ty, ev->string, strlen(ev->string));
     }
}

// http://en.wikipedia.org/wiki/Compose_key
// http://andrew.triumf.ca/iso8859-1-compose.html
// http://cgit.freedesktop.org/xorg/lib/libX11/plain/nls/en_US.UTF-8/Compose.pre

// isolate compose tree into its own file
#include "keycomp.h"

int
keyin_handle_compose(Termpty *ty, char **seq)
{
   Comp *c, *cend;
   const char *s;
   int i = 0;
   
   s = seq[i];
   cend = (Comp *)comp + (sizeof(comp) / sizeof(comp[0]));
   for (c = (Comp *)comp; c->s && s;)
     {
        // doesn't match -> jump to next level entry
        if (!(!strcmp(s, c->s)))
          {
             c += c->jump + 1;
             if (c >= cend) return 0;
          }
        else
          {
             cend = c + c->jump;
             // advance to next sequence member 
             i++;
             s = seq[i];
             c++;
             // if advanced item jump is an endpoint - it's the string we want
             if (c->jump == 0)
               {
                  termpty_write(ty, c->s, strlen(c->s));
                  return -1;
               }
          }
     }
   if (i > 0) return 1;
   return 0;
}

/*
typedef struct _Compose Compose;

struct _Compose
{
   unsigned char c1, c2;
   const char *out;
   int         outlen;
};

#define COM(c1, c2, out) {c1, c2, out, sizeof(out) - 1}

static Compose composes[] =
{
   COM('!',  '!',     "¡"),
   COM('|',  'c',     "¢"),
   COM('-',  'L',     "£"),
   COM('o',  'x',     "¤"),
   COM('Y',  '-',     "¥"),
   COM('|',  '|',     "¦"),
   COM('s',  'o',     "§"),
   COM('"',  '"',     "¨"),
   COM('O',  'c',     "©"),
   COM('_',  'a',     "ª"),
   COM('<',  '<',     "«"),
   COM(',',  '-',     "¬"),
   COM('-',  '-',     "­"),
   COM('O',  'R',     "®"),
   COM('-',  '^',     "¯"),
   COM('^',  '0',     "°"),
   COM('+',  '-',     "±"),
   COM('^',  '2',     "²"),
   COM('^',  '3',     "³"),
   COM('\'', '\'',    "´"),
   COM('/',  'u',     "µ"),
   COM('p',  '!',     "¶"),
   COM('.',  '.',     "…"),
   COM(',',  ',',     "¸"),
   COM('^',  '1',     "¹"),
   COM('_',  'o',     "º"),
   COM('>',  '>',     "»"),
   COM('1',  '4',     "¼"),
   COM('1',  '2',     "½"),
   COM('3',  '4',     "¾"),
   COM('?',  '?',     "¿"),
   COM('`',  'A',     "À"),
   COM('\'', 'A',     "Á"),
   COM('^',  'A',     "Â"),
   COM('~',  'A',     "Ã"),
   COM('"',  'A',     "Ä"),
   COM('*',  'A',     "Å"),
   COM('A',  'E',     "Æ"),
   COM(',',  'C',     "Ç"),
   COM('`',  'E',     "È"),
   COM('\'', 'E',     "É"),
   COM('^',  'E',     "Ê"),
   COM('"',  'E',     "Ë"),
   COM('`',  'I',     "Ì"),
   COM('\'', 'I',     "Í"),
   COM('^',  'I',     "Î"),
   COM('"',  'I',     "Ï"),
   COM('-',  'D',     "Ð"),
   COM('~',  'N',     "Ñ"),
   COM('`',  'O',     "Ò"),
   COM('\'', 'O',     "Ó"),
   COM('^',  'O',     "Ô"),
   COM('~',  'O',     "Õ"),
   COM('"',  'O',     "Ö"),
   COM('x',  'x',     "×"),
   COM('/',  'O',     "Ø"),
   COM('`',  'U',     "Ù"),
   COM('\'', 'U',     "Ú"),
   COM('^',  'U',     "Û"),
   COM('"',  'U',     "Ü"),
   COM('\'', 'Y',     "Ý"),
   COM('T',  'H',     "þ"),
   COM('s',  's',     "ß"),
   COM('`',  'a',     "à"),
   COM('\'', 'a',     "á"),
   COM('^',  'a',     "â"),
   COM('~',  'a',     "ã"),
   COM('"',  'a',     "ä"),
   COM('*',  'a',     "å"),
   COM('a',  'e',     "æ"),
   COM(',',  'c',     "ç"),
   COM('`',  'e',     "è"),
   COM('\'', 'e',     "é"),
   COM('^',  'e',     "ê"),
   COM('"',  'e',     "ë"),
   COM('`',  'i',     "ì"),
   COM('\'', 'i',     "í"),
   COM('^',  'i',     "î"),
   COM('"',  'i',     "ï"),
   COM('-',  'd',     "ð"),
   COM('~',  'n',     "ñ"),
   COM('`',  'o',     "ò"),
   COM('\'', 'o',     "ó"),
   COM('^',  'o',     "ô"),
   COM('~',  'o',     "õ"),
   COM('"',  'o',     "ö"),
   COM('-',  ':',     "÷"),
   COM('/',  'o',     "ø"),
   COM('`',  'u',     "ù"),
   COM('\'', 'u',     "ú"),
   COM('^',  'u',     "û"),
   COM('"',  'u',     "ü"),
   COM('\'', 'y',     "ý"),
   COM('t',  'h',     "þ"),
   COM('"',  'y',     "ÿ"),

   COM(0, 0, "END")
};

void
keyin_handle_compose(Termpty *ty, unsigned char c1, unsigned char c2)
{
   int i;
   
   for (i = 0; composes[i].c1; i++)
     {
        if ((c1 == composes[i].c1) && (c2 == composes[i].c2))
          {
             termpty_write(ty, composes[i].out, composes[i].outlen);
             break;
          }
     }
}
*/
