#include <Elementary.h>
#include <Ecore_Input.h>
#include <Ecore_IMF.h>
#include <Ecore_IMF_Evas.h>
#include "private.h"
#include "termpty.h"
#include "termio.h"
#include "termcmd.h"
#include "keyin.h"

typedef struct _Tty_Key Tty_Key;
typedef struct _Key_Values Key_Values;

struct _s {
    char *s;
    ssize_t len;
};

struct _Key_Values {
    struct _s plain;
    struct _s alt;
    struct _s ctrl;
    struct _s ctrl_alt;
    struct _s shift;
    struct _s shift_alt;
    struct _s shift_ctrl;
    struct _s shift_ctrl_alt;
};
struct _Tty_Key
{
    char *key;
    int key_len;
    Key_Values default_mode;
    Key_Values cursor;
};

typedef struct _Key_Binding Key_Binding;

struct _Key_Binding
{
   uint16_t ctrl  : 1;
   uint16_t alt   : 1;
   uint16_t shift : 1;
   uint16_t win   : 1;

   uint16_t len;

   Key_Binding_Cb cb;
   const char *keyname;
};

static Eina_Hash *_key_bindings = NULL;

/* {{{ Keys to TTY */

static Eina_Bool
_key_try(Termpty *ty, const Tty_Key *map, int len, const Evas_Event_Key_Down *ev,
         int alt, int shift, int ctrl)
{
   int i, inlen;

   inlen = strlen(ev->key);
   for (i = 0; i < len; i++)
     {
        if ((inlen == map[i].key_len) && (!memcmp(ev->key, map[i].key, inlen)))
          {
             const struct _s *s = NULL;
             const Key_Values *kv;

             if (!ty->state.appcursor) kv = &map[i].default_mode;
             else                      kv = &map[i].cursor;
             if (!alt && !ctrl && !shift)     s = &kv->plain;
             else if (alt && !ctrl && !shift) s = &kv->alt;
             else if (!alt && ctrl && !shift) s = &kv->ctrl;
             else if (alt && ctrl && !shift)  s = &kv->ctrl_alt;
             else if (!alt && !ctrl && shift) s = &kv->shift;
             else if (alt && !ctrl && shift)  s = &kv->shift_alt;
             else if (!alt && ctrl && shift)  s = &kv->shift_ctrl;
             else if (alt && ctrl && shift)   s = &kv->shift_ctrl_alt;

             if (s) termpty_write(ty, s->s, s->len);
             return EINA_TRUE;
          }
     }
   return EINA_FALSE;
}

/* }}} */
/* {{{ Handle keys */

void
keyin_compose_seq_reset(Keys_Handler *khdl)
{
   char *str;

   EINA_LIST_FREE(khdl->seq, str) eina_stringshare_del(str);
   khdl->composing = EINA_FALSE;
}

#include "tty_keys.h"

static void
_handle_key_to_pty(Termpty *ty, const Evas_Event_Key_Down *ev,
                   int alt, int shift, int ctrl)
{
   if (!ev->key)
     return;

   if (!strcmp(ev->key, "BackSpace"))
     {
        if (alt)
          termpty_write(ty, "\033", 1);
        if (ty->state.send_bs)
          {
             termpty_write(ty, "\b", 1);
          }
        else
          {
             Config *cfg = termpty_config_get(ty);

             if (cfg->erase_is_del && !ctrl)
               {
                  termpty_write(ty, "\177", sizeof("\177") - 1);
               }
             else
               {
                  termpty_write(ty, "\b", sizeof("\b") - 1);
               }
          }
        return;
     }
   if (!strcmp(ev->key, "Return"))
     {
        if (alt)
          termpty_write(ty, "\033", 1);
        if (ty->state.crlf)
          {
             termpty_write(ty, "\r\n", sizeof("\r\n") - 1);
             return;
          }
        else
          {
             termpty_write(ty, "\r", sizeof("\r") - 1);
             return;
          }
     }
   if (ev->key && ev->key[0] == 'K' && ev->key[1] == 'k')
     {
        if (!evas_key_lock_is_set(ev->locks, "Num_Lock"))
          {
             if (ty->state.alt_kp)
               {
                  if (_key_try(ty, tty_keys_kp_app,
                               sizeof(tty_keys_kp_app)/sizeof(tty_keys_kp_app[0]),
                               ev, alt, shift, ctrl))
                    return;
               }
             else
               {
                  if (_key_try(ty, tty_keys_kp_plain,
                               sizeof(tty_keys_kp_plain)/sizeof(tty_keys_kp_plain[0]),
                               ev, alt, shift, ctrl))
                    return;
               }
          }
     }
   else
     if (_key_try(ty, tty_keys, sizeof(tty_keys)/sizeof(tty_keys[0]), ev,
                  alt, shift, ctrl))
       return;

   if (ev->string)
     {
        if (alt)
          termpty_write(ty, "\033", 1);
        termpty_write(ty, ev->string, strlen(ev->string));
     }
}

