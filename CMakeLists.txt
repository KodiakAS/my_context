cmake_minimum_required(VERSION 3.5)
project(my_context C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)

enable_testing()

option(WITH_ASAN "Build with address sanitizer" OFF)
if(WITH_ASAN)
  set(ASAN_FLAGS "-fsanitize=address -fsanitize=leak -fsanitize-address-use-after-scope -fno-omit-frame-pointer")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ASAN_FLAGS}")
endif()

add_library(my_context src/ma_context.cpp)
target_include_directories(my_context PUBLIC include)

find_package(Threads REQUIRED)

add_subdirectory(/usr/src/googletest build_gtest)

add_executable(my_context_test tests/my_context_test.cpp)
target_link_libraries(my_context_test PRIVATE my_context gtest_main Threads::Threads m)
add_test(NAME my_context_test COMMAND my_context_test)
