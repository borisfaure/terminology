#include "private.h"

#include <Elementary.h>
#include <assert.h>
#include "about.h"
#include "config.h"
#include "termio.h"

#define TWITTER_HANDLE  "@_Terminology_"

typedef struct _about_ctx {
     Evas_Object *layout;
     Evas_Object *over;
     Evas_Object *win;
     Evas_Object *base;
     Evas_Object *term;
     Evas_Object *ctxpopup;
     void (*donecb) (void *data);
     void *donedata;
     Config *config;
} About_Ctx;


static Eina_Bool
_cb_del_delay(void *data)
{
   Evas_Object *layout = data;
   evas_object_del(layout);
   elm_cache_all_flush();
   return EINA_FALSE;
}

static void
_run_url(const About_Ctx *ctx,
         const char *url)
{
   char buf[PATH_MAX];
   const char *cmd = "xdg-open";

   if (ctx->config && ctx->config->helper.url.general &&
       ctx->config->helper.url.general[0])
     cmd = ctx->config->helper.url.general;

   snprintf(buf, sizeof(buf), "%s %s", cmd, url);
   ecore_exe_run(buf, NULL);
}

static void
_cb_ctxpopup_del(void *data,
                 Evas *_e EINA_UNUSED,
                 Evas_Object *_obj EINA_UNUSED,
                 void *_event EINA_UNUSED)
{
   About_Ctx *ctx = data;

   ctx->ctxpopup = NULL;
}

static void
_cb_ctxpopup_dismissed(void *data,
                       Evas_Object *obj,
                       void *_event EINA_UNUSED)
{
   About_Ctx *ctx = data;

   ctx->ctxpopup = NULL;
   evas_object_del(obj);
}

static void
_cb_ctxpopup_copy_twitter(void *data,
                          Evas_Object *obj,
                          void *_event EINA_UNUSED)
{
   About_Ctx *ctx = data;

   elm_cnp_selection_set(ctx->win, ELM_SEL_TYPE_CLIPBOARD,
                         ELM_SEL_FORMAT_TEXT,
                         TWITTER_HANDLE,
                         strlen(TWITTER_HANDLE));
   ctx->ctxpopup = NULL;
   evas_object_del(obj);
}

static void
_cb_ctxpopup_open_twitter_as_url(void *data,
                                 Evas_Object *obj,
                                 void *_event EINA_UNUSED)
{
   About_Ctx *ctx = data;

   _run_url(ctx, "https://twitter.com/_Terminology_");

   ctx->ctxpopup = NULL;
   evas_object_del(obj);
}



static void
_cb_twitter(void *data,
            Evas_Object *_obj EINA_UNUSED,
            const char *_sig EINA_UNUSED,
            const char *_src EINA_UNUSED)
{
   About_Ctx *ctx = data;

   _run_url(ctx, "https://twitter.com/_Terminology_");
}

static void
_cb_twitter_ctx(void *data,
                Evas_Object *_obj EINA_UNUSED,
                const char *_sig EINA_UNUSED,
                const char *_src EINA_UNUSED)
{
   About_Ctx *ctx = data;
   Evas_Object *popup;
   const char *fmt;
   Evas *e = evas_object_evas_get(ctx->base);
   Evas_Coord x;
   Evas_Coord y;

   evas_pointer_canvas_xy_get(e, &x, &y);

   popup = elm_ctxpopup_add(ctx->win);
   ctx->ctxpopup = popup;
   fmt = eina_stringshare_printf(_("Copy '%s'"),
                                 TWITTER_HANDLE);

   elm_ctxpopup_item_append(popup, fmt, NULL,
                            _cb_ctxpopup_copy_twitter, ctx);
   elm_ctxpopup_item_append(popup, _("Open"), NULL,
                            _cb_ctxpopup_open_twitter_as_url, ctx);
   evas_object_move(popup, x, y);
   evas_object_show(popup);
   evas_object_smart_callback_add(popup, "dismissed",
                                  _cb_ctxpopup_dismissed, ctx);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL,
                                  _cb_ctxpopup_del, ctx);
}

static void
_cb_mouse_down(void *data,
               Evas *_e EINA_UNUSED,
               Evas_Object *_obj EINA_UNUSED,
               void *_ev EINA_UNUSED)
{
   About_Ctx *ctx = data;

   if (ctx->over)
     {
        evas_object_del(ctx->over);
     }
   elm_object_focus_set(ctx->layout, EINA_FALSE);
   elm_layout_signal_emit(ctx->base, "about,hide", "terminology");

   ecore_timer_add(10.0, _cb_del_delay, ctx->layout);
   ctx->layout = NULL;

   if (ctx->donecb)
     ctx->donecb(ctx->donedata);
   elm_layout_signal_callback_del(ctx->base, "about,twitter", "*",
                                  _cb_twitter);
   elm_layout_signal_callback_del(ctx->base, "about,twitter,ctx", "*",
                                  _cb_twitter_ctx);

   free(ctx);
}

