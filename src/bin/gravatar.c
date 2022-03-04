#include "private.h"

#include <Elementary.h>

#include "gravatar.h"
#include "config.h"
#include "termio.h"
#include "media.h"
#include "md5.h"
#include "theme.h"

/* specific log domain to help debug the gravatar module */
int _gravatar_log_dom = -1;

#undef CRITICAL
#undef ERR
#undef WRN
#undef INF
#undef DBG

#define CRIT(...)     EINA_LOG_DOM_CRIT(_gravatar_log_dom, __VA_ARGS__)
#define ERR(...)      EINA_LOG_DOM_ERR (_gravatar_log_dom, __VA_ARGS__)
#define WRN(...)      EINA_LOG_DOM_WARN(_gravatar_log_dom, __VA_ARGS__)
#define INF(...)      EINA_LOG_DOM_INFO(_gravatar_log_dom, __VA_ARGS__)
#define DBG(...)      EINA_LOG_DOM_DBG (_gravatar_log_dom, __VA_ARGS__)

#define GRAVATAR_URL_START "https://www.gravatar.com/avatar/"
#define GRAVATAR_URL_END ""

typedef struct tag_Gravatar {
     const char *url;
     const Config *config;
} Gravatar;

static Evas_Object *
_tooltip_content(void *data,
                 Evas_Object *obj,
                 Evas_Object *_tt EINA_UNUSED)
{
   Gravatar *g = data;
   Evas_Object *o;

   o = media_add(obj, g->url, g->config, MEDIA_STRETCH, MEDIA_TYPE_IMG);
   evas_object_size_hint_min_set(o, 80, 80);

   return o;
}

static void
_tooltip_del(void            *data,
             Evas_Object     *_obj EINA_UNUSED,
             void            *_event_info EINA_UNUSED)
{
   Gravatar *g = data;
   eina_stringshare_del(g->url);
   free(g);
}

void
gravatar_tooltip(Evas_Object *obj, const Config *config, const char *email)
{
   int n;
   MD5_CTX ctx;
   char md5out[(2 * MD5_HASHBYTES) + 1];
   unsigned char hash[MD5_HASHBYTES];
   static const char hex[] = "0123456789abcdef";
   const char *url;
   Gravatar *g;
   size_t len;
   char *str;

   if (!config->gravatar)
     return;

   g = calloc(sizeof(Gravatar), 1);
   if (!g)
     return;
   g->config = config;

   if (casestartswith(email, "mailto:"))
     email += strlen("mailto:");

   len = strlen(email);
   str = strndup(email, len);
   if (!str)
     {
        free(g);
        return;
     }

   eina_str_tolower(&str);

   MD5Init(&ctx);
   MD5Update(&ctx, (unsigned char const*)str, (unsigned)len);
   MD5Final(hash, &ctx);

   for (n = 0; n < MD5_HASHBYTES; n++)
     {
        md5out[2 * n] = hex[hash[n] >> 4];
        md5out[2 * n + 1] = hex[hash[n] & 0x0f];
     }
   md5out[2 * MD5_HASHBYTES] = '\0';

   url = eina_stringshare_printf(GRAVATAR_URL_START"%s"GRAVATAR_URL_END,
                                 md5out);

   g->url = url;
   elm_object_tooltip_content_cb_set(obj, _tooltip_content,
                                     g,
                                     _tooltip_del);
   free(str);
}

void
gravatar_init(void)
{
   if (_gravatar_log_dom >= 0) return;

   _gravatar_log_dom = eina_log_domain_register("gravatar", NULL);
   if (_gravatar_log_dom < 0)
     EINA_LOG_CRIT(_("Could not create logging domain '%s'"), "gravatar");
}

void
gravatar_shutdown(void)
{
   if (_gravatar_log_dom < 0) return;
   eina_log_domain_unregister(_gravatar_log_dom);
   _gravatar_log_dom = -1;
}

