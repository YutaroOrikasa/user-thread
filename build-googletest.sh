#!/bin/bash

# google test をビルドしlibgtest.aを生成します。
# はじめに一度だけ実行する必要があります。

set -xv

cd $(dirname "$0")

GTEST_DIR=googletest/googletest

g++ -isystem ${GTEST_DIR}/include -I${GTEST_DIR} \
    -pthread -c ${GTEST_DIR}/src/gtest-all.cc
ar -rv libgtest.a gtest-all.o
