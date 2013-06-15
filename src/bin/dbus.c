#include "private.h"
#include <Elementary.h>
#include "dbus.h"
#ifdef HAVE_ELDBUS
#include <Eldbus.h>

static Eldbus_Connection *ty_dbus_conn = NULL;
static Eldbus_Object *ty_e_object = NULL;
static Eina_Stringshare *_current_url = NULL;

void
_cleanup_current_url(void)
{
   Eldbus_Message *msg;

   if ((!ty_e_object) || (!_current_url)) return;

   msg = eldbus_message_method_call_new("org.enlightenment.wm.service",
                                        "/org/enlightenment/wm/RemoteObject",
                                        "org.enlightenment.wm.Teamwork",
                                        "LinkMouseOut");

   eldbus_message_arguments_append(msg, "suii",
                                   _current_url, time(NULL), 0, 0);
   eldbus_object_send(ty_e_object, msg, NULL, NULL, -1);

   eina_stringshare_del(_current_url);
   _current_url = NULL;
}

void
ty_dbus_link_mouseout(const char *url, int x, int y)
{
   Eldbus_Message *msg;

   if (!ty_e_object) return;

   if ((!url) ||
       ((_current_url) && (!strcmp(url, _current_url))))
     {
        _cleanup_current_url();
        return;
     }

   _cleanup_current_url();

   msg = eldbus_message_method_call_new("org.enlightenment.wm.service",
                                        "/org/enlightenment/wm/RemoteObject",
                                        "org.enlightenment.wm.Teamwork",
                                        "LinkMouseOut");

   eldbus_message_arguments_append(msg, "suii", url, time(NULL), x, y);
   eldbus_object_send(ty_e_object, msg, NULL, NULL, -1);
}


void
ty_dbus_link_mousein(const char *url, int x, int y)
{
   Eldbus_Message *msg;

   if (!ty_e_object) return;

   if ((_current_url) && (!strcmp(url, _current_url))) return;

   msg = eldbus_message_method_call_new("org.enlightenment.wm.service",
                                        "/org/enlightenment/wm/RemoteObject",
                                        "org.enlightenment.wm.Teamwork",
                                        "LinkMouseIn");

   _cleanup_current_url();

   _current_url = eina_stringshare_add(url);

   eldbus_message_arguments_append(msg, "suii",
                                   _current_url, time(NULL), x, y);
   eldbus_object_send(ty_e_object, msg, NULL, NULL, -1);
}

void
ty_dbus_init(void)
{
   if (ty_dbus_conn) return;

   eldbus_init();

   ty_dbus_conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SESSION);
   ty_e_object = eldbus_object_get(ty_dbus_conn,
                                   "org.enlightenment.wm.service",
                                   "/org/enlightenment/wm/RemoteObject");
}

void
ty_dbus_shutdown(void)
{
   _cleanup_current_url();
   if (ty_dbus_conn) eldbus_connection_unref(ty_dbus_conn);
   ty_dbus_conn = NULL;
   ty_e_object = NULL;
   eldbus_shutdown();
}

#else

void ty_dbus_link_detect(const char *url __UNUSED__) {}
void ty_dbus_init(void) {}
void ty_dbus_shutdown(void) {}

#endif
