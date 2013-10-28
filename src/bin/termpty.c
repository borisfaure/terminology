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
_cb_fd_read(void *data, Ecore_Fd_Handler *fd_handler EINA_UNUSED)
{
   Termpty *ty = data;
   char buf[4097];
   Eina_Unicode codepoint[4097];
   int len, i, j, k, reads;

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
termpty_new(const char *cmd, Eina_Bool login_shell, const char *cd,
            int w, int h, int backscroll, Eina_Bool xterm_256color,
            Eina_Bool erase_is_del)
{
   Termpty *ty;
   const char *pty;
   int mode;

   ty = calloc(1, sizeof(Termpty));
   if (!ty) return NULL;
   ty->w = w;
   ty->h = h;
   ty->backmax = backscroll;

   _termpty_reset_state(ty);
   ty->save = ty->state;
   ty->swap = ty->state;

   ty->screen = calloc(1, sizeof(Termcell) * ty->w * ty->h);
   if (!ty->screen)
     {
        ERR("Allocation of term screen %ix%i", ty->w, ty->h);
        goto err;
     }
   ty->screen2 = calloc(1, sizeof(Termcell) * ty->w * ty->h);
   if (!ty->screen2)
     {
        ERR("Allocation of term screen2 %ix%i", ty->w, ty->h);
        goto err;
     }

   ty->circular_offset = 0;

   ty->fd = posix_openpt(O_RDWR | O_NOCTTY);
   if (ty->fd < 0)
     {
        ERR("posix_openpt failed: %s", strerror(errno));
        goto err;
     }
   if (grantpt(ty->fd) != 0)
     {
        WRN("grantpt failed: %s", strerror(errno));
     }
   if (unlockpt(ty->fd) != 0)
     {
        ERR("unlockpt failed: %s", strerror(errno));
        goto err;
     }
   pty = ptsname(ty->fd);
   ty->slavefd = open(pty, O_RDWR | O_NOCTTY);
   if (ty->slavefd < 0)
     {
        ERR("open of pty '%s' failed: %s", pty, strerror(errno));
        goto err;
     }
   mode = fcntl(ty->fd, F_GETFL, 0);
   if (mode < 0)
     {
        ERR("fcntl on pty '%s' failed: %s", pty, strerror(errno));
        goto err;
     }
   if (!(mode & O_NDELAY))
      if (fcntl(ty->fd, F_SETFL, mode | O_NDELAY))
        {
           ERR("fcntl on pty '%s' failed: %s", pty, strerror(errno));
           goto err;
        }

   if (erase_is_del)
     {
        struct termios t;

        tcgetattr(ty->fd, &t);
        t.c_cc[VERASE] = 0x7f;
        tcsetattr(ty->fd, TCSANOW, &t);
     }
   else
     {
        struct termios t;

        tcgetattr(ty->fd, &t);
        t.c_cc[VERASE] = 0x8;
        tcsetattr(ty->fd, TCSANOW, &t);
     }

   ty->hand_exe_exit = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
                                               _cb_exe_exit, ty);
   if (!ty->hand_exe_exit)
     {
        ERR("event handler add failed");
        goto err;
     }
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
             if (ty->back[i])
               {
                  termpty_save_free(ty->back[i]);
                  ty->back[i] = NULL;
               }
          }
        free(ty->back);
        ty->back = NULL;
     }
   if (ty->screen) free(ty->screen);
   if (ty->screen2) free(ty->screen2);
   if (ty->buf) free(ty->buf);
   memset(ty, 0, sizeof(Termpty));
   free(ty);
}

void
termpty_cellcomp_freeze(Termpty *ty EINA_UNUSED)
{
   termpty_save_freeze();
}

void
termpty_cellcomp_thaw(Termpty *ty EINA_UNUSED)
{
   termpty_save_thaw();
}

Termcell *
termpty_cellrow_get(Termpty *ty, int y, int *wret)
{
   Termsave *ts, **tssrc;

   if (y >= 0)
     {
        if (y >= ty->h) return NULL;
        *wret = ty->w;
	/* fprintf(stderr, "getting: %i (%i, %i)\n", y, ty->circular_offset, ty->h); */
        return &(TERMPTY_SCREEN(ty, 0, y));
     }
   if ((y < -ty->backmax) || !ty->back) return NULL;
   tssrc = &(ty->back[(ty->backmax + ty->backpos + y) % ty->backmax]);
   ts = termpty_save_extract(*tssrc);
   if (!ts) return NULL;
   *tssrc = ts;
   *wret = ts->w;
   return ts->cell;
}
   
