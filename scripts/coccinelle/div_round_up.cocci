@@
expression n, d;
@@

(
- (((n + d) - 1) / d)
+ DIV_ROUND_UP(n, d)
|
- ((n + (d - 1)) / d)
+ DIV_ROUND_UP(n, d)
)
