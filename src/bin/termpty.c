#include "private.h"
#include <Elementary.h>
#include "termpty.h"
#include "termptyesc.h"
#include "termptyops.h"
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

/* specific log domain to help debug only terminal code parser */
int _termpty_log_dom = -1;

#undef CRITICAL
#undef ERR
#undef WRN
#undef INF
#undef DBG

#define CRITICAL(...) EINA_LOG_DOM_CRIT(_termpty_log_dom, __VA_ARGS__)
#define ERR(...)      EINA_LOG_DOM_ERR(_termpty_log_dom, __VA_ARGS__)
#define WRN(...)      EINA_LOG_DOM_WARN(_termpty_log_dom, __VA_ARGS__)
#define INF(...)      EINA_LOG_DOM_INFO(_termpty_log_dom, __VA_ARGS__)
#define DBG(...)      EINA_LOG_DOM_DBG(_termpty_log_dom, __VA_ARGS__)

void
termpty_init(void)
{
   if (_termpty_log_dom >= 0) return;

   _termpty_log_dom = eina_log_domain_register("termpty", NULL);
   if (_termpty_log_dom < 0)
     EINA_LOG_CRIT("could not create log domain 'termpty'.");
}

void
termpty_shutdown(void)
{
   if (_termpty_log_dom < 0) return;
   eina_log_domain_unregister(_termpty_log_dom);
   _termpty_log_dom = -1;
}

static void
_handle_buf(Termpty *ty, const Eina_Unicode *codepoints, int len)
{
   Eina_Unicode *c, *ce, *b;
   int n, bytes;

   c = (int *)codepoints;
   ce = &(c[len]);

   if (ty->buf)
     {
        bytes = (ty->buflen + len + 1) * sizeof(int);
        b = realloc(ty->buf, bytes);
        if (!b)
          {
             ERR("memerr");
          }
        INF("realloc add %i + %i", (int)(ty->buflen * sizeof(int)), (int)(len * sizeof(int)));
        bytes = len * sizeof(Eina_Unicode);
        memcpy(&(b[ty->buflen]), codepoints, bytes);
        ty->buf = b;
        ty->buflen += len;
        ty->buf[ty->buflen] = 0;
        c = ty->buf;
        ce = c + ty->buflen;
        while (c < ce)
          {
             n = _termpty_handle_seq(ty, c, ce);
             if (n == 0)
               {
                  Eina_Unicode *tmp = ty->buf;
                  ty->buf = NULL;
                  ty->buflen = 0;
                  bytes = ((char *)ce - (char *)c) + sizeof(Eina_Unicode);
                  INF("malloc til %i", (int)(bytes - sizeof(Eina_Unicode)));
                  ty->buf = malloc(bytes);
                  if (!ty->buf)
                    {
                       ERR("memerr");
                    }
                  bytes = (char *)ce - (char *)c;
                  memcpy(ty->buf, c, bytes);
                  ty->buflen = bytes / sizeof(Eina_Unicode);
                  ty->buf[ty->buflen] = 0;
                  free(tmp);
                  break;
               }
             c += n;
          }
        if (c == ce)
          {
             if (ty->buf)
               {
                  free(ty->buf);
                  ty->buf = NULL;
               }
             ty->buflen = 0;
          }
     }
   else
     {
        while (c < ce)
          {
             n = _termpty_handle_seq(ty, c, ce);
             if (n == 0)
               {
                  bytes = ((char *)ce - (char *)c) + sizeof(Eina_Unicode);
                  ty->buf = malloc(bytes);
                  INF("malloc %i", (int)(bytes - sizeof(Eina_Unicode)));
                  if (!ty->buf)
                    {
                       ERR("memerr");
                    }
                  bytes = (char *)ce - (char *)c;
                  memcpy(ty->buf, c, bytes);
                  ty->buflen = bytes / sizeof(Eina_Unicode);
                  ty->buf[ty->buflen] = 0;
                  break;
               }
             c += n;
          }
     }
}