void
termpty_write(Termpty *ty, const char *input, int len)
{
   if (ty->fd < 0) return;
   if (write(ty->fd, input, len) < 0) ERR("write %s", strerror(errno));
}

ssize_t
termpty_line_length(const Termcell *cells, ssize_t nb_cells)
{
   ssize_t len = nb_cells;

   for (len = nb_cells - 1; len >= 0; len--)
     {
        const Termcell *cell = cells + len;

        if ((cell->codepoint != 0) &&
            (cell->att.bg != COL_INVIS))
          return len + 1;
     }

   return 0;
}

#define OLD_SCREEN(_X, _Y) \
   old_screen[_X + (((_Y + old_circular_offset) % old_h) * old_w)]

static void
_termpty_horizontally_expand(Termpty *ty, int old_w, int old_h,
                             Termcell *old_screen)
{
   int i,
       new_back_pos = 0,
       old_y,
       old_x,
       old_circular_offset = ty->circular_offset,
       x = 0,
       y = 0;
   Termsave **new_back, *new_ts = NULL;
   Eina_Bool rewrapping = EINA_FALSE;

   if ((!ty->backmax) || (!ty->back)) goto expand_screen;

   new_back = calloc(sizeof(Termsave *), ty->backmax);
   if (!new_back) return;
   
   termpty_save_freeze();
   for (i = 0; i < ty->backmax; i++)
     {
        Termsave *ts;

        if (ty->backscroll_num == ty->backmax - 1)
          ts = termpty_save_extract(ty->back[(ty->backpos + i) % ty->backmax]);
        else
          ts = termpty_save_extract(ty->back[i]);
        if (!ts) break;

        if (!ts->w)
          {
             if (rewrapping)
               {
                  rewrapping = EINA_FALSE;
                  new_ts->cell[new_ts->w - 1].att.autowrapped = 0;
               }
             else
               new_back[new_back_pos++] = ts;
             continue;
          }

        if (rewrapping)
          {
             int remaining_width = ty->w - new_ts->w;
             int len = ts->w;
             Termcell *cells = ts->cell;

             if (new_ts->w)
               {
                  new_ts->cell[new_ts->w - 1].att.autowrapped = 0;
               }
             if (ts->w >= remaining_width)
               {
                  termpty_cell_copy(ty, cells, new_ts->cell + new_ts->w,
                                    remaining_width);
                  new_ts->w = ty->w;
                  new_ts->cell[new_ts->w - 1].att.autowrapped = 1;
                  len -= remaining_width;
                  cells += remaining_width;

                  new_ts = termpty_save_new(ty->w);
                  new_ts->w = 0;
                  new_back[new_back_pos++] = new_ts;
               }
             if (len)
               {
                  termpty_cell_copy(ty, cells, new_ts->cell + new_ts->w, len);
                  new_ts->w += len;
               }

             rewrapping = ts->cell[ts->w - 1].att.autowrapped;
             if (!rewrapping) new_ts = NULL;
             termpty_save_free(ts);
          }
        else
          {
             if (ts->cell[ts->w - 1].att.autowrapped)
               {
                  rewrapping = EINA_TRUE;
                  new_ts = termpty_save_new(ty->w);
                  new_ts->w = ts->w;
                  termpty_cell_copy(ty, ts->cell, new_ts->cell, ts->w);
                  new_ts->cell[ts->w - 1].att.autowrapped = 0;
                  new_back[new_back_pos++] = new_ts;
                  termpty_save_free(ts);
               }
             else
               new_back[new_back_pos++] = ts;
          }
     }

   if (new_back_pos >= ty->backmax)
     {
        ty->backscroll_num = ty->backmax - 1;
        ty->backpos = 0;
     }
   else
     {
        ty->backscroll_num = new_back_pos;
        ty->backpos = new_back_pos;
     }

   free(ty->back);
   ty->back = new_back;

expand_screen:

   if (ty->altbuf)
     {
        termpty_save_thaw();
        return;
     }

   ty->circular_offset = 0;
   /* TODO double-width :) */
   for (old_y = 0; old_y < old_h; old_y++)
     {
        ssize_t cur_line_length;

        cur_line_length = termpty_line_length(&OLD_SCREEN(0, old_y), old_w);
        if (rewrapping)
          {
             if (new_ts)
               {
                  ssize_t remaining_width = ty->w - new_ts->w;
                  ssize_t len = MIN(cur_line_length, remaining_width);
                  Termcell *cells = &OLD_SCREEN(0, old_y);

                  termpty_cell_copy(ty, cells, new_ts->cell + new_ts->w, len);
                  new_ts->w += len;
                  cells += len;
                  if (cur_line_length > remaining_width)
                    {
                       new_ts->cell[new_ts->w - 1].att.autowrapped = 1;
                       new_ts = NULL;
                       len = cur_line_length - remaining_width;
                       termpty_cell_copy(ty, cells, ty->screen + (y * ty->w),
                                         len);
                       x += len;
                    }

                  rewrapping = OLD_SCREEN(old_w - 1, old_y).att.autowrapped;
                  if (!rewrapping)
                    {
                       new_ts = NULL;
                    }
               }
             else
               {
                  int remaining_width = ty->w - x,
                      len = cur_line_length;

                  old_x = 0;
                  if (cur_line_length >= remaining_width)
                    {
                       termpty_cell_copy(ty,
                                         &OLD_SCREEN(0, old_y),
                                         ty->screen + (y * ty->w) + x,
                                         remaining_width);
                       TERMPTY_SCREEN(ty, ty->w - 1, y).att.autowrapped = 1;
                       y++;
                       x = 0;
                       old_x = remaining_width;
                       len -= remaining_width;
                    }
                  if (len)
                    {
                       termpty_cell_copy(ty,
                                         &OLD_SCREEN(old_x, old_y),
                                         ty->screen + (y * ty->w) + x,
                                         len);
                       x += len;
                       TERMPTY_SCREEN(ty, x - 1, y).att.autowrapped = 0;
                    }
                  rewrapping = OLD_SCREEN(old_w - 1, old_y).att.autowrapped;
                  if (!rewrapping) y++;
               }
          }
        else
          {
             termpty_cell_copy(ty,
                               &OLD_SCREEN(0, old_y),
                               ty->screen + (y * ty->w),
                               cur_line_length);
             if (OLD_SCREEN(old_w - 1, old_y).att.autowrapped)
               {
                  rewrapping = EINA_TRUE;
                  TERMPTY_SCREEN(ty, old_w - 1, old_y).att.autowrapped = 0;
                  x = cur_line_length;
               }
             else y++;
          }
     }
   if (y < old_h)
     {
        ty->state.cy -= old_h - y;
        if (ty->state.cy < 0) ty->state.cy = 0;
     }
   termpty_save_thaw();
}

