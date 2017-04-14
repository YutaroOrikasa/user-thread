cmake_minimum_required(VERSION 2.0)
enable_language(ASM)

file(GLOB SRCS *.cpp *.s)
add_library(user_thread STATIC ${SRCS})
