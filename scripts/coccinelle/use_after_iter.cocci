@@
identifier Iter, Next, M, Data;
iterator name EINA_LIST_FOREACH;
iterator name EINA_LIST_REVERSE_FOREACH;
iterator name EINA_LIST_FREE;
iterator name EINA_INLIST_FOREACH;
iterator name EINA_INLIST_FOREACH_SAFE;
iterator name EINA_ITERATOR_FOREACH;
iterator name EINA_ARRAY_ITER_NEXT;
expression E,x, i;
position p1,p2;
statement S;
@@

(
EINA_INLIST_FOREACH(x, Data@p1) { ... when != break;
                                      when forall
                                      when strict
}
|
EINA_INLIST_FOREACH_SAFE(x, Data@p1) { ... when != break;
                                           when forall
                                           when strict
}
|
EINA_LIST_FOREACH(x, Iter@p1, Data@p1) { ... when != break;
                                             when forall
                                             when strict
}
|
EINA_LIST_REVERSE_FOREACH(x, Iter@p1, Data@p1) { ... when != break;
                                                     when forall
                                                     when strict
}
|
EINA_LIST_FREE(x, Data@p1) { ... when != break;
                                 when forall
                                 when strict
}
|
EINA_ARRAY_ITER_NEXT(x, i, Data@p1, Iter@p1) { ... when != break;
                                                  when forall
                                                  when strict
}
)
...
(
EINA_INLIST_FOREACH(x, Data) S
|
EINA_INLIST_FOREACH_SAFE(x, Data) S
|
EINA_LIST_FREE(x, Data) S
|
EINA_LIST_FOREACH(x, Iter, Data) S
|
EINA_LIST_REVERSE_FOREACH(x, Iter, Data) S
|
EINA_ARRAY_ITER_NEXT(x, i, Data, Iter) S
|
- *Next@p2
|
- *Iter@p2
|
- *Data@p2
|
- Data->M@p2
|
- E = Data@p2
)
