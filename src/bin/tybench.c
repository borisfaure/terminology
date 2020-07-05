#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if defined(__clang__)
#define FALLTHROUGH
#else
#define FALLTHROUGH __attribute__((fallthrough))
#endif

const int nb_codepoints = 1000 * 1000 * 1000;
unsigned int *cps;

__attribute__((const))
static bool
_switch_fallthrough(const unsigned int codepoint)
{
   switch (codepoint)
     {
       case 0: FALLTHROUGH;
       case 1: FALLTHROUGH;
       case 2: FALLTHROUGH;
       case 3: FALLTHROUGH;
       case 4: FALLTHROUGH;
       case 5: FALLTHROUGH;
       case 6: FALLTHROUGH;
       case 7: FALLTHROUGH;
       case 8: FALLTHROUGH;
       case 9: FALLTHROUGH;
       case 10: FALLTHROUGH;
       case 11: FALLTHROUGH;
       case 12: FALLTHROUGH;
       case 13: FALLTHROUGH;
       case 14: FALLTHROUGH;
       case 15: FALLTHROUGH;
       case 16: FALLTHROUGH;
       case 17: FALLTHROUGH;
       case 18: FALLTHROUGH;
       case 19: FALLTHROUGH;
       case 20: FALLTHROUGH;
       case 21: FALLTHROUGH;
       case 22: FALLTHROUGH;
       case 23: FALLTHROUGH;
       case 24: FALLTHROUGH;
       case 25: FALLTHROUGH;
       case 26: FALLTHROUGH;
       case 27: FALLTHROUGH;
       case 28: FALLTHROUGH;
       case 29: FALLTHROUGH;
       case 30: FALLTHROUGH;
       case 31: FALLTHROUGH;
       case 32: FALLTHROUGH;
       case 33: FALLTHROUGH;
       case 34: FALLTHROUGH;
       case 35: FALLTHROUGH;
       case 36: FALLTHROUGH;
       case 37: FALLTHROUGH;
       case 38: FALLTHROUGH;
       case 39: FALLTHROUGH;
       case 40: FALLTHROUGH;
       case 41: FALLTHROUGH;
       case 42: FALLTHROUGH;
       case 44: FALLTHROUGH;
       case 59: FALLTHROUGH;
       case 61: FALLTHROUGH;
       case 63: FALLTHROUGH;
       case 91: FALLTHROUGH;
       case 92: FALLTHROUGH;
       case 93: FALLTHROUGH;
       case 94: FALLTHROUGH;
       case 96: FALLTHROUGH;
       case 123: FALLTHROUGH;
       case 124: FALLTHROUGH;
       case 125: FALLTHROUGH;
       case 0x00a0: FALLTHROUGH;
       case 0x00ab: FALLTHROUGH;
       case 0x00b7: FALLTHROUGH;
       case 0x00bb: FALLTHROUGH;
       case 0x0294: FALLTHROUGH;
       case 0x02bb: FALLTHROUGH;
       case 0x02bd: FALLTHROUGH;
       case 0x02d0: FALLTHROUGH;
       case 0x0312: FALLTHROUGH;
       case 0x0313: FALLTHROUGH;
       case 0x0314: FALLTHROUGH;
       case 0x0315: FALLTHROUGH;
       case 0x0326: FALLTHROUGH;
       case 0x0387: FALLTHROUGH;
       case 0x055d: FALLTHROUGH;
       case 0x055e: FALLTHROUGH;
       case 0x060c: FALLTHROUGH;
       case 0x061f: FALLTHROUGH;
       case 0x066d: FALLTHROUGH;
       case 0x07fb: FALLTHROUGH;
       case 0x1363: FALLTHROUGH;
       case 0x1367: FALLTHROUGH;
       case 0x14fe: FALLTHROUGH;
       case 0x1680: FALLTHROUGH;
       case 0x1802: FALLTHROUGH;
       case 0x1808: FALLTHROUGH;
       case 0x180e: FALLTHROUGH;
       case 0x2000: FALLTHROUGH;
       case 0x2001: FALLTHROUGH;
       case 0x2002: FALLTHROUGH;
       case 0x2003: FALLTHROUGH;
       case 0x2004: FALLTHROUGH;
       case 0x2005: FALLTHROUGH;
       case 0x2006: FALLTHROUGH;
       case 0x2007: FALLTHROUGH;
       case 0x2008: FALLTHROUGH;
       case 0x2009: FALLTHROUGH;
       case 0x200a: FALLTHROUGH;
       case 0x200b: FALLTHROUGH;
       case 0x2018: FALLTHROUGH;
       case 0x2019: FALLTHROUGH;
       case 0x201a: FALLTHROUGH;
       case 0x201b: FALLTHROUGH;
       case 0x201c: FALLTHROUGH;
       case 0x201d: FALLTHROUGH;
       case 0x201e: FALLTHROUGH;
       case 0x201f: FALLTHROUGH;
       case 0x2022: FALLTHROUGH;
       case 0x2027: FALLTHROUGH;
       case 0x202f: FALLTHROUGH;
       case 0x2039: FALLTHROUGH;
       case 0x203a: FALLTHROUGH;
       case 0x203b: FALLTHROUGH;
       case 0x203d: FALLTHROUGH;
       case 0x2047: FALLTHROUGH;
       case 0x2048: FALLTHROUGH;
       case 0x2049: FALLTHROUGH;
       case 0x204e: FALLTHROUGH;
       case 0x205f: FALLTHROUGH;
       case 0x2217: FALLTHROUGH;
       case 0x225f: FALLTHROUGH;
       case 0x2308: FALLTHROUGH;
       case 0x2309: FALLTHROUGH;
       case 0x2420: FALLTHROUGH;
       case 0x2422: FALLTHROUGH;
       case 0x2423: FALLTHROUGH;
       case 0x2722: FALLTHROUGH;
       case 0x2723: FALLTHROUGH;
       case 0x2724: FALLTHROUGH;
       case 0x2725: FALLTHROUGH;
       case 0x2731: FALLTHROUGH;
       case 0x2732: FALLTHROUGH;
       case 0x2733: FALLTHROUGH;
       case 0x273a: FALLTHROUGH;
       case 0x273b: FALLTHROUGH;
       case 0x273c: FALLTHROUGH;
       case 0x273d: FALLTHROUGH;
       case 0x2743: FALLTHROUGH;
       case 0x2749: FALLTHROUGH;
       case 0x274a: FALLTHROUGH;
       case 0x274b: FALLTHROUGH;
       case 0x2a7b: FALLTHROUGH;
       case 0x2a7c: FALLTHROUGH;
       case 0x2cfa: FALLTHROUGH;
       case 0x2e2e: FALLTHROUGH;
       case 0x3000: FALLTHROUGH;
       case 0x3001: FALLTHROUGH;
       case 0x3008: FALLTHROUGH;
       case 0x3009: FALLTHROUGH;
       case 0x300a: FALLTHROUGH;
       case 0x300b: FALLTHROUGH;
       case 0x300d: FALLTHROUGH;
       case 0x300e: FALLTHROUGH;
       case 0x300f: FALLTHROUGH;
       case 0x3010: FALLTHROUGH;
       case 0x3011: FALLTHROUGH;
       case 0x301d: FALLTHROUGH;
       case 0x301e: FALLTHROUGH;
       case 0x301f: FALLTHROUGH;
       case 0x30fb: FALLTHROUGH;
       case 0xa60d: FALLTHROUGH;
       case 0xa60f: FALLTHROUGH;
       case 0xa6f5: FALLTHROUGH;
       case 0xe0a0: FALLTHROUGH;
       case 0xe0b0: FALLTHROUGH;
       case 0xe0b2: FALLTHROUGH;
       case 0xfe10: FALLTHROUGH;
       case 0xfe41: FALLTHROUGH;
       case 0xfe42: FALLTHROUGH;
       case 0xfe43: FALLTHROUGH;
       case 0xfe44: FALLTHROUGH;
       case 0xfe50: FALLTHROUGH;
       case 0xfe51: FALLTHROUGH;
       case 0xfe56: FALLTHROUGH;
       case 0xfe61: FALLTHROUGH;
       case 0xfe62: FALLTHROUGH;
       case 0xfe63: FALLTHROUGH;
       case 0xfeff: FALLTHROUGH;
       case 0xff02: FALLTHROUGH;
       case 0xff07: FALLTHROUGH;
       case 0xff08: FALLTHROUGH;
       case 0xff09: FALLTHROUGH;
       case 0xff0a: FALLTHROUGH;
       case 0xff0c: FALLTHROUGH;
       case 0xff1b: FALLTHROUGH;
       case 0xff1c: FALLTHROUGH;
       case 0xff1e: FALLTHROUGH;
       case 0xff1f: FALLTHROUGH;
       case 0xff3b: FALLTHROUGH;
       case 0xff3d: FALLTHROUGH;
       case 0xff5b: FALLTHROUGH;
       case 0xff5d: FALLTHROUGH;
       case 0xff62: FALLTHROUGH;
       case 0xff63: FALLTHROUGH;
       case 0xff64: FALLTHROUGH;
       case 0xff65: FALLTHROUGH;
       case 0xe002a:
         return true;
     }
   return false;
}

