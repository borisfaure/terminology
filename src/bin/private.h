#ifndef TERMINOLOGY_PRIVATE_H_
#define TERMINOLOGY_PRIVATE_H_ 1

#ifdef HAVE_CONFIG_H
#include "terminology_config.h"
#endif

#if ENABLE_NLS
#include <libintl.h>
#define _(string) gettext (string)
#else
#define _(string) (string)
#endif
#define gettext_noop(String) String


extern int terminology_starting_up;

/* Uncommenting the following enables processing of testing escape codes in
 * the normal 'terminology' binary.
 * This is only useful to write tests
 */
#if !defined(BINARY_TYFUZZ) && !defined(BINARY_TYTEST)
//#define ENABLE_TEST_UI
#endif

#if defined(BINARY_TYFUZZ) || defined(BINARY_TYTEST)
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


#define casestartswith(str, constref) \
  (!strncasecmp(str, constref, sizeof(constref) - 1))
#define startswith(str, constref) \
  (!strncmp(str, constref, sizeof(constref) - 1))

#define static_strequal(STR, STATIC_STR)  \
   (!strncmp(STR, STATIC_STR, strlen(STATIC_STR)))

#if !defined(HAVE_STRCHRNUL)
static inline const char *
strchrnul(const char *s, int c)
{
   const char *p = s;

   while (*p)
     {
        if (*p == c)
          return (char *)p;

        ++p;
     }
   return (char *)  (p);
}
#endif

#endif
