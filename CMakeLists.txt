cmake_minimum_required(VERSION 3.18)
project(pen_tools VERSION 0.1.0)

INCLUDE (cmake/pen.cmake)

pen_package_check("pen_socket")
pen_package_check("pen_utils")

add_subdirectory(source)

configure_file (
  "${PROJECT_SOURCE_DIR}/pen_config.h.in"
  "${PROJECT_SOURCE_DIR}/include/pen_config.h"
)