__attribute__((const))
static bool
_switch_fallthrough_ranges(const unsigned int codepoint)
{
   switch (codepoint)
     {
       case 0 ... 42: FALLTHROUGH;
       case 44: FALLTHROUGH;
       case 59: FALLTHROUGH;
       case 61: FALLTHROUGH;
       case 63: FALLTHROUGH;
       case 91 ... 94: FALLTHROUGH;
       case 96: FALLTHROUGH;
       case 123 ... 125: FALLTHROUGH;
       case 0x00a0: FALLTHROUGH;
       case 0x00ab: FALLTHROUGH;
       case 0x00b7: FALLTHROUGH;
       case 0x00bb: FALLTHROUGH;
       case 0x0294: FALLTHROUGH;
       case 0x02bb: FALLTHROUGH;
       case 0x02bd: FALLTHROUGH;
       case 0x02d0: FALLTHROUGH;
       case 0x0312 ... 0x0315: FALLTHROUGH;
       case 0x0326: FALLTHROUGH;
       case 0x0387: FALLTHROUGH;
       case 0x055d ... 0x055e: FALLTHROUGH;
       case 0x060c: FALLTHROUGH;
       case 0x061f: FALLTHROUGH;
       case 0x066d: FALLTHROUGH;
       case 0x07fb: FALLTHROUGH;
       case 0x1363: FALLTHROUGH;
       case 0x1367: FALLTHROUGH;
       case 0x14fe: FALLTHROUGH;
       case 0x1680: FALLTHROUGH;
       case 0x1802: FALLTHROUGH;
       case 0x1808: FALLTHROUGH;
       case 0x180e: FALLTHROUGH;
       case 0x2000 ... 0x200b: FALLTHROUGH;
       case 0x2018 ... 0x201f: FALLTHROUGH;
       case 0x2022: FALLTHROUGH;
       case 0x2027: FALLTHROUGH;
       case 0x202f: FALLTHROUGH;
       case 0x2039: FALLTHROUGH;
       case 0x203a ... 0x203b: FALLTHROUGH;
       case 0x203d: FALLTHROUGH;
       case 0x2047 ... 0x2049: FALLTHROUGH;
       case 0x204e: FALLTHROUGH;
       case 0x205f: FALLTHROUGH;
       case 0x2217: FALLTHROUGH;
       case 0x225f: FALLTHROUGH;
       case 0x2308 ... 0x2309: FALLTHROUGH;
       case 0x2420: FALLTHROUGH;
       case 0x2422 ... 0x2423: FALLTHROUGH;
       case 0x2722 ... 0x2725: FALLTHROUGH;
       case 0x2731 ... 0x2733: FALLTHROUGH;
       case 0x273a ... 0x273d: FALLTHROUGH;
       case 0x2743: FALLTHROUGH;
       case 0x2749: FALLTHROUGH;
       case 0x274a ... 0x274b: FALLTHROUGH;
       case 0x2a7b ... 0x2a7c: FALLTHROUGH;
       case 0x2cfa: FALLTHROUGH;
       case 0x2e2e: FALLTHROUGH;
       case 0x3000 ... 0x3001: FALLTHROUGH;
       case 0x3008 ... 0x300b: FALLTHROUGH;
       case 0x300d ... 0x3011: FALLTHROUGH;
       case 0x301d ... 0x301f: FALLTHROUGH;
       case 0x30fb: FALLTHROUGH;
       case 0xa60d: FALLTHROUGH;
       case 0xa60f: FALLTHROUGH;
       case 0xa6f5: FALLTHROUGH;
       case 0xe0a0: FALLTHROUGH;
       case 0xe0b0: FALLTHROUGH;
       case 0xe0b2: FALLTHROUGH;
       case 0xfe10: FALLTHROUGH;
       case 0xfe41 ... 0xfe44: FALLTHROUGH;
       case 0xfe50 ... 0xfe51: FALLTHROUGH;
       case 0xfe56: FALLTHROUGH;
       case 0xfe61 ... 0xfe63: FALLTHROUGH;
       case 0xfeff: FALLTHROUGH;
       case 0xff02: FALLTHROUGH;
       case 0xff07 ... 0xff0a: FALLTHROUGH;
       case 0xff0c: FALLTHROUGH;
       case 0xff1b ... 0xff1f: FALLTHROUGH;
       case 0xff3b: FALLTHROUGH;
       case 0xff3d: FALLTHROUGH;
       case 0xff5b: FALLTHROUGH;
       case 0xff5d: FALLTHROUGH;
       case 0xff62 ... 0xff65: FALLTHROUGH;
       case 0xe002a:
         return true;
     }
   return false;
}

