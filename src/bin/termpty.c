#include "private.h"
#include <Elementary.h>
#include <Ecore_Input.h>
#include <Ecore_IMF.h>
#include <Ecore_IMF_Evas.h>
#include "termpty.h"
#include "termptyesc.h"
#include "termptyops.h"
#include "backlog.h"
#include "keyin.h"
#if !defined(BINARY_TYFUZZ) && !defined(BINARY_TYTEST)
# include "win.h"
#endif
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
     EINA_LOG_CRIT("Could not create logging domain '%s'", "termpty");
}

void
termpty_shutdown(void)
{
   if (_termpty_log_dom < 0) return;
   eina_log_domain_unregister(_termpty_log_dom);
   _termpty_log_dom = -1;
}


Eina_Bool
termpty_can_handle_key(const Termpty *ty,
                       const Keys_Handler *khdl,
                       const Evas_Event_Key_Down *ev)
{
   // if term app asked for kbd lock - don't handle here
   if (ty->termstate.kbd_lock)
     return EINA_FALSE;
   // if app asked us to not do autorepeat - ignore press if is it is the same
   // timestamp as last one
   if ((ty->termstate.no_autorepeat) &&
       (ev->timestamp == khdl->last_keyup))
     return EINA_FALSE;
   return EINA_TRUE;
}




