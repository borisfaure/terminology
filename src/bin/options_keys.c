#include "private.h"

#include <Elementary.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_keys.h"
#include "keyin.h"

/*XXX: can have only one widget at a time… */
static Config *_config;
static Evas_Object *_fr;
static Evas_Object *_rect, *_bg, *_lbl;

static void _hover_del(Evas_Object *o);

static void
_shortcut_delete(void *data, Evas_Object *obj EINA_UNUSED,
                 void *event_info EINA_UNUSED)
{
   Evas_Object *hs, *bx;
   Config_Keys *cfg_key;
   Evas_Coord w, min_w, min_h;

   hs = data;
   bx = evas_object_data_get(hs, "bx");
   cfg_key = evas_object_data_get(hs, "cfg");

   _config->keys = eina_list_remove(_config->keys, cfg_key);
   evas_object_size_hint_min_get(hs, &w, NULL);
   evas_object_size_hint_min_get(bx, &min_w, &min_h);
   min_w -= w;
   evas_object_size_hint_min_set(bx, min_w, min_h);

   evas_object_del(hs);
   keyin_remove_config(cfg_key);

   eina_stringshare_del(cfg_key->keyname);
   eina_stringshare_del(cfg_key->cb);
   free(cfg_key);

   config_save(_config, NULL);
}

static Evas_Object *
_shortcut_button_add(Evas_Object *bx, const Config_Keys *key)
{
   const char *txt;
   Evas_Object *hs;

   txt = eina_stringshare_printf("%s%s%s%s",
                                 key->ctrl ? _("Ctrl+") : "",
                                 key->alt ? _("Alt+") : "",
                                 key->shift ? _("Shift+") : "",
                                 key->keyname);
   hs = elm_hoversel_add(bx);
   elm_hoversel_hover_parent_set(hs, _fr);

   evas_object_data_set(hs, "bx", bx);
   evas_object_data_set(hs, "cfg", key);
   elm_layout_text_set(hs, NULL, txt);

   elm_hoversel_item_add(hs, _("Delete"), "delete", ELM_ICON_STANDARD,
                         _shortcut_delete, hs);

   eina_stringshare_del(txt);
   return hs;
}

static void
_cb_key_up(void *data, Evas *e EINA_UNUSED,
           Evas_Object *obj, void *event)
{
   Evas_Event_Key_Up *ev = event;
   int ctrl, alt, shift, res;
   Config_Keys *cfg_key;
   Shortcut_Action *action;
   Evas_Object *bx = data;

   if (key_is_modifier(ev->keyname))
     return;

   ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   alt = evas_key_modifier_is_set(ev->modifiers, "Alt");
   shift = evas_key_modifier_is_set(ev->modifiers, "Shift");

   _hover_del(obj);

   action = evas_object_data_get(bx, "action");

   cfg_key = calloc(1, sizeof(Config_Keys));
   if (!cfg_key)
     return;
   cfg_key->keyname = eina_stringshare_add(ev->keyname);
   cfg_key->ctrl = ctrl;
   cfg_key->alt = alt;
   cfg_key->shift = shift;
   cfg_key->cb = eina_stringshare_add(action->action);

   res = keyin_add_config(cfg_key);
   if (res == 0)
     {
        Evas_Object *bt, *last;
        Evas_Coord w, h, min_w, min_h;

        last = evas_object_data_get(bx, "last");
        _config->keys = eina_list_append(_config->keys, cfg_key);
        bt = _shortcut_button_add(bx, cfg_key);
        evas_object_show(bt);
        evas_object_size_hint_min_get(bt, &w, &h);
        evas_object_size_hint_min_get(bx, &min_w, &min_h);
        min_w += w;
        if (h > min_h) min_h = h;
        evas_object_size_hint_min_set(bx, min_w, min_h);
        elm_box_pack_before(bx, bt, last);

        config_save(_config, NULL);
     }
   else
     {
        eina_stringshare_del(cfg_key->keyname);
        eina_stringshare_del(cfg_key->cb);
        free(cfg_key);
     }
}

static void
_cb_mouse_down(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
               Evas_Object *obj, void *event EINA_UNUSED)
{
   _hover_del(obj);
}


