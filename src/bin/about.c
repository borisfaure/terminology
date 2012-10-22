#include "private.h"

#include <Elementary.h>
#include "about.h"

static Evas_Object *ab_layout = NULL, *ab_over = NULL;
static Eina_Bool ab_out = EINA_FALSE;
static Ecore_Timer *ab_del_timer = NULL;
static Evas_Object *saved_win = NULL;
static Evas_Object *saved_bg = NULL;

static Eina_Bool
_cb_ab_del_delay(void *data __UNUSED__)
{
   evas_object_del(ab_layout);
   ab_layout = NULL;
   ab_del_timer = NULL;
   elm_cache_all_flush();
   return EINA_FALSE;
}

static void
_cb_mouse_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *ev __UNUSED__)
{
   about_toggle(saved_win, saved_bg, data);
}

void
about_toggle(Evas_Object *win, Evas_Object *bg, Evas_Object *term)
{
   Evas_Object *o;

   saved_win = win;
   saved_bg = bg;
   if (!ab_layout)
     {
        char buf[PATH_MAX];
        
        ab_layout = o = elm_layout_add(win);
        snprintf(buf, sizeof(buf), "%s/themes/default.edj",
                 elm_app_data_dir_get());
        elm_layout_file_set(o, buf, "terminology/about");
        elm_object_part_text_set
          (o, "terminology.text",
              "<b>Terminology</b><br>"
              "Why should terminals be boring?<br>"
              "<br>"
              "This terminal was written for Enlightenment, to use EFL "
              "and otherwise push the boundaries of what a modern terminal "
              "emulator should be. We hope you enjoy it.<br>"
              "<br>"
              "Copyright © 2012 by:<br>"
              "<br>"
              "Carsten Haitzler<br>"
              "Anisse Astier<br>"
              "Cedric Bail<br>"
              "Daniel Juyung Seo<br>"
              "Nicholas Hughart<br>"
              "Davide Andreoli<br>"
              "Boris Faure<br>"
              "Gustavo Sverzut Barbieri<br>"
              "Christopher Michael<br>"
              "Tom Hacohen<br>"
              "Sebastian Dransfeld<br>"
              "Mike McCormack<br>"
              "Jihoon Kim<br>"
              "Samuel F. Baggen<br>"
              "Rafael Antognolli<br>"
              "Jerome Pinot<br>"
              "<br>"
              "<br>"
              "Distributed under the 2-clause BSD license detailed below:<br>"
              "<br>"
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
              "DAMAGE.</b>"
          );
        evas_object_show(o);
        edje_object_part_swallow(bg, "terminology.about", ab_layout);
     }
   if (!ab_out)
     {
        ab_over = o = evas_object_rectangle_add(evas_object_evas_get(win));
        evas_object_color_set(o, 0, 0, 0, 0);
        edje_object_part_swallow(bg, "terminology.dismiss", o);
        evas_object_show(o);
        evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                       _cb_mouse_down, term);
        
        edje_object_signal_emit(bg, "about,show", "terminology");
        elm_object_signal_emit(ab_layout, "begin" ,"terminology");
        ab_out = EINA_TRUE;
        elm_object_focus_set(ab_layout, EINA_TRUE);
        if (ab_del_timer)
          {
             ecore_timer_del(ab_del_timer);
             ab_del_timer = NULL;
          }
     }
   else
     {
        evas_object_del(ab_over);
        ab_over = NULL;
        
        edje_object_signal_emit(bg, "about,hide", "terminology");
        ab_out = EINA_FALSE;
        elm_object_focus_set(ab_layout, EINA_FALSE);
        elm_object_focus_set(term, EINA_TRUE);
        if (ab_del_timer) ecore_timer_del(ab_del_timer);
        ab_del_timer = ecore_timer_add(10.0,  _cb_ab_del_delay, NULL);
     }
}
