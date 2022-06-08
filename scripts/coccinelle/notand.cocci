// !x&y combines boolean negation with bitwise and
//
// Confidence: High
// Copyright: (C) Gilles Muller, Julia Lawall, EMN, INRIA, DIKU.  GPLv2.
// URL: https://coccinelle.gitlabpages.inria.fr/website/rules/notand.html
// Options:

@@ expression E; constant C; @@
(
  !E & !C
|
- !E & C
+ !(E & C)
)
