cmake_minimum_required(VERSION 3.5)
project(my_context C)

enable_testing()

add_library(my_context src/ma_context.c)
target_include_directories(my_context PUBLIC include)

add_executable(context_test tests/context_test.c)
target_link_libraries(context_test my_context)
add_test(NAME context_test COMMAND context_test)