static void
_termpty_vertically_expand(Termpty *ty, int old_w, int old_h,
                           Termcell *old_screen)
{
   int from_history = 0, y;
   
   termpty_save_freeze();
   
   if (ty->backmax > 0)
     from_history = MIN(ty->h - old_h, ty->backscroll_num);
   if (old_screen)
     {
        int old_circular_offset = ty->circular_offset;

        ty->circular_offset = 0;

        for (y = 0; y < old_h; y++)
          {
             Termcell *c1, *c2;

             c1 = &(OLD_SCREEN(0, y));
             c2 = &(TERMPTY_SCREEN(ty, 0, y));
             termpty_cell_copy(ty, c1, c2, old_w);
          }
     }

   if ((from_history <= 0) || (!ty->back))
     {
        termpty_save_thaw();
        return;
     }

   /* display content from backlog */
   for (y = from_history - 1; y >= 0; y--)
     {
        Termsave *ts;
        Termcell *src, *dst;

        ty->backpos--;
        if (ty->backpos < 0)
          ty->backpos = ty->backscroll_num - 1;
        ts = termpty_save_extract(ty->back[ty->backpos]);

        src = ts->cell;
        dst = &(TERMPTY_SCREEN(ty, 0, ty->h - from_history + y));
        termpty_cell_copy(ty, src, dst, ts->w);

        termpty_save_free(ts);
        ty->back[ty->backpos] = NULL;
        ty->backscroll_num--;
     }

   ty->circular_offset = (ty->circular_offset + ty->h - from_history) % ty->h;

   ty->state.cy += from_history;
   termpty_save_thaw();
}


