#ifndef _TERM_CONTAINER_H__
#define _TERM_CONTAINER_H__ 1


typedef struct _Term_Container Term_Container;
typedef struct _Term  Term;
typedef struct _Win Win;
typedef struct _Sizeinfo Sizeinfo;

struct _Sizeinfo
{
   int min_w;
   int min_h;
   int step_x;
   int step_y;
   int req_w;
   int req_h;
   int bg_min_w;
   int bg_min_h;
   int req;
};

typedef enum _Term_Container_Type
{
   TERM_CONTAINER_TYPE_UNKNOWN,
   TERM_CONTAINER_TYPE_SOLO,
   TERM_CONTAINER_TYPE_SPLIT,
   TERM_CONTAINER_TYPE_TABS,
   TERM_CONTAINER_TYPE_WIN
} Term_Container_Type;

struct _Term_Container {
     Term_Container_Type type;
     Term_Container *parent;
     Win *wn;
     Evas_Object *selector_img;
     Eina_Bool is_focused;
     const char *title;

     Term *(*term_next)(const Term_Container *tc, const Term_Container *child);
     Term *(*term_prev)(const Term_Container *tc, const Term_Container *child);
     Term *(*term_up)(const Term_Container *tc, const Term_Container *child);
     Term *(*term_down)(const Term_Container *tc, const Term_Container *child);
     Term *(*term_left)(const Term_Container *tc, const Term_Container *child);
     Term *(*term_right)(const Term_Container *tc, const Term_Container *child);
     Term *(*term_first)(const Term_Container *tc);
     Term *(*term_last)(const Term_Container *tc);
     Term *(*focused_term_get)(const Term_Container *tc);
     Evas_Object* (*get_evas_object)(const Term_Container *container);
     Term *(*find_term_at_coords)(const Term_Container *container,
                                  Evas_Coord mx, Evas_Coord my);
     void (*split)(Term_Container *tc, Term_Container *child,
                   Term *from, const char *cmd, Eina_Bool is_horizontal);
     void (*size_eval)(Term_Container *container, Sizeinfo *info);
     void (*swallow)(Term_Container *container, Term_Container *orig,
                     Term_Container *new_child);
     void (*focus)(Term_Container *tc, Term_Container *relative);
     void (*unfocus)(Term_Container *tc, Term_Container *relative);
     void (*set_title)(Term_Container *tc, Term_Container *child, const char *title);
     void (*bell)(Term_Container *tc, Term_Container *child);
     void (*close)(Term_Container *container, Term_Container *child);
     void (*update)(Term_Container *tc);
     Eina_Bool (*is_visible)(Term_Container *tc, Term_Container *child);
};

#endif
