#include "private.h"
#include <stdlib.h>
#include "tycommon.h"


int
is_running_in_terminology(void)
{
   if (!getenv("TERMINOLOGY")) return 0;
   // Terminology's escape codes do not got through tmux
   if (getenv("TMUX")) return 0;
   // Terminology's escape codes do not got through screen
   if (getenv("STY")) return 0;

   return 1;
}