static void
_termpty_vertically_shrink(Termpty *ty, int old_w, int old_h,
                           Termcell *old_screen)
{
   int to_history,
       real_h = old_h,
       old_circular_offset,
       y;
   Termcell *src, *dst;

   termpty_save_freeze();
   
   old_circular_offset = ty->circular_offset;
   for (y = old_h - 1; y >= 0; y--)
     {
        ssize_t screen_length = termpty_line_length(&OLD_SCREEN(0, y), old_w);

        if (screen_length) break;
        else real_h--;
     }

   to_history = real_h - ty->h;
   if (to_history > 0)
     {
        for (y = 0; y < to_history; y++)
          {
             termpty_text_save_top(ty, &(OLD_SCREEN(0, y)), old_w);
          }
        ty->state.cy -= to_history;
        if (ty->state.cy < 0) ty->state.cy = 0;
     }

   if (old_w == ty->w)
     {
        if (to_history < 0)
          to_history = 0;
        ty->circular_offset = 0;
        for (y = 0; y < ty->h; y++)
          {
             src = &(OLD_SCREEN(0, y + to_history));
             dst = &(TERMPTY_SCREEN(ty, 0, y));
             termpty_cell_copy(ty, src, dst, old_w);
          }
     }
   else
     {
        /* in place */
        int len, pos, offset;

        if ((to_history <= 0) || (ty->circular_offset == 0))
          {
             termpty_save_thaw();
             return;
          }

        ty->circular_offset = (ty->circular_offset + to_history) % old_h;
        len = real_h - to_history;
        pos = (len + ty->circular_offset) % old_h;
        offset = len - pos;

        /* 2 times */
        if (offset > 0)
          {
           for (y = pos - 1; y >= 0; y--)
             {
                src = &(old_screen[y * old_w]);
                dst = &(old_screen[(y + offset) * old_w]);
                termpty_cell_copy(ty, src, dst, old_w);
             }
          }
        else
          offset = len;

        for (y = 0; y < offset; y++)
          {
             src = &(old_screen[(ty->circular_offset + y) * old_w]);
             dst = &(old_screen[y * old_w]);
             termpty_cell_copy(ty, src, dst, old_w);
          }
        ty->circular_offset = 0;

        len = ty->h - len;
        if (len)
          termpty_cell_fill(ty, NULL,
                            &old_screen[(old_h - len) * old_w],
                            len * old_w);
     }
   termpty_save_thaw();
}


