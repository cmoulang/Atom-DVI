#!/usr/bin/sh
~/cc64/cc65-2.19/bin/ca65 -l test.lis -t none test.s
~/cc64/cc65-2.19/bin/ld65 -o test -t none test.o
xxd -i -n _6502_prog test > test.h
