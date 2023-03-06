set(PROJECT_NAME "pen_tools")
project(${PROJECT_NAME})
cmake_minimum_required(VERSION 3.18)

INCLUDE (include/pen.cmake)

add_subdirectory(source)

configure_file (
  "${PROJECT_SOURCE_DIR}/pen_config.h.in"
  "${PROJECT_SOURCE_DIR}/include/pen_config.h"
)

