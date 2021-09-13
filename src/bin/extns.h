#ifndef _EXTNS_H__
#define _EXTNS_H__ 1

extern const char *extn_img[];
extern const char *extn_scale[];
extern const char *extn_edj[];
extern const char *extn_mov[];
extern const char *extn_aud[];

Eina_Bool
extn_matches(const char *path, size_t path_len, const char **extns);
Eina_Bool
extn_is_media(const char *path, size_t path_len);

#endif