static void
_termpty_horizontally_shrink(Termpty *ty, int old_w, int old_h,
                             Termcell *old_screen)
{
   int i,
       new_back_pos = 0,
       new_back_scroll_num = 0,
       old_y,
       old_x,
       screen_height_used,
       old_circular_offset = ty->circular_offset,
       x = 0,
       y = 0;
   ssize_t *screen_lengths;
   Termsave **new_back,
            *new_ts = NULL,
            *old_ts = NULL;
   Termcell *old_cells = NULL;
   Eina_Bool rewrapping = EINA_FALSE,
             done = EINA_FALSE,
             cy_pushed_back = EINA_FALSE;

   termpty_save_freeze();
   
   if (!ty->backmax || !ty->back)
     goto shrink_screen;

   new_back = calloc(sizeof(Termsave*), ty->backmax);

   for (i = 0; i < ty->backmax; i++)
     {
        Termsave *ts;
        Termcell *cells;
        int remaining_width;

        if (ty->backscroll_num == ty->backmax - 1)
          ts = termpty_save_extract(ty->back[(ty->backpos + i) % ty->backmax]);
        else
          ts = termpty_save_extract(ty->back[i]);
        if (!ts)
          break;

#define PUSH_BACK_TS(_ts) do {           \
        if (new_back[new_back_pos])      \
          termpty_save_free(new_back[new_back_pos]); \
        new_back[new_back_pos++] = _ts;  \
        new_back_scroll_num++;           \
        if (new_back_pos >= ty->backmax) \
          new_back_pos = 0;              \
        } while(0)

        if (!ts->w)
          {
             if (rewrapping)
               {
                  rewrapping = EINA_FALSE;
                  new_ts = termpty_save_new(old_ts->w);
                  termpty_cell_copy(ty, old_cells, new_ts->cell, old_ts->w);
                  PUSH_BACK_TS(new_ts);
                  termpty_save_free(old_ts);
                  old_ts = NULL;
                  termpty_save_free(ts);
               }
             else
               PUSH_BACK_TS(ts);
             continue;
          }

        if (!old_ts && ts->w <= ty->w)
          {
             PUSH_BACK_TS(ts);
             continue;
          }

        cells = ts->cell;

        if (old_ts)
          {
             int len = MIN(old_ts->w + ts->w, ty->w);

             new_ts = termpty_save_new(len);
             termpty_cell_copy(ty, old_cells, new_ts->cell, old_ts->w);

             remaining_width = len - old_ts->w;

             termpty_cell_copy(ty, cells, new_ts->cell + old_ts->w,
                               remaining_width);

             rewrapping = ts->cell[ts->w - 1].att.autowrapped;
             cells += remaining_width;
             ts->w -= remaining_width;

             new_ts->cell[new_ts->w - 1].att.autowrapped =
                rewrapping || ts->w > 0;
             
             termpty_save_free(old_ts);
             old_ts = NULL;

             PUSH_BACK_TS(new_ts);
          }

        while (ts->w >= ty->w)
          {
             new_ts = termpty_save_new(ty->w);
             termpty_cell_copy(ty, cells, new_ts->cell, ty->w);
             rewrapping = ts->cell[ts->w - 1].att.autowrapped;

             new_ts->cell[new_ts->w - 1].att.autowrapped = 1;
             if (!rewrapping && ty->w == ts->w)
               new_ts->cell[new_ts->w - 1].att.autowrapped = 0;

             cells += ty->w;
             ts->w -= ty->w;

             PUSH_BACK_TS(new_ts);
          }

        if (!ts->w)
          {
             old_cells = 0;
             termpty_save_free(old_ts);
             old_ts = NULL;
             rewrapping = EINA_FALSE;
             continue;
          }

        if (rewrapping)
          {
             old_cells = cells;
             old_ts = ts;
          }
        else
          {
             new_ts = termpty_save_new(ts->w);
             termpty_cell_copy(ty, cells, new_ts->cell, ts->w);
             PUSH_BACK_TS(new_ts);
             termpty_save_free(ts);
          }
     }
#undef PUSH_BACK_TS

   ty->backpos = new_back_pos;
   ty->backscroll_num = new_back_scroll_num;
   if (ty->backscroll_num >= ty->backmax)
     ty->backscroll_num = ty->backmax - 1;
   free(ty->back);
   ty->back = new_back;