__attribute__((const))
static bool
_switch_return(const unsigned int codepoint)
{
   switch (codepoint)
     {
      case 0: return true;
      case 1: return true;
      case 2: return true;
      case 3: return true;
      case 4: return true;
      case 5: return true;
      case 6: return true;
      case 7: return true;
      case 8: return true;
      case 9: return true;
      case 10: return true;
      case 11: return true;
      case 12: return true;
      case 13: return true;
      case 14: return true;
      case 15: return true;
      case 16: return true;
      case 17: return true;
      case 18: return true;
      case 19: return true;
      case 20: return true;
      case 21: return true;
      case 22: return true;
      case 23: return true;
      case 24: return true;
      case 25: return true;
      case 26: return true;
      case 27: return true;
      case 28: return true;
      case 29: return true;
      case 30: return true;
      case 31: return true;
      case 32: return true;
      case 33: return true;
      case 34: return true;
      case 35: return true;
      case 36: return true;
      case 37: return true;
      case 38: return true;
      case 39: return true;
      case 40: return true;
      case 41: return true;
      case 42: return true;
      case 44: return true;
      case 59: return true;
      case 61: return true;
      case 63: return true;
      case 91: return true;
      case 92: return true;
      case 93: return true;
      case 94: return true;
      case 96: return true;
      case 123: return true;
      case 124: return true;
      case 125: return true;
      case 0x00a0: return true;
      case 0x00ab: return true;
      case 0x00b7: return true;
      case 0x00bb: return true;
      case 0x0294: return true;
      case 0x02bb: return true;
      case 0x02bd: return true;
      case 0x02d0: return true;
      case 0x0312: return true;
      case 0x0313: return true;
      case 0x0314: return true;
      case 0x0315: return true;
      case 0x0326: return true;
      case 0x0387: return true;
      case 0x055d: return true;
      case 0x055e: return true;
      case 0x060c: return true;
      case 0x061f: return true;
      case 0x066d: return true;
      case 0x07fb: return true;
      case 0x1363: return true;
      case 0x1367: return true;
      case 0x14fe: return true;
      case 0x1680: return true;
      case 0x1802: return true;
      case 0x1808: return true;
      case 0x180e: return true;
      case 0x2000: return true;
      case 0x2001: return true;
      case 0x2002: return true;
      case 0x2003: return true;
      case 0x2004: return true;
      case 0x2005: return true;
      case 0x2006: return true;
      case 0x2007: return true;
      case 0x2008: return true;
      case 0x2009: return true;
      case 0x200a: return true;
      case 0x200b: return true;
      case 0x2018: return true;
      case 0x2019: return true;
      case 0x201a: return true;
      case 0x201b: return true;
      case 0x201c: return true;
      case 0x201d: return true;
      case 0x201e: return true;
      case 0x201f: return true;
      case 0x2022: return true;
      case 0x2027: return true;
      case 0x202f: return true;
      case 0x2039: return true;
      case 0x203a: return true;
      case 0x203b: return true;
      case 0x203d: return true;
      case 0x2047: return true;
      case 0x2048: return true;
      case 0x2049: return true;
      case 0x204e: return true;
      case 0x205f: return true;
      case 0x2217: return true;
      case 0x225f: return true;
      case 0x2308: return true;
      case 0x2309: return true;
      case 0x2420: return true;
      case 0x2422: return true;
      case 0x2423: return true;
      case 0x2722: return true;
      case 0x2723: return true;
      case 0x2724: return true;
      case 0x2725: return true;
      case 0x2731: return true;
      case 0x2732: return true;
      case 0x2733: return true;
      case 0x273a: return true;
      case 0x273b: return true;
      case 0x273c: return true;
      case 0x273d: return true;
      case 0x2743: return true;
      case 0x2749: return true;
      case 0x274a: return true;
      case 0x274b: return true;
      case 0x2a7b: return true;
      case 0x2a7c: return true;
      case 0x2cfa: return true;
      case 0x2e2e: return true;
      case 0x3000: return true;
      case 0x3001: return true;
      case 0x3008: return true;
      case 0x3009: return true;
      case 0x300a: return true;
      case 0x300b: return true;
      case 0x300c: return true;
      case 0x300d: return true;
      case 0x300e: return true;
      case 0x300f: return true;
      case 0x3010: return true;
      case 0x3011: return true;
      case 0x301d: return true;
      case 0x301e: return true;
      case 0x301f: return true;
      case 0x30fb: return true;
      case 0xa60d: return true;
      case 0xa60f: return true;
      case 0xa6f5: return true;
      case 0xe0a0: return true;
      case 0xe0b0: return true;
      case 0xe0b2: return true;
      case 0xfe10: return true;
      case 0xfe41: return true;
      case 0xfe42: return true;
      case 0xfe43: return true;
      case 0xfe44: return true;
      case 0xfe50: return true;
      case 0xfe51: return true;
      case 0xfe56: return true;
      case 0xfe61: return true;
      case 0xfe62: return true;
      case 0xfe63: return true;
      case 0xfeff: return true;
      case 0xff02: return true;
      case 0xff07: return true;
      case 0xff08: return true;
      case 0xff09: return true;
      case 0xff0a: return true;
      case 0xff0c: return true;
      case 0xff1b: return true;
      case 0xff1c: return true;
      case 0xff1e: return true;
      case 0xff1f: return true;
      case 0xff3b: return true;
      case 0xff3d: return true;
      case 0xff5b: return true;
      case 0xff5d: return true;
      case 0xff62: return true;
      case 0xff63: return true;
      case 0xff64: return true;
      case 0xff65: return true;
      case 0xe002a: return true;
     }
   return false;
}


