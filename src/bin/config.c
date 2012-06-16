#include <Elementary.h>
#include "config.h"

Config *config = NULL;

void
config_init(void)
{
   // XXX: need to load config and only if not found use this
   config = calloc(1, sizeof(Config));
//   config->font.bitmap = 0;
// config->font.name = eina_stringshare_add("Monospace");
   config->font.bitmap = 1;
   config->font.name = eina_stringshare_add("nex6x10.pcf");
   config->font.size = 10;
   config->scrollback = 4096;
   config->theme = "default.edj";
   config->jump_on_change = 1;
   config->wordsep = "'\"()[]{}=*!#$^\\:;,?` ";
}

void
config_shutdown(void)
{
   // XXX: free config
}