static void
_pty_size(Termpty *ty)
{
   struct winsize sz;

   sz.ws_col = ty->w;
   sz.ws_row = ty->h;
   sz.ws_xpixel = 0;
   sz.ws_ypixel = 0;
   if (ioctl(ty->fd, TIOCSWINSZ, &sz) < 0)
     ERR("Size set ioctl failed: %s", strerror(errno));
}

static Eina_Bool
_cb_exe_exit(void *data, int type __UNUSED__, void *event)
{
   Ecore_Exe_Event_Del *ev = event;
   Termpty *ty = data;

   if (ev->pid != ty->pid) return ECORE_CALLBACK_PASS_ON;
   ty->exit_code = ev->exit_code;
   if (ty->cb.exited.func) ty->cb.exited.func(ty->cb.exited.data);
   
   ty->pid = -1;

   if (ty->hand_exe_exit) ecore_event_handler_del(ty->hand_exe_exit);
   ty->hand_exe_exit = NULL;
   if (ty->hand_fd) ecore_main_fd_handler_del(ty->hand_fd);
   ty->hand_fd = NULL;
   if (ty->fd >= 0) close(ty->fd);
   ty->fd = -1;
   if (ty->slavefd >= 0) close(ty->slavefd);
   ty->slavefd = -1;

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_fd_read(void *data, Ecore_Fd_Handler *fd_handler __UNUSED__)
{
   Termpty *ty = data;
   char buf[4097];
   Eina_Unicode codepoint[4097];
   int len, i, j, reads;

   // read up to 64 * 4096 bytes
   for (reads = 0; reads < 64; reads++)
     {
        len = read(ty->fd, buf, sizeof(buf) - 1);
        if (len <= 0) break;
        
/*        
        printf(" I: ");
        int jj;
        for (jj = 0; jj < len && jj < 100; jj++)
          {
             if ((buf[jj] < ' ') || (buf[jj] >= 0x7f))
               printf("\033[33m%02x\033[0m", (unsigned char)buf[jj]);
             else
               printf("%c", buf[jj]);
          }
        printf("\n");
 */
        buf[len] = 0;
        // convert UTF8 to codepoint integers
        j = 0;
        for (i = 0; i < len;)
          {
             int g = 0;

             if (buf[i])
               {
                  i = evas_string_char_next_get(buf, i, &g);
                  if (i < 0) break;
//                  DBG("(%i) %02x '%c'", j, g, g);
               }
             else
               {
                  g = 0;
                  i++;
               }
             codepoint[j] = g;
             j++;
          }
        codepoint[j] = 0;
//        DBG("---------------- handle buf %i", j);
        _handle_buf(ty, codepoint, j);
     }
   if (ty->cb.change.func) ty->cb.change.func(ty->cb.change.data);
   return EINA_TRUE;
}

static void
_limit_coord(Termpty *ty, Termstate *state)
{
   state->wrapnext = 0;
   if (state->cx >= ty->w) state->cx = ty->w - 1;
   if (state->cy >= ty->h) state->cy = ty->h - 1;
   if (state->had_cr_x >= ty->w) state->had_cr_x = ty->w - 1;
   if (state->had_cr_y >= ty->h) state->had_cr_y = ty->h - 1;
}

Termpty *
termpty_new(const char *cmd, Eina_Bool login_shell, const char *cd, int w, int h, int backscroll)
{
   Termpty *ty;
   const char *pty;

   ty = calloc(1, sizeof(Termpty));
   if (!ty) return NULL;
   ty->w = w;
   ty->h = h;
   ty->backmax = backscroll;

   _termpty_reset_state(ty);
   ty->save = ty->state;
   ty->swap = ty->state;

   ty->screen = calloc(1, sizeof(Termcell) * ty->w * ty->h);
   if (!ty->screen) goto err;
   ty->screen2 = calloc(1, sizeof(Termcell) * ty->w * ty->h);
   if (!ty->screen2) goto err;

   ty->fd = posix_openpt(O_RDWR | O_NOCTTY);
   if (ty->fd < 0) goto err;
   if (grantpt(ty->fd) != 0) goto err;
   if (unlockpt(ty->fd) != 0) goto err;
   pty = ptsname(ty->fd);
   ty->slavefd = open(pty, O_RDWR | O_NOCTTY);
   if (ty->slavefd < 0) goto err;
   fcntl(ty->fd, F_SETFL, O_NDELAY);

   ty->hand_exe_exit = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
                                               _cb_exe_exit, ty);
   ty->pid = fork();
   if (!ty->pid)
     {
        const char *shell = NULL;
        const char *args[4] = {NULL, NULL, NULL, NULL};
        Eina_Bool needs_shell;
        int i;

        if (cd)
          {
             if (chdir(cd) != 0)
               {
                  perror("chdir");
                  ERR("Cannot change to directory '%s'", cd);
                  exit(127);
               }
          }
        
        needs_shell = ((!cmd) ||
                       (strpbrk(cmd, " |&;<>()$`\\\"'*?#") != NULL));
        DBG("cmd='%s' needs_shell=%u", cmd ? cmd : "", needs_shell);

        if (needs_shell)
          {
             shell = getenv("SHELL");
             if (!shell)
               {
                  uid_t uid = getuid();
                  struct passwd *pw = getpwuid(uid);
                  if (pw) shell = pw->pw_shell;
               }
             if (!shell)
               {
                  WRN("Could not find shell, fallback to /bin/sh");
                  shell = "/bin/sh";
               }
          }

        if (!needs_shell)
          args[0] = cmd;
        else
          {
             args[0] = shell;
             if (cmd)
               {
                  args[1] = "-c";
                  args[2] = cmd;
               }
          }

#define NC(x) (args[x] != NULL ? args[x] : "")
        DBG("exec %s %s %s %s", NC(0), NC(1), NC(2), NC(3));
#undef NC

        for (i = 0; i < 100; i++)
          {
             if (i != ty->slavefd) close(i);
          }
        ty->fd = ty->slavefd;
        setsid();

        dup2(ty->fd, 0);
        dup2(ty->fd, 1);
        dup2(ty->fd, 2);

        if (ioctl(ty->fd, TIOCSCTTY, NULL) < 0) exit(1);

        /* TODO: should we reset signals here? */

        // pretend to be xterm
        putenv("TERM=xterm");
//        putenv("TERM=xterm-256color");
        putenv("XTERM_256_COLORS=1");
        if (!login_shell)
          execvp(args[0], (char *const *)args);
        else
          {
             char *cmdfile, *cmd0;
             
             cmdfile = (char *)args[0];
             cmd0 = alloca(strlen(cmdfile) + 2);
             cmd0[0] = '-';
             strcpy(cmd0 + 1, cmdfile);
             args[0] = cmd0;
             execvp(cmdfile, (char *const *)args);
          }
        exit(127); /* same as system() for failed commands */
     }
   ty->hand_fd = ecore_main_fd_handler_add(ty->fd, ECORE_FD_READ,
                                           _cb_fd_read, ty,
                                           NULL, NULL);
   close(ty->slavefd);
   _pty_size(ty);
   return ty;
err:
   if (ty->screen) free(ty->screen);
   if (ty->screen2) free(ty->screen2);
   if (ty->fd >= 0) close(ty->fd);
   if (ty->slavefd >= 0) close(ty->slavefd);
   free(ty);
   return NULL;
}

