#include "private.h"

#include <Elementary.h>

#include "main.h"
#include "termio.h"
#include "app_server_eet.h"

#if (ELM_VERSION_MAJOR > 1) || (ELM_VERSION_MINOR >= 10)

#ifndef ELM_APP_SERVER_VIEW_EVENT_CLOSED
#define ELM_APP_SERVER_VIEW_EVENT_CLOSED    ELM_APP_SERVER_VIEW_EV_CLOSED
#endif

#ifndef ELM_APP_SERVER_VIEW_EVENT_RESUMED
#define ELM_APP_SERVER_VIEW_EVENT_RESUMED   ELM_APP_SERVER_VIEW_EV_RESUMED
#endif

#ifndef ELM_APP_SERVER_VIEW_EVENT_SAVE
#define ELM_APP_SERVER_VIEW_EVENT_SAVE      ELM_APP_SERVER_VIEW_EV_SAVE
#endif

#ifndef ELM_APP_SERVER_EVENT_TERMINATE
#define ELM_APP_SERVER_EVENT_TERMINATE      ELM_APP_SERVER_EV_TERMINATE
#endif

static Elm_App_Server *_server = NULL;
static Eina_Bool _ignore_term_add = EINA_FALSE;
static Terminology_Item *views_eet = NULL;

static void
_user_config_file_path_build(char *dir, unsigned int size, const char *id)
{
   const char *home = getenv("HOME");

   if (!home)
     home = "";

   snprintf(dir, size, "%s/.terminology/", home);
   if (!ecore_file_is_dir(dir))
     ecore_file_mkpath(dir);

   snprintf(dir, size, "%s/.terminology/%s", home, id);
}

void
app_server_term_del(Evas_Object *term)
{
   Elm_App_Server_View *view;
   const char *id = NULL;

   view = evas_object_data_del(term, "app_view");
   if (!view)
     return;

   eo_do(view, id = elm_app_server_view_id_get());
   terminology_item_term_entries_del(views_eet, id);

   eo_del(view);
}

static Eina_Bool
_view_closed_cb(void *data, Eo *view,
                const Eo_Event_Description *desc EINA_UNUSED,
                void *event_info EINA_UNUSED)
{
   Term *term = data;
   const char *id = NULL;

   if (term)
     {
        Evas_Object *term_object;

        term_object = main_term_evas_object_get(term);
        evas_object_data_del(term_object, "app_view");
        main_close(main_win_evas_object_get(main_term_win_get(term)),
                   term_object);
     }

   eo_do(view, id = elm_app_server_view_id_get());
   terminology_item_term_entries_del(views_eet, id);

   eo_del(view);
   return EINA_TRUE;
}

static void
_term_title_changed_cb(void *data, Evas_Object *obj,
                       void *event_info EINA_UNUSED)
{
   const char *title = termio_title_get(obj);
   eo_do(data, elm_app_server_view_title_set(title));
}

static void
_term_icon_changed_cb(void *data, Evas_Object *obj,
                      void *event_info EINA_UNUSED)
{
   const char *icon = termio_icon_name_get(obj);
   eo_do(data, elm_app_server_view_icon_set(icon));
}

static Eina_Bool
_view_save_cb(void *data EINA_UNUSED,
              Eo *view,
              const Eo_Event_Description *desc EINA_UNUSED,
              void *event_info EINA_UNUSED)
{
   char dir[PATH_MAX];
   Evas_Object *term_object;
   const char *id = NULL;
   Term_Item *term_eet;

   term_object = main_term_evas_object_get(data);

   /*
    * if we call elm_app_server_save() in some case that the terminology
    * will continue run, this data_del will lead to issues.
    */
   evas_object_data_del(term_object, "app_view");

   termio_cwd_get(term_object, dir, sizeof(dir));
   eo_do(view, id = elm_app_server_view_id_get());

   term_eet = terminology_item_term_entries_get(views_eet, id);
   if (term_eet)
     {
        term_item_dir_set(term_eet, dir);
        return EINA_TRUE;
     }

   term_eet = term_item_new(id, dir);
   terminology_item_term_entries_add(views_eet, id, term_eet);

   return EINA_TRUE;
}

