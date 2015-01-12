#include "private.h"
#include <Elementary.h>
#include "dbus.h"
#ifdef HAVE_ELDBUS
#include <Eldbus.h>

static Eldbus_Connection *ty_dbus_conn = NULL;
static Eldbus_Object *ty_e_object = NULL;
static Eina_Stringshare *_current_url = NULL;

void
ty_dbus_link_hide(void)
{
   Eldbus_Message *msg;

   if ((!ty_e_object) || (!_current_url)) return;

   msg = eldbus_message_method_call_new("org.enlightenment.wm.service",
                                        "/org/enlightenment/wm/RemoteObject",
                                        "org.enlightenment.wm.Teamwork",
                                        "LinkHide");

   eldbus_message_arguments_append(msg, "s", _current_url);
   eldbus_object_send(ty_e_object, msg, NULL, NULL, -1);

   eina_stringshare_replace(&_current_url, NULL);
}

void
ty_dbus_link_mouseout(uint64_t win, const char *url, int x, int y)
{
   Eldbus_Message *msg;

   if (!ty_e_object) return;

   msg = eldbus_message_method_call_new("org.enlightenment.wm.service",
                                        "/org/enlightenment/wm/RemoteObject",
                                        "org.enlightenment.wm.Teamwork",
                                        "LinkMouseOut");
#if (ELM_VERSION_MAJOR > 1) || (ELM_VERSION_MINOR > 8) // not a typo
   eldbus_message_arguments_append(msg, "sutii", url, time(NULL), win, x, y);
#else
   eldbus_message_arguments_append(msg, "suxii", url, time(NULL), (int64_t)win, x, y);
#endif
   eldbus_object_send(ty_e_object, msg, NULL, NULL, -1);
   eina_stringshare_replace(&_current_url, NULL);
}


void
ty_dbus_link_mousein(uint64_t win, const char *url, int x, int y)
{
   Eldbus_Message *msg;
   Eina_Stringshare *u;

   if (!ty_e_object) return;

   u = eina_stringshare_add(url);
   /* if previous link exists, do MouseOut now */
   if (_current_url && (u != _current_url))
     ty_dbus_link_mouseout(win, _current_url, x, y);
   eina_stringshare_del(_current_url);
   _current_url = u;

   msg = eldbus_message_method_call_new("org.enlightenment.wm.service",
                                        "/org/enlightenment/wm/RemoteObject",
                                        "org.enlightenment.wm.Teamwork",
                                        "LinkMouseIn");

#if (ELM_VERSION_MAJOR > 1) || (ELM_VERSION_MINOR > 8) // not a typo
   eldbus_message_arguments_append(msg, "sutii", _current_url, time(NULL), win, x, y);
#else
   eldbus_message_arguments_append(msg, "suxii", _current_url, time(NULL), (int64_t)win, x, y);
#endif
   eldbus_object_send(ty_e_object, msg, NULL, NULL, -1);
}

void
ty_dbus_init(void)
{
   if (ty_dbus_conn) return;

   eldbus_init();

   if (!elm_need_sys_notify())
     {
        WRN("no elementary system notification support");
     }

   ty_dbus_conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SESSION);
   ty_e_object = eldbus_object_get(ty_dbus_conn,
                                   "org.enlightenment.wm.service",
                                   "/org/enlightenment/wm/RemoteObject");
}

void
ty_dbus_shutdown(void)
{
   ty_dbus_link_hide();
   if (ty_dbus_conn) eldbus_connection_unref(ty_dbus_conn);
   ty_dbus_conn = NULL;
   ty_e_object = NULL;
   eldbus_shutdown();
}

#else

void ty_dbus_link_hide(void) {}
void ty_dbus_link_mousein (uint64_t win EINA_UNUSED, const char *url EINA_UNUSED, int x EINA_UNUSED, int y EINA_UNUSED) {}
void ty_dbus_link_mouseout(uint64_t win EINA_UNUSED, const char *url EINA_UNUSED, int x EINA_UNUSED, int y EINA_UNUSED) {}
void ty_dbus_init(void) {}
void ty_dbus_shutdown(void) {}

#endif
