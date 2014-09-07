#include "private.h"

#include <Elementary.h>
#include "config.h"
#include "termio.h"
#include "options.h"
#include "options_behavior.h"
#include "main.h"

static Evas_Object *op_w, *op_h;

static void
_cb_op_behavior_drag_links_chg(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->drag_links = elm_check_state_get(obj);
   config_save(config, NULL);
}

static void
_cb_op_behavior_jump_keypress_chg(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->jump_on_keypress = elm_check_state_get(obj);
   termio_config_update(term);
   config_save(config, NULL);
}

static void
_cb_op_behavior_jump_chg(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->jump_on_change = elm_check_state_get(obj);
   termio_config_update(term);
   config_save(config, NULL);
}

static void
_cb_op_behavior_cursor_blink_chg(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->disable_cursor_blink = !elm_check_state_get(obj);
   termio_config_update(term);
   config_save(config, NULL);
}

static void
_cb_op_behavior_visual_bell_chg(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->disable_visual_bell = !elm_check_state_get(obj);
   config_save(config, NULL);
}

static void
_cb_op_behavior_bell_rings_chg(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->bell_rings = elm_check_state_get(obj);
   config_save(config, NULL);
}

static void
_cb_op_behavior_flicker_chg(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->flicker_on_key = elm_check_state_get(obj);
   termio_config_update(term);
   config_save(config, NULL);
}

static void
_cb_op_behavior_urg_bell_chg(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->urg_bell = elm_check_state_get(obj);
   config_save(config, NULL);
}

static void
_cb_op_behavior_active_links_chg(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->active_links = elm_check_state_get(obj);
   config_save(config, NULL);
}

static void
_cb_op_behavior_multi_instance_chg(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->multi_instance = elm_check_state_get(obj);
   config_save(config, NULL);
}

static void
_cb_op_behavior_application_server_restore_views_chg(void *data, Evas_Object *obj,
                                                     void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->application_server_restore_views = elm_check_state_get(obj);
   config_save(config, NULL);
}

static void
_behavior_option_restore_opened_views_add(Evas_Object *term,
                                                      Evas_Object *check)
{
   Evas_Object *bx = evas_object_data_get(check, "box");
   Evas_Object *o;
   Config *config = termio_config_get(term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Restore opened views"));
   elm_check_state_set(o, config->application_server_restore_views);
   elm_box_pack_after(bx, o, check);
   evas_object_show(o);
   evas_object_data_set(check, "restore_views", o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_application_server_restore_views_chg,
                                  term);
}


static void
_behavior_option_restore_opened_views_del(Evas_Object *check)
{
   Evas_Object *o = evas_object_data_del(check, "restore_views");
   if (o)
     evas_object_del(o);
}

static void
_cb_op_behavior_application_server_chg(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   Eina_Bool old = config->application_server;
   config->application_server = elm_check_state_get(obj);

   if (old == config->application_server)
     return;

   if (!config->application_server)
     _behavior_option_restore_opened_views_del(obj);
   else
     _behavior_option_restore_opened_views_add(term, obj);
   config_save(config, NULL);
}

static void
_cb_op_behavior_xterm_256color_chg(void *data, Evas_Object *obj,
                                   void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->xterm_256color = elm_check_state_get(obj);
   config_save(config, NULL);
}

static void
_cb_op_behavior_erase_is_del_chg(void *data, Evas_Object *obj,
                                 void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->erase_is_del = elm_check_state_get(obj);
   config_save(config, NULL);
}

static unsigned int
sback_double_to_expo_int(double d)
{
    if (d < 1.0)
        return 0;
    if (d >= 19.0)
        d = 19.0;
    return 1 << (unsigned char) d;
}

static char *
sback_indicator_units_format(double d)
{
    return (char*)eina_stringshare_printf("%'d", sback_double_to_expo_int(d));
}
static char *
sback_units_format(double d)
{
    return (char*)eina_stringshare_printf(_("%'d lines"), sback_double_to_expo_int(d));
}

static void
_cb_op_behavior_sback_chg(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);

   config->scrollback = (double) sback_double_to_expo_int(elm_slider_value_get(obj));
   termio_config_update(term);
   config_save(config, NULL);
}

static void
_cb_op_behavior_tab_zoom_slider_chg(void *data, Evas_Object *obj,
                                    void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);

   config->tab_zoom = (double)(int)round(elm_slider_value_get(obj) * 10.0) / 10.0;
   termio_config_update(term);
   config_save(config, NULL);
}

static void
_cb_op_behavior_custom_geometry(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);

   config->custom_geometry = elm_check_state_get(obj);
   if (config->custom_geometry)
     {
        config->cg_width = (int) elm_spinner_value_get(op_w);
        config->cg_height = (int) elm_spinner_value_get(op_h);
     }
   config_save(config, NULL);

   elm_object_disabled_set(op_w, !config->custom_geometry);
   elm_object_disabled_set(op_h, !config->custom_geometry);
}

static void
_cb_op_behavior_cg_width(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);

   if (config->custom_geometry)
     {
        config->cg_width = (int) elm_spinner_value_get(obj);
        config_save(config, NULL);
     }
}

static void
_cb_op_behavior_cg_height(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);

   if (config->custom_geometry)
     {
        config->cg_height = (int) elm_spinner_value_get(obj);
        config_save(config, NULL);
     }
}

static void
_cb_op_behavior_login_shell_chg(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->login_shell = elm_check_state_get(obj);
   config_save(config, NULL);
}

static void
_cb_op_behavior_mouse_over_focus_chg(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *term = data;
   Config *config = termio_config_get(term);
   config->mouse_over_focus = elm_check_state_get(obj);
   config_save(config, NULL);
}

