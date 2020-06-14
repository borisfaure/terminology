#ifndef _TERMINOLOGY_OPTIONS_H__
#define _TERMINOLOGY_OPTIONS_H__ 1

void options_show(Evas_Object *win, Evas_Object *base, Evas_Object *bg,
                  Evas_Object *term,
                  void (*donecb) (void *data), void *donedata);


/* helpers to generate callbacks and checkbox to change config parameters */
#define OPTIONS_CB(_ctx, _cfg_name, _inv)                       \
static void                                                     \
_cb_op_##_cfg_name(void *data, Evas_Object *obj,                \
                   void *_event EINA_UNUSED)                    \
{                                                               \
   _ctx *ctx = data;                                            \
   Config *config = ctx->config;                                \
   if (_inv)                                                    \
     config->_cfg_name = !elm_check_state_get(obj);             \
   else                                                         \
     config->_cfg_name = elm_check_state_get(obj);              \
   termio_config_update(ctx->term);                             \
   windows_update();                                            \
   config_save(config);                                         \
}

#define OPTIONS_CX(_lbl, _cfg_name, _inv)                                 \
   do {                                                                   \
   o = elm_check_add(bx);                                                 \
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);            \
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);               \
   elm_object_text_set(o, _lbl);                                          \
   elm_check_state_set(o, _inv ? !config->_cfg_name : config->_cfg_name); \
   elm_box_pack_end(bx, o);                                               \
   evas_object_show(o);                                                   \
   evas_object_smart_callback_add(o, "changed",                           \
                                  _cb_op_##_cfg_name, ctx);               \
   } while (0)

#define OPTIONS_SEPARATOR                                      \
   do {                                                        \
   o = elm_separator_add(bx);                                  \
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0); \
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);    \
   elm_separator_horizontal_set(o, EINA_TRUE);                 \
   elm_box_pack_end(bx, o);                                    \
   evas_object_show(o);                                        \
   } while (0)

#endif
