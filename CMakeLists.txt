cmake_minimum_required(VERSION 3.5)
project(my_context C)

enable_testing()

option(WITH_ASAN "Build with address sanitizer" OFF)
if(WITH_ASAN)
  set(ASAN_FLAGS "-fsanitize=address -fsanitize=leak -fsanitize-address-use-after-return=always -fno-omit-frame-pointer")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ASAN_FLAGS}")
endif()

add_library(my_context src/ma_context.c)
target_include_directories(my_context PUBLIC include)

add_executable(context_test tests/context_test.c)
target_link_libraries(context_test my_context)
add_test(NAME context_test COMMAND context_test)

add_executable(leak_test tests/leak_test.c)
target_link_libraries(leak_test my_context)

add_executable(stack_uaf_test tests/stack_uaf_test.c)
target_link_libraries(stack_uaf_test my_context)
