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

/* Feed a file as one test case. afl-showmap and afl-cmin with @@ pass the
 * input this way, and so do saved crash and hang files; the fork server's
 * shared memory test case does not exist in those runs, so it has to be
 * read from the file regardless of the AFL build. Returns 0 on success. */
static int
_feed_file(const char *path)
{
   unsigned char *data;
   struct stat st;
   ssize_t len;
   int fd = open(path, O_RDONLY);

   if ((fd < 0) || (fstat(fd, &st) != 0) || (st.st_size < 0))
     return 1;

   data = malloc(st.st_size);
   assert(data);
   len = 0;
   while (len < st.st_size)
     {
        ssize_t res = read(fd, data + len, st.st_size - len);

        if (res <= 0) break;
        len += res;
     }
   close(fd);

   tytest_common_feed_input(data, len);
   free(data);
   return 0;
}

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
#endif

   if (argc > 1)
     {
        int res = _feed_file(argv[1]);

        tytest_common_shutdown();
        ecore_shutdown();
        eina_shutdown();
        return res;
     }

#if defined(__AFL_HAVE_MANUAL_CONTROL)

   {
      unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

      while (__AFL_LOOP(10000))
        {
           int len = __AFL_FUZZ_TESTCASE_LEN;

           if (len <= 0) continue;
           tytest_common_reset();
           tytest_common_feed_input(buf, len);
        }
   }
#else
   tytest_common_set_fd(STDIN_FILENO);
   tytest_common_main_loop();

   tytest_common_shutdown();
#endif

   ecore_shutdown();
   eina_shutdown();

   return 0;
}