static void
_hover_sizing_eval(void)
{
   Evas_Coord x = 0, y = 0, w = 0, h = 0, min_w, min_h, new_x, new_y;
   evas_object_geometry_get(_fr, &x, &y, &w, &h);
   evas_object_geometry_set(_rect, x, y, w, h);
   evas_object_size_hint_min_get(_lbl, &min_w, &min_h);
   new_x = x + w/2 - min_w/2;
   new_y = y + h/2 - min_h/2;
   evas_object_geometry_set(_lbl, new_x, new_y, min_w, min_h);
   evas_object_geometry_set(_bg, new_x - 1, new_y - 1, min_w + 2, min_h + 2);
}
static void
_parent_move_cb(void *data EINA_UNUSED,
                Evas *e EINA_UNUSED,
                Evas_Object *obj EINA_UNUSED,
                void *event_info EINA_UNUSED)
{
   _hover_sizing_eval();
}
static void
_parent_resize_cb(void *data EINA_UNUSED,
                  Evas *e EINA_UNUSED,
                  Evas_Object *obj EINA_UNUSED,
                  void *event_info EINA_UNUSED)
{
   _hover_sizing_eval();
}

static void
_parent_hide_cb(void *data,
                Evas *e EINA_UNUSED,
                Evas_Object *obj EINA_UNUSED,
                void *event_info EINA_UNUSED)
{
   _hover_del(data);
}
static void
_parent_del_cb(void *data,
               Evas *e EINA_UNUSED,
               Evas_Object *obj EINA_UNUSED,
               void *event_info EINA_UNUSED)
{
   _hover_del(data);
}

static void
_hover_del(Evas_Object *o)
{
   elm_object_focus_set(o, EINA_FALSE);
   evas_object_event_callback_del(o, EVAS_CALLBACK_KEY_UP,
                                  _cb_key_up);
   evas_object_event_callback_del(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _cb_mouse_down);
   evas_object_event_callback_del(_fr, EVAS_CALLBACK_MOVE,
                                  _parent_move_cb);
   evas_object_event_callback_del(_fr, EVAS_CALLBACK_RESIZE,
                                  _parent_resize_cb);
   evas_object_event_callback_del(_fr, EVAS_CALLBACK_HIDE,
                                  _parent_hide_cb);
   evas_object_event_callback_del(_fr, EVAS_CALLBACK_DEL,
                                  _parent_del_cb);
   evas_object_del(o);
   _rect = NULL;
   evas_object_del(_bg);
   _bg = NULL;
   evas_object_del(_lbl);
   _lbl = NULL;
}

static void
on_shortcut_add(void *data,
                Evas_Object *obj,
                void *event_info EINA_UNUSED)
{
   Evas_Object *o, *bx;

   bx = data;

   _rect = o = evas_object_rectangle_add(evas_object_evas_get(obj));
   evas_object_repeat_events_set(o, EINA_TRUE);
   evas_object_color_set(o, 0, 0, 0, 127);
   elm_object_focus_set(o, EINA_TRUE);
   evas_object_show(o);

   _bg = elm_bg_add(obj);
   evas_object_show(_bg);
   _lbl = elm_label_add(obj);
   elm_layout_text_set(_lbl, NULL, _("Please press key sequence"));
   evas_object_show(_lbl);

   _hover_sizing_eval();

   evas_object_event_callback_add(o, EVAS_CALLBACK_KEY_UP,
                                  _cb_key_up, bx);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _cb_mouse_down, o);
   evas_object_event_callback_add(_fr, EVAS_CALLBACK_MOVE,
                                  _parent_move_cb, o);
   evas_object_event_callback_add(_fr, EVAS_CALLBACK_RESIZE,
                                  _parent_resize_cb, o);
   evas_object_event_callback_add(_fr, EVAS_CALLBACK_HIDE,
                                  _parent_hide_cb, o);
   evas_object_event_callback_add(_fr, EVAS_CALLBACK_DEL,
                                  _parent_del_cb, o);
}

