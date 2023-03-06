/*
 * Copyright (C) 2020  linas <linas@justforfun.cn>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <errno.h>
#include <signal.h>

#include <pen_event.h>
#include <pen_socket.h>
#include <pen_listener.h>
#include <pen_log.h>
#include <pen_signal.h>
#include <pen_options.h>

static bool running = true;
static uint16_t port = 1234;

static inline void
_init_options(int argc, char *argv[])
{
#define _i(a,b,d) {#a, &b, sizeof(b), PEN_OPTION_UINT16, d},

    pen_option_t opts[] = {
        _i(--port, port, "port(default 1234)")
    };

    PEN_OPTIONS_INIT(argc, argv, opts);
#undef _i
}

static void
_on_signal(int sig PEN_UNUSED)
{
    fflush(NULL);
    running = false;
}

static void
on_close(pen_event_base_t *eb)
{
    PEN_INFO("client closed.");
    free(eb);
}

static void
do_client(pen_event_base_t *eb)
{
#define PEN_BUF_SIZE 10240
    static char buf[PEN_BUF_SIZE];
    int ret = 0;

again:
    ret = read(eb->fd_, buf, PEN_BUF_SIZE);
    if (ret == 0)
        goto end;

    if (ret < 0) {
        perror("unknown error");
        goto end;
    }

    printf("%.*s", ret, buf);
    if (ret == PEN_BUF_SIZE)
        goto again;

    return;
end:
    PEN_INFO("client closed.");
    close(eb->fd_);
    free(eb);
}

static bool
on_new_client(pen_event_t ev,
              pen_socket_t fd,
              void *user PEN_UNUSED,
              struct sockaddr_in *addr PEN_UNUSED)
{
    pen_event_base_t *eb = NULL;

    eb = calloc(1, sizeof(*eb));
    eb->fd_ = fd;
    eb->on_read_ = do_client;
    eb->on_close_ = on_close;

    pen_assert2(pen_event_add_r(ev, eb));

    return true;
}

static void
start_server(pen_event_t ev)
{
    int ret = 0;

    do {
        fflush(NULL);
        ret = pen_event_wait(ev, -1);
    } while (running && ret >= 0);
}

int
main(int argc, char *argv[])
{
    pen_listener_t listener;
    pen_event_t ev;

    _init_options(argc, argv);

    ev = pen_event_init(16);
    pen_assert2(ev != NULL);

    pen_assert2(pen_signal_init(ev));
    pen_assert2(pen_signal(SIGTERM, _on_signal));
    pen_assert2(pen_signal(SIGINT, _on_signal));

    listener = pen_listener_init(ev, NULL, port, 10, on_new_client, NULL);
    pen_assert2(listener != NULL);

    start_server(ev);

    pen_listener_destroy(listener);
    pen_signal_destroy();
    pen_event_destroy(ev);
    puts("exit.\n");

    return 0;
}

