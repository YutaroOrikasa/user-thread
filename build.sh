#!/bin/bash

set -xve

cd src

g++ -std=c++14 -I../include mysetjmp.s user-thread.cpp -lpthread -O3 -c

ar rcs ../libuser-thread.a mysetjmp.o user-thread.o
