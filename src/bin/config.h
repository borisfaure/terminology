typedef struct _Config Config;

struct _Config
{
   struct {
      const char    *name;
      int            size;
      unsigned char  bitmap;
   } font;
   int               scrollback;
   const char       *theme;
   unsigned char     jump_on_change;
   const char       *wordsep;
};

extern Config *config;

void config_init(void);
void config_shutdown(void);
    
