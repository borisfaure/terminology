// A mutex_lock is not matched by a mutex_unlock before an error return/goto.
@@
expression l;
identifier LOCK =~ "^.*_lock$";
identifier UN =~ "^.*_unlock$";
@@

LOCK(l);
... when != UN(l)
    when any
    when strict
(
{ ... when != UN(l)
+   todo_add_unlock(l);
    return ...;
}
|
UN(l);
)


// unlock from not locked code
//@@
//expression E;
//identifier LOCK =~ "^.*_lock$";
//identifier UN =~ "^.*_unlock$";
//@@
//... when != LOCK(E)
//(
//if (...) {
//+add_LOCK(E);
//... when != LOCK(E)
// UN(E);
// return ...;
//}
//|
//UN(E)
//)