void
termpty_handle_buf(Termpty *ty, const Eina_Unicode *codepoints, int len)
{
   Eina_Unicode *c, *ce, *c2, *b, *d;
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
        if (!ty->buf_have_zero)
          {
             d = &(b[ty->buflen]);
             ce = (Eina_Unicode *)codepoints + len;
             for (c = (Eina_Unicode *)codepoints; c < ce; c++, d++)
               {
                  *d = *c;
                  if (*c == 0x0)
                    {
                       ty->buf_have_zero = EINA_TRUE;
                       break;
                    }
               }
             for (; c < ce; c++, d++) *d = *c;
          }
        else
          {
             bytes = len * sizeof(Eina_Unicode);
             memcpy(&(b[ty->buflen]), codepoints, bytes);
          }
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
                  ty->buf_have_zero = EINA_FALSE;
                  for (d = ty->buf, c2 = c; c2 < ce; c2++, d++)
                    {
                       *d = *c2;
                       if (*c2 == 0x0)
                         {
                            ty->buf_have_zero = EINA_TRUE;
                            break;
                         }
                    }
                  for (; c2 < ce; c2++, d++) *d = *c2;
                  bytes = (char *)ce - (char *)c;
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
                  ty->buf_have_zero = EINA_FALSE;
                  free(ty->buf);
                  ty->buf = NULL;
               }
             ty->buflen = 0;
          }
     }
   else
     {
        ty->buf_have_zero = EINA_TRUE;
        while (c < ce)
          {
             n = termpty_handle_seq(ty, c, ce);
             if (n == 0)
               {
                  bytes = ((char *)ce - (char *)c) + sizeof(Eina_Unicode);
                  ty->buf_have_zero = EINA_FALSE;
                  ty->buf = malloc(bytes);
                  DBG("malloc %i", (int)(bytes - sizeof(Eina_Unicode)));
                  if (!ty->buf)
                    {
                       ERR(_("memerr: %s"), strerror(errno));
                    }
                  else
                    {
                       ty->buf_have_zero = EINA_FALSE;
                       for (d = ty->buf, c2 = c; c2 < ce; c2++, d++)
                         {
                            *d = *c2;
                            if (*c2 == 0x0)
                              {
                                 ty->buf_have_zero = EINA_TRUE;
                                 break;
                              }
                         }
                       for (; c2 < ce; c2++, d++) *d = *c2;
                       bytes = (char *)ce - (char *)c;
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
   if (ty->fd >= 0)
     if (ioctl(ty->fd, TIOCSWINSZ, &sz) < 0)
       ERR(_("Size set ioctl failed: %s"), strerror(errno));
}

static Eina_Bool
_handle_read(Termpty *ty, Eina_Bool false_on_empty)
{
   int len, reads;

   // read up to 64 * 4096 bytes
   for (reads = 0; reads < 64; reads++)
     {
        Eina_Unicode codepoint[4097];
        char buf[4097];
        char *rbuf = buf;
        int i, j;
        len = sizeof(buf) - 1;

        for (i = 0; i < (int)sizeof(ty->oldbuf) && ty->oldbuf[i] & 0x80; i++)
          {
             *rbuf = ty->oldbuf[i];
             rbuf++;
             len--;
          }
        errno = 0;
        len = read(ty->fd, rbuf, len);
        if ((len < 0 && !(errno == EAGAIN || errno == EINTR)) ||
            (len == 0 && errno != 0))
          {
             /* Do not print error if the child has exited */
             if (ty->pid != -1)
               {
                  ERR("error while reading from tty slave fd: %s", strerror(errno));
               }
             close(ty->fd);
             ty->fd = -1;
             if (ty->hand_fd)
               ecore_main_fd_handler_del(ty->hand_fd);
             ty->hand_fd = NULL;
             return ECORE_CALLBACK_CANCEL;
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
             Eina_Unicode g = 0, prev_i = i;

             if (buf[i])
               {
                  g = eina_unicode_utf8_next_get(buf, &i);
                  if ((0xdc80 <= g) && (g <= 0xdcff) &&
                      (len - (int)prev_i) <= (int)sizeof(ty->oldbuf))
                    {
                       unsigned int k;

                       for (k = 0;
                            (k < (unsigned int)sizeof(ty->oldbuf)) &&
                            (k < (unsigned int)(len - prev_i));
                            k++)
                         {
                            ty->oldbuf[k] = buf[prev_i+k];
                         }
                       DBG("failure at %d/%d/%d", (int)prev_i, (int)i, len);
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
        termpty_handle_buf(ty, codepoint, j);
     }
   if (ty->cb.change.func)
     ty->cb.change.func(ty->cb.change.data);
#if defined(BINARY_TYFUZZ) || defined(BINARY_TYTEST)
   if (len <= 0)
     {
        ty->exit_code = 0;
        ty->pid = -1;

        if (ty->hand_exe_exit)
          ecore_event_handler_del(ty->hand_exe_exit);
        ty->hand_exe_exit = NULL;
        if (ty->hand_fd)
          ecore_main_fd_handler_del(ty->hand_fd);
        ty->hand_fd = NULL;
        ty->fd = -1;
        ty->slavefd = -1;
        if (ty->cb.exited.func)
          ty->cb.exited.func(ty->cb.exited.data);
        return ECORE_CALLBACK_CANCEL;
     }
#endif
   if ((false_on_empty) && (len <= 0))
     return ECORE_CALLBACK_CANCEL;

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_handle_write(Termpty *ty)
{
   struct ty_sb *sb = &ty->write_buffer;
   ssize_t len;

   if (!sb->len)
     return ECORE_CALLBACK_RENEW;

   len = write(ty->fd, sb->buf, sb->len);
   if (len < 0 && (errno != EINTR && errno != EAGAIN))
     {
        ERR(_("Could not write to file descriptor %d: %s"),
            ty->fd, strerror(errno));
        return ECORE_CALLBACK_CANCEL;
     }
   ty_sb_lskip(sb, len);

   if (!sb->len && ty->hand_fd)
     ecore_main_fd_handler_active_set(ty->hand_fd,
                                      ECORE_FD_ERROR |
                                      ECORE_FD_READ);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_fd_do(Termpty *ty, Ecore_Fd_Handler *fd_handler, Eina_Bool false_on_empty)
{
   if (ecore_main_fd_handler_active_get(fd_handler, ECORE_FD_ERROR))
     {
        DBG("error while doing I/O on tty slave fd");
        ty->hand_fd = NULL;
        return ECORE_CALLBACK_CANCEL;
     }
   if (ty->fd == -1)
     {
        ty->hand_fd = NULL;
        return ECORE_CALLBACK_CANCEL;
     }

   // it seems one can not read from this side of the pair if the other side
   // is closed
   if (ty->pid == -1)
     return ECORE_CALLBACK_CANCEL;

   if (ecore_main_fd_handler_active_get(fd_handler, ECORE_FD_READ))
     {
        if (!_handle_read(ty, false_on_empty))
          return ECORE_CALLBACK_CANCEL;
     }

   if (ecore_main_fd_handler_active_get(fd_handler, ECORE_FD_WRITE))
     {
        if (!_handle_write(ty))
          return ECORE_CALLBACK_CANCEL;
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_fd(void *data, Ecore_Fd_Handler *fd_handler)
{
   return _fd_do(data, fd_handler, EINA_FALSE);
}

static Eina_Bool
_cb_exe_exit(void *data,
             int _type EINA_UNUSED,
             void *event)
{
   Ecore_Exe_Event_Del *ev = event;
   Termpty *ty = data;
   Eina_Bool res;

   DBG("got exit (code:%d) on pid %d (ty->pid=%d)", ev->exit_code, ev->pid, ty->pid);
   if (ev->pid != ty->pid)
     return ECORE_CALLBACK_PASS_ON;
   ty->exit_code = ev->exit_code;

   ty->pid = -1;

   if (ty->hand_exe_exit)
     ecore_event_handler_del(ty->hand_exe_exit);
   ty->hand_exe_exit = NULL;

   /* Read everything till the end */
   res = ECORE_CALLBACK_PASS_ON;
   while (ty->hand_fd && res != ECORE_CALLBACK_CANCEL)
     {
        res = _fd_do(ty, ty->hand_fd, EINA_TRUE);
     }

   if (ty->hand_fd)
     ecore_main_fd_handler_del(ty->hand_fd);
   ty->hand_fd = NULL;
   if (ty->fd >= 0)
     close(ty->fd);
   ty->fd = -1;
   if (ty->slavefd >= 0)
     close(ty->slavefd);
   ty->slavefd = -1;

   if (ty->cb.exited.func)
     ty->cb.exited.func(ty->cb.exited.data);

   return ECORE_CALLBACK_PASS_ON;
}


static void
_limit_coord(Termpty *ty)
{
   TERMPTY_RESTRICT_FIELD(ty->termstate.had_cr_x, 0, ty->w);
   TERMPTY_RESTRICT_FIELD(ty->termstate.had_cr_y, 0, ty->h);

   TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
   TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);

   TERMPTY_RESTRICT_FIELD(ty->cursor_save[0].cx, 0, ty->w);
   TERMPTY_RESTRICT_FIELD(ty->cursor_save[0].cy, 0, ty->h);
   TERMPTY_RESTRICT_FIELD(ty->cursor_save[1].cx, 0, ty->w);
   TERMPTY_RESTRICT_FIELD(ty->cursor_save[1].cy, 0, ty->h);
}

void
termpty_resize_tabs(Termpty *ty, int old_w, int new_w)
{
    unsigned int *new_tabs;
    int i;
    size_t nb_elems;

    if ((new_w == old_w) && ty->tabs) return;

    nb_elems = ROUND_UP(new_w, 8);
    new_tabs = calloc(nb_elems, sizeof(unsigned int));
    if (!new_tabs) return;

    if (ty->tabs)
      {
         size_t n = old_w;
         if (nb_elems < n) n = nb_elems;
         if (n > 0) memcpy(new_tabs, ty->tabs, n * sizeof(unsigned int));
         free(ty->tabs);
      }

    ty->tabs = new_tabs;
    for (i = ROUND_UP(ty->w, TAB_WIDTH); i < new_w; i += TAB_WIDTH)
      {
         TAB_SET(ty, i);
      }
}

static Eina_Bool
_is_shell_valid(const char *cmd)
{
    struct stat st;

   if (!cmd)
     return EINA_FALSE;
   if (cmd[0] == '\0')
     return EINA_FALSE;
   if (cmd[0] != '/')
     {
        ERR("shell command '%s' is not an absolute path", cmd);
        return EINA_FALSE;
     }
   if (stat(cmd, &st) != 0)
     {
        ERR("shell command '%s' can not be stat(): %s", cmd, strerror(errno));
        return EINA_FALSE;
     }
   if ((st.st_mode & S_IFMT) != S_IFREG)
     {
        ERR("shell command '%s' is not a regular file", cmd);
        return EINA_FALSE;
     }
   if ((st.st_mode & S_IXOTH) == 0)
     {
        ERR("shell command '%s' is not executable", cmd);
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

Termpty *
termpty_new(const char *cmd, Eina_Bool login_shell, const char *cd,
            int w, int h, Config *config, const char *title,
            Ecore_Window window_id)
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
   ty->config = config;
   ty->w = w;
   ty->h = h;
   ty->backsize = config->scrollback;

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

   ty->hl.bitmap = calloc(1, HL_LINKS_MAX / 8); /* bit map for 1 << 16 elements */
   if (!ty->hl.bitmap)
     {
        ERR("Allocation of %d bytes failed: %s",
            HL_LINKS_MAX / 8, strerror(errno));
        goto err;
     }
   /* Mark id 0 as set */
   ty->hl.bitmap[0] = 1;

   termpty_resize_tabs(ty, 0, w);

   termpty_reset_state(ty);

   ty->circular_offset = 0;

#if defined(BINARY_TYFUZZ) || defined(BINARY_TYTEST)
   ty->fd = STDIN_FILENO;
   ty->hand_fd = ecore_main_fd_handler_add(ty->fd,
                                           ECORE_FD_READ | ECORE_FD_ERROR,
                                           _cb_fd, ty,
                                           NULL, NULL);
   _pty_size(ty);
   termpty_save_register(ty);
   return ty;
#endif

   needs_shell = ((!cmd) ||
                  (strpbrk(cmd, " |&;<>()$`\\\"'*?#") != NULL));
   DBG("cmd='%s' needs_shell=%u", cmd ? cmd : "", needs_shell);

   if (needs_shell)
     {
        shell = getenv("SHELL");
        if (!_is_shell_valid(shell))
          shell = NULL;
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

   if (title)
     {
        ty->prop.title = eina_stringshare_add(title);
        ty->prop.user_title = eina_stringshare_add(title);
     }
   else
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
   t.c_cc[VERASE] =  (config->erase_is_del) ? 0x7f : 0x8;
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
        if (cd)
          {
             int ret = chdir(cd);
             if (ret != 0)
               {
                  ERR(_("Could not change current directory to '%s': %s"),
                        cd, strerror(errno));
                  cd = getenv("HOME");
                  if (cd)
                    {
                       ret = chdir(cd);
                       if (ret != 0)
                         {
                            ERR(_("Could not change current directory to '%s': %s"),
                                cd, strerror(errno));
                         }
                    }
                  if (ret != 0)
                    {
                       cd = "/";
                       if (chdir(cd) != 0)
                         {
                            ERR(_("Could not change current directory to '%s': %s"),
                                cd, strerror(errno));
                         }
                    }
               }
          }


#define NC(x) (args[x] != NULL ? args[x] : "")
        DBG("exec %s %s %s %s", NC(0), NC(1), NC(2), NC(3));
#undef NC

        setsid();

        dup2(ty->slavefd, 0);
        dup2(ty->slavefd, 1);
        dup2(ty->slavefd, 2);

        if (ioctl(ty->slavefd, TIOCSCTTY, NULL) < 0) exit(1);

        close(ty->slavefd);
        close(ty->fd);

        /* Unset env variables that no longer apply */
        unsetenv("TERMCAP");
        unsetenv("COLUMNS");
        unsetenv("LINES");

        /* pretend to be xterm */
        if (config->xterm_256color)
          {
             putenv("TERM=xterm-256color");
          }
        else
          {
             putenv("TERM=xterm");
          }
        putenv("XTERM_256_COLORS=1");
        putenv("TERM_PROGRAM=terminology");
        putenv("TERM_PROGRAM_VERSION=" PACKAGE_VERSION);
#if defined(ENABLE_TEST_UI)
        putenv("IN_TY_TEST_UI=1" PACKAGE_VERSION);
#endif
        if (window_id)
          {
             char buf[256];

             snprintf(buf, sizeof(buf), "WINDOWID=%lu", (unsigned long)window_id);
             putenv(buf);
          }
#if ((EFL_VERSION_MAJOR > 1) || (EFL_VERSION_MINOR >= 24)) || ((EFL_VERSION_MAJOR == 1) && (EFL_VERSION_MINOR == 23) && (EFL_VERSION_MICRO == 99))
        eina_file_close_from(3, NULL);
#endif
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
   close(ty->slavefd);
   ty->slavefd = -1;

   ty->hand_fd = ecore_main_fd_handler_add(ty->fd, ECORE_FD_READ,
                                           _cb_fd, ty,
                                           NULL, NULL);
   /* ensure we're not missing a read */
   _cb_fd(ty, ty->hand_fd);

   _pty_size(ty);
   termpty_save_register(ty);
   return ty;
err:
   free(ty->screen);
   free(ty->screen2);
   free(ty->hl.bitmap);
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
   if (ty->fd >= 0)
     {
        close(ty->fd);
        ty->fd = -1;
     }
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
   if (ty->hand_exe_exit)
     ecore_event_handler_del(ty->hand_exe_exit);
   if (ty->hand_fd)
     ecore_main_fd_handler_del(ty->hand_fd);
   eina_stringshare_del(ty->prop.title);
   eina_stringshare_del(ty->prop.user_title);
   eina_stringshare_del(ty->prop.icon);
   termpty_backlog_free(ty);
   free(ty->screen);
   free(ty->screen2);
   if (ty->hl.links)
     {
        uint16_t i;

        for (i = 0; i < ty->hl.size; i++)
          {
             Term_Link *l = ty->hl.links + i;

             term_link_free(ty, l);
          }
       free(ty->hl.links);
     }
   free(ty->hl.bitmap);
   free(ty->buf);
   free(ty->tabs);
   ty_sb_free(&ty->write_buffer);
   free(ty);
}

void
termpty_config_update(Termpty *ty, Config *config)
{
   ty->config = config;
   termpty_backlog_size_set(ty, config->scrollback);
}

static Eina_Bool
_termpty_cell_is_empty(const Termcell *cell)
{
   return ((cell->codepoint == 0) ||
           (cell->att.invisible) ||
           ((cell->att.fg256 == 0) && (cell->att.fg == COL_INVIS))) &&
      (((cell->att.bg256 == 0) && (cell->att.bg == COL_INVIS)) || (cell->att.bg == COL_DEF));
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
   ssize_t len;

   if (!cells)
     return 0;

   for (len = nb_cells - 1; len >= 0; len--)
     {
        const Termcell *cell = cells + len;

        if (!_termpty_cell_is_empty(cell))
          return len + 1;
     }

   return 0;
}


void
termpty_text_save_top(Termpty *ty, Termcell *cells, ssize_t w_max)
{
   Termsave *ts;
   ssize_t w, i;

   if (ty->backsize == 0)
     return;
   assert(ty->back);

   termpty_backlog_lock();

   w = termpty_line_length(cells, w_max);
   for (i = 0; i < w - 1; i++)
     {
        cells[i].att.autowrapped = 1;
     }
   if (ty->backsize > 0)
     {
        ts = BACKLOG_ROW_GET(ty, 1);
        if (!ts->cells)
          goto add_new_ts;
        /* TODO: RESIZE uncompress ? */
        if (ts->w && ts->cells[ts->w - 1].att.autowrapped)
          {
             int old_len = ts->w;
             termpty_save_expand(ty, ts, cells, w);
             ty->backlog_beacon.screen_y += DIV_ROUND_UP(ts->w, ty->w)
                                          - DIV_ROUND_UP(old_len, ty->w);
             return;
          }
     }

add_new_ts:
   ts = BACKLOG_ROW_GET(ty, 0);
   ts = termpty_save_new(ty, ts, w);
   if (!ts)
     return;
   TERMPTY_CELL_COPY(ty, cells, ts->cells, w);
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

   if (!cells)
     return 0;
   if (y >= 0)
     return termpty_line_length(cells, ty->w);
   return wret;
}

void
termpty_backscroll_adjust(Termpty *ty, int *scroll)
{
   int backlog_y = ty->backlog_beacon.backlog_y;
   int screen_y = ty->backlog_beacon.screen_y;

   if ((ty->backsize == 0) || (*scroll <= 0))
     {
        *scroll = 0;
        return;
     }
   if (*scroll < screen_y)
     {
        return;
     }

   backlog_y++;
   while (42)
     {
        int nb_lines;
        Termsave *ts;

        ts = BACKLOG_ROW_GET(ty, backlog_y);
        if (!ts->cells || backlog_y >= (int)ty->backsize)
          {
             *scroll = ty->backlog_beacon.screen_y;
             return;
          }
        nb_lines = (ts->w == 0) ? 1 : DIV_ROUND_UP(ts->w, ty->w);
        screen_y += nb_lines;
        ty->backlog_beacon.screen_y = screen_y;
        ty->backlog_beacon.backlog_y = backlog_y;
        backlog_y++;
        if (*scroll <= screen_y)
          {
             return;
          }
     }
}

/* @requested_y unit is in visual lines on the screen */
static Termcell*
_termpty_cellrow_from_beacon_get(Termpty *ty, int requested_y, ssize_t *wret)
{
   int backlog_y = ty->backlog_beacon.backlog_y;
   int screen_y = ty->backlog_beacon.screen_y;
   Termsave *ts;
   int nb_lines;
   int first_loop = EINA_TRUE;

   requested_y = -requested_y;

   /* check if going from 0,0 is faster than using the beacon */
   if (screen_y - requested_y > requested_y)
     {
        ty->backlog_beacon.backlog_y = 0;
        ty->backlog_beacon.screen_y = 0;
     }

   /* going upward */
   while (requested_y >= screen_y)
     {
        ts = BACKLOG_ROW_GET(ty, backlog_y);
        if (!ts->cells || backlog_y >= (int)ty->backsize)
          {
             return NULL;
          }
        nb_lines = (ts->w == 0) ? 1 : DIV_ROUND_UP(ts->w, ty->w);

        /* Only update the beacon if working on different line than the one
         * from the beacon */
        if (!first_loop)
          {
             screen_y += nb_lines;
             ty->backlog_beacon.screen_y = screen_y;
             ty->backlog_beacon.backlog_y = backlog_y;
          }

        if ((screen_y - nb_lines < requested_y) && (requested_y <= screen_y))
          {
             /* found the line */
             int delta = screen_y - requested_y;
             *wret = ts->w - delta * ty->w;
             if (*wret > ty->w)
               *wret = ty->w;
             return &ts->cells[delta * ty->w];
          }
        backlog_y++;
        first_loop = EINA_FALSE;
     }
   /* else, going downward */
   while (requested_y <= screen_y)
     {
        ts = BACKLOG_ROW_GET(ty, backlog_y);
        if (!ts->cells)
          {
             return NULL;
          }
        nb_lines = (ts->w == 0) ? 1 : DIV_ROUND_UP(ts->w, ty->w);

        ty->backlog_beacon.screen_y = screen_y;
        ty->backlog_beacon.backlog_y = backlog_y;

        if ((screen_y - nb_lines < requested_y) && (requested_y <= screen_y))
          {
             /* found the line */
             int delta = screen_y - requested_y;
             *wret = ts->w - delta * ty->w;
             if (*wret > ty->w)
               *wret = ty->w;
             return &ts->cells[delta * ty->w];
          }
        screen_y -= nb_lines;
        backlog_y--;
     }

   return NULL;
}

/* @requested_y unit is in visual lines on the screen */
Termcell *
termpty_cellrow_get(Termpty *ty, int y_requested, ssize_t *wret)
{
   if (y_requested >= 0)
     {
        Termcell *cells = &(TERMPTY_SCREEN(ty, 0, y_requested));
        if (y_requested >= ty->h)
          return NULL;

        *wret = termpty_line_length(cells, ty->w);
        return cells;
     }
   if (!ty->back)
     return NULL;

   return _termpty_cellrow_from_beacon_get(ty, y_requested, wret);
}

/* @requested_y unit is in visual lines on the screen */
Termcell *
termpty_cell_get(Termpty *ty, int y_requested, int x_requested)
{
   ssize_t wret = 0;
   Termcell *cells;

   if (y_requested >= 0)
     {
        if (y_requested >= ty->h)
          return NULL;
        if (x_requested >= ty->w)
          return NULL;
        return &(TERMPTY_SCREEN(ty, 0, y_requested)) + x_requested;
     }
   if (!ty->back)
     return NULL;

   cells = _termpty_cellrow_from_beacon_get(ty, y_requested, &wret);
   if (!cells || x_requested >= wret)
     return NULL;
   return cells + x_requested;
}

void
termpty_write(Termpty *ty, const char *input, int len)
{
#if defined(BINARY_TYFUZZ)
   return;
#endif
   int res = ty_sb_add(&ty->write_buffer, input, len);

   if (res < 0)
     {
        ERR("failure to add %d characters to write buffer", len);
     }
   else if (ty->hand_fd)
     {
        ecore_main_fd_handler_active_set(ty->hand_fd,
                                         ECORE_FD_ERROR |
                                         ECORE_FD_READ |
                                         ECORE_FD_WRITE);
     }
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
_termpty_line_rewrap(Termpty *ty, Termcell *src_cells, int len,
                     struct screen_info *si,
                     Eina_Bool set_cursor)
{
   int autowrapped;

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

   autowrapped = src_cells[len-1].att.autowrapped;

   while (len > 0)
     {
        int copy_width = MIN(len, si->w - si->x);
        Termcell *dst_cells = &SCREEN_INFO_GET_CELLS(si, si->x, si->y);

        TERMPTY_CELL_COPY(ty,
                          /*src*/ src_cells,
                          /*dst*/ dst_cells,
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
        src_cells += copy_width;
        if (si->x >= si->w)
          {
             if ((len > 0) || (len == 0 && autowrapped))
               {
                  dst_cells = &SCREEN_INFO_GET_CELLS(si, 0, si->y);
                  dst_cells[si->w - 1].att.autowrapped = 1;
               }
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

static void
_backlog_remove_latest_nolock(Termpty *ty)
{
   Termsave *ts;
   if (ty->backsize == 0)
     return;
   ts = BACKLOG_ROW_GET(ty, 1);

   if (ty->backpos == 0)
     ty->backpos = ty->backsize - 1;
   else
     ty->backpos--;

   /* reset beacon */
   ty->backlog_beacon.screen_y = 0;
   ty->backlog_beacon.backlog_y = 0;

   termpty_save_free(ty, ts);
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

   termpty_resize_tabs(ty, old_w, new_w);

   if (effective_old_h <= ty->cursor_state.cy)
     effective_old_h = ty->cursor_state.cy + 1;

   old_y = 0;
   /* Rewrap the first line from the history if needed */
   if (ty->backsize > 0)
     {
        Termsave *ts;
        ts = BACKLOG_ROW_GET(ty, 1);
        ts = termpty_save_extract(ts);
        if (ts->cells && ts->w && ts->cells[ts->w - 1].att.autowrapped)
          {
             Termcell *cells = &(TERMPTY_SCREEN(ty, 0, old_y)),
                      *new_cells;
             int len;

             len = termpty_line_length(cells, old_w);

             new_cells = malloc((ts->w + len) * sizeof(Termcell));
             if (!new_cells)
               goto bad;
             memcpy(new_cells, ts->cells, ts->w * sizeof(Termcell));
             memcpy(new_cells + ts->w, cells, len * sizeof(Termcell));

             len+= ts->w;

             _backlog_remove_latest_nolock(ty);

             _termpty_line_rewrap(ty, new_cells, len, &new_si,
                                  old_y == ty->cursor_state.cy);

             free(new_cells);
             old_y = 1;
          }
     }
   /* For all the other lines, do not care about the history */
   for (; old_y < effective_old_h; old_y++)
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

   ty->cursor_state.cy = MAX(new_si.cy, 0);
   ty->cursor_state.cx = MAX(new_si.cx, 0);
   ty->circular_offset = new_si.circular_offset;
   ty->circular_offset2 = 0;

   ty->w = new_w;
   ty->h = new_h;
   ty->cursor_state.wrapnext = 0;

   if (altbuf)
     termpty_screen_swap(ty);

   _limit_coord(ty);

   _pty_size(ty);

   termpty_backlog_unlock();

   ty->backlog_beacon.backlog_y = 0;
   ty->backlog_beacon.screen_y = 0;

   return;

bad:
   termpty_backlog_unlock();
   free(new_screen);
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

   if (!tb)
     return;

   eina_stringshare_del(tb->path);
   eina_stringshare_del(tb->link);
   eina_stringshare_del(tb->chid);
   if (tb->obj)
     evas_object_del(tb->obj);
   EINA_LIST_FREE(tb->cmds, s)
      free(s);
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
termpty_block_id_get(const Termcell *cell, int *x, int *y)
{
   int id;

   if (!(cell->codepoint & 0x80000000)) return -1;
   id = (cell->codepoint >> 18) & 0x1fff;
   *x = (cell->codepoint >> 9) & 0x1ff;
   *y = cell->codepoint & 0x1ff;
   return id;
}

Termblock *
termpty_block_get(const Termpty *ty, int id)
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
termpty_block_chid_get(const Termpty *ty, const char *chid)
{
   Termblock *tb;

   tb = eina_hash_find(ty->block.chid_map, chid);
   return tb;
}

void
termpty_handle_block_codepoint_overwrite_heavy(Termpty *ty, int oldc, int newc)
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
termpty_cells_set_content(Termpty *ty, Termcell *cells,
                          Eina_Unicode codepoint, int count)
{
   int i;
   for (i = 0; i < count; i++)
     {
        HANDLE_BLOCK_CODEPOINT_OVERWRITE(ty, cells[i].codepoint, codepoint);
        cells[i].codepoint = codepoint;
     }
}

void
termpty_cells_att_fill_preserve_colors(Termpty *ty, Termcell *cells,
                                       Eina_Unicode codepoint, int count)
{
   int i;
   Termcell local = { .codepoint = codepoint, .att = ty->termstate.att};

   if (EINA_UNLIKELY(local.att.link_id))
     term_link_refcount_inc(ty, local.att.link_id, count);

   for (i = 0; i < count; i++)
     {
        Termatt att = cells[i].att;
        HANDLE_BLOCK_CODEPOINT_OVERWRITE(ty, cells[i].codepoint, codepoint);
        if (EINA_UNLIKELY(cells[i].att.link_id))
          term_link_refcount_dec(ty, cells[i].att.link_id, 1);

        cells[i] = local;
        if (ty->termstate.att.fg == 0 && ty->termstate.att.bg == 0)
          {
             cells[i].att.fg = att.fg;
             cells[i].att.fg256 = att.fg256;
             cells[i].att.fgintense = att.fgintense;

             cells[i].att.bg = att.bg;
             cells[i].att.bg256 = att.bg256;
             cells[i].att.bgintense = att.bgintense;
          }
     }
}


void
termpty_cell_codepoint_att_fill(Termpty *ty, Eina_Unicode codepoint,
                                Termatt att, Termcell *dst, int n)
{
   Termcell local = { .codepoint = codepoint, .att = att };
   int i;

   if (EINA_UNLIKELY(local.att.link_id))
     term_link_refcount_inc(ty, local.att.link_id, n);

   for (i = 0; i < n; i++)
     {
        HANDLE_BLOCK_CODEPOINT_OVERWRITE(ty, dst[i].codepoint, codepoint);
        if (EINA_UNLIKELY(dst[i].att.link_id))
          term_link_refcount_dec(ty, dst[i].att.link_id, 1);

        dst[i] = local;
     }
}

/* 0 means error here */
static uint16_t
_find_empty_slot(const Termpty *ty)
{
   int pos;
   int max_pos = HL_LINKS_MAX / 8;

   for (pos = 0; pos < max_pos && ty->hl.bitmap[pos] == 0xff; pos++)
     {
     }

   if (pos <= max_pos)
     {
        int bit;
        for (bit = 0; bit < 8; bit++)
          {
             if (!(ty->hl.bitmap[pos] & (1<<bit)))
               {
                  return pos * 8 + bit;
               }
          }
     }
   return 0;
}

static void
hl_bitmap_set_bit(Termpty *ty, uint16_t id)
{
   uint8_t *pos = &ty->hl.bitmap[id / 8];
   uint8_t bit = 1 << (id % 8);

   *pos |= bit;
}

static void
hl_bitmap_clear_bit(Termpty *ty, uint16_t id)
{
   uint8_t *pos = &ty->hl.bitmap[id / 8];
   uint8_t bit = 1 << (id % 8);

   *pos &= ~bit;
}

void
termpty_focus_report(Termpty *ty, Eina_Bool focus)
{
   if (!ty || !ty->focus_reporting)
     return;
   if (focus)
     TERMPTY_WRITE_STR("\033[I");
   else
     TERMPTY_WRITE_STR("\033[O");
}

Term_Link *
term_link_new(Termpty *ty)
{
   uint16_t id;
   Term_Link *link;

   /* 1st/ Find empty slot in bitmap */
   id = _find_empty_slot(ty);
   if (!id)
     {
        ERR("hyper links: can't find empty slot");
        return NULL;
     }

   /* 2nd/ Do we need to realloc? */
   if (id >= ty->hl.size)
     {
        Term_Link *links;
        uint16_t old_size = ty->hl.size;

        if (!ty->hl.size)
          ty->hl.size = 256;
        links = realloc(ty->hl.links,
                        ty->hl.size * 2 * sizeof(Term_Link));
        if (!links)
          return NULL;
        ty->hl.size *= 2;
        ty->hl.links = links;
        memset(ty->hl.links + old_size,
               0,
               (ty->hl.size - old_size) * sizeof(Term_Link));
     }

   link = ty->hl.links + id;
   link->key = NULL;
   link->url = NULL;
   link->refcount = 0;

   /* Mark in bitmap */
   hl_bitmap_set_bit(ty, id);

   return link;
}

void
term_link_free(Termpty *ty, Term_Link *link)
{
   if (!link || !ty)
     return;
   uint16_t id = (link - ty->hl.links);

   eina_stringshare_del(link->key);
   link->key = NULL;
   eina_stringshare_del(link->url);
   link->url = NULL;

   /* Remove from bitmap */
   hl_bitmap_clear_bit(ty, id);
}
