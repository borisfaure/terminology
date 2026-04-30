#if defined(BINARY_TYTEST)

#include <assert.h>
#include <string.h>

#include "uri_decode.h"
#include "unit_tests.h"

int
tytest_percent_decode(void)
{
   char buf[256];

   /* 1. Empty string */
   strcpy(buf, "");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "") == 0);

   /* 2. No percent sequences */
   strcpy(buf, "hello");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "hello") == 0);

   /* 3. Single %20 (space) */
   strcpy(buf, "a%20b");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "a b") == 0);

   /* 4. Multiple %20 */
   strcpy(buf, "a%20b%20c");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "a b c") == 0);

   /* 5. UTF-8 bytes (%C3%A9 = é) */
   strcpy(buf, "r%C3%A9sum%C3%A9");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "résumé") == 0);

   /* 6. Lowercase hex */
   strcpy(buf, "%2f");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "/") == 0);

   /* 7. Uppercase hex */
   strcpy(buf, "%2F");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "/") == 0);

   /* 8. Mixed case */
   strcpy(buf, "%2A%2a");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "**") == 0);

   /* 9. Malformed: % at end of string */
   strcpy(buf, "a%");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "a%") == 0);

   /* 10. Malformed: %X at end (only one hex digit) */
   strcpy(buf, "a%5");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "a%5") == 0);

   /* 11. Malformed: non-hex digit */
   strcpy(buf, "a%GG");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "a%GG") == 0);

   /* 12. % followed by another % then valid hex.
    *     The first % passes through; %20 decodes. */
   strcpy(buf, "%%20");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "% ") == 0);

   /* 13. Just % alone (no following bytes) */
   strcpy(buf, "%");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "%") == 0);

   /* 15. %00 NUL byte: truncates at the embedded NUL.
    *     Prevents path-confusion attack:
    *     file:///etc/passwd%00.safe.txt → /etc/passwd */
   strcpy(buf, "/etc/passwd%00.safe.txt");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "/etc/passwd") == 0);
   assert(strlen(buf) == 11);

   /* 15b. %00 at start of input: returns empty string */
   strcpy(buf, "%00abc");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "") == 0);

   /* 15c. %00 alone: empty string */
   strcpy(buf, "%00");
   uri_percent_decode_inplace(buf);
   assert(strcmp(buf, "") == 0);

   /* 16. NULL input doesn't crash */
   uri_percent_decode_inplace(NULL);

   return 0;
}

#endif