static Evas_Object *
gl_content_get(void *data, Evas_Object *obj, const char *part EINA_UNUSED)
{
   Evas_Coord min_w = 0, w = 0, min_h = 0, h = 0;
   const Shortcut_Action *action = data;
   Evas_Object *bx, *bt, *lbl, *sep;
   Config_Keys *key;
   Eina_List *l;

   bx = elm_box_add(obj);
   elm_box_horizontal_set(bx, EINA_TRUE);
   elm_box_homogeneous_set(bx, EINA_FALSE);
   evas_object_size_hint_align_set(bx, 0, 0);

   lbl = elm_label_add(obj);
   elm_layout_text_set(lbl, NULL, action->description);
   elm_box_pack_end(bx, lbl);
   evas_object_show(lbl);
   evas_object_size_hint_min_get(lbl, &w, &h);
   min_w += w;
   if (h > min_h) min_h = h;

   sep = elm_separator_add(obj);
   elm_box_pack_end(bx, sep);
   evas_object_show(sep);
   evas_object_size_hint_min_get(sep, &w, &h);
   min_w += w;
   if (h > min_h) min_h = h;

   // TODO: have a better data structure
   EINA_LIST_FOREACH(_config->keys, l, key)
     {
        if (!strcmp(key->cb, action->action))
          {
             bt = _shortcut_button_add(bx, key);
             evas_object_show(bt);
             evas_object_size_hint_min_get(bt, &w, &h);
             min_w += w;
             if (h > min_h) min_h = h;
             elm_box_pack_end(bx, bt);
          }
     }

   bt = elm_button_add(obj);
   evas_object_smart_callback_add(bt, "clicked", on_shortcut_add, bx);
   evas_object_propagate_events_set(bt, EINA_FALSE);
   elm_layout_text_set(bt, NULL, "+");
   evas_object_size_hint_min_get(bt, &w, &h);
   min_w += w;
   if (h > min_h) min_h = h;
   evas_object_show(bt);
   elm_box_pack_end(bx, bt);

   evas_object_data_set(bx, "last", bt);
   evas_object_data_set(bx, "action", action);

   evas_object_show(bx);
   evas_object_size_hint_min_set(bx, min_w, min_h);

   return bx;
}



char *gl_group_text_get(void *data, Evas_Object *obj EINA_UNUSED,
                        const char *part EINA_UNUSED)
{
   Shortcut_Action *action = data;
   return strdup(action->description);
}

void
options_keys(Evas_Object *opbox, Evas_Object *term)
{
   Evas_Object *o, *gl, *bx;
   const Shortcut_Action *action;
   Elm_Genlist_Item_Class *itc, *itc_group;
   Elm_Object_Item *git = NULL;

   _config = termio_config_get(term);

   _fr = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Key Bindings"));
   elm_box_pack_end(opbox, o);
   evas_object_show(o);

   bx = elm_box_add(opbox);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(bx, 0.0, 0.0);
   elm_object_content_set(_fr, bx);
   evas_object_show(bx);

   gl = elm_genlist_add(opbox);
   evas_object_size_hint_weight_set(gl, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(gl, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(gl, ELM_SCROLLER_POLICY_AUTO, ELM_SCROLLER_POLICY_AUTO);
   elm_genlist_mode_set(gl, ELM_LIST_SCROLL);
   elm_genlist_homogeneous_set(gl, EINA_FALSE);
   elm_box_pack_end(bx, gl);
   evas_object_show(gl);

   itc = elm_genlist_item_class_new();
   itc->item_style = "one_icon";
   itc->func.text_get = NULL;
   itc->func.content_get = gl_content_get;
   itc->func.state_get = NULL;
   itc->func.del = NULL;

   itc_group = elm_genlist_item_class_new();
   itc_group->item_style = "group_index";
   itc_group->func.text_get = gl_group_text_get;
   itc_group->func.content_get = NULL;
   itc_group->func.state_get = NULL;
   itc_group->func.del = NULL;

   action = shortcut_actions_get();
   while (action->action)
     {
        Elm_Object_Item *gli;
        if (action->cb)
          {
             gli = elm_genlist_item_append(gl, itc,
                                           (void *)action/* item data */,
                                           git /* parent */,
                                           ELM_GENLIST_ITEM_NONE,
                                           NULL/* func */,
                                           NULL/* func data */);
          }
        else
          {
             git = gli = elm_genlist_item_append(gl, itc_group,
                                                 (void *)action/* item data */,
                                                 NULL/* parent */,
                                                 ELM_GENLIST_ITEM_NONE,
                                                 NULL/* func */,
                                                 NULL/* func data */);
          }
        elm_genlist_item_select_mode_set(gli, ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
        action++;
     }

   /* TODO: reset button ? */
}
