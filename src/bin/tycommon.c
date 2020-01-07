#include "private.h"
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
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

ssize_t
ty_write(int fd, const void *buf, size_t count)
{
   const char *data = buf;
   ssize_t len = count;

   while (len > 0)
     {
        ssize_t res = write(fd, data, len);

        if (res < 0)
          {
             if (errno == EINTR || errno == EAGAIN)
               continue;
             return res;
          }
        data += res;
        len  -= res;
     }
   return count;
}