__attribute__((const))
static bool
_switch_return_ranges(const unsigned int codepoint)
{
   switch (codepoint)
     {
      case 0 ... 42: return true;
      case 44: return true;
      case 59: return true;
      case 61: return true;
      case 63: return true;
      case 91 ... 94: return true;
      case 96: return true;
      case 123 ... 125: return true;
      case 0x00a0: return true;
      case 0x00ab: return true;
      case 0x00b7: return true;
      case 0x00bb: return true;
      case 0x0294: return true;
      case 0x02bb: return true;
      case 0x02bd: return true;
      case 0x02d0: return true;
      case 0x0312 ... 0x315: return true;
      case 0x0326: return true;
      case 0x0387: return true;
      case 0x055d ... 0x055e: return true;
      case 0x060c: return true;
      case 0x061f: return true;
      case 0x066d: return true;
      case 0x07fb: return true;
      case 0x1363: return true;
      case 0x1367: return true;
      case 0x14fe: return true;
      case 0x1680: return true;
      case 0x1802: return true;
      case 0x1808: return true;
      case 0x180e: return true;
      case 0x2000 ... 0x200b: return true;
      case 0x2018 ... 0x201f: return true;
      case 0x2022: return true;
      case 0x2027: return true;
      case 0x202f: return true;
      case 0x2039: return true;
      case 0x203a ... 0x203b: return true;
      case 0x203d: return true;
      case 0x2047 ... 0x2049: return true;
      case 0x204e: return true;
      case 0x205f: return true;
      case 0x2217: return true;
      case 0x225f: return true;
      case 0x2308 ... 0x2309: return true;
      case 0x2420: return true;
      case 0x2422 ... 0x2423: return true;
      case 0x2722 ... 0x2725: return true;
      case 0x2731 ... 0x2733: return true;
      case 0x273a ... 0x273d: return true;
      case 0x2743: return true;
      case 0x2749: return true;
      case 0x274a ... 0x274b: return true;
      case 0x2a7b ... 0x2a7c: return true;
      case 0x2cfa: return true;
      case 0x2e2e: return true;
      case 0x3000 ... 0x3001: return true;
      case 0x3008 ... 0x300b: return true;
      case 0x300d ... 0x3011: return true;
      case 0x301d ... 0x301f: return true;
      case 0x30fb: return true;
      case 0xa60d: return true;
      case 0xa60f: return true;
      case 0xa6f5: return true;
      case 0xe0a0: return true;
      case 0xe0b0: return true;
      case 0xe0b2: return true;
      case 0xfe10: return true;
      case 0xfe41 ... 0xfe44: return true;
      case 0xfe50 ... 0xfe51: return true;
      case 0xfe56: return true;
      case 0xfe61 ... 0xfe63: return true;
      case 0xfeff: return true;
      case 0xff02: return true;
      case 0xff07 ... 0xff0a: return true;
      case 0xff0c: return true;
      case 0xff1b ... 0xff1f: return true;
      case 0xff3b: return true;
      case 0xff3d: return true;
      case 0xff5b: return true;
      case 0xff5d: return true;
      case 0xff62 ... 0xff65: return true;
      case 0xe002a: return true;
     }
   return false;
}

