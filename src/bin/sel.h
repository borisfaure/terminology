#ifndef _SEL_H__
#define _SEL_H__ 1

#include "config.h"

Evas_Object *sel_add(Evas_Object *parent);
void *sel_entry_add(Evas_Object *obj, Evas_Object *entry, Eina_Bool selected, Eina_Bool bell, Config *config);
void sel_entry_title_set(void *entry, const char *title);
void sel_entry_close(void *entry);
void sel_entry_update(void *entry);
void sel_go(Evas_Object *obj);
void sel_entry_selected_set(Evas_Object *obj, Evas_Object *entry, Eina_Bool keep_before);
void sel_zoom(Evas_Object *obj, double zoom);
void sel_orig_zoom_set(Evas_Object *obj, double zoom);
void sel_exit(Evas_Object *obj);

#endif
