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
   printf("Usage: %s"HELP_ARGUMENT_SHORT" FILE1 [FILE2 ...]\n"
          "  Pop up a given media file/uri right now\n"
          HELP_ARGUMENT_DOC"\n"
          "\n",
          argv0);
}

int
main(int argc, char **argv)
{
   int i;

   ON_NOT_RUNNING_IN_TERMINOLOGY_EXIT_1();
   ARGUMENT_ENTRY_CHECK(argc, argv, print_usage);

   if (argc <= 1)
     {
        print_usage(argv[0]);
        return 0;
     }

   for (i = 1; i < argc; i++)
     {
        char *path, buf[PATH_MAX * 2], tbuf[PATH_MAX * 3];

        path = argv[i];
        if (realpath(path, buf)) path = buf;
        snprintf(tbuf, sizeof(tbuf), "%c}pn%s", 0x1b, path);
        if (write(0, tbuf, strlen(tbuf) + 1) != (signed)(strlen(tbuf) + 1)) perror("write");
     }
   return 0;
}
