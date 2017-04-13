#!/bin/bash
set -e

cd $(dirname "$0")

(cd ..; ./build.sh)
g++ -std=c++14  -I../include fibo.cpp ../libuser-thread.a -lpthread -o fibo -O3