void
termpty_free(Termpty *ty)
{
   if (ty->fd >= 0) close(ty->fd);
   if (ty->slavefd >= 0) close(ty->slavefd);
   if (ty->pid >= 0)
     {
        int i;
        
        // in case someone stopped the child - cont it
        kill(ty->pid, SIGCONT);
        // signpipe for shells
        kill(ty->pid, SIGPIPE);
        // try 400 time (sleeping for 1ms) to check for death of child
        for (i = 0; i < 400; i++)
          {
             int status = 0;

             // poll exit of child pid
             if (waitpid(ty->pid, &status, WNOHANG) == ty->pid)
               {
                  // if child exited - break loop and mark pid as done
                  ty->pid = -1;
                  break;
               }
             // after 100ms set sigint
             if      (i == 100) kill(ty->pid, SIGINT);
             // after 200ms send term signal
             else if (i == 200) kill(ty->pid, SIGTERM);
             // after 300ms send quit signal
             else if (i == 300) kill(ty->pid, SIGQUIT);
             usleep(1000); // sleep 1ms
          }
        // so 400ms and child not gone - KILL!
        if (ty->pid >= 0)
          {
             kill(ty->pid, SIGKILL);
             ty->pid = -1;
          }
     }
   if (ty->hand_exe_exit) ecore_event_handler_del(ty->hand_exe_exit);
   if (ty->hand_fd) ecore_main_fd_handler_del(ty->hand_fd);
   if (ty->prop.title) eina_stringshare_del(ty->prop.title);
   if (ty->prop.icon) eina_stringshare_del(ty->prop.icon);
   if (ty->back)
     {
        int i;

        for (i = 0; i < ty->backmax; i++)
          {
             if (ty->back[i]) free(ty->back[i]);
          }
        free(ty->back);
     }
   if (ty->screen) free(ty->screen);
   if (ty->screen2) free(ty->screen2);
   if (ty->buf) free(ty->buf);
   memset(ty, 0, sizeof(Termpty));
   free(ty);
}

