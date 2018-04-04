#ifndef _PRIVATE_H__
#define _PRIVATE_H__ 1

#ifdef HAVE_CONFIG_H
#include "terminology_config.h"
#endif

#if HAVE_GETTEXT && ENABLE_NLS
#include <libintl.h>
#define _(string) gettext (string)
#else
#define _(string) (string)
#endif
#define gettext_noop(String) String

#include "coverity.h"

extern int terminology_starting_up;

#ifdef ENABLE_FUZZING
#define EINA_LOG_LEVEL_MAXIMUM (-1)
#endif
extern int _log_domain;

#define CRITICAL(...) EINA_LOG_DOM_CRIT(_log_domain, __VA_ARGS__)
#define ERR(...)      EINA_LOG_DOM_ERR(_log_domain, __VA_ARGS__)
#define WRN(...)      EINA_LOG_DOM_WARN(_log_domain, __VA_ARGS__)
#define INF(...)      EINA_LOG_DOM_INFO(_log_domain, __VA_ARGS__)
#define DBG(...)      EINA_LOG_DOM_DBG(_log_domain, __VA_ARGS__)

#ifndef MIN
# define MIN(x, y) (((x) > (y)) ? (y) : (x))
#endif
#ifndef MAX
# define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef DIV_ROUND_UP
# define DIV_ROUND_UP(_v, _n) \
     (((_v) + (_n) - 1) / (_n))
#endif

#ifndef ROUND_UP
# define ROUND_UP(_v, _n) \
     (DIV_ROUND_UP((_v), (_n)) * (_n))
#endif

#endif
