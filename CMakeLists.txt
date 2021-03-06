cmake_minimum_required(VERSION 2.0)
project(user_thread)

set(CMAKE_VERBOSE_MAKEFILE TRUE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG  -march=native")

# set(CMAKE_CXX_FLAGS_DEBUG "-g")
# set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG -march=native")
# set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g3 -Og -pg")
# set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG -march=native")

option(USE_SPLITSTACKS "splitstacks integration" OFF)
if(USE_SPLITSTACKS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsplit-stack")
endif()

option(USE_GOLD "use gold linker" OFF)
if(USE_GOLD)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=gold")
endif()

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
                ${CMAKE_CURRENT_SOURCE_DIR}/include/config.h)

set(CMAKE_CXX_STANDARD 14)
include_directories("include")
add_subdirectory(src)
add_subdirectory(googletest)
add_subdirectory(test)
add_subdirectory(sample)


