#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "private.h"
#include <Elementary.h>
#include "config.h"
#include "termpty.h"
#include "termptyops.h"
#include "termiointernals.h"
#include "tytest_common.h"

int _log_domain = -1;

int
main(int argc, char **argv)
{
   eina_init();

   _log_domain = eina_log_domain_register("tyfuzz", NULL);

   tytest_common_init();

   if (argc > 1)
     {
       tytest_common_set_fd(open(argv[1], O_RDONLY));
     }

   tytest_common_main_loop();

   tytest_common_shutdown();
   eina_shutdown();

   return 0;
}
