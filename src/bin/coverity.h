#ifndef _COVERITY_H__
#define _COVERITY_H__ 1
#ifdef __COVERITY__

#ifdef __x86_64__
typedef struct { long double x; long double y; } _Float128;
#endif

#endif /* __COVERITY__ */

#endif /* _COVERITY_H__ */
