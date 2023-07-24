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
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include <pen_utils/pen_options.h>
#include <pen_socket/pen_event.h>
#include <pen_socket/pen_signal.h>
#include <pen_socket/pen_socket.h>

#define DATA \
    "CONNECT registry-1.docker.io:443 HTTP/1.1\r\n" \
    "Host: registry-1.docker.io:443\r\n" \
    "User-Agent: Go-http-client/1.1\r\n\r\n"
#define DATA_SIZE sizeof(DATA) - 1

bool running = true;
static uint16_t port = 8124;
static const char *host = "127.0.0.1";

typedef struct {
    pen_event_base_t eb_;
    bool connected_;
} pen_connector_t;

static void _on_event(pen_event_base_t *, uint16_t);

static void
create_connector(pen_event_t ev, pen_connector_t *self)
{
    pen_assert2(pen_connect_tcp(&self->eb_, host, port));

    self->eb_.on_event_ = _on_event;

    pen_assert2(pen_event_add_rw(ev, (pen_event_base_t*)self));
}

static inline void
_init_options(int argc, char *argv[])
{
#define _i(a,b,d) {#a, &b, sizeof(b), PEN_OPTION_UINT16, d},
#define _s(a,b,d) {#a, &b, sizeof(b), PEN_OPTION_STRING, d},

    pen_option_t opts[] = {
        _i(--port, port, "port(default 1234)")
        _s(--host, host, "remote host(default 127.0.0.1)")
    };

    PEN_OPTIONS_INIT(argc, argv, opts);
#undef _i
#undef _s
}

static void
_on_signal(int sig PEN_UNUSED)
{
    fflush(NULL);
    running = false;
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

static void
_on_close(pen_event_base_t *eb)
{
    close(eb->fd_);
    running = false;
}

static bool
_on_write(pen_event_base_t *eb)
{
    pen_connector_t *self = (pen_connector_t *)eb;
    if (self->connected_)
        return true;

    PEN_DEBUG("connected.\n");
    self->connected_ = true;
    pen_assert2(write(eb->fd_, DATA, DATA_SIZE) == DATA_SIZE);
    return true;
}

static void
_on_event(pen_event_base_t *eb, uint16_t pe)
{
    static char buf[10240];

    if (pe == PEN_EVENT_CLOSE)
        return _on_close(eb);

    if (pe & PEN_EVENT_WRITE)
        _on_write(eb);

    if ((pe & PEN_EVENT_READ) == 0)
        return;

    int ret = read(eb->fd_, buf, sizeof(buf) - 1);

    if (ret <= 0) {
        PEN_WARN("read error!!!");
        _on_close(eb);
        return;
    }

    buf[ret] = '\0';
    printf("%s\n", buf);
}

int
main(int argc, char *argv[])
{
    pen_event_t ev;
    pen_connector_t conns;

    _init_options(argc, argv);

    ev = pen_event_init(16);
    pen_assert2(ev != NULL);

    pen_assert2(pen_signal_init(ev));
    pen_assert2(pen_signal(SIGTERM, _on_signal));
    pen_assert2(pen_signal(SIGINT, _on_signal));

    memset(&conns, 0, sizeof(conns));
    create_connector(ev, &conns);

    start_server(ev);

    pen_signal_destroy();
    pen_event_destroy(ev);

    puts("exit.");
    return 0;
}