static Eina_Bool
_view_resumed_cb(void *data, Eo *view,
                 const Eo_Event_Description *desc EINA_UNUSED,
                 void *event_info EINA_UNUSED)
{
   Term *term = data;
   Win *wn;
   Eina_List **wins = NULL;
   const char *title, *id = NULL;
   Evas_Object *term_object;
   const char *dir = NULL;
   Term_Item *term_eet;

   if (term)
     {
        main_term_focus(term);
        return EINA_TRUE;
     }

   eo_do(_server, wins = eo_key_data_get("wins"));
   if (!wins || !(wn = eina_list_data_get(*wins)))
     {
        ERR("There is no window open");
        return EINA_TRUE;
     }

   term = eina_list_data_get(main_win_terms_get(wn));

   eo_do(view, id = elm_app_server_view_id_get());
   term_eet = terminology_item_term_entries_get(views_eet, id);
   if (term_eet)
     {
        dir = term_item_dir_get(term_eet);
        //not valid data saved
        if (!dir || dir[0] != '/')
          {
             terminology_item_term_entries_del(views_eet, id);
             dir = NULL;
          }
     }

   _ignore_term_add = EINA_TRUE;
   if (dir)
     main_new_with_dir(main_win_evas_object_get(wn),
                        main_term_evas_object_get(term), dir);
   else
     main_new(main_win_evas_object_get(wn), main_term_evas_object_get(term));
   _ignore_term_add = EINA_FALSE;

   //just add term
   term = eina_list_last_data_get(main_win_terms_get(wn));
   term_object = main_term_evas_object_get(term);
   title = termio_title_get(term_object);

   evas_object_data_set(term_object, "app_view", view);

   eo_do(view, elm_app_server_view_title_set(title),
         elm_app_server_view_window_set(
                  main_win_evas_object_get(main_term_win_get(term))),
         eo_event_callback_del(ELM_APP_SERVER_VIEW_EVENT_CLOSED, _view_closed_cb,
                               NULL),
         eo_event_callback_del(ELM_APP_SERVER_VIEW_EVENT_RESUMED, _view_resumed_cb,
                               NULL),
         eo_event_callback_add(ELM_APP_SERVER_VIEW_EVENT_CLOSED, _view_closed_cb,
                               term),
         eo_event_callback_add(ELM_APP_SERVER_VIEW_EVENT_RESUMED, _view_resumed_cb,
                               term),
         eo_event_callback_add(ELM_APP_SERVER_VIEW_EVENT_SAVE, _view_save_cb,
                               term));

   evas_object_smart_callback_add(term_object, "title,change",
                                  _term_title_changed_cb, view);
   evas_object_smart_callback_add(term_object, "icon,change",
                                  _term_icon_changed_cb, term);

   return EINA_TRUE;
}

static Eina_Bool
_server_terminate_cb(void *data, Eo *obj EINA_UNUSED,
                     const Eo_Event_Description *desc EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Eina_List **wins = data, *l, *l2;
   Win *wn;

   EINA_LIST_FOREACH_SAFE(*wins, l, l2, wn)
     evas_object_del(main_win_evas_object_get(wn));

   return EINA_TRUE;
}

void
app_server_shutdown(void)
{
   char lock_file[PATH_MAX];

   if (!_server)
     return;

   _user_config_file_path_build(lock_file, sizeof(lock_file), ".lock");
   ecore_file_remove(lock_file);

   eo_do(_server, elm_app_server_save());

   if (views_eet)
     {
        char eet_dir[PATH_MAX];

        _user_config_file_path_build(eet_dir, sizeof(eet_dir), "terms.eet");
        terminology_item_save(views_eet, eet_dir);

        terminology_item_free(views_eet);
     }
   app_server_eet_shutdown();

   eo_unref(_server);
   _server = NULL;
}

void
app_server_win_del_request_cb(void *data EINA_UNUSED,
                              Evas_Object *obj EINA_UNUSED,
                              void *event_info EINA_UNUSED)
{
   Eina_List **wins = NULL;

   if (!_server)
     return;

   eo_do(_server, wins = eo_key_data_get("wins"));

   if (wins && eina_list_count(*wins) > 1)
     return;

   /*
    * this way the terms of view are already alive
    * and we can get pwd and backlog
    */
   app_server_shutdown();
}

static Elm_App_Server_View *
_app_server_term_add(Term *term)
{
   Elm_App_Server_View *view;
   const char *title;
   Evas_Object *term_object;

   if (_ignore_term_add)
     return NULL;

   view = eo_add_custom(ELM_APP_SERVER_VIEW_CLASS, _server,
                        elm_app_server_view_constructor(NULL));

   term_object = main_term_evas_object_get(term);

   title = termio_title_get(term_object);

   eo_do(view, elm_app_server_view_title_set(title),
         elm_app_server_view_window_set(
                  main_win_evas_object_get(main_term_win_get(term))),
         elm_app_server_view_resume(),
         eo_event_callback_add(ELM_APP_SERVER_VIEW_EVENT_CLOSED,
                               _view_closed_cb, term),
         eo_event_callback_add(ELM_APP_SERVER_VIEW_EVENT_RESUMED,
                               _view_resumed_cb, term),
         eo_event_callback_add(ELM_APP_SERVER_VIEW_EVENT_SAVE,
                               _view_save_cb, term));

   evas_object_smart_callback_add(term_object, "title,change",
                                  _term_title_changed_cb, view);
   evas_object_smart_callback_add(term_object, "icon,change",
                                  _term_icon_changed_cb, term);
   evas_object_data_set(term_object, "app_view", view);

   return view;
}

