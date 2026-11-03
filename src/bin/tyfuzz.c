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

#if defined(__AFL_HAVE_MANUAL_CONTROL)
__AFL_FUZZ_INIT();
#endif

int
main(int argc, char **argv)
{
   eina_init();
   ecore_init();

   _log_domain = eina_log_domain_register("tyfuzz", NULL);

   tytest_common_init();

#if defined(__AFL_HAVE_MANUAL_CONTROL)
   /* The fork server installs after the eina and target setup; every input
    * then only pays for the parse, and the target is reset to a fresh state
    * between iterations. Necessarily single-threaded state (the feedback
    * map) comes from the AFL runtime. */
   __AFL_INIT();

   {
      unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

      while (__AFL_LOOP(10000))
        {
           int len = __AFL_FUZZ_TESTCASE_LEN;

           if (len <= 0) continue;
           tytest_common_reset();
           tytest_common_feed_chunked((const char *)buf, len, 4096);
        }
   }
#else
   if (argc > 1)
     {
       tytest_common_set_fd(open(argv[1], O_RDONLY));
     }

   tytest_common_main_loop();

   tytest_common_shutdown();
#endif

   ecore_shutdown();
   eina_shutdown();

   return 0;
}
