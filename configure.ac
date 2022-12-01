#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# Copyright (C) 2020 by linas<linas@justforfun.cn>

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

AC_PREREQ([2.69])

m4_include([pen.m4])

m4_define([v_major], [0]) # v_major = v_current - 1
m4_define([v_minor], [0])
m4_define([v_micro], [1])
PEN_INIT([pen_tools], [source/pen_echo.c])

PEN_CHECK_HOST
PEN_DEFAULT_ARGS
pen_includes="-I${abs_srcdir}/include"
PEN_INCLUDE_DIR
PEN_DEFAULT_FLAGS

PEN_COMMON_DEP_CHECK

# Checks for programs.

# Checks for libraries.
PEN_CHECK_LIB_UTILS
PEN_CHECK_LIB_SOCKET

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

# Export varible
PEN_EXPORT_VARS([PEN_TOOLS_VERSION])

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 source/Makefile])
AC_OUTPUT