void
app_server_term_add(Term *term)
{
   Elm_App_Server_View *view;

   if (!_server)
     return;

   view = _app_server_term_add(term);
   if (!view)
     return;

   eo_do(_server, elm_app_server_view_add(view));
}

static Elm_App_Server_View *
_app_server_create_view_cb(Elm_App_Server *server, const Eina_Value *args EINA_UNUSED,
                           Eina_Stringshare **error_name,
                           Eina_Stringshare **error_message EINA_UNUSED)
{
   Win *wn;
   Term *term;
   Eina_List **wins = NULL;

   eo_do(server, wins = eo_key_data_get("wins"));
   if (!wins || !(wn = eina_list_data_get(*wins)))
     {
        *error_name = eina_stringshare_add(_("There is no window open"));
        ERR("%s", *error_name);
        return NULL;
     }
   term = eina_list_data_get(main_win_terms_get(wn));

   _ignore_term_add = EINA_TRUE;
   main_new(main_win_evas_object_get(wn), main_term_evas_object_get(term));
   _ignore_term_add = EINA_FALSE;

   //Term just added by main_new()
   term = eina_list_last_data_get(main_win_terms_get(wn));

   return _app_server_term_add(term);
}

static Eina_Bool
_restore_view_cb(void *data)
{
   Elm_App_Server_View *view = data;
   eo_do(view, elm_app_server_view_resume());
   return EINA_FALSE;
}

void
app_server_init(Eina_List **wins, Eina_Bool restore_views)
{
   Win *wn;
   Eina_Iterator *views = NULL;
   Elm_App_Server_View *view;
   const char *title;
   char lock_file[PATH_MAX], eet_dir[PATH_MAX];
   FILE *f;

   if (!wins)
     return;
   wn = eina_list_data_get(*wins);
   if (!wn)
     return;

   //we only can have one instance of Terminology running app_server
   _user_config_file_path_build(lock_file, sizeof(lock_file), ".lock");
   if (ecore_file_exists(lock_file))
     return;

   //create lock file
   f = fopen(lock_file, "w");
   if (!f)
     return;
   fprintf(f, "locked");
   fclose(f);

   app_server_eet_init();
   _user_config_file_path_build(eet_dir, sizeof(eet_dir), "terms.eet");
   views_eet = terminology_item_load(eet_dir);
   if (!views_eet)
     views_eet = terminology_item_new(1);

   title = elm_win_title_get(main_win_evas_object_get(wn));


   _server = eo_add_custom(ELM_APP_SERVER_CLASS, NULL,
                           elm_app_server_constructor(
                              "org.enlightenment.Terminology",
                              _app_server_create_view_cb));

   eo_do(_server, elm_app_server_title_set(title),
         eo_key_data_set("wins", wins, NULL),
         views = elm_app_server_views_get(),
         eo_event_callback_add(ELM_APP_SERVER_EVENT_TERMINATE,
                               _server_terminate_cb, wins));
   //views saved
   EINA_ITERATOR_FOREACH(views, view)
     {
        if (restore_views)
          ecore_idler_add(_restore_view_cb, view);
        eo_do(view,
              eo_event_callback_add(ELM_APP_SERVER_VIEW_EVENT_CLOSED,
                                    _view_closed_cb, NULL),
              eo_event_callback_add(ELM_APP_SERVER_VIEW_EVENT_RESUMED,
                                    _view_resumed_cb, NULL));
     }
   eina_iterator_free(views);
}


#else

void app_server_init(Eina_List **wins EINA_UNUSED,
                     Eina_Bool restore_views EINA_UNUSED) {}
void app_server_shutdown(void) {}
void app_server_term_add(Term *term EINA_UNUSED) {}
void app_server_term_del(Evas_Object *term EINA_UNUSED) {}
void app_server_win_del_request_cb(void *data EINA_UNUSED,
                                   Evas_Object *obj EINA_UNUSED,
                                   void *event_info EINA_UNUSED) {}

#endif
