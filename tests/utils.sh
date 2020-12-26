#!/bin/sh

test_sleep()
{
    #only sleep when running the test within terminology with test ui on
    if [ "$IN_TY_TEST_UI" = "1" ]; then
        sleep "$1"
    fi
}
