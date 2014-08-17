#!/usr/bin/env perl
use Time::HiRes qw(sleep);
$|++;
$A=0;
$F=0.1;
while (1) {
    $A == 628318 ? $A=0 : ++$A;
    ($R, $B, $G) = map { sin($F*$A + $_) * 127 + 128 } qw(0 2 4);
    printf "\033]10;#%02x%02x%02x\007", $R, $B, $G;
    sleep 0.01;
}
