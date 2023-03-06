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

#include <pen_log.h>
#include <pen_event.h>
#include <pen_signal.h>
#include <pen_options.h>
#include <pen_memory_lpool.h>
#include <pen_listener.h>
#include <pen_thread_pool.h>
#include <pen_thread.h>

#define PING "ping"
#define PING_SIZE sizeof(PING) - 1

static bool running = true;
static uint16_t port = 1234;
static uint16_t pool_size = 8;
static uint16_t thread_num = 8;

static pen_memory_lpool_t pool;
static pen_thread_pool_t tpool;

typedef struct {
    pen_event_base_t eb_;
    char buf_[PING_SIZE];
    unsigned offset_;
} pen_client_t;

static inline void
_init_options(int argc, char *argv[])
{
#define _i(a,b,d) {#a, &b, sizeof(b), PEN_OPTION_UINT16, d},

    pen_option_t opts[] = {
        _i(--port, port, "port(default 1234)")
        _i(--pool, pool_size, "pool size(default 8)")
        _i(--thread, thread_num, "thread number(default 8)")
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
_on_close(pen_event_base_t *eb)
{
    pen_memory_lpool_put(pool, eb);
}

static void
_on_read(pen_event_base_t *eb)
{
    int ret;
    pen_client_t *self = (pen_client_t *)eb;
    ret = read(eb->fd_, self->buf_ + self->offset_, PING_SIZE - self->offset_);

    if (ret <= 0) {
        PEN_WARN("server read error!!! %d", ret);
        close(eb->fd_);
        _on_close(eb);
        return;
    }

    self->offset_ += ret;
    if (self->offset_ < PING_SIZE)
        return;

    self->offset_ = 0;
    pen_assert2(strncmp(PING, self->buf_, PING_SIZE) == 0);

    pen_assert2(write(eb->fd_, "pong", 4) == 4);
}

static bool
on_new_client(pen_event_t ev PEN_UNUSED,
              pen_socket_t fd, void *user,
              struct sockaddr_in *addr PEN_UNUSED)
{
    static uint32_t idx = 0;
    pen_client_t *self = NULL;
    pen_event_t *tevs = user;

    self = pen_memory_lpool_get(pool);
    pen_assert2(self != NULL);
    self->eb_.fd_ = fd;
    self->eb_.on_read_ = _on_read;
    self->eb_.on_close_ = _on_close;

    pen_assert2(pen_event_add_r(tevs[idx], (pen_event_base_t*)self));
    idx ++;
    if (idx == thread_num)
        idx = 0;
    return true;
}

static void
start_server(pen_event_t ev)
{
    int ret = 0;

    do {
        fflush(NULL);
        ret = pen_event_wait(ev, 1000);
    } while (running && ret >= 0);
}

static void
_thread_main(void *user, int tid)
{
    start_server(((pen_event_t*)user)[tid]);
    PEN_INFO("thread %d exit", tid);
}

int
main(int argc, char *argv[])
{
    pen_event_t ev;
    pen_listener_t listener;
    pen_event_t *tevs;

    _init_options(argc, argv);

    pool = PEN_MEMORY_LPOOL_INIT(pool_size, pen_client_t);
    pen_assert2(pool != NULL);

    ev = pen_event_init(1);
    pen_assert2(ev != NULL);

    tevs = calloc(thread_num, sizeof(pen_event_t));
    pen_assert2(tevs != NULL);

    for (uint16_t i = 0; i < thread_num; i++) {
        tevs[i] = pen_event_init(128);
        pen_assert2(tevs[i] != NULL);
    }

    pen_assert2(pen_signal_init(ev));
    pen_assert2(pen_signal(SIGTERM, _on_signal));
    pen_assert2(pen_signal(SIGINT, _on_signal));

    listener = pen_listener_init(ev, NULL, port, 128, on_new_client, tevs);
    pen_assert2(listener != NULL);

    tpool = pen_thread_pool_init(thread_num, 0, _thread_main, NULL, NULL, tevs);
    pen_assert2(tpool != NULL);

    pen_assert2(pen_thread_pool_start(tpool));
    start_server(ev);
    PEN_INFO("main thread exit");

    pen_thread_pool_wait(tpool);
    pen_listener_destroy(listener);
    pen_signal_destroy();
    pen_event_destroy(ev);
    pen_memory_lpool_destroy(pool);
    puts("exit.");

    return 0;
}

