#ifndef _PRIVATE_H__
#define _PRIVATE_H__ 1

#ifdef HAVE_CONFIG_H
#include "terminology_config.h"
#endif

#ifdef HAVE_GETTEXT
#define _(string) gettext (string)
#else
#define _(string) (string)
#endif

extern int _log_domain;

#define CRITICAL(...) EINA_LOG_DOM_CRIT(_log_domain, __VA_ARGS__)
#define ERR(...)      EINA_LOG_DOM_ERR(_log_domain, __VA_ARGS__)
#define WRN(...)      EINA_LOG_DOM_WARN(_log_domain, __VA_ARGS__)
#define INF(...)      EINA_LOG_DOM_INFO(_log_domain, __VA_ARGS__)
#define DBG(...)      EINA_LOG_DOM_DBG(_log_domain, __VA_ARGS__)

#endif
