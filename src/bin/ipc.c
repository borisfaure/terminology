#include "private.h"

#include <Ecore.h>
#include <Ecore_Ipc.h>
#include <Eet.h>
#include "ipc.h"

static Ecore_Ipc_Server *ipc = NULL;
static Ecore_Event_Handler *hnd_data = NULL;
static void (*func_new_inst) (Ipc_Instance *inst) = NULL;
static Eet_Data_Descriptor *new_inst_edd = NULL;

static Eina_Bool
_ipc_cb_client_data(void *_data EINA_UNUSED,
                    int _type EINA_UNUSED,
                    void *event)
{
   Ecore_Ipc_Event_Client_Data *e = event;
   
   if (ecore_ipc_client_server_get(e->client) != ipc)
     return ECORE_CALLBACK_PASS_ON;
   if ((e->major == 3) && (e->minor == 7) && (e->data) && (e->size > 0))
     {
        Ipc_Instance *inst;
        
        inst = eet_data_descriptor_decode(new_inst_edd, e->data, e->size);
        if (inst)
          {
             if (func_new_inst) func_new_inst(inst);
             // NOTE strings in inst are part of the inst alloc blob and
             // dont need separate frees.
             free(inst);
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static char *
_ipc_hash_get(void)
{
   char buf[1024], hash[64];
   const char *disp, *session, *xdg_session, *xdg_id, *xdg_seat, *xdg_vt;
   char *s;
   unsigned int i;
   unsigned char c1, c2;
   
   /* dumb stoopid hash - i'm feeling lazy */
   disp = getenv("DISPLAY");
   if (!disp) disp = "-unknown-";
   session = getenv("DBUS_SESSION_BUS_ADDRESS");
   if (!session) session = ":unknown:";
   xdg_session = getenv("XDG_SESSION_COOKIE");
   if (!xdg_session) xdg_session = "/unknown/";
   xdg_id = getenv("XDG_SESSION_ID");
   if (!xdg_id) xdg_id = "=unknown=";
   xdg_seat = getenv("XDG_SEAT");
   if (!xdg_seat) xdg_seat = "@unknown@";
   xdg_vt = getenv("XDG_VTNR");
   if (!xdg_vt) xdg_vt = "!unknown!";
   snprintf(buf, sizeof(buf), "%s.%s.%s.%s.%s.%s", 
            disp, session, xdg_session, 
            xdg_id, xdg_seat, xdg_vt);
   memset(hash, 0, sizeof(hash));
   memset(hash, 'x', 12 + 32);
   memcpy(hash, "terminology-", 12);
   for (i = 0, s = buf; *s; s++)
     {
        c1 = (((unsigned char)*s) >> 4) & 0xf;
        c2 = ((unsigned char)*s) & 0x0f;
        hash[12 + (i % 32)] = (((hash[12 + (i % 32)] - 'a') ^ c1) % 26) + 'a';
        i++;
        hash[12 + (i % 32)] = (((hash[12 + (i % 32)] - 'a') ^ c2) % 26) + 'a';
        i++;
     }
   return strdup(hash);
}

void
ipc_init(void)
{
   Eet_Data_Descriptor_Class eddc;
   
   ecore_ipc_init();
   eet_init();
   eet_eina_stream_data_descriptor_class_set(&eddc, sizeof(eddc),
                                             "inst", sizeof(Ipc_Instance));
   new_inst_edd = eet_data_descriptor_stream_new(&eddc);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "cmd", cmd, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "cd", cd, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "background", background, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "name", name, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "role", role, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "title", title, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "icon_name", icon_name, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "font", font, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "startup_id", startup_id, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "x", x, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "y", y, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "w", w, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "h", h, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "pos", pos, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "login_shell", login_shell, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "fullscreen", fullscreen, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "iconic", iconic, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "borderless", borderless, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "override", override, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "maximized", maximized, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "hold", hold, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(new_inst_edd, Ipc_Instance,
                                 "nowm", nowm, EET_T_INT);
}

Eina_Bool
ipc_serve(void)
{
   char *hash = _ipc_hash_get();
   if (!hash) return EINA_FALSE;
   ipc = ecore_ipc_server_add(ECORE_IPC_LOCAL_USER, hash, 0, NULL);
   free(hash);
   if (!ipc) return EINA_FALSE;
   hnd_data = ecore_event_handler_add
     (ECORE_IPC_EVENT_CLIENT_DATA, _ipc_cb_client_data, NULL);
   return EINA_TRUE;
}

void
ipc_shutdown(void)
{
   if (ipc)
     {
        ecore_ipc_server_del(ipc);
        ipc = NULL;
     }
   if (new_inst_edd)
     {
        eet_data_descriptor_free(new_inst_edd);
        new_inst_edd = NULL;
     }
   eet_shutdown();
   ecore_ipc_shutdown();
   if (hnd_data)
     {
        ecore_event_handler_del(hnd_data);
        hnd_data = NULL;
     }
}

void
ipc_instance_new_func_set(void (*func) (Ipc_Instance *inst))
{
   func_new_inst = func;
}

Eina_Bool
ipc_instance_add(Ipc_Instance *inst)
{
   int size = 0;
   void *data;
   char *hash = _ipc_hash_get();
   Ecore_Ipc_Server *ipcsrv;
   
   if (!hash) return EINA_FALSE;
   data = eet_data_descriptor_encode(new_inst_edd, inst, &size);
   if (!data)
     {
        free(hash);
        return EINA_FALSE;
     }
   ipcsrv = ecore_ipc_server_connect(ECORE_IPC_LOCAL_USER, hash, 0, NULL);
   if (ipcsrv)
     {
        ecore_ipc_server_send(ipcsrv, 3, 7, 0, 0, 0, data, size);
        ecore_ipc_server_flush(ipcsrv);
        free(data);
        free(hash);
        ecore_ipc_server_del(ipcsrv);
        return EINA_TRUE;
     }
   free(data);
   free(hash);
   return EINA_FALSE;
}
