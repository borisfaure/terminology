#ifndef _IPC_H__
#define _IPC_H__ 1

#include "config.h"

typedef struct _Ipc_Instance Ipc_Instance;

struct _Ipc_Instance
{
   char *cmd;
   char *cd;
   char *background;
   char *name;
   char *theme;
   char *role;
   char *title;
   char *icon_name;
   char *font;
   char *startup_id;
   char *startup_split;
   int x, y, w, h;
   Eina_Bool pos;
   Eina_Bool login_shell;
   Eina_Bool fullscreen;
   Eina_Bool iconic;
   Eina_Bool borderless;
   Eina_Bool override;
   Eina_Bool maximized;
   Eina_Bool hold;
   Eina_Bool nowm;
   Eina_Bool xterm_256color;
   Eina_Bool video_mute;
   Eina_Bool active_links;
   Eina_Bool cursor_blink;
   Eina_Bool visual_bell;
   Config *config;
};

void ipc_init(void);
void ipc_shutdown(void);
Eina_Bool ipc_serve(void);
void ipc_instance_new_func_set(void (*func) (Ipc_Instance *inst));
Eina_Bool ipc_instance_add(Ipc_Instance *inst);
void ipc_instance_conn_free(void);

#endif
