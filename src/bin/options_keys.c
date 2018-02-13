#include "private.h"

#include <Elementary.h>
#include <assert.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_keys.h"
#include "keyin.h"
#include "utils.h"

typedef struct _Keys_Ctx {
     Config *config;
     Evas_Object *frame;
     Evas_Object *gl;
     Evas_Object *layout;
} Keys_Ctx;

static void _hover_del(Keys_Ctx *ctx);

static void
_shortcut_delete(void *data,
                 Evas_Object *_obj EINA_UNUSED,
                 void *_event_info EINA_UNUSED)
{
   Evas_Object *hs, *bx;
   Config_Keys *cfg_key;
   Evas_Coord w, min_w, min_h;
   Keys_Ctx *ctx;

   hs = data;
   bx = evas_object_data_get(hs, "bx");
   assert(bx);
   cfg_key = evas_object_data_get(hs, "cfg");
   assert(cfg_key);
   ctx = evas_object_data_get(hs, "ctx");
   assert(ctx);

   ctx->config->keys = eina_list_remove(ctx->config->keys, cfg_key);
   evas_object_size_hint_min_get(hs, &w, NULL);
   evas_object_size_hint_min_get(bx, &min_w, &min_h);
   min_w -= w;
   evas_object_size_hint_min_set(bx, min_w, min_h);

   evas_object_del(hs);
   keyin_remove_config(cfg_key);

   eina_stringshare_del(cfg_key->keyname);
   eina_stringshare_del(cfg_key->cb);
   free(cfg_key);

   config_save(ctx->config, NULL);
}

static Evas_Object *
_shortcut_button_add(Keys_Ctx *ctx,
                     Evas_Object *bx,
                     const Config_Keys *key)
{
   const char *txt;
   Evas_Object *hs;

   txt = eina_stringshare_printf("%s%s%s%s%s%s%s",
                                 key->ctrl ? _("Ctrl+") : "",
                                 key->alt ? _("Alt+") : "",
                                 key->shift ? _("Shift+") : "",
                                 key->win ? _("Win+") : "",
                                 key->meta ? _("Meta+") : "",
                                 key->hyper ? _("Hyper+") : "",
                                 key->keyname);
   hs = elm_hoversel_add(ctx->frame);
   elm_hoversel_hover_parent_set(hs, ctx->frame);

   evas_object_data_set(hs, "bx", bx);
   evas_object_data_set(hs, "cfg", key);
   evas_object_data_set(hs, "ctx", ctx);

   elm_layout_text_set(hs, NULL, txt);

   elm_hoversel_item_add(hs, _("Delete"), "delete", ELM_ICON_STANDARD,
                         _shortcut_delete, hs);

   eina_stringshare_del(txt);
   return hs;
}

static void
_cb_key_up(void *data,
           Evas *_e EINA_UNUSED,
           Evas_Object *obj EINA_UNUSED,
           void *event)
{
   Evas_Event_Key_Up *ev = event;
   int ctrl, alt, shift, win, meta, hyper, res;
   Config_Keys *cfg_key;
   Shortcut_Action *action;
   Evas_Object *bx = data;
   Keys_Ctx *ctx;

   if (key_is_modifier(ev->keyname))
     return;

   ctx = evas_object_data_get(bx, "ctx");
   assert(ctx);

   ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   alt = evas_key_modifier_is_set(ev->modifiers, "Alt");
   shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   win = evas_key_modifier_is_set(ev->modifiers, "Super");
   meta = evas_key_modifier_is_set(ev->modifiers, "Meta") ||
          evas_key_modifier_is_set(ev->modifiers, "AltGr") ||
          evas_key_modifier_is_set(ev->modifiers, "ISO_Level3_Shift");
   hyper = evas_key_modifier_is_set(ev->modifiers, "Hyper");

   _hover_del(ctx);

   action = evas_object_data_get(bx, "action");
   if (!action)
     {
        ERR("can't find any action to associate with.");
        return;
     }

   cfg_key = calloc(1, sizeof(Config_Keys));
   if (!cfg_key)
     return;
   cfg_key->keyname = eina_stringshare_add(ev->keyname);
   cfg_key->ctrl = ctrl;
   cfg_key->alt = alt;
   cfg_key->shift = shift;
   cfg_key->win = win;
   cfg_key->meta = meta;
   cfg_key->hyper = hyper;
   cfg_key->cb = eina_stringshare_add(action->action);

   res = keyin_add_config(cfg_key);
   if (res == 0)
     {
        Evas_Object *bt, *last;
        Evas_Coord w, h, min_w, min_h;

        last = evas_object_data_get(bx, "last");
        ctx->config->keys = eina_list_append(ctx->config->keys, cfg_key);
        bt = _shortcut_button_add(ctx, bx, cfg_key);
        evas_object_show(bt);
        evas_object_size_hint_min_get(bt, &w, &h);
        evas_object_size_hint_min_get(bx, &min_w, &min_h);
        min_w += w;
        if (h > min_h) min_h = h;
        evas_object_size_hint_min_set(bx, min_w, min_h);
        elm_box_pack_before(bx, bt, last);

        config_save(ctx->config, NULL);
     }
   else
     {
        eina_stringshare_del(cfg_key->keyname);
        eina_stringshare_del(cfg_key->cb);
        free(cfg_key);
     }
}

