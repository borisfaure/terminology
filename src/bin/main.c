#ifdef HAVE_PO
#include <locale.h>
#endif
#include "private.h"

#include <Ecore_Getopt.h>
#include <Elementary.h>
#include "main.h"
#include "win.h"
#include "termio.h"
#include "termpty.h"
#include "config.h"
#include "controls.h"
#include "media.h"
#include "utils.h"
#include "ipc.h"
#include "sel.h"
#include "app_server.h"
#include "dbus.h"
#include "miniview.h"
#include "gravatar.h"
#include "keyin.h"


int _log_domain = -1;

static Config *_main_config = NULL;

Config *
main_config_get(void)
{
   return _main_config;
}


static void
main_ipc_new(Ipc_Instance *inst)
{
   Win *wn;
   Term *term;
   Config *config;
   int pargc = 0, nargc, i;
   char **pargv = NULL, **nargv = NULL, geom[256];
   Evas_Object *win;

   if (inst->startup_id)
     {
        char buf[4096];

        snprintf(buf, sizeof(buf), "DESKTOP_STARTUP_ID=%s", inst->startup_id);
        putenv(buf);
     }
   ecore_app_args_get(&pargc, &pargv);
   nargc = 1;

   if (inst->cd) nargc += 2;
   if (inst->background) nargc += 2;
   if (inst->name) nargc += 2;
   if (inst->role) nargc += 2;
   if (inst->title) nargc += 2;
   if (inst->font) nargc += 2;
   if (inst->startup_split) nargc += 2;
   if ((inst->pos) || (inst->w > 0) || (inst->h > 0)) nargc += 2;
   if (inst->login_shell) nargc += 1;
   if (inst->fullscreen) nargc += 1;
   if (inst->iconic) nargc += 1;
   if (inst->borderless) nargc += 1;
   if (inst->override) nargc += 1;
   if (inst->maximized) nargc += 1;
   if (inst->hold) nargc += 1;
   if (inst->nowm) nargc += 1;
   if (inst->xterm_256color) nargc += 1;
   if (inst->active_links) nargc += 1;
   if (inst->cmd) nargc += 2;
   
   nargv = calloc(nargc + 1, sizeof(char *));
   if (!nargv) return;
   
   i = 0;
   nargv[i++] = pargv[0];
   if (inst->cd)
     {
        nargv[i++] = "-d";
        nargv[i++] = (char *)inst->cd;
     }
   if (inst->background)
     {
        nargv[i++] = "-b";
        nargv[i++] = (char *)inst->background;
     }
   if (inst->name)
     {
        nargv[i++] = "-n";
        nargv[i++] = (char *)inst->name;
     }
   if (inst->role)
     {
        nargv[i++] = "-r";
        nargv[i++] = (char *)inst->role;
     }
   if (inst->title)
     {
        nargv[i++] = "-t";
        nargv[i++] = (char *)inst->title;
     }
   if (inst->font)
     {
        nargv[i++] = "-f";
        nargv[i++] = (char *)inst->font;
     }
   if (inst->startup_split)
     {
        nargv[i++] = "-S";
        nargv[i++] = (char *)inst->startup_split;
     }
   if ((inst->pos) || (inst->w > 0) || (inst->h > 0))
     {
        if (!inst->pos)
          snprintf(geom, sizeof(geom), "%ix%i", inst->w, inst->h);
        else
          {
             if ((inst->w > 0) && (inst->h > 0))
               {
                  if (inst->x >= 0)
                    {
                       if (inst->y >= 0)
                         snprintf(geom, sizeof(geom), "%ix%i+%i+%i",
                                  inst->w, inst->h, inst->x, inst->y);
                       else
                         snprintf(geom, sizeof(geom), "%ix%i+%i%i",
                                  inst->w, inst->h, inst->x, inst->y);
                    }
                  else
                    {
                       if (inst->y >= 0)
                         snprintf(geom, sizeof(geom), "%ix%i%i+%i",
                                  inst->w, inst->h, inst->x, inst->y);
                       else
                         snprintf(geom, sizeof(geom), "%ix%i%i%i",
                                  inst->w, inst->h, inst->x, inst->y);
                    }
               }
             else
               {
                  if (inst->x >= 0)
                    {
                       if (inst->y >= 0)
                         snprintf(geom, sizeof(geom), "+%i+%i",
                                  inst->x, inst->y);
                       else
                         snprintf(geom, sizeof(geom), "+%i%i",
                                  inst->x, inst->y);
                    }
                  else
                    {
                       if (inst->y >= 0)
                         snprintf(geom, sizeof(geom), "%i+%i",
                                  inst->x, inst->y);
                       else
                         snprintf(geom, sizeof(geom), "%i%i",
                                  inst->x, inst->y);
                    }
               }
          }
        nargv[i++] = "-g";
        nargv[i++] = geom;
     }
   if (inst->login_shell)
     {
        nargv[i++] = "-l";
     }
   if (inst->fullscreen)
     {
        nargv[i++] = "-F";
     }
   if (inst->iconic)
     {
        nargv[i++] = "-I";
     }
   if (inst->borderless)
     {
        nargv[i++] = "-B";
     }
   if (inst->override)
     {
        nargv[i++] = "-O";
     }
   if (inst->maximized)
     {
        nargv[i++] = "-M";
     }
   if (inst->hold)
     {
        nargv[i++] = "-H";
     }
   if (inst->nowm)
     {
        nargv[i++] = "-W";
     }
   if (inst->xterm_256color)
     {
        nargv[i++] = "-2";
     }
   if (inst->active_links)
     {
        nargv[i++] = "--active-links";
     }
   if (inst->cmd)
     {
        nargv[i++] = "-e";
        nargv[i++] = (char *)inst->cmd;
     }

   ecore_app_args_set(nargc, (const char **)nargv);
   wn = win_new(inst->name, inst->role, inst->title, inst->icon_name,
                _main_config, inst->fullscreen, inst->iconic,
                inst->borderless, inst->override, inst->maximized);
   if (!wn)
     {
        ecore_app_args_set(pargc, (const char **)pargv);
        free(nargv);
        return;
     }

   win = win_evas_object_get(wn);
   config = win_config_get(wn);

   unsetenv("DESKTOP_STARTUP_ID");
   if (inst->background)
     {
        eina_stringshare_replace(&(config->background), inst->background);
        config->temporary = EINA_TRUE;
     }

   if (inst->font)
     {
        if (strchr(inst->font, '/'))
          {
             char *fname = alloca(strlen(inst->font) + 1);
             char *p;
             
             strcpy(fname, inst->font);
             p = strrchr(fname, '/');
             if (p)
               {
                  int sz;
                  
                  *p = 0;
                  p++;
                  sz = atoi(p);
                  if (sz > 0) config->font.size = sz;
                  eina_stringshare_replace(&(config->font.name), fname);
               }
             config->font.bitmap = 0;
          }
        else
          {
             char buf[4096], *file;
             Eina_List *files;
             int n = strlen(inst->font);
             
             snprintf(buf, sizeof(buf), "%s/fonts", elm_app_data_dir_get());
             files = ecore_file_ls(buf);
             EINA_LIST_FREE(files, file)
               {
                  if (n > 0)
                    {
                       if (!strncasecmp(file, inst->font, n))
                         {
                            n = -1;
                            eina_stringshare_replace(&(config->font.name), file);
                            config->font.bitmap = 1;
                         }
                    }
                  free(file);
               }
          }
        config->font_set = EINA_TRUE;
        config->temporary = EINA_TRUE;
     }

   if (inst->w <= 0) inst->w = 80;
   if (inst->h <= 0) inst->h = 24;
   term = term_new(wn, config, inst->cmd, inst->login_shell,
                   inst->cd, inst->w, inst->h, inst->hold);
   if (!term)
     {
        win_free(wn);
        ecore_app_args_set(pargc, (const char **)pargv);
        free(nargv);
        return;
     }
   else
     {
        win_term_swallow(wn, term);
     }

   win_add_split(wn, term);

   main_trans_update(config);
   main_media_update(config);
   if (inst->pos)
     {
        int screen_w, screen_h;

        elm_win_screen_size_get(win, NULL, NULL, &screen_w, &screen_h);
        if (inst->x < 0) inst->x = screen_w + inst->x;
        if (inst->y < 0) inst->y = screen_h + inst->y;
        evas_object_move(win, inst->x, inst->y);
     }
   win_sizing_handle(wn);
   evas_object_show(win_evas_object_get(wn));
   if (inst->nowm)
     ecore_evas_focus_set
     (ecore_evas_ecore_evas_get(evas_object_evas_get(win)), 1);
   ecore_app_args_set(pargc, (const char **)pargv);
   free(nargv);
}

