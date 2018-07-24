#!/bin/bash

I=0
while [ 1 ]; do
    sleep 1
    echo $I
    I=$(($I + 1))
done