static Key_Binding *
key_binding_lookup(const char *keyname,
                   Eina_Bool ctrl, Eina_Bool alt, Eina_Bool shift, Eina_Bool win)
{
   Key_Binding *kb;
   size_t len = strlen(keyname);

   if (len > UINT16_MAX) return NULL;

   kb = alloca(sizeof(Key_Binding));
   if (!kb) return NULL;
   kb->ctrl = ctrl;
   kb->alt = alt;
   kb->shift = shift;
   kb->win = win;
   kb->len = len;
   kb->keyname = alloca(sizeof(char) * len + 1);
   strncpy((char *)kb->keyname, keyname, kb->len + 1);

   return eina_hash_find(_key_bindings, kb);
}

Eina_Bool
keyin_handle(Keys_Handler *khdl, Termpty *ty, const Evas_Event_Key_Down *ev,
             Eina_Bool ctrl, Eina_Bool alt, Eina_Bool shift, Eina_Bool win)
{

   if (!evas_key_modifier_is_set(ev->modifiers, "Meta") &&
       !evas_key_modifier_is_set(ev->modifiers, "Hyper") &&
       !evas_key_modifier_is_set(ev->modifiers, "AltGr") &&
       !evas_key_modifier_is_set(ev->modifiers, "ISO_Level3_Shift"))
     {
        Key_Binding *kb;
        kb = key_binding_lookup(ev->keyname, ctrl, alt, shift, win);
        if (kb)
          {
             if (kb->cb(ty->obj))
               {
                  keyin_compose_seq_reset(khdl);
                  return EINA_TRUE;
               }
          }
     }

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
       STATIC_STR_EQUAL("AltGr") ||
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

/* }}} */
/* {{{ Callbacks */

static Eina_Bool
cb_term_prev(Evas_Object *term)
{
   evas_object_smart_callback_call(term, "prev", NULL);
   return EINA_TRUE;
}

static Eina_Bool
cb_term_next(Evas_Object *term)
{
   evas_object_smart_callback_call(term, "next", NULL);
   return EINA_TRUE;
}

#define CB_TAB(N)\
static Eina_Bool \
cb_tab_##N(Evas_Object *term) \
{                            \
   evas_object_smart_callback_call(term, "tab,"#N, NULL); \
   return EINA_TRUE; \
}

CB_TAB(0)
CB_TAB(1)
CB_TAB(2)
CB_TAB(3)
CB_TAB(4)
CB_TAB(5)
CB_TAB(6)
CB_TAB(7)
CB_TAB(8)
CB_TAB(9)
#undef CB_TAB

static Eina_Bool
cb_cmd_box(Evas_Object *term)
{
   evas_object_smart_callback_call(term, "cmdbox", NULL);
   return EINA_TRUE;
}

static Eina_Bool
cb_split_h(Evas_Object *term)
{
   evas_object_smart_callback_call(term, "split,h", NULL);
   return EINA_TRUE;
}

static Eina_Bool
cb_split_v(Evas_Object *term)
{
   evas_object_smart_callback_call(term, "split,v", NULL);
   return EINA_TRUE;
}

static Eina_Bool
cb_tab_new(Evas_Object *term)
{
   evas_object_smart_callback_call(term, "new", NULL);
   return EINA_TRUE;
}

static Eina_Bool
cb_exited(Evas_Object *term)
{
   evas_object_smart_callback_call(term, "exited", NULL);
   return EINA_TRUE;
}

static Eina_Bool
cb_tab_select(Evas_Object *term)
{
   evas_object_smart_callback_call(term, "select", NULL);
   return EINA_TRUE;
}

static Eina_Bool
cb_copy_clipboard(Evas_Object *term)
{
   Termpty *ty = termio_pty_get(term);

   if (!ty || !ty->selection.is_active)
     return EINA_FALSE;
   termio_take_selection(term, ELM_SEL_TYPE_CLIPBOARD);
   return EINA_TRUE;
}

static Eina_Bool
cb_paste_clipboard(Evas_Object *term)
{
   termio_paste_selection(term, ELM_SEL_TYPE_CLIPBOARD);
   return EINA_TRUE;
}

static Eina_Bool
cb_paste_primary(Evas_Object *term)
{
   termio_paste_selection(term, ELM_SEL_TYPE_PRIMARY);
   return EINA_TRUE;
}

static Eina_Bool
cb_copy_primary(Evas_Object *term)
{
   Termpty *ty = termio_pty_get(term);

   if (!ty || !ty->selection.is_active)
     return EINA_FALSE;
   termio_take_selection(term, ELM_SEL_TYPE_PRIMARY);
   return EINA_TRUE;
}

static Eina_Bool
cb_miniview(Evas_Object *term)
{
   term_miniview_toggle(termio_term_get(term));
   return EINA_TRUE;
}

static Eina_Bool
cb_increase_font_size(Evas_Object *term)
{
   termcmd_do(term, NULL, NULL, "f+");
   return EINA_TRUE;
}

static Eina_Bool
cb_decrease_font_size(Evas_Object *term)
{
   termcmd_do(term, NULL, NULL, "f-");
   return EINA_TRUE;
}

static Eina_Bool
cb_reset_font_size(Evas_Object *term)
{
   termcmd_do(term, NULL, NULL, "f");
   return EINA_TRUE;
}

static Eina_Bool
cb_big_font_size(Evas_Object *term)
{
   termcmd_do(term, NULL, NULL, "fb");
   return EINA_TRUE;
}

static Eina_Bool
cb_scroll_up_page(Evas_Object *term)
{
   Termpty *ty = termio_pty_get(term);

   if (!ty || ty->altbuf)
     return EINA_FALSE;

   termio_scroll_delta(term, 1, 1);
   return EINA_TRUE;
}

static Eina_Bool
cb_scroll_down_page(Evas_Object *term)
{
   Termpty *ty = termio_pty_get(term);

   if (!ty || ty->altbuf)
     return EINA_FALSE;

   termio_scroll_delta(term, -1, 1);
   return EINA_TRUE;
}

static Eina_Bool
cb_scroll_up_line(Evas_Object *term)
{
   Termpty *ty = termio_pty_get(term);

   if (!ty || ty->altbuf)
     return EINA_FALSE;

   termio_scroll_delta(term, 1, 0);
   return EINA_TRUE;
}

static Eina_Bool
cb_scroll_down_line(Evas_Object *term)
{
   Termpty *ty = termio_pty_get(term);

   if (!ty || ty->altbuf)
     return EINA_FALSE;

   termio_scroll_delta(term, -1, 0);
   return EINA_TRUE;
}


static Shortcut_Action _actions[] =
{
     {"group", gettext_noop("Scrolling"), NULL},
     {"one_page_up", gettext_noop("Scroll one page up"), cb_scroll_up_page},
     {"one_page_down", gettext_noop("Scroll one page down"), cb_scroll_down_page},
     {"one_line_up", gettext_noop("Scroll one line up"), cb_scroll_up_line},
     {"one_line_down", gettext_noop("Scroll one line down"), cb_scroll_down_line},

     {"group", gettext_noop("Copy/Paste"), NULL},
     {"copy_primary", gettext_noop("Copy selection to Primary buffer"), cb_copy_primary},
     {"copy_clipboard", gettext_noop("Copy selection to Clipboard buffer"), cb_copy_clipboard},
     {"paste_primary", gettext_noop("Paste Primary buffer (highlight)"), cb_paste_primary},
     {"paste_clipboard", gettext_noop("Paste Clipboard buffer (ctrl+c/v)"), cb_paste_clipboard},

     {"group", gettext_noop("Splits/Tabs"), NULL},
     {"term_prev", gettext_noop("Focus to the previous terminal"), cb_term_prev},
     {"term_next", gettext_noop("Focus to the next terminal"), cb_term_next},
     {"split_h", gettext_noop("Split horizontally (new below)"), cb_split_h},
     {"split_v", gettext_noop("Split vertically (new on right)"), cb_split_v},
     {"tab_new", gettext_noop("Create a new \"tab\""), cb_tab_new},
     {"exited", gettext_noop("Close the focused terminal"), cb_exited},
     {"tab_select", gettext_noop("Bring up \"tab\" switcher"), cb_tab_select},
     {"tab_1", gettext_noop("Switch to terminal tab 1"), cb_tab_1},
     {"tab_2", gettext_noop("Switch to terminal tab 2"), cb_tab_2},
     {"tab_3", gettext_noop("Switch to terminal tab 3"), cb_tab_3},
     {"tab_4", gettext_noop("Switch to terminal tab 4"), cb_tab_4},
     {"tab_5", gettext_noop("Switch to terminal tab 5"), cb_tab_5},
     {"tab_6", gettext_noop("Switch to terminal tab 6"), cb_tab_6},
     {"tab_7", gettext_noop("Switch to terminal tab 7"), cb_tab_7},
     {"tab_8", gettext_noop("Switch to terminal tab 8"), cb_tab_8},
     {"tab_9", gettext_noop("Switch to terminal tab 9"), cb_tab_9},
     {"tab_10", gettext_noop("Switch to terminal tab 10"), cb_tab_0},

     {"group", gettext_noop("Font size"), NULL},
     {"increase_font_size", gettext_noop("Font size up 1"), cb_increase_font_size},
     {"decrease_font_size", gettext_noop("Font size down 1"), cb_decrease_font_size},
     {"big_font_size", gettext_noop("Display big font size"), cb_big_font_size},
     {"reset_font_size", gettext_noop("Reset font size"), cb_reset_font_size},

     {"group", gettext_noop("Actions"), NULL},
     {"miniview", gettext_noop("Display the history miniview"), cb_miniview},
     {"cmd_box", gettext_noop("Display the command box"), cb_cmd_box},

     {NULL, NULL, NULL}
};

const Shortcut_Action *
shortcut_actions_get(void)
{
   return _actions;
}

/* }}} */
/* {{{ Key bindings */

static unsigned int
_key_binding_key_length(EINA_UNUSED const void *key)
{
   return 0;
}

static int
_key_binding_key_cmp(const void *key1, int key1_length,
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
        unsigned int m1 = (kb1->win << 3) | (kb1->ctrl << 2) | (kb1->alt << 1) | kb1->shift,
                     m2 = (kb2->win << 3) | (kb2->ctrl << 2) | (kb2->alt << 1) | kb2->shift;
        if (m1 < m2)
          return -1;
        else if (m1 > m2)
          return 1;
        else
          return strncmp(kb1->keyname, kb2->keyname, kb1->len);
     }
}

static int
_key_binding_key_hash(const void *key, int key_length)
{
   const Key_Binding *kb = key;
   int hash;

   hash = eina_hash_djb2(key, key_length);
   hash &= 0x0fffffff;
   hash |= (kb->win << 31) | (kb->ctrl << 30) | (kb->alt << 29) | (kb->shift << 28);
   return hash;
}


static Key_Binding *
_key_binding_new(const char *keyname,
                 Eina_Bool ctrl, Eina_Bool alt, Eina_Bool shift, Eina_Bool win,
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
   kb->win = win;
   kb->len = len;
   kb->keyname = eina_stringshare_add(keyname);
   kb->cb = cb;

   return kb;
}

static void
_key_binding_free(void *data)
{
   Key_Binding *kb = data;
   if (!kb) return;
   eina_stringshare_del(kb->keyname);
   free(kb);
}

int
keyin_add_config(Config_Keys *key)
{
   Shortcut_Action *action = _actions;

   while (action->action && strcmp(action->action, key->cb))
     action++;

   if (action->action)
     {
        Key_Binding *kb;
        kb = _key_binding_new(key->keyname, key->ctrl, key->alt,
                              key->shift, key->win, action->cb);
        if (!kb) return -1;
        if (eina_hash_find(_key_bindings, kb) ||
            (!eina_hash_direct_add(_key_bindings, kb, kb)))
          {
             _key_binding_free(kb);
             return -1;
          }
        return 0;
     }
   return -1;
}

int
keyin_remove_config(Config_Keys *key)
{
   Key_Binding *kb;

   kb = key_binding_lookup(key->keyname, key->ctrl, key->alt, key->shift, key->win);
   if (kb)
     eina_hash_del_by_key(_key_bindings, kb);
   return 0;
}

int 
key_bindings_load(Config *config)
{
   Config_Keys *key;
   Eina_List *l;

   if (!_key_bindings)
     {
#if HAVE_GETTEXT && ENABLE_NLS
        Shortcut_Action *action = _actions;

        while (action->action)
          {
             action->description = gettext(action->description);
             action++;
          }
#endif
        _key_bindings = eina_hash_new(_key_binding_key_length,
                                      _key_binding_key_cmp,
                                      _key_binding_key_hash,
                                      _key_binding_free,
                                      5);
        if (!_key_bindings) return -1;
     }
   else
     {
        eina_hash_free_buckets(_key_bindings);
     }

   EINA_LIST_FOREACH(config->keys, l, key)
     {
        int res = keyin_add_config(key);
        if (res != 0)
          return res;
     }

   return 0;
}

void 
key_bindings_shutdown(void)
{
   if (_key_bindings)
     eina_hash_free(_key_bindings);
   _key_bindings = NULL;
}

/* }}} */