__attribute__((const))
static bool
_binary_search(const unsigned int codepoint)
{
   static const unsigned int authorized[] = {
       0,
       1,
       2,
       3,
       4,
       5,
       6,
       7,
       8,
       9,
       10,
       11,
       12,
       13,
       14,
       15,
       16,
       17,
       18,
       19,
       20,
       21,
       22,
       23,
       24,
       25,
       26,
       27,
       28,
       29,
       30,
       31,
       32,
       33,
       34,
       35,
       36,
       37,
       38,
       39,
       40,
       41,
       42,
       44,
       59,
       61,
       63,
       91,
       92,
       93,
       94,
       96,
       123,
       124,
       125,
       0x00a0,
       0x00ab,
       0x00b7,
       0x00bb,
       0x0294,
       0x02bb,
       0x02bd,
       0x02d0,
       0x0312,
       0x0313,
       0x0314,
       0x0315,
       0x0326,
       0x0387,
       0x055d,
       0x055e,
       0x060c,
       0x061f,
       0x066d,
       0x07fb,
       0x1363,
       0x1367,
       0x14fe,
       0x1680,
       0x1802,
       0x1808,
       0x180e,
       0x2000,
       0x2001,
       0x2002,
       0x2003,
       0x2004,
       0x2005,
       0x2006,
       0x2007,
       0x2008,
       0x2009,
       0x200a,
       0x200b,
       0x2018,
       0x2019,
       0x201a,
       0x201b,
       0x201c,
       0x201d,
       0x201e,
       0x201f,
       0x2022,
       0x2027,
       0x202f,
       0x2039,
       0x203a,
       0x203b,
       0x203d,
       0x2047,
       0x2048,
       0x2049,
       0x204e,
       0x205f,
       0x2217,
       0x225f,
       0x2308,
       0x2309,
       0x2420,
       0x2422,
       0x2423,
       0x2722,
       0x2723,
       0x2724,
       0x2725,
       0x2731,
       0x2732,
       0x2733,
       0x273a,
       0x273b,
       0x273c,
       0x273d,
       0x2743,
       0x2749,
       0x274a,
       0x274b,
       0x2a7b,
       0x2a7c,
       0x2cfa,
       0x2e2e,
       0x2e2e,
       0x3000,
       0x3001,
       0x3008,
       0x3009,
       0x300a,
       0x300b,
       0x300c,
       0x300c,
       0x300d,
       0x300d,
       0x300e,
       0x300f,
       0x3010,
       0x3011,
       0x301d,
       0x301e,
       0x301f,
       0x30fb,
       0xa60d,
       0xa60f,
       0xa6f5,
       0xe0a0,
       0xe0b0,
       0xe0b2,
       0xfe10,
       0xfe41,
       0xfe42,
       0xfe43,
       0xfe44,
       0xfe50,
       0xfe51,
       0xfe56,
       0xfe61,
       0xfe62,
       0xfe63,
       0xfeff,
       0xff02,
       0xff07,
       0xff08,
       0xff09,
       0xff0a,
       0xff0c,
       0xff1b,
       0xff1c,
       0xff1e,
       0xff1f,
       0xff3b,
       0xff3d,
       0xff5b,
       0xff5d,
       0xff62,
       0xff63,
       0xff64,
       0xff65,
       0xe002a
   };
   size_t imax = (sizeof(authorized) / sizeof(authorized[0])) - 1,
          imin = 0;
   size_t imaxmax = imax;

   while (imax >= imin)
     {
        size_t imid = (imin + imax) / 2;

        if (authorized[imid] == codepoint) return true;
        else if (authorized[imid] < codepoint) imin = imid + 1;
        else imax = imid - 1;
        if (imax > imaxmax) break;
     }
   return false;
}

