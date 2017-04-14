cmake_minimum_required(VERSION 2.0)
file(GLOB SRCS *.cpp)
add_executable(fibo ${SRCS})
target_link_libraries(fibo pthread user_thread)