static const char *emotion_choices[] = {
  "auto", "gstreamer", "xine", "generic", "gstreamer1",
  NULL
};

static Ecore_Getopt options = {
   PACKAGE_NAME,
   "%prog [options]",
   PACKAGE_VERSION,
   gettext_noop("(C) 2012-%d Carsten Haitzler and others"),
   "BSD 2-Clause",
   gettext_noop("Terminal emulator written with Enlightenment Foundation Libraries."),
   EINA_TRUE,
   {
#if (ECORE_VERSION_MAJOR > 1) || (ECORE_VERSION_MINOR >= 8)
      ECORE_GETOPT_BREAK_STR ('e', "exec",
#else
      ECORE_GETOPT_STORE_STR ('e', "exec",
#endif
                              gettext_noop("Command to execute. Defaults to $SHELL (or passwd shell or /bin/sh)")),
      ECORE_GETOPT_STORE_STR ('d', "current-directory",
                              gettext_noop("Change to directory for execution of terminal command.")),
      ECORE_GETOPT_STORE_STR ('t', "theme",
                              gettext_noop("Use the named edje theme or path to theme file.")),
      ECORE_GETOPT_STORE_STR ('b', "background",
                              gettext_noop("Use the named file as a background wallpaper.")),
      ECORE_GETOPT_STORE_STR ('g', "geometry",
                              gettext_noop("Terminal geometry to use (eg 80x24 or 80x24+50+20 etc.).")),
      ECORE_GETOPT_STORE_STR ('n', "name",
                              gettext_noop("Set window name.")),
      ECORE_GETOPT_STORE_STR ('r', "role",
                              gettext_noop("Set window role.")),
      ECORE_GETOPT_STORE_STR ('T', "title",
                              gettext_noop("Set window title.")),
      ECORE_GETOPT_STORE_STR ('i', "icon-name",
                              gettext_noop("Set icon name.")),
      ECORE_GETOPT_STORE_STR ('f', "font",
                              gettext_noop("Set font (NAME/SIZE for scalable, NAME for bitmap.")),
      ECORE_GETOPT_STORE_STR ('S', "split",
                              gettext_noop("Split the terminal window."
                              " 'v' for vertical and 'h' for horizontal."
                              " Can be used multiple times. eg -S vhvv or --split hv"
                              " More description available on the man page.")),
      ECORE_GETOPT_CHOICE    ('v', "video-module",
                              gettext_noop("Set emotion module to use."), emotion_choices),

      ECORE_GETOPT_STORE_BOOL('l', "login",
                              gettext_noop("Run the shell as a login shell.")),
      ECORE_GETOPT_STORE_BOOL('m', "video-mute",
                              gettext_noop("Set mute mode for video playback.")),
      ECORE_GETOPT_STORE_BOOL('c', "cursor-blink",
                              gettext_noop("Set cursor blink mode.")),
      ECORE_GETOPT_STORE_BOOL('G', "visual-bell",
                              gettext_noop("Set visual bell mode.")),
      ECORE_GETOPT_STORE_TRUE('F', "fullscreen",
                              gettext_noop("Go into the fullscreen mode from the start.")),
      ECORE_GETOPT_STORE_TRUE('I', "iconic",
                              gettext_noop("Go into an iconic state from the start.")),
      ECORE_GETOPT_STORE_TRUE('B', "borderless",
                              gettext_noop("Become a borderless managed window.")),
      ECORE_GETOPT_STORE_TRUE('O', "override",
                              gettext_noop("Become an override-redirect window.")),
      ECORE_GETOPT_STORE_TRUE('M', "maximized",
                              gettext_noop("Become maximized from the start.")),
      ECORE_GETOPT_STORE_TRUE('W', "nowm",
                              gettext_noop("Terminology is run without a window manager.")),
      ECORE_GETOPT_STORE_TRUE('H', "hold",
                              gettext_noop("Do not exit when the command process exits.")),
      ECORE_GETOPT_STORE_TRUE('s', "single",
                              gettext_noop("Force single executable if multi-instance is enabled.")),
      ECORE_GETOPT_STORE_TRUE('2', "256color",
                              gettext_noop("Set TERM to 'xterm-256color' instead of 'xterm'.")),
      ECORE_GETOPT_STORE_BOOL('\0', "active-links",
                              gettext_noop("Highlight links.")),

      ECORE_GETOPT_VERSION   ('V', "version"),
      ECORE_GETOPT_COPYRIGHT ('C', "copyright"),
      ECORE_GETOPT_LICENSE   ('L', "license"),
      ECORE_GETOPT_HELP      ('h', "help"),
      ECORE_GETOPT_SENTINEL
   }
};

#if HAVE_GETTEXT && ENABLE_NLS
static void
_translate_options(void)
{
   options.copyright = eina_stringshare_printf(gettext(options.copyright),
                                               2014);

   Ecore_Getopt_Desc *desc = (Ecore_Getopt_Desc *) options.descs;
   while ((desc->shortname != '\0') || (desc->longname)
#if (ECORE_VERSION_MAJOR > 1) || (ECORE_VERSION_MINOR >= 9)
     || (desc->action == ECORE_GETOPT_ACTION_CATEGORY)
#endif
     )
     {
        if (desc->help)
          {
             switch (desc->action)
               {
                case ECORE_GETOPT_ACTION_VERSION:
                   desc->help = _("show program version.");
                   break;
                case ECORE_GETOPT_ACTION_COPYRIGHT:
                   desc->help = _("show copyright.");
                   break;
                case ECORE_GETOPT_ACTION_LICENSE:
                   desc->help = _("show license.");
                   break;
                case ECORE_GETOPT_ACTION_HELP:
                   desc->help = _("show this message.");
                   break;
                default:
                   desc->help = gettext(desc->help);
               }
          }
        desc++;
     }
}
#endif

EAPI_MAIN int
elm_main(int argc, char **argv)
{
   char *cmd = NULL;
   char *cd = NULL;
   char *theme = NULL;
   char *background = NULL;
   char *geometry = NULL;
   char *name = NULL;
   char *role = NULL;
   char *title = NULL;
   char *icon_name = NULL;
   char *font = NULL;
   char *startup_split = NULL;
   char *video_module = NULL;
   Eina_Bool login_shell = 0xff; /* unset */
   Eina_Bool video_mute = 0xff; /* unset */
   Eina_Bool cursor_blink = 0xff; /* unset */
   Eina_Bool visual_bell = 0xff; /* unset */
   Eina_Bool active_links = 0xff; /* unset */
   Eina_Bool fullscreen = EINA_FALSE;
   Eina_Bool iconic = EINA_FALSE;
   Eina_Bool borderless = EINA_FALSE;
   Eina_Bool override = EINA_FALSE;
   Eina_Bool maximized = EINA_FALSE;
   Eina_Bool nowm = EINA_FALSE;
   Eina_Bool quit_option = EINA_FALSE;
   Eina_Bool hold = EINA_FALSE;
   Eina_Bool single = EINA_FALSE;
#if (ECORE_VERSION_MAJOR > 1) || (ECORE_VERSION_MINOR >= 8)
   Eina_Bool cmd_options = EINA_FALSE;
#endif
   Eina_Bool xterm_256color = EINA_FALSE;
   Ecore_Getopt_Value values[] = {
#if (ECORE_VERSION_MAJOR > 1) || (ECORE_VERSION_MINOR >= 8)
     ECORE_GETOPT_VALUE_BOOL(cmd_options),
#else
     ECORE_GETOPT_VALUE_STR(cmd),
#endif
     ECORE_GETOPT_VALUE_STR(cd),
     ECORE_GETOPT_VALUE_STR(theme),
     ECORE_GETOPT_VALUE_STR(background),
     ECORE_GETOPT_VALUE_STR(geometry),
     ECORE_GETOPT_VALUE_STR(name),
     ECORE_GETOPT_VALUE_STR(role),
     ECORE_GETOPT_VALUE_STR(title),
     ECORE_GETOPT_VALUE_STR(icon_name),
     ECORE_GETOPT_VALUE_STR(font),
     ECORE_GETOPT_VALUE_STR(startup_split),
     ECORE_GETOPT_VALUE_STR(video_module),

     ECORE_GETOPT_VALUE_BOOL(login_shell),
     ECORE_GETOPT_VALUE_BOOL(video_mute),
     ECORE_GETOPT_VALUE_BOOL(cursor_blink),
     ECORE_GETOPT_VALUE_BOOL(visual_bell),
     ECORE_GETOPT_VALUE_BOOL(fullscreen),
     ECORE_GETOPT_VALUE_BOOL(iconic),
     ECORE_GETOPT_VALUE_BOOL(borderless),
     ECORE_GETOPT_VALUE_BOOL(override),
     ECORE_GETOPT_VALUE_BOOL(maximized),
     ECORE_GETOPT_VALUE_BOOL(nowm),
     ECORE_GETOPT_VALUE_BOOL(hold),
     ECORE_GETOPT_VALUE_BOOL(single),
     ECORE_GETOPT_VALUE_BOOL(xterm_256color),
     ECORE_GETOPT_VALUE_BOOL(active_links),

     ECORE_GETOPT_VALUE_BOOL(quit_option),
     ECORE_GETOPT_VALUE_BOOL(quit_option),
     ECORE_GETOPT_VALUE_BOOL(quit_option),
     ECORE_GETOPT_VALUE_BOOL(quit_option),

     ECORE_GETOPT_VALUE_NONE
   };
   Win *wn;
   Term *term;
   Config *config = NULL;
   Evas_Object *win;
   int args, retval = EXIT_SUCCESS;
   int remote_try = 0;
   int pos_set = 0, size_set = 0;
   int pos_x = 0, pos_y = 0;
   int size_w = 1, size_h = 1;
   Eina_List *cmds_list = NULL;

   elm_language_set("");
   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
   elm_app_compile_bin_dir_set(PACKAGE_BIN_DIR);
   elm_app_compile_lib_dir_set(PACKAGE_LIB_DIR);
   elm_app_compile_data_dir_set(PACKAGE_DATA_DIR);
#if HAVE_GETTEXT && ENABLE_NLS
   elm_app_compile_locale_set(LOCALEDIR);
#endif
   elm_app_info_set(elm_main, "terminology", "themes/default.edj");

#if HAVE_GETTEXT && ENABLE_NLS
   bindtextdomain(PACKAGE, elm_app_locale_dir_get());
   textdomain(PACKAGE);
   _translate_options();
#else
   options.copyright = "(C) 2012-2014 Carsten Haitzler and others";
#endif

   _log_domain = eina_log_domain_register("terminology", NULL);
   if (_log_domain < 0)
     {
        EINA_LOG_CRIT(_("Could not create logging domain '%s'."), "terminology");
        elm_shutdown();
        return EXIT_FAILURE;
     }

   config_init();

   _main_config = config_load("config");
   if (key_bindings_load(_main_config) < 0)
     {
        ERR(_("Could not initialize key bindings."));
        retval = EXIT_FAILURE;
        goto end;
     }

   ipc_init();

   config = config_fork(_main_config);

   args = ecore_getopt_parse(&options, values, argc, argv);
   if (args < 0)
     {
        ERR(_("Could not parse command line options."));
        retval = EXIT_FAILURE;
        goto end;
     }

   if (quit_option) goto end;

#if (ECORE_VERSION_MAJOR > 1) || (ECORE_VERSION_MINOR >= 8)
   if (cmd_options)
     {
        int i;
        Eina_Strbuf *strb;

        if (args == argc)
          {
             ERR(_("option %s requires an argument!"), argv[args-1]);
             ERR(_("invalid options found. See --help."));
             goto end;
          }

        strb = eina_strbuf_new();
        for(i = args; i < argc; i++)
          {
             eina_strbuf_append(strb, argv[i]);
             eina_strbuf_append_char(strb, ' ');
             if (startup_split)
               cmds_list = eina_list_append(cmds_list, argv[i]);
          }
        cmd = eina_strbuf_string_steal(strb);
        eina_strbuf_free(strb);
     }
#endif
   
   if (theme)
     {
        char path[PATH_MAX];
        char theme_name[PATH_MAX];
        const char *theme_path = (const char *)&path;

        if (eina_str_has_suffix(theme, ".edj"))
          eina_strlcpy(theme_name, theme, sizeof(theme_name));
        else
          snprintf(theme_name, sizeof(theme_name), "%s.edj", theme);

        if (strchr(theme_name, '/'))
          eina_strlcpy(path, theme_name, sizeof(path));
        else
          theme_path = theme_path_get(theme_name);

        eina_stringshare_replace(&(config->theme), theme_path);
        config->temporary = EINA_TRUE;
     }

   if (background)
     {
        eina_stringshare_replace(&(config->background), background);
        config->temporary = EINA_TRUE;
     }

   if (font)
     {
        if (strchr(font, '/'))
          {
             char *fname = alloca(strlen(font) + 1);
             char *p;
             
             strcpy(fname, font);
             p = strrchr(fname, '/');
             if (p)
               {
                  int sz;
                  
                  *p = 0;
                  p++;
                  sz = atoi(p);
                  if (sz > 0) config->font.size = sz;
                  eina_stringshare_replace(&(config->font.name), fname);
               }
             config->font.bitmap = 0;
          }
        else
          {
             char buf[4096], *file;
             Eina_List *files;
             int n = strlen(font);
             
             snprintf(buf, sizeof(buf), "%s/fonts", elm_app_data_dir_get());
             files = ecore_file_ls(buf);
             EINA_LIST_FREE(files, file)
               {
                  if (n > 0)
                    {
                       if (!strncasecmp(file, font, n))
                         {
                            n = -1;
                            eina_stringshare_replace(&(config->font.name), file);
                            config->font.bitmap = 1;
                         }
                    }
                  free(file);
               }
          }
        config->temporary = EINA_TRUE;
     }

   if (video_module)
     {
        int i;
        for (i = 0; i < (int)EINA_C_ARRAY_LENGTH(emotion_choices); i++)
          {
             if (video_module == emotion_choices[i])
               break;
          }

        if (i == EINA_C_ARRAY_LENGTH(emotion_choices))
          i = 0; /* ecore getopt shouldn't let this happen, but... */
        config->vidmod = i;
        config->temporary = EINA_TRUE;
     }

   if (video_mute != 0xff)
     {
        config->mute = video_mute;
        config->temporary = EINA_TRUE;
     }
   if (cursor_blink != 0xff)
     {
        config->disable_cursor_blink = !cursor_blink;
        config->temporary = EINA_TRUE;
     }
   if (visual_bell != 0xff)
     {
        config->disable_visual_bell = !visual_bell;
        config->temporary = EINA_TRUE;
     }
   if (active_links != 0xff)
     {
        config->active_links = !!active_links;
        config->temporary = EINA_TRUE;
     }

   if (xterm_256color)
     {
        config->xterm_256color = EINA_TRUE;
        config->temporary = EINA_TRUE;
     }

   if (geometry)
     {
        if (sscanf(geometry,"%ix%i+%i+%i", &size_w, &size_h, &pos_x, &pos_y) == 4)
          {
             pos_set = 1;
             size_set = 1;
          }
        else if (sscanf(geometry,"%ix%i-%i+%i", &size_w, &size_h, &pos_x, &pos_y) == 4)
          {
             pos_x = -pos_x;
             pos_set = 1;
             size_set = 1;
          }
        else if (sscanf(geometry,"%ix%i-%i-%i", &size_w, &size_h, &pos_x, &pos_y) == 4)
          {
             pos_x = -pos_x;
             pos_y = -pos_y;
             pos_set = 1;
             size_set = 1;
          }
        else if (sscanf(geometry,"%ix%i+%i-%i", &size_w, &size_h, &pos_x, &pos_y) == 4)
          {
             pos_y = -pos_y;
             pos_set = 1;
             size_set = 1;
          }
        else if (sscanf(geometry,"%ix%i", &size_w, &size_h) == 2)
          {
             size_set = 1;
          }
        else if (sscanf(geometry,"+%i+%i", &pos_x, &pos_y) == 2)
          {
             pos_set = 1;
          }
        else if (sscanf(geometry,"-%i+%i", &pos_x, &pos_y) == 2)
          {
             pos_x = -pos_x;
             pos_set = 1;
          }
        else if (sscanf(geometry,"+%i-%i", &pos_x, &pos_y) == 2)
          {
             pos_y = -pos_y;
             pos_set = 1;
          }
        else if (sscanf(geometry,"-%i-%i", &pos_x, &pos_y) == 2)
          {
             pos_x = -pos_x;
             pos_y = -pos_y;
             pos_set = 1;
          }
     }
   
   if (!size_set)
     {
        if (config->custom_geometry)
          {
             size_w = config->cg_width;
             size_h = config->cg_height;
          }
        else
          {
             size_w = 80;
             size_h = 24;
          }
     }

   if (login_shell != 0xff)
     {
        config->login_shell = login_shell;
        config->temporary = EINA_TRUE;
     }
   login_shell = config->login_shell;

   elm_theme_overlay_add(NULL, config_theme_path_default_get(config));
   elm_theme_overlay_add(NULL, config_theme_path_get(config));

remote:
   if ((!single) && (config->multi_instance))
     {
        Ipc_Instance inst;
        char cwdbuf[4096];
        
        memset(&inst, 0, sizeof(Ipc_Instance));
        
        inst.cmd = cmd;
        if (cd) inst.cd = cd;
        else inst.cd = getcwd(cwdbuf, sizeof(cwdbuf));
        inst.background = background;
        inst.name = name;
        inst.role = role;
        inst.title = title;
        inst.icon_name = icon_name;
        inst.font = font;
        inst.startup_id = getenv("DESKTOP_STARTUP_ID");
        inst.x = pos_x;
        inst.y = pos_y;
        inst.w = size_w;
        inst.h = size_h;
        inst.pos = pos_set;
        inst.login_shell = login_shell;
        inst.fullscreen = fullscreen;
        inst.iconic = iconic;
        inst.borderless = borderless;
        inst.override = override;
        inst.maximized = maximized;
        inst.hold = hold;
        inst.nowm = nowm;
        inst.startup_split = startup_split;
        if (ipc_instance_add(&inst))
          goto end;
     }
   if ((!single) && (config->multi_instance))
     {
        ipc_instance_new_func_set(main_ipc_new);
        if (!ipc_serve())
          {
             if (remote_try < 1)
               {
                  remote_try++;
                  goto remote;
               }
          }
     }

   wn = win_new(name, role, title, icon_name, config,
                fullscreen, iconic, borderless, override, maximized);
   // set an env so terminal apps can detect they are in terminology :)
   putenv("TERMINOLOGY=1");
   unsetenv("DESKTOP_STARTUP_ID");

   config_del(config);
   config = NULL;
   if (!wn)
     {
        retval = EXIT_FAILURE;
        goto end;
     }

   config = win_config_get(wn);
#if 0
   if (config->application_server)
     app_server_init(&wins, config->application_server_restore_views);
#endif

   term = term_new(wn, config, cmd, login_shell, cd,
                   size_w, size_h, hold);
   if (!term)
     {
        retval = EXIT_FAILURE;
        goto end;
     }
   else
     {
        win_term_swallow(wn, term);
     }

   win_add_split(wn, term);

   main_trans_update(config);
   main_media_update(config);
   win_sizing_handle(wn);
   win = win_evas_object_get(wn);
   evas_object_show(win);
   if (startup_split)
     {
        /* TODO: boris */
#if 0
        unsigned int i = 0;
        void *pch = NULL;
        Term *next = term;

        for (i=0; i<strlen(startup_split); i++)
          {
             if (startup_split[i] == 'v')
               {
                  pch = eina_list_nth(cmds_list, 1);
                  main_split_v(next->wn->win, next->term, pch);
                  cmds_list = eina_list_remove(cmds_list, pch);
               }
             else if (startup_split[i] == 'h')
               {
                  pch = eina_list_nth(cmds_list, 1);
                  main_split_h(next->wn->win, next->term, pch);
                  cmds_list = eina_list_remove(cmds_list, pch);
               }
             else if (startup_split[i] == '-')
               next = _term_next_get(next);
             else
               {
                  ERR(_("invalid argument found for option -S/--split. See --help."));
                  goto end;
               }
          }
        if (cmds_list) eina_list_free(cmds_list);
#endif
     }
   if (pos_set)
     {
        int screen_w, screen_h;

        elm_win_screen_size_get(win, NULL, NULL, &screen_w, &screen_h);
        if (pos_x < 0) pos_x = screen_w + pos_x;
        if (pos_y < 0) pos_y = screen_h + pos_y;
        evas_object_move(win, pos_x, pos_y);
     }
   if (nowm)
      ecore_evas_focus_set(ecore_evas_ecore_evas_get(
            evas_object_evas_get(win)), 1);

   ty_dbus_init();

   elm_run();

   app_server_shutdown();

   ty_dbus_shutdown();
   config = NULL;
 end:
#if (ECORE_VERSION_MAJOR > 1) || (ECORE_VERSION_MINOR >= 8)
   free(cmd);
#endif
   if (config)
     {
        config_del(config);
        config = NULL;
     }

   ipc_shutdown();

   termpty_shutdown();
   miniview_shutdown();
   gravatar_shutdown();

   windows_free();

   config_del(_main_config);
   key_bindings_shutdown();
   config_shutdown();
   eina_log_domain_unregister(_log_domain);
   _log_domain = -1;

#if HAVE_GETTEXT && ENABLE_NLS
   eina_stringshare_del(options.copyright);
#endif

   elm_shutdown();

   return retval;
}
ELM_MAIN()