static void
_bench(bool (*f)(const unsigned int codepoint), const char *name)
{
   int i = 0;
   bool res;
   struct timespec start = {}, end = {};
   clockid_t clk_id = CLOCK_THREAD_CPUTIME_ID;

   assert (clock_gettime(clk_id, &start) == 0);

   for (i = 0; i < nb_codepoints; i++)
     {
        res ^= f(cps[i]);
     }
   assert (clock_gettime(clk_id, &end) == 0);
   fprintf(stderr, "%s: %ld ns\n",
           name, ((end.tv_sec - start.tv_sec) * 1000 * 1000 * 1000  + (end.tv_nsec - start.tv_nsec)));
}

static void
_setup(void)
{
   int i;
   cps = malloc(nb_codepoints * sizeof(cps[0]));
   assert(cps);
   for (i = 0; i < nb_codepoints; i++)
     {
        /* consider 2/3rd are pure ascii */
        if (rand()%3 <= 1)
          cps[i] = rand()% 256;
        else
          cps[i] = rand()% 0x31fff;
     }
}

int main(int argc, char *argv[])
{
   int seed = 0;

   if (argc > 1)
     seed = atoi(argv[1]);
   srand(seed);
   _setup();

   _bench(_switch_fallthrough, "       switch_fallthrough");
   _bench(_switch_fallthrough, "switch_fallthrough_ranges");
   _bench(_switch_return,      "            switch_return");
   _bench(_switch_return,      "     switch_return_ranges");
   _bench(_binary_search,      "            binary_search");

   _bench(_switch_fallthrough, "       switch_fallthrough");
   _bench(_switch_fallthrough, "switch_fallthrough_ranges");
   _bench(_switch_return,      "            switch_return");
   _bench(_switch_return,      "     switch_return_ranges");
   _bench(_binary_search,      "            binary_search");

   return 0;
}
