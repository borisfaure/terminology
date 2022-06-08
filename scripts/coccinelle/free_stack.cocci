// For fun, but gcc actually detects those by itself
@@
type T;
identifier K;
expression E;
@@
(
(
T K;
|
T K = E;
)
...
- free(&K);
)