shrink_screen:

   if (ty->altbuf)
     {
        termpty_save_thaw();
        return;
     }

   ty->circular_offset = 0;

   /* TODO double-width :) */
   x = 0;
   y = 0;

   if (old_ts)
     {
        termpty_cell_copy(ty, old_cells, ty->screen, old_ts->w);
        x = old_ts->w;
        if (!rewrapping) y++;
        termpty_save_free(old_ts);
        old_ts = NULL;
        old_cells = NULL;
     }
   screen_height_used = old_h;
   screen_lengths = malloc(sizeof(ssize_t) * old_h);
   for (old_y = old_h - 1; old_y >= 0; old_y--)
     {
        screen_lengths[old_y] = termpty_line_length(&OLD_SCREEN(0, old_y), old_w);

        if (!screen_lengths[old_y] && done != EINA_TRUE)
          screen_height_used--;
        else
          done = EINA_TRUE;
     }

   for (old_y = 0; old_y < screen_height_used; old_y++)
     {
        ssize_t cur_line_length;
        int remaining_width, len;

        cur_line_length = screen_lengths[old_y];

        if (old_y == ty->state.cy)
          ty->state.cy = y;

        old_x = 0;
        do
          {
             Eina_Bool need_new_line = EINA_FALSE;

             remaining_width = ty->w - x;
             len = MIN(remaining_width, cur_line_length);
             termpty_cell_copy(ty,
                               &OLD_SCREEN(old_x, old_y),
                               &TERMPTY_SCREEN(ty, x, y),
                               len);
             x += len;
             old_x += len;
             cur_line_length -= len;
             if (cur_line_length > 0)
               {
                  TERMPTY_SCREEN(ty, x - 1, y).att.autowrapped = 1;
                  need_new_line = EINA_TRUE;
               }
             else
               {
                  Termcell *cell = &TERMPTY_SCREEN(ty, x - 1, y);
                  if (cell->att.autowrapped)
                    {
                       if (x >= ty->w)
                         need_new_line = EINA_TRUE;
                       else
                         {
                            cell->att.autowrapped = 0;
                            need_new_line = EINA_FALSE;
                         }
                    }
                  else
                    {
                       if (old_y < old_h - 1)
                         need_new_line = EINA_TRUE;
                    }
               }
             if (need_new_line)
               {
                  x = 0;
                  if (y >= ty->h - 1)
                    {
                       Termcell *cells;

                       ty->circular_offset++;
                       if (ty->circular_offset >= ty->h)
                         ty->circular_offset = 0;
                       cells = &TERMPTY_SCREEN(ty, 0, y);
                       len = termpty_line_length(cells, ty->w);
                       termpty_text_save_top(ty, cells, len);
                       termpty_cell_fill(ty, NULL, cells, len);
                       if (ty->state.cy == old_y || cy_pushed_back)
                         {
                            cy_pushed_back = EINA_TRUE;
                            ty->state.cy--;
                         }
                    }
                  else
                    y++;
               }
          }
        while (cur_line_length > 0);
     }
   if (ty->state.cy >= ty->h) ty->state.cy = ty->h - 1;
   else if (ty->state.cy < 0) ty->state.cy = 0;
   
   termpty_save_thaw();

   free(screen_lengths);

   termpty_save_thaw();
}
#undef OLD_SCREEN


void
termpty_resize(Termpty *ty, int w, int h)
{
   Termcell *olds, *olds2;
   int oldw, oldh;

   if ((ty->w == w) && (ty->h == h)) return;

   if (w == h && h == 1) // fuck off
     return;

   olds = ty->screen;
   olds2 = ty->screen2;
   oldw = ty->w;
   oldh = ty->h;

   ty->w = w;
   ty->h = h;
   ty->state.had_cr = 0;

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

   /* Shrink vertically, in place, if needed */
   if (ty->h < oldh)
     {
        _termpty_vertically_shrink(ty, oldw, oldh, olds);
        oldh = ty->h;
     }

   if (oldw != ty->w)
     {
        if (ty->w > oldw)
          _termpty_horizontally_expand(ty, oldw, oldh, olds);
        else
          _termpty_horizontally_shrink(ty, oldw, oldh, olds);

        free(olds); olds = NULL;
        free(olds2); olds2 = NULL;
     }

   if (ty->h > oldh)
     _termpty_vertically_expand(ty, oldw, oldh, olds);

   free(olds);
   free(olds2);

   _limit_coord(ty, &(ty->state));
   _limit_coord(ty, &(ty->swap));
   _limit_coord(ty, &(ty->save));

   _pty_size(ty);
}

void
termpty_backscroll_set(Termpty *ty, int size)
{
   int i;

   if (ty->backmax == size) return;
   
   termpty_save_freeze();

   if (ty->back)
     {
        for (i = 0; i < ty->backmax; i++)
          {
             if (ty->back[i]) termpty_save_free(ty->back[i]);
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
   termpty_save_thaw();
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
_handle_block_codepoint_overwrite(Termpty *ty, int oldc, int newc)
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
termpty_cell_swap(Termpty *ty EINA_UNUSED, Termcell *src, Termcell *dst, int n)
{
   int i;
   Termcell t;
   
   for (i = 0; i < n; i++)
     {
        t = dst[i];
        dst[i] = src[i];
        dst[i] = t;
     }
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
termpty_cell_codepoint_att_fill(Termpty *ty, int codepoint, Termatt att, Termcell *dst, int n)
{
   Termcell local = { codepoint, att };
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