static void
_cb_mouse_down(void *data,
               Evas *_e EINA_UNUSED,
               Evas_Object *obj EINA_UNUSED,
               void *_event EINA_UNUSED)
{
   Keys_Ctx *ctx = data;

   _hover_del(ctx);
}


static void
_hover_sizing_eval(Keys_Ctx *ctx)
{
   Evas_Coord x = 0, y = 0, w = 0, h = 0;

   evas_object_geometry_get(ctx->frame, &x, &y, &w, &h);
   evas_object_geometry_set(ctx->layout, x, y, w, h);
}

static void
_parent_move_cb(void *data,
                Evas *_e EINA_UNUSED,
                Evas_Object *_obj EINA_UNUSED,
                void *_event_info EINA_UNUSED)
{
   Keys_Ctx *ctx = data;
   _hover_sizing_eval(ctx);
}

static void
_parent_resize_cb(void *data,
                  Evas *_e EINA_UNUSED,
                  Evas_Object *_obj EINA_UNUSED,
                  void *_event_info EINA_UNUSED)
{
   Keys_Ctx *ctx = data;
   _hover_sizing_eval(ctx);
}

static void
_parent_hide_cb(void *data,
                Evas *_e EINA_UNUSED,
                Evas_Object *_obj EINA_UNUSED,
                void *_event_info EINA_UNUSED)
{
   Keys_Ctx *ctx = data;
   _hover_del(ctx);
}

static void
_parent_del_cb(void *data,
               Evas *_e EINA_UNUSED,
               Evas_Object *_obj EINA_UNUSED,
               void *_event_info EINA_UNUSED)
{
   Keys_Ctx *ctx = data;
   _hover_del(ctx);

   evas_object_event_callback_del(ctx->frame, EVAS_CALLBACK_DEL,
                                  _parent_del_cb);
   evas_object_event_callback_del(ctx->frame, EVAS_CALLBACK_HIDE,
                                  _parent_hide_cb);
   free(ctx);
}

static void
_hover_del(Keys_Ctx *ctx)
{
   if (ctx->layout)
     {
        evas_object_event_callback_del(ctx->frame, EVAS_CALLBACK_KEY_UP,
                                       _cb_key_up);
        evas_object_event_callback_del(ctx->frame, EVAS_CALLBACK_MOUSE_DOWN,
                                       _cb_mouse_down);
        evas_object_event_callback_del(ctx->frame, EVAS_CALLBACK_MOVE,
                                       _parent_move_cb);
        evas_object_event_callback_del(ctx->frame, EVAS_CALLBACK_RESIZE,
                                       _parent_resize_cb);
        evas_object_del(ctx->layout);
     }
   ctx->layout = NULL;
}

static void
_on_shortcut_add(void *data,
                 Evas_Object *bt,
                 void *_event_info EINA_UNUSED)
{
   Evas_Object *o, *oe;
   Evas_Object *bx = data;
   Keys_Ctx *ctx;

   ctx = evas_object_data_get(bx, "ctx");
   assert(ctx);

   assert(ctx->layout == NULL);
   ctx->layout = o = elm_layout_add(bt);
   evas_object_data_set(ctx->layout, "ctx", ctx);
   oe = elm_layout_edje_get(o);
   theme_apply(oe, ctx->config, "terminology/keybinding");
   theme_auto_reload_enable(oe);
   elm_layout_text_set(o, "label", _("Please press key sequence"));
   evas_object_show(o);

   evas_object_event_callback_add(ctx->frame, EVAS_CALLBACK_KEY_UP,
                                  _cb_key_up, bx);
   evas_object_event_callback_add(ctx->frame, EVAS_CALLBACK_MOUSE_DOWN,
                                  _cb_mouse_down, ctx);
   elm_object_focus_set(ctx->frame, EINA_TRUE);
   elm_object_focus_allow_set(ctx->frame, EINA_TRUE);

   _hover_sizing_eval(ctx);

}

