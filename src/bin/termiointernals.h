#ifndef _TERMIOINTERNALS_H__
#define _TERMIOINTERNALS_H__ 1

#if defined(ENABLE_FUZZING) || defined(ENABLE_TESTS)
typedef void Term;
#endif

typedef struct _Termio Termio;

struct _Termio
{
   Evas_Object_Smart_Clipped_Data __clipped_data;
   struct {
      const char *name;
      int size;
      int chw, chh;
   } font;
   struct {
      int w, h;
      Evas_Object *obj;
   } grid;
   struct {
        Evas_Object *top, *bottom, *theme;
   } sel;
   struct {
      Evas_Object *obj;
      int x, y;
      Cursor_Shape shape;
   } cursor;
   struct {
      int cx, cy;
      int button;
   } mouse;
   struct {
      const char *string;
      int x1, y1, x2, y2;
      int suspend;
      uint16_t id;
      Eina_List *objs;
      struct {
         Evas_Object *dndobj;
         Evas_Coord x, y;
         unsigned char down : 1;
         unsigned char dnd : 1;
         unsigned char dndobjdel : 1;
      } down;
   } link;
   struct {
      const char *file;
      FILE *f;
      double progress;
      unsigned long long total, size;
      Eina_Bool active : 1;
   } sendfile;
   Evas_Object *ctxpopup;
   int zoom_fontsize_start;
   int scroll;
   Evas_Object *self;
   Evas_Object *event;
   Term *term;

   Termpty *pty;
   Ecore_Animator *anim;
   Ecore_Timer *delayed_size_timer;
   Ecore_Timer *link_do_timer;
   Ecore_Timer *mouse_selection_scroll_timer;
   Ecore_Job *mouse_move_job;
   Ecore_Timer *mouseover_delay;
   Evas_Object *win, *theme, *glayer;
   Config *config;
   const char *sel_str;
   Eina_List *cur_chids;
   Ecore_Job *sel_reset_job;
   double set_sel_at;
   Elm_Sel_Type sel_type;
   unsigned char jump_on_change : 1;
   unsigned char jump_on_keypress : 1;
   unsigned char have_sel : 1;
   unsigned char noreqsize : 1;
   unsigned char didclick : 1;
   unsigned char moved : 1;
   unsigned char bottom_right : 1;
   unsigned char top_left : 1;
   unsigned char reset_sel : 1;
   unsigned char cb_added : 1;
   double gesture_zoom_start_size;
};

#define INT_SWAP(_a, _b) do {    \
    int _swap = _a; _a = _b; _b = _swap; \
} while (0)


void
termio_internal_mouse_down(Termio *sd, Evas_Event_Mouse_Down *ev);
void
termio_internal_mouse_up(Termio *sd, Evas_Event_Mouse_Up *ev);
void
termio_internal_mouse_move(Termio *sd, Evas_Event_Mouse_Move *ev);

void
termio_selection_dbl_fix(Termio *sd);
void
termio_selection_get(Termio *sd,
                     int c1x, int c1y, int c2x, int c2y,
                     struct ty_sb *sb,
                     Eina_Bool rtrim);
void
termio_scroll(Evas_Object *obj, int direction, int start_y, int end_y);
void
termio_cursor_to_xy(Termio *sd, Evas_Coord x, Evas_Coord y,
                    int *cx, int *cy);
#endif
