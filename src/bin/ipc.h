#ifndef _IPC_H__
#define _IPC_H__ 1

#include "config.h"

typedef struct _Ipc_Instance Ipc_Instance;

struct _Ipc_Instance
{
   const char *cmd;
   const char *cd;
   const char *background;
   const char *name;
   const char *role;
   const char *title;
   const char *icon_name;
   const char *font;
   const char *startup_id;
   const char *startup_split;
   int x, y, w, h;
   int pos;
   int login_shell;
   int fullscreen;
   int iconic;
   int borderless;
   int override;
   int maximized;
   int hold;
   int nowm;
   int xterm_256color;
   int active_links;
};

void ipc_init(void);
void ipc_shutdown(void);
Eina_Bool ipc_serve(void);
void ipc_instance_new_func_set(void (*func) (Ipc_Instance *inst));
Eina_Bool ipc_instance_add(Ipc_Instance *inst);

#endif
