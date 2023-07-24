/*
 * Copyright (C) 2020  Linas <linas@justforfun.cn>
 * Author: linas <linas@justforfun.cn>
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

#include <pen_utils/pen_options.h>
#include <pen_utils/pen_profile.h>
#include <pen_utils/pen_memory_pool.h>
#include <pen_socket/pen_event.h>
#include <pen_socket/pen_listener.h>
#include <pen_socket/pen_signal.h>
#include <pen_socket/pen_socket.h>
#include <pen_crypt/pen_aes.h>

typedef struct {
    pen_event_base_t eb_;
    uint32_t ip_;
} pen_client_t;

static const char *profile = NULL;
static bool running = true;
static uint16_t port = 1234;
static const char *passwd = NULL;
static pen_memory_pool_t pool = NULL;
static pen_crypt_t enkey = NULL;
static pen_crypt_t dekey = NULL;
#define MAX_CLIENT_NUMBER 1
static pen_client_t *clients[MAX_CLIENT_NUMBER];

static inline void
_init_options(int argc, char *argv[])
{
#define _s(a,b,d) PEN_OPTIONS_ITEM(PEN_OPTION_STRING, a, b, d)

    pen_option_t opts[] = {
        _s(--config, profile, "profile file name(default NULL)")
    };

    PEN_OPTIONS_INIT(argc, argv, opts);
#undef _s
}

static inline bool
_init_profile(void)
{
#define _s(a,b,d) PEN_OPTIONS_ITEM(PEN_OPTION_STRING, a, b, d)
#define _i(a,b,d) PEN_OPTIONS_ITEM(PEN_OPTION_UINT16, a, b, d)

    pen_assert2(profile != NULL);

    pen_option_t opts[] = {
        _i(port, port, "port(default 1234)")
        _s(password, passwd, "password")
        _s(log_info, __pen_log_filename, "log info file name(default NULL)")
        _s(log_err, __pen_err_filename, "log error file name(default NULL)")
    };

    return pen_profile_init(profile, opts, sizeof(opts) / sizeof(opts[0]));
#undef _i
#undef _s
}

static void
_on_close(pen_event_base_t *eb)
{
    pen_client_t *self = (pen_client_t*)eb;

    PEN_INFO("client closed: %s", pen_ntop(&self->ip_));
    eb->fd_ = -1;
}

static inline void
_on_client_ip_changed(pen_client_t *self)
{
    pen_aes_data_t data;
    data.llu_[0] = time(NULL);
    data.u_[2] = 0;
    data.u_[3] = self->ip_;

    pen_crypt_aes_encrypt(enkey, &data);
    pen_assert2(write(self->eb_.fd_, &data, sizeof(data)) == sizeof(data));
}

static void
_on_event(pen_event_base_t *eb, uint16_t pe)
{
    uint64_t buf[3];
    pen_client_t *self = (pen_client_t*)eb;
    pen_aes_data_t *data;
    int idx;

    if (pe == PEN_EVENT_CLOSE)
        return _on_close(eb);

    int ret = read(eb->fd_, buf, sizeof(buf));
    if (ret == 0)
        goto error;
    if (ret != sizeof(uint64_t) * 2) {
        PEN_WARN("unknown data received!");
        goto error;
    }

    data = (pen_aes_data_t*)buf;
    pen_crypt_aes_decrypt(dekey, data);
    if (data->u_[2] != 0x1c2b8695 || data->u_[3] >= MAX_CLIENT_NUMBER)
        goto error;
    idx = data->u_[3];
    if (clients[idx] == NULL || self->ip_ != clients[idx]->ip_)
        _on_client_ip_changed(self);
    if (clients[idx] != NULL) {
        if (clients[idx]->eb_.fd_ != -1)
            close(clients[idx]->eb_.fd_);
        pen_memory_pool_put(pool, clients[idx]);
    }
    clients[idx] = self;
    return;
error:
    close(eb->fd_);
    _on_close(eb);
}

static pen_event_base_t *
on_new_client(pen_event_t ev,
              pen_socket_t fd,
              void *user PEN_UNUSED,
              struct sockaddr_in *addr)
{
    pen_client_t *client = pen_memory_pool_get(pool);
    pen_assert2(client != NULL);
    pen_event_base_t *eb = &client->eb_;

    pen_assert2(pen_set_sockopt(fd, SO_RCVLOWAT, sizeof(uint64_t) * 2));
    pen_assert2(pen_set_sockopt(fd, SO_RCVBUF, sizeof(uint64_t) * 3));
    pen_assert2(pen_set_sockopt(fd, SO_SNDBUF, sizeof(uint64_t) * 3));

    eb->fd_ = fd;
    eb->on_event_= _on_event;
    client->ip_ = addr->sin_addr.s_addr;

    pen_assert2(pen_event_add_r(ev, eb));
    return eb;
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

int
main(int argc, char *argv[])
{
    pen_listener_t listener = NULL;
    pen_event_t ev = NULL;

    _init_options(argc, argv);

    pen_assert2(_init_profile());

    pen_assert2(pen_log_init());
    pen_assert2(passwd != NULL);
    enkey = pen_crypt_aes_encrypt_init((uint8_t*)passwd);
    pen_assert2(enkey != NULL);
    dekey = pen_crypt_aes_decrypt_init((uint8_t*)passwd);
    pen_assert2(dekey != NULL);

    pool = PEN_MEMORY_POOL_INIT(8, pen_client_t);
    pen_assert2(pool != NULL);

    ev = pen_event_init(8);
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
    pen_memory_pool_destroy(pool);
    pen_crypt_aes_destroy(enkey);
    pen_crypt_aes_destroy(dekey);
    pen_log_destroy();
    return 0;
}

