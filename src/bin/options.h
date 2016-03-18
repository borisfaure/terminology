#ifndef _TERMINOLOGY_OPTIONS_H__
#define _TERMINOLOGY_OPTIONS_H__ 1

void options_toggle(Evas_Object *win, Evas_Object *bg, Evas_Object *term,
                    void (*donecb) (void *data), void *donedata);
Eina_Bool options_active_get(void);

#define OPTIONS_CB(_cfg_name, _inv)                             \
static void                                                     \
_cb_op_behavior_##_cfg_name(void *data, Evas_Object *obj,       \
                            void *event EINA_UNUSED)            \
{                                                               \
   Evas_Object *term = data;                                    \
   Config *config = termio_config_get(term);                    \
   if (_inv)                                                    \
     config->_cfg_name = !elm_check_state_get(obj);             \
   else                                                         \
     config->_cfg_name = elm_check_state_get(obj);              \
   termio_config_update(term);                                  \
   windows_update();                                            \
   config_save(config, NULL);                                   \
}

#define OPTIONS_CX(_bx, _lbl, _cfg_name, _inv)                            \
   do {                                                                   \
   o = elm_check_add(_bx);                                                \
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);            \
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);               \
   elm_object_text_set(o, _lbl);                                          \
   elm_check_state_set(o, _inv ? !config->_cfg_name : config->_cfg_name); \
   elm_box_pack_end(_bx, o);                                              \
   evas_object_show(o);                                                   \
   evas_object_smart_callback_add(o, "changed",                           \
                                  _cb_op_behavior_##_cfg_name, term)      \
   } while (0)


#endif
