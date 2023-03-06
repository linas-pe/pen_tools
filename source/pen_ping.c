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

#include <pen_log.h>
#include <pen_event.h>
#include <pen_signal.h>
#include <pen_options.h>
#include <pen_socket.h>
#include <pen_speed.h>
#include <pen_timer.h>

#define PONG "pong"
#define PONG_SIZE sizeof(PONG) - 1

static bool running = true;
static uint16_t port = 1234;
static uint16_t conn_num = 128;
static uint16_t group = 2;
static uint32_t count = 5000;
static const char *host = "127.0.0.1";

typedef struct {
    pen_event_base_t eb_;
    pen_event_t ev_;
    char buf_[PONG_SIZE];
    unsigned offset_;
    uint32_t count_;
    pen_speed_t *speeder_;
    bool connected_;
} pen_connector_t;

static void _on_close(pen_event_base_t *);
static void _on_read(pen_event_base_t *);
static bool _on_write(pen_event_base_t *);

static void
create_connector(pen_event_t ev, pen_connector_t *self, pen_speed_t *speeder)
{
    pen_assert2(pen_connect_tcp(&self->eb_, host, port));

    self->ev_ = ev;
    self->eb_.on_read_ = _on_read;
    self->eb_.on_write_ = _on_write;
    self->eb_.on_close_ = _on_close;
    self->speeder_ = speeder;

    pen_assert2(pen_event_add_rw(ev, (pen_event_base_t*)self));
}

static inline void
_init_options(int argc, char *argv[])
{
#define _i(a,b,d) {#a, &b, sizeof(b), PEN_OPTION_UINT16, d},
#define _li(a,b,d) {#a, &b, sizeof(b), PEN_OPTION_UINT32, d},
#define _s(a,b,d) {#a, &b, sizeof(b), PEN_OPTION_STRING, d},

    pen_option_t opts[] = {
        _i(--port, port, "port(default 1234)")
        _i(--conn, conn_num, "connector number(default 128)")
        _i(--group, group, "number of connector groups(default 2)")
        _li(--repeat, count, "request number(default 5000)")
        _s(--host, host, "remote host(default 127.0.0.1)")
    };

    PEN_OPTIONS_INIT(argc, argv, opts);
#undef _i
#undef _li
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
    pen_connector_t *self = (pen_connector_t*)eb;
    static uint32_t num = 0;
    static uint16_t gp = 0;
    pen_speed_t *speeder = self->speeder_;
    pen_event_t ev = self->ev_;

    pen_assert2(self->count_ == count);
    close(eb->fd_);
    if (++num == conn_num) {
        if (++gp >= group) {
            running = false;
            return;
        }
        num = 0;
    }

    memset(self, 0, sizeof(*self));
    create_connector(ev, self, speeder);
}

static void
_on_read(pen_event_base_t *eb)
{
    int ret;
    pen_connector_t *self = (pen_connector_t *)eb;
    ret = read(eb->fd_, self->buf_ + self->offset_, PONG_SIZE - self->offset_);

    if (ret <= 0) {
        PEN_WARN("read error!!!");
        _on_close(eb);
        return;
    }

    self->offset_ += ret;
    if (self->offset_ < PONG_SIZE)
        return;

    self->offset_ = 0;
    pen_assert2(strncmp(PONG, self->buf_, PONG_SIZE) == 0);

    pen_speed_add(self->speeder_, 1);

    self->count_ ++;
    if (self->count_ == count)
        _on_close(eb);
    else
        pen_assert2(write(eb->fd_, "ping", 4) == 4);
}

static bool
_on_write(pen_event_base_t *eb)
{
    pen_connector_t *self = (pen_connector_t *)eb;
    if (self->connected_)
        return true;
    self->connected_ = true;
    pen_assert2(write(eb->fd_, "ping", 4) == 4);
    pen_assert2(pen_event_mod_r(self->ev_, eb));
    return true;
}

static void
_on_timer(void *arg)
{
    pen_speed_t *speeder = arg;
    pen_speed_current(speeder);
}

int
main(int argc, char *argv[])
{
    pen_event_t ev;
    pen_connector_t *conns;
    pen_event_base_t *timer;
    pen_speed_t speeder;

    _init_options(argc, argv);

    ev = pen_event_init(16);
    pen_assert2(ev != NULL);

    pen_assert2(pen_signal_init(ev));
    pen_assert2(pen_signal(SIGTERM, _on_signal));
    pen_assert2(pen_signal(SIGINT, _on_signal));

    timer = pen_timer_init(ev, _on_timer, &speeder);
    pen_assert2(timer != NULL);

    conns = calloc(conn_num, sizeof(pen_connector_t));
    pen_assert2(conns != NULL);
    for (uint16_t i = 0; i < conn_num; i++)
        create_connector(ev, &conns[i], &speeder);

    pen_speed_init(&speeder, "client test");
    pen_timer_settime(timer, 10000);

    start_server(ev);

    pen_speed_end(&speeder);
    pen_timer_destroy(timer);
    pen_signal_destroy();
    pen_event_destroy(ev);
    free(conns);

    puts("exit.");
    return 0;
}

