#!/bin/bash

set -xv

cd $(dirname "$0")

GTEST_DIR=googletest/googletest

g++ -std=gnu++14 -isystem ${GTEST_DIR}/include -I./include -pthread test/test.cpp libuser-thread.a libgtest.a -o test_
