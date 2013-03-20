#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int
main(int argc, char **argv)
{
   int i;
   
   if (!getenv("TERMINOLOGY")) return 0;
   if (argc <= 1)
     {
        printf("Usage: %s FILE1 [FILE2 ...]\n"
               "  Pop up a given media file/uri right now\n"
               "\n",
               argv[0]);
        return 0;
     }
   for (i = 1; i < argc; i++)
     {
        char *path, buf[PATH_MAX * 2], tbuf[PATH_MAX * 3];

        path = argv[i];
        if (realpath(path, buf)) path = buf;
        snprintf(tbuf, sizeof(tbuf), "%c}pn%s", 0x1b, path);
        if (write(0, tbuf, strlen(tbuf)) < 1) perror("write");
        tbuf[0] = 0;
        if (write(0, tbuf, 1) < 1) perror("write");
     }
   exit(0);
   return 0;
}
