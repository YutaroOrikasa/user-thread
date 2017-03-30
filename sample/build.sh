#!/bin/bash
set -e
(cd ..; ./build.sh)
g++ -std=c++14  -I.. fibo.cpp ../libuser-thread.a -lpthread -o fibo -O3
