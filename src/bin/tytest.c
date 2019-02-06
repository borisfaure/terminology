#define TYTEST 1
#include "tyfuzz.c"

const char *_cursor_shape = "undefined";

typedef struct _Termpty_Tests
{
   size_t backsize, backpos;
   Backlog_Beacon backlog_beacon;
   Term_State termstate;
   Term_Cursor cursor_state;
   Term_Cursor cursor_save[2];
   int w, h;
   unsigned int altbuf     : 1;
   unsigned int mouse_mode : 3;
   unsigned int mouse_ext  : 2;
   unsigned int bracketed_paste : 1;
} Termpty_Tests;

Evas_Object *
termio_textgrid_get(const Evas_Object *obj EINA_UNUSED)
{
   return NULL;
}

void
test_textgrid_palette_get(const Evas_Object *obj EINA_UNUSED,
                          Evas_Textgrid_Palette pal,
                          int idx,
                          int *r,
                          int *g,
                          int *b,
                          int *a)
{
   if (pal == EVAS_TEXTGRID_PALETTE_EXTENDED)
     {
        colors_256_get(idx,
                       (unsigned char *)r,
                       (unsigned char *)g,
                       (unsigned char *)b,
                       (unsigned char *)a);
     }
   else
     {
        int set = idx / 12;
        int col = idx % 12;
        colors_standard_get(set, col,
                            (unsigned char*)r,
                            (unsigned char*)g,
                            (unsigned char*)b,
                            (unsigned char*)a);
     }
}

void
termio_set_cursor_shape(Evas_Object *obj EINA_UNUSED,
                        Cursor_Shape shape EINA_UNUSED)
{
   switch (shape)
     {
      case CURSOR_SHAPE_UNDERLINE:
         _cursor_shape = "underline";
         break;
      case CURSOR_SHAPE_BAR:
         _cursor_shape = "bar";
         break;
      default:
      case CURSOR_SHAPE_BLOCK:
         _cursor_shape = "block";
     }
}

static void
_termpty_to_termpty_tests(Termpty *ty, Termpty_Tests *tt)
{
   memset(tt, '\0', sizeof(*tt));
   tt->backsize = ty->backsize;
   tt->backpos = ty->backpos;
   tt->backlog_beacon = ty->backlog_beacon;
   tt->termstate = ty->termstate;
   tt->cursor_state = ty->cursor_state;
   tt->cursor_save[0] = ty->cursor_save[0];
   tt->cursor_save[1] = ty->cursor_save[1];
   tt->w = ty->w;
   tt->h = ty->h;
   tt->altbuf = ty->altbuf;
   tt->mouse_mode = ty->mouse_mode;
   tt->mouse_ext = ty->mouse_ext;
   tt->bracketed_paste = ty->bracketed_paste;
}

static void
_tytest_checksum(Termpty *ty)
{
   MD5_CTX ctx;
   Termpty_Tests tests;
   char md5out[(2 * MD5_HASHBYTES) + 1];
   unsigned char hash[MD5_HASHBYTES];
   static const char hex[] = "0123456789abcdef";
   int n;

   _termpty_to_termpty_tests(ty, &tests);

   MD5Init(&ctx);
   /* Termpty */
   MD5Update(&ctx,
             (unsigned char const*)&tests,
             sizeof(tests));
   /* The screens */
   MD5Update(&ctx,
             (unsigned char const*)ty->screen,
             sizeof(Termcell) * ty->w * ty->h);
   MD5Update(&ctx,
             (unsigned char const*)ty->screen2,
             sizeof(Termcell) * ty->w * ty->h);
   /* Icon/Title */
   if (ty->prop.icon)
     {
        MD5Update(&ctx,
                  (unsigned char const*)ty->prop.icon,
                  strlen(ty->prop.icon));
     }
   else
     {
        MD5Update(&ctx, (unsigned char const*)"(NULL)", 6);
     }
   if (ty->prop.title)
     {
        MD5Update(&ctx,
                  (unsigned char const*)ty->prop.title,
                  strlen(ty->prop.title));
     }
   else
     {
        MD5Update(&ctx, (unsigned char const*)"(NULL)", 6);
     }
   /* Cursor shape */
   MD5Update(&ctx, (unsigned char const*)_cursor_shape,
             strlen(_cursor_shape));
   /* Write buffer */
   MD5Update(&ctx, (unsigned char const*)ty->write_buffer.buf,
             ty->write_buffer.len);

   MD5Final(hash, &ctx);

   for (n = 0; n < MD5_HASHBYTES; n++)
     {
        md5out[2 * n] = hex[hash[n] >> 4];
        md5out[2 * n + 1] = hex[hash[n] & 0x0f];
     }
   md5out[2 * MD5_HASHBYTES] = '\0';
   printf("%s", md5out);
}
