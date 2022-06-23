@@
type T, B != T;
identifier func, i;
expression E;
@@

T func (...) {
...
(
T i;
|
T i = E;
|
static B i;
|
static B i = E;
|
-B i;
+T i;
|
-B i = E;
+T i = E;
)
<+...
(
return i;
|
return (T) i;
)
...+>
}



