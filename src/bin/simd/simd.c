/* Kernel selection and the runtime kill-switch. */
#include "private.h"
#include "simd.h"
#include <stdlib.h>
#include <string.h>

#if defined(TERMINOLOGY_HAVE_NEON)
static Eina_Bool _use_simd = EINA_TRUE;
#else
static Eina_Bool _use_simd = EINA_FALSE;
#endif

void
simd_init(void)
{
#if defined(TERMINOLOGY_HAVE_NEON)
   const char *s = getenv("TERMINOLOGY_SIMD_DISABLE");

   /* Any non-empty value other than "0" disables. */
   if (s && s[0] && strcmp(s, "0") != 0)
     _use_simd = EINA_FALSE;
#endif
}

Eina_Bool
simd_enabled(void)
{
   return _use_simd;
}
