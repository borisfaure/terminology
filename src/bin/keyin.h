#ifndef _KEYIN_H__
#define _KEYIN_H__ 1

void keyin_handle(Termpty *ty, const Evas_Event_Key_Down *ev,
                  int alt, int shift, int ctrl);

#endif
