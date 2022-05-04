cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
project(FullTextFindSystem)

set(CMAKE_CXX_STANDARD 17)
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /JMC")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Werror -Wno-unused-parameter -Wno-implicit-fallthrough")
endif()

add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

file(GLOB sources *.cpp *.h)

add_executable(FullTextFindSystem ${sources})

if(MINGW)
  target_link_libraries(FullTextFindSystem "libtbb.dll.a" "libtbbmalloc.dll.a" "libtbbmalloc_proxy.dll.a")
endif(MINGW)
