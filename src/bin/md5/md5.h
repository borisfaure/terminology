#ifndef _MD5_H_
#define _MD5_H_

#include <stdint.h>
#include <sys/types.h>

#define MD5_HASHBYTES 16

typedef struct MD5Context {
	uint32_t buf[4];
	uint32_t bits[2];
	union
	  {
	     unsigned char s[64];
	     uint32_t i[16];
	  } in;
} MD5_CTX;

extern void   MD5Init(MD5_CTX *context);
extern void   MD5Update(MD5_CTX *context,unsigned char const *buf,unsigned len);
extern void   MD5Final(unsigned char digest[MD5_HASHBYTES], MD5_CTX *context);

extern void   MD5Transform(uint32_t buf[4], uint32_t const in[16]);
extern char  *MD5End(MD5_CTX *, char *);
extern char  *MD5File(const char *, char *);
extern char  *MD5Data (const unsigned char *, unsigned int, char *);

#endif
