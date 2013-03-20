#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int
main(int argc, char **argv)
{
   int i, perm = 0;
   
   if (!getenv("TERMINOLOGY")) return 0;
   if (argc <= 1)
     {
        printf("Usage: %s [-p] FILE1 [FILE2 ...]\n"
               "  Change the terminal background to the given file/uri\n"
               "  -p  Make change permanent (stored in config)\n"
               "\n",
               argv[0]);
        return 0;
     }
   for (i = 1; i < argc; i++)
     {
        char *path, buf[PATH_MAX * 2], tbuf[PATH_MAX * 3];

        if (!strcmp(argv[i], "-p"))
          {
             perm = 1;
             i++;
             if (i >= argc) break;
          }
        path = argv[i];
        if (realpath(path, buf)) path = buf;
        if (perm)
          snprintf(tbuf, sizeof(tbuf), "%c}bp%s", 0x1b, path);
        else
          snprintf(tbuf, sizeof(tbuf), "%c}bt%s", 0x1b, path);
        if (write(0, tbuf, strlen(tbuf)) < 1) perror("write");
        tbuf[0] = 0;
        if (write(0, tbuf, 1) < 1) perror("write");
     }
   exit(0);
   return 0;
}