void
options_behavior(Evas_Object *opbox, Evas_Object *term)
{
   Config *config = termio_config_get(term);
   Evas_Object *o, *bx, *sc, *fr;
   int w, h;
   const char *tooltip;

   termio_size_get(term, &w, &h);

   fr = o = elm_frame_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(o, _("Behavior"));
   elm_box_pack_end(opbox, o);
   evas_object_show(o);

   sc = o = elm_scroller_add(opbox);
   elm_scroller_content_min_limit(sc, EINA_TRUE, EINA_FALSE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(fr, o);
   evas_object_show(o);

   bx = o = elm_box_add(opbox);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_object_content_set(sc, o);
   evas_object_show(o);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Scroll to bottom on new content"));
   elm_check_state_set(o, config->jump_on_change);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_jump_chg, term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Scroll to bottom when a key is pressed"));
   elm_check_state_set(o, config->jump_on_keypress);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_jump_keypress_chg, term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("React to key presses"));
   elm_check_state_set(o, config->flicker_on_key);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_flicker_chg, term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Cursor blinking"));
   elm_check_state_set(o, !config->disable_cursor_blink);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_cursor_blink_chg, term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Visual Bell"));
   elm_check_state_set(o, !config->disable_visual_bell);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_visual_bell_chg, term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Bell rings"));
   elm_check_state_set(o, config->bell_rings);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_bell_rings_chg, term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Urgent Bell"));
   elm_check_state_set(o, config->urg_bell);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_urg_bell_chg, term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Active Links"));
   elm_check_state_set(o, config->active_links);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_active_links_chg, term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Enable application server"));
   elm_check_state_set(o, config->application_server);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_application_server_chg, term);

   evas_object_data_set(o, "box", bx);
   if (config->application_server)
     _behavior_option_restore_opened_views_add(term, o);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Multiple instances, one process"));
   elm_check_state_set(o, config->multi_instance);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_multi_instance_chg, term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Set TERM to xterm-256color"));
   elm_check_state_set(o, config->xterm_256color);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_xterm_256color_chg, term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("BackArrow sends Del (instead of BackSpace)"));
   elm_check_state_set(o, config->erase_is_del);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_erase_is_del_chg, term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Drag & drop links"));
   elm_check_state_set(o, config->drag_links);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_drag_links_chg, term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Start as login shell"));
   elm_check_state_set(o, config->login_shell);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_login_shell_chg, term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Focus split under the Mouse"));
   elm_check_state_set(o, config->mouse_over_focus);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_mouse_over_focus_chg, term);

   o = elm_check_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(o, _("Always open at size:"));
   elm_check_state_set(o, config->custom_geometry);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_custom_geometry, term);

   o = elm_label_add(bx);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, _("Width:"));
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   op_w = o = elm_spinner_add(bx);
   elm_spinner_editable_set(o, EINA_TRUE);
   elm_spinner_min_max_set(o, 2.0, 350.0);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   if (config->custom_geometry) elm_spinner_value_set(o, config->cg_width);
   else elm_spinner_value_set(o, w);
   elm_object_disabled_set(o, !config->custom_geometry);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_cg_width, term);

   o = elm_label_add(bx);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, _("Height:"));
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   op_h = o = elm_spinner_add(bx);
   elm_spinner_editable_set(o, EINA_TRUE);
   elm_spinner_min_max_set(o, 1.0, 150.0);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   if (config->custom_geometry) elm_spinner_value_set(o, config->cg_height);
   else elm_spinner_value_set(o, h);
   elm_object_disabled_set(o, !config->custom_geometry);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed",
                                  _cb_op_behavior_cg_height, term);

   o = elm_separator_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   elm_separator_horizontal_set(o, EINA_TRUE);
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_label_add(bx);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, _("Scrollback:"));
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_slider_add(bx);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_slider_span_size_set(o, 40);
#if (EINA_VERSION_MAJOR > 1) || (EINA_VERSION_MINOR >= 8)
   elm_slider_step_set(o, 1);
#endif
   elm_slider_units_format_function_set(o,
                                        sback_units_format,
                                        (void(*)(char*))eina_stringshare_del);
   elm_slider_indicator_format_function_set(o,
                                            sback_indicator_units_format,
                                            (void(*)(char*))eina_stringshare_del);
   /* http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogFloat */
   union {
       float v;
       int c;
   } u;
   u.v = config->scrollback;
   u.c = (u.c >> 23) - 127;
   elm_slider_min_max_set(o, 0.0, 19.0);
   elm_slider_value_set(o, u.c);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "delay,changed",
                                  _cb_op_behavior_sback_chg, term);

   o = elm_label_add(bx);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, _("Tab zoom/switch animation time:"));
   tooltip = _("Set the time of the animation that<br>"
       "takes places on tab switches,<br>"
       "be them by key binding, mouse<br>"
       "wheel or tabs panel mouse move");
   elm_object_tooltip_text_set(o, tooltip);
   elm_box_pack_end(bx, o);
   evas_object_show(o);

   o = elm_slider_add(bx);
   elm_object_tooltip_text_set(o, tooltip);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_slider_span_size_set(o, 40);
   elm_slider_unit_format_set(o, _("%1.1f s"));
   elm_slider_indicator_format_set(o, _("%1.1f s"));
   elm_slider_min_max_set(o, 0.0, 1.0);
   elm_slider_value_set(o, config->tab_zoom);
   elm_box_pack_end(bx, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "delay,changed",
                                  _cb_op_behavior_tab_zoom_slider_chg, term);

   evas_object_size_hint_weight_set(opbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(opbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(o);
}