Termcell *
termpty_cellrow_get(Termpty *ty, int y, int *wret)
{
   Termsave *ts;

   if (y >= 0)
     {
        if (y >= ty->h) return NULL;
        *wret = ty->w;
        return &(ty->screen[y * ty->w]);
     }
   if (y < -ty->backmax) return NULL;
   ts = ty->back[(ty->backmax + ty->backpos + y) % ty->backmax];
   if (!ts) return NULL;
   *wret = ts->w;
   return ts->cell;
}

void
termpty_write(Termpty *ty, const char *input, int len)
{
   if (ty->fd < 0) return;
   if (write(ty->fd, input, len) < 0) ERR("write %s", strerror(errno));
}

void
termpty_resize(Termpty *ty, int w, int h)
{
   Termcell *olds, *olds2;
   int y, ww, hh, oldw, oldh;

   if ((ty->w == w) && (ty->h == h)) return;

   olds = ty->screen;
   olds2 = ty->screen2;
   oldw = ty->w;
   oldh = ty->h;

   ty->w = w;
   ty->h = h;
   ty->state.had_cr = 0;
   _limit_coord(ty, &(ty->state));
   _limit_coord(ty, &(ty->swap));
   _limit_coord(ty, &(ty->save));

   ty->screen = calloc(1, sizeof(Termcell) * ty->w * ty->h);
   if (!ty->screen)
     {
        ty->screen2 = NULL;
        ERR("memerr");
     }
   ty->screen2 = calloc(1, sizeof(Termcell) * ty->w * ty->h);
   if (!ty->screen2)
     {
        ERR("memerr");
     }

   ww = ty->w;
   hh = ty->h;
   if (ww > oldw) ww = oldw;
   if (hh > oldh) hh = oldh;

   for (y = 0; y < hh; y++)
     {
        Termcell *c1, *c2;

        c1 = &(olds[y * oldw]);
        c2 = &(ty->screen[y * ty->w]);
        _termpty_text_copy(ty, c1, c2, ww);

        c1 = &(olds2[y * oldw]);
        c2 = &(ty->screen2[y * ty->w]);
        _termpty_text_copy(ty, c1, c2, ww);
     }

   free(olds);
   free(olds2);

   _pty_size(ty);
}

void
termpty_backscroll_set(Termpty *ty, int size)
{
   int i;

   if (ty->backmax == size) return;

   if (ty->back)
     {
        for (i = 0; i < ty->backmax; i++)
          {
             if (ty->back[i]) free(ty->back[i]);
          }
        free(ty->back);
     }
   if (size > 0)
     ty->back = calloc(1, sizeof(Termsave *) * size);
   else
     ty->back = NULL;
   ty->backscroll_num = 0;
   ty->backpos = 0;
   ty->backmax = size;
}
