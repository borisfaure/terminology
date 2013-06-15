#include "private.h"
#include <Elementary.h>
#include "dbus.h"
#ifdef HAVE_ELDBUS
#include <Eldbus.h>

static Eldbus_Connection *ty_dbus_conn = NULL;
static Eldbus_Object *ty_e_object = NULL;

void
ty_dbus_link_detect(const char *url)
{
   Eldbus_Message *msg;

   msg = eldbus_message_method_call_new("org.enlightenment.wm.service",
                                        "/org/enlightenment/wm/RemoteObject",
                                        "org.enlightenment.wm.Teamwork",
                                        "LinkDetect");

   eldbus_message_arguments_append(msg, "s", url);
   eldbus_object_send(ty_e_object, msg, NULL, NULL, -1);
}

void
ty_dbus_init(void)
{
   Eldbus_Service_Interface *iface;

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
