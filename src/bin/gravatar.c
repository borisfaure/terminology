#include "private.h"

#include <Elementary.h>

#include "gravatar.h"

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

void
gravatar_tooltip(const char *email)
{
   DBG("need to show tooltip for email:%s", email);

   /* TODO */
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

