#/bin/bash

I=0
while [ 1 ]; do
    sleep 1
    echo $I
    I=`expr $I + 1`
done