void
about_show(Evas_Object *win, Evas_Object *base, Evas_Object *term,
             void (*donecb) (void *data), void *donedata)
{
   Evas_Object *o;
   About_Ctx *ctx;
   Config *config = termio_config_get(term);
   const char *txt;

   ctx = malloc(sizeof(*ctx));
   assert(ctx);

   ctx->win = win;
   ctx->base = base;
   ctx->term = term;
   ctx->donecb = donecb;
   ctx->donedata = donedata;
   ctx->config = config;

   elm_object_part_text_set(base, "twitter.txt",
                            _("Twitter: @_Terminology_"));

   ctx->layout = o = elm_layout_add(win);
   if (elm_layout_file_set(o, config_theme_path_get(config),
                           "terminology/about") == 0)
     {
        char buf[PATH_MAX];

        snprintf(buf, sizeof(buf), "%s/themes/default.edj",
                 elm_app_data_dir_get());
        elm_layout_file_set(o, buf, "terminology/about");
     }

   txt = eina_stringshare_printf(_(
         "<b>Terminology %s</b><br>"
         "Why should terminals be boring?<br>"
         "<br>"
         "This terminal was written for Enlightenment, to use EFL "
         "and otherwise push the boundaries of what a modern terminal "
         "emulator should be. We hope you enjoy it.<br>"
         "<br>"
         "Copyright © 2012-%d by:<br>"
         "<br>"
         "%s" // AUTHORS
         "<br>"
         "<br>"
         "Distributed under the 2-clause BSD license detailed below:<br>"
         "<br>"
         "%s" // LICENSE
         ),
      PACKAGE_VERSION, 2020,
      "Boris Faure<br>"
      "Carsten Haitzler<br>"
      "Gustavo Sverzut Barbieri<br>"
      "Cedric BAIL<br>"
      "Sebastian Dransfeld<br>"
      "Wonguk Jeong<br>"
      "Christopher Michael<br>"
      "Daniel Juyung Seo<br>"
      "Panagiotis Galatsanos<br>"
      "Mike Blumenkrantz<br>"
      "Aleksandar Popadić<br>"
      "Massimo Maiurana<br>"
      "Stefan Schmidt<br>"
      "Davide Andreoli<br>"
      "Gustavo Lima Chaves<br>"
      "Jean-Philippe ANDRÉ<br>"
      "Tom Hacohen<br>"
      "Alex-P. Natsios<br>"
      "Lee Gwang-O<br>"
      "Jean Guyomarc'h<br>"
      "Jihoon Kim<br>"
      "Kai Huuhko<br>"
      "Mike McCormack<br>"
      "Iván Briano<br>"
      "Jerome Pinot<br>"
      "José Roberto de Souza<br>"
      "Leandro Pereira<br>"
      "Leif Middelschulte<br>"
      "Markus Törnqvist<br>"
      "Thibaut Broggi<br>"
      "Lucas De Marchi<br>"
      "Marcel Hollerbach<br>"
      "Anisse Astier<br>"
      "Daniel Zaoui<br>"
      "Doug Newgard<br>"
      "Flavio Vinicius Alvares Ceolin<br>"
      "Samuel F. Baggen<br>"
      "Amitesh Singh<br>"
      "Anthony F McInerney<br>"
      "Aurélien Larcher<br>"
      "Bruno Dilly<br>"
      "Conrad Meyer<br>"
      "Daniel Kolesa<br>"
      "Eduardo Lima<br>"
      "Flavio Ceolin<br>"
      "Jason L. Cook<br>"
      "Jérémy Anger<br>"
      "Michael BOUCHAUD<br>"
      "Michael Jennings<br>"
      "Nicholas Hughart<br>"
      "Rafael Antognolli<br>"
      "Rui Seabra<br>"
      "Sanjeev BA<br>"
      "Theodor van Nahl<br>"
      "Vincent Torri<br>"
      "tantSinnister<br>",
      "All rights reserved.<br>"
         "<br>"
         "Redistribution and use in source and binary forms, with or "
         "without modification, are permitted provided that the "
         "following conditions are met:<br>"
         "<br>"
         "1. Redistributions of source code must retain the above "
         "copyright notice, this list of conditions and the following "
         "disclaimer.<br>"
         "2. Redistributions in binary form must reproduce the above "
         "copyright notice, this list of conditions and the following "
         "disclaimer in the documentation and/or other materials "
         "provided with the distribution.<br>"
         "<br>"
         "<b>THIS SOFTWARE IS PROVIDED \"AS IS\" AND ANY EXPRESS OR "
         "IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED "
         "WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR "
         "PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER "
         "OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, "
         "INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES "
         "(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE "
         "GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS "
         "INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, "
         "WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING "
         "NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF "
         "THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH "
         "DAMAGE.</b>");
   elm_object_part_text_set(o, "terminology.text", txt);
   eina_stringshare_del(txt);
   evas_object_show(o);
   elm_layout_content_set(base, "terminology.about", ctx->layout);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _cb_mouse_down, ctx);

   ctx->over = o = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_color_set(o, 0, 0, 0, 0);
   elm_layout_content_set(base, "terminology.dismiss", o);
   evas_object_show(o);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _cb_mouse_down, ctx);

   elm_layout_signal_callback_add(base, "about,twitter", "*",
                                  _cb_twitter, ctx);
   elm_layout_signal_callback_add(base, "about,twitter,ctx", "*",
                                  _cb_twitter_ctx, ctx);

   elm_layout_signal_emit(base, "about,show", "terminology");
   elm_object_signal_emit(ctx->layout, "begin" ,"terminology");
   elm_object_focus_set(ctx->layout, EINA_TRUE);
}