static Evas_Object *
gl_content_get(void *data, Evas_Object *obj, const char *_part EINA_UNUSED)
{
   Evas_Coord min_w = 0, w = 0, min_h = 0, h = 0;
   const Shortcut_Action *action = data;
   Evas_Object *bx, *bt, *lbl, *sep;
   Config_Keys *key;
   Eina_List *l;
   Keys_Ctx *ctx;

   ctx = evas_object_data_get(obj, "ctx");
   assert(ctx);

   bx = elm_box_add(obj);
   elm_box_horizontal_set(bx, EINA_TRUE);
   elm_box_homogeneous_set(bx, EINA_FALSE);
   evas_object_size_hint_align_set(bx, 0, 0);
   evas_object_data_set(bx, "ctx", ctx);

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
   EINA_LIST_FOREACH(ctx->config->keys, l, key)
     {
        if (!strcmp(key->cb, action->action))
          {
             bt = _shortcut_button_add(ctx, bx, key);
             evas_object_show(bt);
             evas_object_size_hint_min_get(bt, &w, &h);
             min_w += w;
             if (h > min_h) min_h = h;
             elm_box_pack_end(bx, bt);
          }
     }

   bt = elm_button_add(obj);
   evas_object_smart_callback_add(bt, "clicked", _on_shortcut_add, bx);
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



char *gl_group_text_get(void *data,
                        Evas_Object *_obj EINA_UNUSED,
                        const char *_part EINA_UNUSED)
{
   Shortcut_Action *action = data;
   return strdup(action->description);
}

static void
_cb_reset_keys(void *data,
               Evas_Object *_obj EINA_UNUSED,
               void *_event EINA_UNUSED)
{
   Keys_Ctx *ctx = data;

   config_reset_keys(ctx->config);
   elm_genlist_realized_items_update(ctx->gl);
}

void
options_keys(Evas_Object *opbox, Evas_Object *term)
{
   Evas_Object *o, *gl, *bx;
   const Shortcut_Action *action;
   Elm_Genlist_Item_Class *itc, *itc_group;
   Elm_Object_Item *git = NULL;
   Config *config = termio_config_get(term);
   Keys_Ctx *ctx;

   ctx = calloc(1, sizeof(*ctx));
   assert(ctx);

   ctx->config = config;

   ctx->frame = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Key Bindings"));
   elm_box_pack_end(opbox, o);
   evas_object_show(o);

   evas_object_event_callback_add(ctx->frame, EVAS_CALLBACK_DEL,
                                  _parent_del_cb, ctx);
   evas_object_event_callback_add(ctx->frame, EVAS_CALLBACK_HIDE,
                                  _parent_hide_cb, ctx);

   bx = elm_box_add(opbox);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(bx, 0.0, 0.0);
   elm_object_content_set(ctx->frame, bx);
   evas_object_show(bx);

   ctx->gl = gl = elm_genlist_add(opbox);
   evas_object_size_hint_weight_set(gl, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(gl, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(gl, ELM_SCROLLER_POLICY_AUTO, ELM_SCROLLER_POLICY_AUTO);
   elm_genlist_mode_set(gl, ELM_LIST_SCROLL);
   elm_genlist_homogeneous_set(gl, EINA_FALSE);
   elm_box_pack_end(bx, gl);
   evas_object_show(gl);
   evas_object_data_set(gl, "ctx", ctx);

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
                                                 ELM_GENLIST_ITEM_GROUP,
                                                 NULL/* func */,
                                                 NULL/* func data */);
          }
        elm_genlist_item_select_mode_set(gli, ELM_OBJECT_SELECT_MODE_DEFAULT);
        action++;
     }

   o = elm_button_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Reset bindings"));
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "clicked", _cb_reset_keys, ctx);
}
