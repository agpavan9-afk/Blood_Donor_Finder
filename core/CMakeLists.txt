cmake_minimum_required(VERSION 3.16)
project(bdf_matcher C)

# Red-Black Tree + Skip List + Matcher library
add_library(bdf_matcher STATIC matcher.c rbtree.c skiplist.c)
target_include_directories(bdf_matcher PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(bdf_matcher PUBLIC c_std_11)

# CLI adapter
add_executable(bdf_matcher_cli matcher_cli.c)
target_link_libraries(bdf_matcher_cli PRIVATE bdf_matcher)

if(NOT MSVC)
  target_compile_options(bdf_matcher PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(bdf_matcher_cli PRIVATE -Wall -Wextra -Wpedantic)
  target_link_libraries(bdf_matcher PUBLIC m)
endif()
