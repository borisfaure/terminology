#include "private.h"
#include <Elementary.h>
#include "termpty.h"
#include "termptyesc.h"
#include "termptyops.h"
#include "termptysave.h"
#include "termio.h"
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#if defined (__sun) || defined (__sun__)
# include <stropts.h>
#endif
#include <assert.h>

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
     EINA_LOG_CRIT("Could not create logging domain '%s'.", "termpty");
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

   c = (Eina_Unicode *)codepoints;
   ce = &(c[len]);

   if (ty->buf)
     {
        bytes = (ty->buflen + len + 1) * sizeof(int);
        b = realloc(ty->buf, bytes);
        if (!b)
          {
             ERR(_("memerr: %s"), strerror(errno));
             return;
          }
        DBG("realloc add %i + %i", (int)(ty->buflen * sizeof(int)), (int)(len * sizeof(int)));
        bytes = len * sizeof(Eina_Unicode);
        memcpy(&(b[ty->buflen]), codepoints, bytes);
        ty->buf = b;
        ty->buflen += len;
        ty->buf[ty->buflen] = 0;
        c = ty->buf;
        ce = c + ty->buflen;
        while (c < ce)
          {
             n = termpty_handle_seq(ty, c, ce);
             if (n == 0)
               {
                  Eina_Unicode *tmp = ty->buf;
                  ty->buf = NULL;
                  ty->buflen = 0;
                  bytes = ((char *)ce - (char *)c) + sizeof(Eina_Unicode);
                  DBG("malloc till %i", (int)(bytes - sizeof(Eina_Unicode)));
                  ty->buf = malloc(bytes);
                  if (!ty->buf)
                    {
                       ERR(_("memerr: %s"), strerror(errno));
                       return;
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
             n = termpty_handle_seq(ty, c, ce);
             if (n == 0)
               {
                  bytes = ((char *)ce - (char *)c) + sizeof(Eina_Unicode);
                  ty->buf = malloc(bytes);
                  DBG("malloc %i", (int)(bytes - sizeof(Eina_Unicode)));
                  if (!ty->buf)
                    {
                       ERR(_("memerr: %s"), strerror(errno));
                    }
                  else
                    {
                       bytes = (char *)ce - (char *)c;
                       memcpy(ty->buf, c, bytes);
                       ty->buflen = bytes / sizeof(Eina_Unicode);
                       ty->buf[ty->buflen] = 0;
                    }
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
     ERR(_("Size set ioctl failed: %s"), strerror(errno));
}

static Eina_Bool
_cb_exe_exit(void *data, int type EINA_UNUSED, void *event)
{
   Ecore_Exe_Event_Del *ev = event;
   Termpty *ty = data;

   if (ev->pid != ty->pid) return ECORE_CALLBACK_PASS_ON;
   ty->exit_code = ev->exit_code;
   
   ty->pid = -1;

   if (ty->hand_exe_exit) ecore_event_handler_del(ty->hand_exe_exit);
   ty->hand_exe_exit = NULL;
   if (ty->hand_fd) ecore_main_fd_handler_del(ty->hand_fd);
   ty->hand_fd = NULL;
   if (ty->fd >= 0) close(ty->fd);
   ty->fd = -1;
   if (ty->slavefd >= 0) close(ty->slavefd);
   ty->slavefd = -1;

   if (ty->cb.exited.func) ty->cb.exited.func(ty->cb.exited.data);
   
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_fd_read(void *data, Ecore_Fd_Handler *fd_handler)
{
   Termpty *ty = data;
   char buf[4097];
   Eina_Unicode codepoint[4097];
   int len, i, j, k, reads;

   if (ecore_main_fd_handler_active_get(fd_handler, ECORE_FD_ERROR))
     {
        ERR("error while reading from tty slave fd");
        return ECORE_CALLBACK_CANCEL;
     }

   // read up to 64 * 4096 bytes
   for (reads = 0; reads < 64; reads++)
     {
        char *rbuf = buf;
        len = sizeof(buf) - 1;

        for (i = 0; i < (int)sizeof(ty->oldbuf) && ty->oldbuf[i] & 0x80; i++)
          {
             *rbuf = ty->oldbuf[i];
             rbuf++;
             len--;
          }
        len = read(ty->fd, rbuf, len);
        if (len < 0 && errno != EAGAIN)
          {
             ERR("error while reading from tty slave fd");
             break;
          }
        if (len <= 0) break;


        for (i = 0; i < (int)sizeof(ty->oldbuf); i++)
          ty->oldbuf[i] = 0;

        len += rbuf - buf;

        /*
        printf(" I: ");
        int jj;
        for (jj = 0; jj < len; jj++)
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
             int g = 0, prev_i = i;

             if (buf[i])
               {
#if (EINA_VERSION_MAJOR > 1) || (EINA_VERSION_MINOR >= 8)
                  g = eina_unicode_utf8_next_get(buf, &i);
                  if ((0xdc80 <= g) && (g <= 0xdcff) &&
                      (len - prev_i) <= (int)sizeof(ty->oldbuf))
#else
                  i = evas_string_char_next_get(buf, i, &g);
                  if (i < 0 &&
                      (len - prev_i) <= (int)sizeof(ty->oldbuf))
#endif
                    {
                       for (k = 0;
                            (k < (int)sizeof(ty->oldbuf)) && 
                            (k < (len - prev_i));
                            k++)
                         {
                            ty->oldbuf[k] = buf[prev_i+k];
                         }
                       DBG("failure at %d/%d/%d", prev_i, i, len);
                       break;
                    }
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
   if (len <= 0)
     {
        if (42)
          {
             ty->exit_code = 0;
             ty->pid = -1;

             if (ty->hand_exe_exit) ecore_event_handler_del(ty->hand_exe_exit);
             ty->hand_exe_exit = NULL;
             if (ty->hand_fd) ecore_main_fd_handler_del(ty->hand_fd);
             ty->hand_fd = NULL;
             ty->fd = -1;
             ty->slavefd = -1;
             if (ty->cb.exited.func)
               ty->cb.exited.func(ty->cb.exited.data);
          }
        return ECORE_CALLBACK_CANCEL;
     }

   return EINA_TRUE;
}

static void
_limit_coord(Termpty *ty)
{
   TERMPTY_RESTRICT_FIELD(ty->termstate.had_cr_x, 0, ty->w);
   TERMPTY_RESTRICT_FIELD(ty->termstate.had_cr_y, 0, ty->h);

   TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
   TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);

   TERMPTY_RESTRICT_FIELD(ty->cursor_save.cx, 0, ty->w);
   TERMPTY_RESTRICT_FIELD(ty->cursor_save.cy, 0, ty->h);
}

Termpty *
termpty_new(const char *cmd, Eina_Bool login_shell, const char *cd,
            int w, int h, int backscroll, Eina_Bool xterm_256color,
            Eina_Bool erase_is_del, const char *emotion_mod)
{
   Termpty *ty;
   const char *pty;
   int mode;
   struct termios t;
   Eina_Bool needs_shell;
   const char *shell = NULL;
   const char *args[4] = {NULL, NULL, NULL, NULL};
   const char *arg0;

   ty = calloc(1, sizeof(Termpty));
   if (!ty) return NULL;
   ty->w = w;
   ty->h = h;
   ty->backsize = backscroll;

   termpty_reset_state(ty);

   ty->screen = calloc(1, sizeof(Termcell) * ty->w * ty->h);
   if (!ty->screen)
     {
        ERR("Allocation of term %s %ix%i failed: %s",
            "screen", ty->w, ty->h, strerror(errno));
        goto err;
     }
   ty->screen2 = calloc(1, sizeof(Termcell) * ty->w * ty->h);
   if (!ty->screen2)
     {
        ERR("Allocation of term %s %ix%i failed: %s",
            "screen2", ty->w, ty->h, strerror(errno));
        goto err;
     }

   ty->circular_offset = 0;

   /* TODO: boris */
   if (42)
     {
        ty->fd = STDIN_FILENO;
        ty->hand_fd = ecore_main_fd_handler_add(ty->fd,
                                                ECORE_FD_READ | ECORE_FD_ERROR,
                                                _cb_fd_read, ty,
                                                NULL, NULL);
        _pty_size(ty);
        termpty_save_register(ty);
        return ty;
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
             WRN(_("Could not find shell, falling back to %s"), "/bin/sh");
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
   arg0 = strrchr(args[0], '/');
   if (!arg0) arg0 = args[0];
   else arg0++;
   ty->prop.title = eina_stringshare_add(arg0);

   ty->fd = posix_openpt(O_RDWR | O_NOCTTY);
   if (ty->fd < 0)
     {
        ERR(_("Function %s failed: %s"), "posix_openpt()", strerror(errno));
        goto err;
     }
   if (grantpt(ty->fd) != 0)
     {
        WRN(_("Function %s failed: %s"), "grantpt()",  strerror(errno));
     }
   if (unlockpt(ty->fd) != 0)
     {
        ERR(_("Function %s failed: %s"), "unlockpt()", strerror(errno));
        goto err;
     }
   pty = ptsname(ty->fd);
   ty->slavefd = open(pty, O_RDWR | O_NOCTTY);
   if (ty->slavefd < 0)
     {
        ERR(_("open() of pty '%s' failed: %s"), pty, strerror(errno));
        goto err;
     }
   mode = fcntl(ty->fd, F_GETFL, 0);
   if (mode < 0)
     {
        ERR(_("fcntl() on pty '%s' failed: %s"), pty, strerror(errno));
        goto err;
     }
   if (!(mode & O_NDELAY))
     if (fcntl(ty->fd, F_SETFL, mode | O_NDELAY))
       {
          ERR(_("fcntl() on pty '%s' failed: %s"), pty, strerror(errno));
          goto err;
       }

#if defined (__sun) || defined (__sun__)
   if (ioctl(ty->slavefd, I_PUSH, "ptem") < 0
       || ioctl(ty->slavefd, I_PUSH, "ldterm") < 0
       || ioctl(ty->slavefd, I_PUSH, "ttcompat") < 0)
     {
        ERR(_("ioctl() on pty '%s' failed: %s"), pty, strerror(errno));
        goto err;
     }
# endif

   if (tcgetattr(ty->slavefd, &t) < 0)
     {
        ERR("unable to tcgetattr: %s", strerror(errno));
        goto err;
     }
   t.c_cc[VERASE] =  (erase_is_del) ? 0x7f : 0x8;
#ifdef IUTF8
   t.c_iflag |= IUTF8;
#endif
   if (tcsetattr(ty->slavefd, TCSANOW, &t) < 0)
     {
        ERR("unable to tcsetattr: %s", strerror(errno));
        goto err;
     }

   ty->hand_exe_exit = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
                                               _cb_exe_exit, ty);
   if (!ty->hand_exe_exit)
     {
        ERR("event handler add failed");
        goto err;
     }
   ty->pid = fork();
   if (ty->pid < 0)
     {
        ERR("unable to fork: %s", strerror(errno));
        goto err;
     }
   if (!ty->pid)
     {
        int i;
        char buf[256];

        if (cd)
          {
             if (chdir(cd) != 0)
               {
                  ERR(_("Could not change current directory to '%s': %s"),
                        cd, strerror(errno));
                  exit(127);
               }
          }


#define NC(x) (args[x] != NULL ? args[x] : "")
        DBG("exec %s %s %s %s", NC(0), NC(1), NC(2), NC(3));
#undef NC

        for (i = 0; i < 100; i++)
          {
             if (i != ty->slavefd) close(i);
          }
        setsid();

        dup2(ty->slavefd, 0);
        dup2(ty->slavefd, 1);
        dup2(ty->slavefd, 2);

        if (ioctl(ty->slavefd, TIOCSCTTY, NULL) < 0) exit(1);

        close(ty->fd);
        close(ty->slavefd);

        /* TODO: should we reset signals here? */

        /* pretend to be xterm */
        if (xterm_256color)
          {
             putenv("TERM=xterm-256color");
          }
        else
          {
             putenv("TERM=xterm");
          }
        putenv("XTERM_256_COLORS=1");
        if (emotion_mod)
          {
             snprintf(buf, sizeof(buf), "EMOTION_ENGINE=%s", emotion_mod);
             putenv(buf);
          }
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
   ty->slavefd = -1;
   _pty_size(ty);
   termpty_save_register(ty);
   return ty;
err:
   free(ty->screen);
   free(ty->screen2);
   if (ty->fd >= 0) close(ty->fd);
   if (ty->slavefd >= 0) close(ty->slavefd);
   free(ty);
   return NULL;
}

void
termpty_free(Termpty *ty)
{
   Termexp *ex;

   termpty_save_unregister(ty);
   EINA_LIST_FREE(ty->block.expecting, ex) free(ex);
   if (ty->block.blocks) eina_hash_free(ty->block.blocks);
   if (ty->block.chid_map) eina_hash_free(ty->block.chid_map);
   if (ty->block.active) eina_list_free(ty->block.active);
   if (ty->fd >= 0) close(ty->fd);
   if (ty->slavefd >= 0) close(ty->slavefd);
   if (ty->pid >= 0)
     {
        int i;

        // in case someone stopped the child - cont it
        kill(ty->pid, SIGCONT);
        // sighup the shell
        kill(ty->pid, SIGHUP);
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
        size_t i;

        for (i = 0; i < ty->backsize; i++)
          termpty_save_free(&ty->back[i]);
        free(ty->back);
     }
   free(ty->screen);
   free(ty->screen2);
   free(ty->buf);
   free(ty);
}

static Eina_Bool
_termpty_cell_is_empty(const Termcell *cell)
{
   if ((cell->codepoint != 0) &&
       (!cell->att.invisible) &&
       (cell->att.fg != COL_INVIS))
     {
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

static Eina_Bool
_termpty_line_is_empty(const Termcell *cells, ssize_t nb_cells)
{
   ssize_t len;

   for (len = nb_cells - 1; len >= 0; len--)
     {
        const Termcell *cell = cells + len;

        if (!_termpty_cell_is_empty(cell))
          return EINA_FALSE;
     }

   return EINA_TRUE;
}


ssize_t
termpty_line_length(const Termcell *cells, ssize_t nb_cells)
{
   ssize_t len = nb_cells;

   for (len = nb_cells - 1; len >= 0; len--)
     {
        const Termcell *cell = cells + len;

        if (!_termpty_cell_is_empty(cell))
          return len + 1;
     }

   return 0;
}

#define BACKLOG_ROW_GET(Ty, Y) \
   (&Ty->back[(Ty->backsize + ty->backpos - ((Y) - 1 )) % Ty->backsize])


#if 0
static void
verify_beacon(Termpty *ty)
{
   Termsave *ts;
   int nb_lines;
   int backlog_y = ty->backlog_beacon.backlog_y;
   int screen_y = ty->backlog_beacon.screen_y;

   assert(ty->backlog_beacon.screen_y >= 0);
   assert(ty->backlog_beacon.backlog_y >= 0);
   assert(ty->backlog_beacon.screen_y >= ty->backlog_beacon.backlog_y);

   //ERR("FROM screen_y:%d backlog_y:%d",
   //    screen_y, backlog_y);
   while (backlog_y > 0)
     {
        ts = BACKLOG_ROW_GET(ty, backlog_y);
        if (!ts->cells)
          {
             ERR("went too far: screen_y:%d backlog_y:%d",
                 screen_y, backlog_y);
             return;
          }

        nb_lines = (ts->w == 0) ? 1 : (ts->w + ty->w - 1) / ty->w;
        screen_y -= nb_lines;
        backlog_y--;
        //ERR("nb_lines:%d screen_y:%d backlog_y:%d ts->w:%d ty->w:%d",
        //    nb_lines, screen_y, backlog_y, ts->w, ty->w);
        assert(screen_y >= backlog_y);

     }
   //ERR("TO screen_y:%d backlog_y:%d",
   //    screen_y, backlog_y);
   assert (backlog_y == 0);
   assert (screen_y == 0);
}
#endif


void
termpty_text_save_top(Termpty *ty, Termcell *cells, ssize_t w_max)
{
   Termsave *ts;
   ssize_t w;

   if (ty->backsize <= 0)
     return;
   assert(ty->back);

   termpty_backlog_lock();

   w = termpty_line_length(cells, w_max);
   if (ty->backsize >= 1)
     {
        ts = BACKLOG_ROW_GET(ty, 1);
        if (!ts->cells)
          goto add_new_ts;
        /* TODO: RESIZE uncompress ? */
        if (ts->w && ts->cells[ts->w - 1].att.autowrapped)
          {
             int old_len = ts->w;
             termpty_save_expand(ts, cells, w);
             ty->backlog_beacon.screen_y += (ts->w + ty->w - 1) / ty->w
                                          - (old_len + ty->w - 1) / ty->w;
             return;
          }
     }

add_new_ts:
   ts = BACKLOG_ROW_GET(ty, 0);
   ts = termpty_save_new(ts, w);
   if (!ts)
     return;
   termpty_cell_copy(ty, cells, ts->cells, w);
   ty->backpos++;
   if (ty->backpos >= ty->backsize)
     ty->backpos = 0;
   termpty_backlog_unlock();

   ty->backlog_beacon.screen_y++;
   ty->backlog_beacon.backlog_y++;
   if (ty->backlog_beacon.backlog_y >= (int)ty->backsize)
     {
        ty->backlog_beacon.screen_y = 0;
        ty->backlog_beacon.backlog_y = 0;
     }
}


ssize_t
termpty_row_length(Termpty *ty, int y)
{
   ssize_t wret;
   Termcell *cells = termpty_cellrow_get(ty, y, &wret);

   if (y >= 0)
     return termpty_line_length(cells, ty->w);
   return cells ? wret : 0;
}

ssize_t
termpty_backlog_length(Termpty *ty)
{
   int backlog_y = ty->backlog_beacon.backlog_y;
   int screen_y = ty->backlog_beacon.screen_y;

   if (!ty->backsize)
     return 0;

   while (42)
     {
        int nb_lines;
        Termsave *ts;

        ts = BACKLOG_ROW_GET(ty, backlog_y);
        if (!ts->cells || backlog_y >= (int)ty->backsize)
          return ty->backlog_beacon.screen_y;

        nb_lines = (ts->w == 0) ? 1 : (ts->w + ty->w - 1) / ty->w;
        ty->backlog_beacon.screen_y = screen_y;
        ty->backlog_beacon.backlog_y = backlog_y;
        screen_y += nb_lines;
        backlog_y++;
     }
}

void
termpty_backscroll_adjust(Termpty *ty, int *scroll)
{
   int backlog_y = ty->backlog_beacon.backlog_y;
   int screen_y = ty->backlog_beacon.screen_y;

   if (!ty->backsize || *scroll <= 0)
     {
        *scroll = 0;
        return;
     }
   if (*scroll < screen_y)
     return;

   while (42)
     {
        int nb_lines;
        Termsave *ts;

        ts = BACKLOG_ROW_GET(ty, backlog_y);
        if (*scroll <= screen_y)
          {
             return;
          }
        if (!ts->cells || backlog_y >= (int)ty->backsize)
          {
             *scroll = ty->backlog_beacon.screen_y;
             return;
          }

        nb_lines = (ts->w == 0) ? 1 : (ts->w + ty->w - 1) / ty->w;
        ty->backlog_beacon.screen_y = screen_y;
        ty->backlog_beacon.backlog_y = backlog_y;
        screen_y += nb_lines;
        backlog_y++;
     }
}

static Termcell*
_termpty_cellrow_from_beacon_get(Termpty *ty, int requested_y, ssize_t *wret)
{
   int backlog_y = ty->backlog_beacon.backlog_y;
   int screen_y = ty->backlog_beacon.screen_y;
   Eina_Bool going_forward = EINA_TRUE;

   requested_y = -requested_y;

   /* check if going from 0,0 is faster than using the beacon */
   if (screen_y - requested_y > requested_y)
     {
        backlog_y = 1;
        screen_y = 1;
     }
   while (42) {
        Termsave *ts;
        int nb_lines;

        ts = BACKLOG_ROW_GET(ty, backlog_y);
        if (!ts->cells)
          return NULL;
        nb_lines = (ts->w == 0) ? 1 : (ts->w + ty->w - 1) / ty->w;
        if (!going_forward) {
             screen_y -= nb_lines;
        }

        if ((screen_y <= requested_y) && (requested_y < screen_y + nb_lines))
          {
             int delta = screen_y + nb_lines - 1 - requested_y;
             *wret = ts->w - delta * ty->w;
             if (*wret > ts->w)
               *wret = ts->w;
             ty->backlog_beacon.screen_y = screen_y;
             ty->backlog_beacon.backlog_y = backlog_y;
             return &ts->cells[delta * ty->w];
          }

        if (requested_y > screen_y)
          {
             screen_y += nb_lines;
             backlog_y++;
          }
        else
          {
             backlog_y--;
             going_forward = EINA_FALSE;
          }
   }

   return NULL;
}

Termcell *
termpty_cellrow_get(Termpty *ty, int y_requested, ssize_t *wret)
{
   if (y_requested >= 0)
     {
        if (y_requested >= ty->h)
          return NULL;
        *wret = ty->w;
        return &(TERMPTY_SCREEN(ty, 0, y_requested));
     }
   if (!ty->back)
     return NULL;

   return _termpty_cellrow_from_beacon_get(ty, y_requested, wret);

}

void
termpty_write(Termpty *ty, const char *input, int len)
{
   int fd = ty->fd;

   /* TODO: boris */
   fd = STDOUT_FILENO;
   if (fd < 0) return;
   if (write(fd, input, len) < 0)
     ERR(_("Could not write to file descriptor %d: %s"),
         fd, strerror(errno));
}

struct screen_info
{
   Termcell *screen;
   int w;
   int h;
   int x;
   int y;
   int cy;
   int cx;
   int circular_offset;
};

#define SCREEN_INFO_GET_CELLS(Tsi, X, Y) \
  Tsi->screen[X + (((Y + Tsi->circular_offset) % Tsi->h) * Tsi->w)]

static void
_check_screen_info(Termpty *ty, struct screen_info *si)
{
   if (si->y >= si->h)
     {
        Termcell *cells = &SCREEN_INFO_GET_CELLS(si, 0, 0);

        si->y--;
        termpty_text_save_top(ty, cells, si->w);
        termpty_cells_clear(ty, cells, si->w);

        si->circular_offset++;
        if (si->circular_offset >= si->h)
          si->circular_offset = 0;

        si->cy--;
     }
}

static void
_termpty_line_rewrap(Termpty *ty, Termcell *cells, int len,
                     struct screen_info *si,
                     Eina_Bool set_cursor)
{
   int autowrapped = cells[len-1].att.autowrapped;

   if (len == 0)
     {
        if (set_cursor)
          {
             si->cy = si->y;
             si->cx = 0;
          }
        si->y++;
        si->x = 0;
        _check_screen_info(ty, si);
        return;
     }
   while (len > 0)
     {
        int copy_width = MIN(len, si->w - si->x);

        termpty_cell_copy(ty,
                          /*src*/ cells,
                          /*dst*/&SCREEN_INFO_GET_CELLS(si, si->x, si->y),
                          copy_width);
        if (set_cursor)
          {
             if (ty->cursor_state.cx <= copy_width)
               {
                  si->cx = ty->cursor_state.cx;
                  si->cy = si->y;
               }
             else
               {
                  ty->cursor_state.cx -= copy_width;
               }
          }
        len -= copy_width;
        si->x += copy_width;
        cells += copy_width;
        if (si->x >= si->w)
          {
             si->y++;
             si->x = 0;
          }
        _check_screen_info(ty, si);
     }
   if (!autowrapped && si->x != 0)
     {
        si->y++;
        si->x = 0;
        _check_screen_info(ty, si);
     }
}

void
termpty_resize(Termpty *ty, int new_w, int new_h)
{
   Termcell *new_screen = NULL;
   int old_y = 0,
       old_w = ty->w,
       old_h = ty->h,
       effective_old_h;
   int altbuf = 0;
   struct screen_info new_si = {.screen = NULL};

   if ((ty->w == new_w) && (ty->h == new_h)) return;
   if ((new_w == new_h) && (new_w == 1)) return; // FIXME: something weird is
                                                 // going on at term init

   termpty_backlog_lock();

   if (ty->altbuf)
     {
        termpty_screen_swap(ty);
        altbuf = 1;
     }

   new_screen = calloc(1, sizeof(Termcell) * new_w * new_h);
   if (!new_screen)
     goto bad;
   free(ty->screen2);
   ty->screen2 = calloc(1, sizeof(Termcell) * new_w * new_h);
   if (!ty->screen2)
     goto bad;

   new_si.screen = new_screen;
   new_si.w = new_w;
   new_si.h = new_h;

   /* compute the effective height on the old screen */
   effective_old_h = 0;
   for (old_y = old_h -1; old_y >= 0; old_y--)
     {
        Termcell *cells = &(TERMPTY_SCREEN(ty, 0, old_y));
        if (!_termpty_line_is_empty(cells, old_w))
          {
             effective_old_h = old_y + 1;
             break;
          }
     }

   if (effective_old_h <= ty->cursor_state.cy)
     effective_old_h = ty->cursor_state.cy + 1;

   for (old_y = 0; old_y < effective_old_h; old_y++)
     {
        /* for each line in the old screen, append it to the new screen */
        Termcell *cells = &(TERMPTY_SCREEN(ty, 0, old_y));
        int len;

        len = termpty_line_length(cells, old_w);
        _termpty_line_rewrap(ty, cells, len, &new_si,
                             old_y == ty->cursor_state.cy);
     }

   free(ty->screen);
   ty->screen = new_screen;

   ty->cursor_state.cy = (new_si.cy >= 0) ? new_si.cy : 0;
   ty->cursor_state.cx = (new_si.cx >= 0) ? new_si.cx : 0;
   ty->circular_offset = new_si.circular_offset;

   ty->w = new_w;
   ty->h = new_h;
   ty->termstate.had_cr = 0;
   ty->termstate.wrapnext = 0;

   if (altbuf)
     termpty_screen_swap(ty);

   _limit_coord(ty);

   _pty_size(ty);

   termpty_backlog_unlock();

   ty->backlog_beacon.backlog_y = 1;
   ty->backlog_beacon.screen_y = 1;

   return;

bad:
   termpty_backlog_unlock();
   free(new_screen);
}

void
termpty_backlog_size_set(Termpty *ty, size_t size)
{
   if (ty->backsize == size)
     return;

   /* TODO: RESIZE: handle that case better: changing backscroll size */
   termpty_backlog_lock();

   if (ty->back)
     {
        size_t i;

        for (i = 0; i < ty->backsize; i++)
          termpty_save_free(&ty->back[i]);
        free(ty->back);
     }
   if (size > 0)
     ty->back = calloc(1, sizeof(Termsave) * size);
   else
     ty->back = NULL;
   ty->backpos = 0;
   ty->backsize = size;
   termpty_backlog_unlock();
}

pid_t
termpty_pid_get(const Termpty *ty)
{
   return ty->pid;
}

void
termpty_block_free(Termblock *tb)
{
   char *s;
   if (tb->path) eina_stringshare_del(tb->path);
   if (tb->link) eina_stringshare_del(tb->link);
   if (tb->chid) eina_stringshare_del(tb->chid);
   if (tb->obj) evas_object_del(tb->obj);
   EINA_LIST_FREE(tb->cmds, s) free(s);
   free(tb);
}

Termblock *
termpty_block_new(Termpty *ty, int w, int h, const char *path, const char *link)
{
   Termblock *tb;
   int id;
   
   id = ty->block.curid;
   if (!ty->block.blocks)
     ty->block.blocks = eina_hash_int32_new((Eina_Free_Cb)termpty_block_free);
   if (!ty->block.blocks) return NULL;
   tb = eina_hash_find(ty->block.blocks, &id);
   if (tb)
     {
        if (tb->active)
          ty->block.active = eina_list_remove(ty->block.active, tb);
        eina_hash_del(ty->block.blocks, &id, tb);
     }
   tb = calloc(1, sizeof(Termblock));
   if (!tb) return NULL;
   tb->pty = ty;
   tb->id = id;
   tb->w = w;
   tb->h = h;
   tb->path = eina_stringshare_add(path);
   if (link) tb->link = eina_stringshare_add(link);
   eina_hash_add(ty->block.blocks, &id, tb);
   ty->block.curid++;
   if (ty->block.curid >= 8192) ty->block.curid = 0;
   return tb;
}

void
termpty_block_insert(Termpty *ty, int ch, Termblock *blk)
{
   // bit 0-8 = y (9b 0->511)
   // bit 9-17 = x (9b 0->511)
   // bit 18-30 = id (13b 0->8191)
   // bit 31 = 1
   // 
   // fg/bg = 8+8bit unused. (use for extra id bits? so 16 + 13 == 29bit?)
   // 
   // cp = (1 << 31) | ((id 0x1fff) << 18) | ((x & 0x1ff) << 9) | (y & 0x1ff);
   Termexp *ex;
   
   ex = calloc(1, sizeof(Termexp));
   if (!ex) return;
   ex->ch = ch;
   ex->left = blk->w * blk->h;
   ex->id = blk->id;
   ex->w = blk->w;
   ex->h = blk->h;
   ty->block.expecting = eina_list_append(ty->block.expecting, ex);
}

int
termpty_block_id_get(Termcell *cell, int *x, int *y)
{
   int id;
   
   if (!(cell->codepoint & 0x80000000)) return -1;
   id = (cell->codepoint >> 18) & 0x1fff;
   *x = (cell->codepoint >> 9) & 0x1ff;
   *y = cell->codepoint & 0x1ff;
   return id;
}

Termblock *
termpty_block_get(Termpty *ty, int id)
{
   if (!ty->block.blocks) return NULL;
   return eina_hash_find(ty->block.blocks, &id);
}

void
termpty_block_chid_update(Termpty *ty, Termblock *blk)
{
   if (!blk->chid) return;
   if (!ty->block.chid_map)
     ty->block.chid_map = eina_hash_string_superfast_new(NULL);
   if (!ty->block.chid_map) return;
   eina_hash_add(ty->block.chid_map, blk->chid, blk);
}

Termblock *
termpty_block_chid_get(Termpty *ty, const char *chid)
{
   Termblock *tb;
   
   tb = eina_hash_find(ty->block.chid_map, chid);
   return tb;
}

static void
_handle_block_codepoint_overwrite_heavy(Termpty *ty, int oldc, int newc)
{
   Termblock *tb;
   int ido = 0, idn = 0;

   if (oldc & 0x80000000) ido = (oldc >> 18) & 0x1fff;
   if (newc & 0x80000000) idn = (newc >> 18) & 0x1fff;
   if (((oldc & 0x80000000) && (newc & 0x80000000)) && (idn == ido)) return;
   
   if (oldc & 0x80000000)
     {
        tb = termpty_block_get(ty, ido);
        if (!tb) return;
        tb->refs--;
        if (tb->refs == 0)
          {
             if (tb->active)
               ty->block.active = eina_list_remove(ty->block.active, tb);
             eina_hash_del(ty->block.blocks, &ido, tb);
          }
     }
   
   if (newc & 0x80000000)
     {
        tb = termpty_block_get(ty, idn);
        if (!tb) return;
        tb->refs++;
     }
}

/* Try to trick the compiler into inlining the first test */
static inline void
_handle_block_codepoint_overwrite(Termpty *ty, Eina_Unicode oldc, Eina_Unicode newc)
{
   if (!((oldc | newc) & 0x80000000)) return;
   _handle_block_codepoint_overwrite_heavy(ty, oldc, newc);
}

void
termpty_cell_copy(Termpty *ty, Termcell *src, Termcell *dst, int n)
{
   int i;

   for (i = 0; i < n; i++)
     {
        _handle_block_codepoint_overwrite(ty, dst[i].codepoint, src[i].codepoint);
        dst[i] = src[i];
     }
}

void
termpty_screen_swap(Termpty *ty)
{
   Termcell *tmp_screen;
   int tmp_circular_offset;

   tmp_screen = ty->screen;
   ty->screen = ty->screen2;
   ty->screen2 = tmp_screen;

   tmp_circular_offset = ty->circular_offset;
   ty->circular_offset = ty->circular_offset2;
   ty->circular_offset2 = tmp_circular_offset;

   ty->altbuf = !ty->altbuf;

   if (ty->cb.cancel_sel.func)
     ty->cb.cancel_sel.func(ty->cb.cancel_sel.data);
}

void
termpty_cell_fill(Termpty *ty, Termcell *src, Termcell *dst, int n)
{
   int i;

   if (src)
     {
        for (i = 0; i < n; i++)
          {
             _handle_block_codepoint_overwrite(ty, dst[i].codepoint, src[0].codepoint);
             dst[i] = src[0];
          }
     }
   else
     {
        for (i = 0; i < n; i++)
          {
             _handle_block_codepoint_overwrite(ty, dst[i].codepoint, 0);
             memset(&(dst[i]), 0, sizeof(*dst));
          }
     }
}

void
termpty_cell_codepoint_att_fill(Termpty *ty, Eina_Unicode codepoint,
                                Termatt att, Termcell *dst, int n)
{
   Termcell local = { .codepoint = codepoint, .att = att };
   int i;

   for (i = 0; i < n; i++)
     {
        _handle_block_codepoint_overwrite(ty, dst[i].codepoint, codepoint);
        dst[i] = local;
     }
}

Config *
termpty_config_get(const Termpty *ty)
{
   return termio_config_get(ty->obj);
}
