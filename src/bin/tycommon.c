#include "private.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include "tycommon.h"

/* returns 0 if running in terminology, something else otherwise */
int
expect_running_in_terminology(void)
{
   char buf[32];
   ssize_t len = 0;
   const char *expected = "\033P!|7E7E5459\033\\";
   const char *pos = expected;
   fd_set readset;
   struct timeval tv;
   struct termios ttystate, ttysave;
   int res = -1;

   /* get current terminal state */
   if (tcgetattr(STDIN_FILENO, &ttystate))
     {
        perror("tcgetattr");
        return -1;
     }
   ttysave = ttystate;

   /* turn off canonical (buffered) mode and echo */
   ttystate.c_lflag &= ~(ICANON | ECHO);
   /* minimum of number input read: 1 byte. */
   ttystate.c_cc[VMIN] = 1;
   ttystate.c_cc[VTIME] = 0;

   /* set new terminal attributes */
   if (tcsetattr(STDIN_FILENO, TCSANOW, &ttystate))
     {
        perror("tcsetattr");
        return -1;
     }

   /* Query device attributes 3 */
   buf[0] = '\033';
   buf[1] = '[';
   buf[2] = '=';
   buf[3] = 'c';
   len = ty_write(STDOUT_FILENO, buf, 4);
   if (len != 4)
     {
        perror("write");
        res = -1;
        goto end;
     }

   while (pos != (expected + strlen(expected)))
     {
        /* wait up to 1 second */
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO(&readset);
        FD_SET(STDIN_FILENO, &readset);

        errno = 0;
        if (select(1, &readset, NULL, NULL, &tv) < 0)
          {
             perror("select");
             res = -1;
             goto end;
          }

        if (FD_ISSET(STDIN_FILENO, &readset))
          {
             len = read(STDIN_FILENO, buf, 1);
             if (len != 1)
               {
                  perror("read");
                  res = -1;
                  goto end;
               }

             if (buf[0] == *pos)
               pos++;
             else if (pos == expected)
               continue;
             else
               {
                  res = -1;
                  goto end;
               }
          }
        else
          {
             res = -1;
             goto end;
          }
     }
   res = 0;

end:

   /* restore attributes */
   if (tcsetattr(STDIN_FILENO, TCSANOW, &ttysave))
     {
        perror("tcsetattr");
        return -1;
     }

   return res;
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
