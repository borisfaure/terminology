// DIV_ROUND_UP
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

// ROUND_UP
@@
expression n, d;
@@
(
- ((((n + d) - 1) / d) * d)
+ ROUND_UP(n, d)
|
- (((n + (d - 1)) / d) * d)
+ ROUND_UP(n, d)
|
- (DIV_ROUND_UP(n,d) * d)
+ ROUND_UP(n, d)
)

// MIN / MAX
@@
expression x, y;
@@
(
- (((x) > (y)) ? (y) : (x))
+ MIN(x, y)
|
- (((x) > (y)) ? (x) : (y))
+ MAX(x, y)
)
