add_executable(pen_echo pen_echo.c)
add_executable(pen_keepalive_server pen_keepalive_server.c)
add_executable(pen_ping pen_ping.c)
add_executable(pen_ping_t pen_ping_t.c)
add_executable(pen_pong pen_pong.c)
add_executable(pen_pong_t pen_pong_t.c)
add_executable(pen_say pen_say.c)

install(TARGETS
    pen_echo
    pen_keepalive_server
    pen_ping
    pen_ping_t
    pen_pong
    pen_pong_t
    pen_say
)

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_executable(pen_keepalive_client pen_keepalive_client.c)
    install(TARGETS pen_keepalive_client)
endif()

