#ifndef _TERMIO_LINK_H__
#define _TERMIO_LINK_H__ 1

char *termio_link_find(const Evas_Object *obj, int cx, int cy, int *x1r, int *y1r, int *x2r, int *y2r);
Eina_Bool
termio_color_find(const Evas_Object *obj, int cx, int cy,
                  int *x1r, int *y1r, int *x2r, int *y2r,
                  uint8_t *rp, uint8_t *gp, uint8_t *bp, uint8_t *ap);
Eina_Bool link_is_protocol(const char *str);
Eina_Bool link_is_file(const char *str);
Eina_Bool link_is_url(const char *str);
Eina_Bool link_is_email(const char *str);


#endif
