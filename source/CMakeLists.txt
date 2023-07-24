add_executable(pen_echo pen_echo.c)
add_executable(pen_keepalive_server pen_keepalive_server.c)
add_executable(pen_ping pen_ping.c)
add_executable(pen_pong pen_pong.c)
add_executable(pen_say pen_say.c)

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_executable(pen_keepalive_client pen_keepalive_client.c)
    pen_package_check_target(pen_crypt pen_keepalive_client)
    install(TARGETS pen_keepalive_client)
endif()

pen_package_check_target(pen_crypt pen_keepalive_server)
pen_package_check_target(pen_test pen_ping)

install(TARGETS
    pen_echo
    pen_keepalive_server
    pen_ping
    pen_pong
    pen_say
)


