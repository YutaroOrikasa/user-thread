#!/bin/bash

g++ -std=c++14 mysetjmp.s user-thread.cpp -lpthread -O3 -c

ar rcs libuser-thread.a mysetjmp.o user-thread.o
