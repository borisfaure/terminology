// Continue at the end of a for loop has no purpose
//
// Confidence: Moderate
// Copyright: (C) Gilles Muller, Julia Lawall, EMN, INRIA, DIKU.  GPLv2.
// URL: https://coccinelle.gitlabpages.inria.fr/website/rules/continue.html
// Options:

@@
@@

for (...;...;...) {
   ...
   if (...) {
     ...
-   continue;
   }
}
