#ifndef _TERMIO_LINK_H__
#define _TERMIO_LINK_H__ 1

#define HL_LINKS_MAX  (1 << 16)

typedef struct _Termlink      Term_Link;

struct _Termlink
{
    char *key;
    char *url;
    unsigned int refcount;
};


char *termio_link_find(const Evas_Object *obj, int cx, int cy, int *x1r, int *y1r, int *x2r, int *y2r);

#endif
