#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <Eina.h>
#include "tycommon.h"

static void
print_usage(const char *argv0)
{
   printf("Usage: %s "HELP_ARGUMENT_SHORT" [-p] [FILE1 FILE2 ...]\n"
          "  Change the terminal background to the given file/uri\n"
          "  -p  Make change permanent (stored in config)\n"
          HELP_ARGUMENT_DOC"\n"
          "\n",
          argv0);
}

int
main(int argc, char **argv)
{
   int i, perm = 0;

   ON_NOT_RUNNING_IN_TERMINOLOGY_EXIT_1();
   ARGUMENT_ENTRY_CHECK(argc, argv, print_usage);

   if (argc <= 1)
     {
        char tbuf[32];
        snprintf(tbuf, sizeof(tbuf), "%c}bt", 0x1b);
        if (write(0, tbuf, strlen(tbuf) + 1) != (signed)(strlen(tbuf) + 1)) perror("write");
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
        if (write(0, tbuf, strlen(tbuf) + 1) != (signed)(strlen(tbuf) + 1)) perror("write");
     }
   return 0;
}
