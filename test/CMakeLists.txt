cmake_minimum_required(VERSION 2.0)

set(TARGET_NAME test)

file(GLOB SRCS *.cpp)
include_directories(${CMAKE_SOURCE_DIR}/googletest/googletest/include)
add_executable(${TARGET_NAME} ${SRCS})
target_link_libraries(${TARGET_NAME} pthread user_thread gtest)
