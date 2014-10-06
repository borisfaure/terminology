#include "private.h"

#include <Elementary.h>

#include "gravatar.h"
#include "config.h"
#include "termio.h"
#include "media.h"
#include "md5/md5.h"

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

#define GRAVATAR_URL_START "http://www.gravatar.com/avatar/"
#define GRAVATAR_URL_END ""

static Evas_Object *
_tooltip_content(void *data,
                 Evas_Object *obj,
                 Evas_Object *tt EINA_UNUSED)
{
   const char *url = data;
   Evas_Object *o;

   o = elm_label_add(obj);
   elm_object_text_set(o, url);
   DBG("url:%s", url);
   /* TODO */
   //o = media_add(obj, url, config, MEDIA_TOOLTIP, &type);

   return o;
}

static void
_tooltip_del(void            *data,
             Evas_Object *obj EINA_UNUSED,
             void            *event_info EINA_UNUSED)
{
   const char *url = data;
   DBG("url:%s", url);
   eina_stringshare_del(data);
}

void
gravatar_tooltip(Evas_Object *obj, char *email)
{
   int n;
   MD5_CTX ctx;
   char md5out[(2 * MD5_HASHBYTES) + 1];
   unsigned char hash[MD5_HASHBYTES];
   static const char hex[] = "0123456789abcdef";
   const char *url;
   //int type;
   //Config *config = termio_config_get(obj);

   DBG("need to show tooltip for email:%s", email);
   eina_str_tolower(&email);
   DBG("lower:%s", email);

   MD5Init(&ctx);
   MD5Update(&ctx, (unsigned char const*)email, (unsigned)strlen(email));
   MD5Final(hash, &ctx);

   for (n = 0; n < MD5_HASHBYTES; n++)
     {
        md5out[2 * n] = hex[hash[n] >> 4];
        md5out[2 * n + 1] = hex[hash[n] & 0x0f];
     }
   md5out[2 * MD5_HASHBYTES] = '\0';

   DBG("md5:%s", md5out);

   url = eina_stringshare_printf(GRAVATAR_URL_START"%s"GRAVATAR_URL_END,
                                 md5out);

   DBG("url:%s", url);

   elm_object_tooltip_content_cb_set(obj, _tooltip_content,
                                     (void *) url,
                                     _tooltip_del);
}

void
gravatar_init(void)
{
   if (_gravatar_log_dom >= 0) return;

   _gravatar_log_dom = eina_log_domain_register("gravatar", NULL);
   if (_gravatar_log_dom < 0)
     EINA_LOG_CRIT(_("Could not create logging domain '%s'."), "gravatar");
}

void
gravatar_shutdown(void)
{
   if (_gravatar_log_dom < 0) return;
   eina_log_domain_unregister(_gravatar_log_dom);
   _gravatar_log_dom = -1;
}

