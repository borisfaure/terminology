void app_server_init(Eina_List **wins);
void app_server_shutdown(void);

void app_server_term_add(Term *term);

void app_server_term_del(Evas_Object *term);

void _app_server_win_del_request_cb(void *data, Evas_Object *obj, void *event_info);
