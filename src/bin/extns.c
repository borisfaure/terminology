#include "private.h"
#include <Elementary.h>
#include <stdio.h>

const char *extn_img[] =
{
   ".png", ".jpg", ".jpeg", ".jpe", ".jfif", ".tif", ".tiff", ".gif", ".thm",
   ".bmp", ".ico", ".ppm", ".pgm", ".pbm", ".pnm", ".xpm", ".psd", ".wbmp",
   ".cur", ".xcf", ".xcf.gz", ".arw", ".cr2", ".crw", ".dcr", ".dng", ".k25",
   ".kdc", ".erf", ".mrw", ".nef", ".nrf", ".nrw", ".orf", ".raw", ".rw2",
   ".pef", ".raf", ".sr2", ".srf", ".x3f", ".webp", ".ppt", ".pptx", ".odp",
   ".tgv", ".tga", ".dds",
   NULL
};

const char *extn_scale[] =
{
   ".svg", ".svgz", ".svg.gz", /* ".ps", ".ps.gz", ".pdf", */
   NULL
};

const char *extn_edj[] =
{
   ".edj",
   NULL
};

const char *extn_mov[] =
{
   ".asf", ".avi", ".bdm", ".bdmv", ".clpi", ".cpi", ".dv", ".fla", ".flv",
   ".m1v", ".m2t", ".m2v", ".m4v", ".mkv", ".mov", ".mp2", ".mp2ts", ".mp4",
   ".mpe", ".mpeg", ".mpg", ".mpl", ".mpls", ".mts", ".mxf", ".nut", ".nuv",
   ".ogg", ".ogm", ".ogv", ".qt", ".rm", ".rmj", ".rmm", ".rms", ".rmvb",
   ".rmx", ".rv", ".swf", ".ts", ".weba", ".webm", ".wmv", ".3g2", ".3gp",
   ".3gp2", ".3gpp", ".3gpp2", ".3p2", ".264",
   ".mp3", ".aac", ".wav", ".flac", ".m4a", ".opus",
   NULL
};

const char *extn_aud[] =
{
   ".mp3", ".aac", ".wav", ".flac", ".m4a", ".opus",
   NULL
};

/**
 * Whether a path ends with one of the extensions listed in @extns
 */
Eina_Bool
extn_matches(const char *path, size_t path_len, const char **extns)
{
   int i;

   for (i = 0; extns[i]; i++)
     {
        size_t ext_len = strlen(extns[i]);
        if (path_len < ext_len)
          continue;
        if (!strcasecmp(extns[i], path + path_len - ext_len))
          return EINA_TRUE;
     }
   return EINA_FALSE;
}

/**
 * Whether a path is a media, if it ends with one of the known extensions
 */
Eina_Bool
extn_is_media(const char *path, size_t path_len)
{
   if (extn_matches(path, path_len, extn_img))
     return EINA_TRUE;
   if (extn_matches(path, path_len, extn_scale))
     return EINA_TRUE;
   if (extn_matches(path, path_len, extn_edj))
     return EINA_TRUE;
   if (extn_matches(path, path_len, extn_mov))
     return EINA_TRUE;
   if (extn_matches(path, path_len, extn_aud))
     return EINA_TRUE;
   return EINA_FALSE;
}

#if defined(BINARY_TYTEST)
#include <assert.h>
int
tytest_extn_matching(void)
{
   const char *invalid = "foobar.inv";
   assert(extn_is_media(invalid, strlen(invalid)) == EINA_FALSE);
   /* Images */
   const char *jpeg = "/home/qux/foo.bar.jpeg";
   assert(extn_is_media(jpeg, strlen(jpeg)) == EINA_TRUE);
   const char *png = "https://foo.bar/qux.PNG";
   assert(extn_is_media(png, strlen(png)) == EINA_TRUE);
   /* Scale */
   const char *svg = "https://foo.bar/qux.svg";
   assert(extn_is_media(svg, strlen(svg)) == EINA_TRUE);
   const char *svggz = "https://foo.bar/qux.svg.gz";
   assert(extn_is_media(svggz, strlen(svggz)) == EINA_TRUE);
   /* EDJ */
   const char *edj = "https://foo.bar/qux.edj";
   assert(extn_is_media(edj, strlen(edj)) == EINA_TRUE);
   /* Movie */
   const char *mkv = "https://foo.bar/qux.mkv";
   assert(extn_is_media(mkv, strlen(mkv)) == EINA_TRUE);
   /* Audio */
   const char *ogg = "https://foo.bar/qux.ogg";
   assert(extn_is_media(ogg, strlen(ogg)) == EINA_TRUE);
   return 0;
}
#endif
