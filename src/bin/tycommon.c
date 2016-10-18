#include "private.h"
#include <stdlib.h>
#include <Eina.h>
#include "tycommon.h"


Eina_Bool
is_running_in_terminology(void)
{
   if (!getenv("TERMINOLOGY"))
     return EINA_FALSE;

   // Terminology's escape codes do not got through tmux
   if (getenv("TMUX"))
     return EINA_FALSE;

   // Terminology's escape codes do not got through screen
   if (getenv("STY"))
     return EINA_FALSE;

   return EINA_TRUE;
}
