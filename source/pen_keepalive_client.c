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
#include <sys/wait.h>

#include <pen_utils/pen_options.h>
#include <pen_utils/pen_profile.h>
#include <pen_socket/pen_event.h>
#include <pen_socket/pen_signal.h>
#include <pen_socket/pen_socket.h>
#include <pen_socket/pen_timer.h>
#include <pen_crypt/pen_aes.h>

#define PEN_CMD "/data/usr/bin/pen_update_ip"

static bool running = true;
static const char *host = "127.0.0.1";
static const char *passwd = NULL;
static uint16_t port = 1234;
static pen_event_t ev = NULL;
static pen_event_base_t *timer = NULL;
static pen_crypt_t enkey = NULL;
static pen_crypt_t dekey = NULL;

static inline bool
_init_profile(const char *profile)
{
#define _s(a,b,d) PEN_OPTIONS_ITEM(PEN_OPTION_STRING, a, b, d)
#define _i(a,b,d) PEN_OPTIONS_ITEM(PEN_OPTION_UINT16, a, b, d)

    pen_option_t opts[] = {
        _s(server, host, "server(define 127.0.0.1)")
        _i(port, port, "port(default 1234)")
        _s(password, passwd, "password")
        _s(log_info, __pen_log_filename, "log info file name(default NULL)")
        _s(log_err, __pen_err_filename, "log error file name(default NULL)")
    };

    return pen_profile_init(profile, opts, sizeof(opts) / sizeof(opts[0]));
#undef _s
#undef _i
}

static inline bool
_init_options(int argc, char *argv[])
{
    const char *profile = NULL;
#define _s(a,b,d) PEN_OPTIONS_ITEM(PEN_OPTION_STRING, a, b, d)
    pen_option_t opts[] = {
        _s(--config, profile, "")
    };

    PEN_OPTIONS_INIT(argc, argv, opts);
    if (profile == NULL)
        return false;

    return _init_profile(profile);
#undef _s
}

static void
_on_close(pen_event_base_t *eb)
{
    PEN_INFO("on close");
    close(eb->fd_);
    eb->fd_ = PEN_INVALID_SOCK;
    pen_timer_settime_once(timer, 1000);
}

static bool
_on_ip_changed(uint32_t ip)
{
    struct in_addr addr;
    int pid, wstatus = 0;

    addr.s_addr = ip;
    pid = fork();
    if (pid == -1) {
        PEN_ERROR("fork failed.");
        exit(1);
    }
    if (pid == 0) {
        if (execl(PEN_CMD, PEN_CMD, inet_ntoa(addr), NULL) == -1) {
            PEN_ERROR("execl failed.");
            exit(1);
        }
    }
    if (waitpid(pid, &wstatus, 0) == -1) {
        PEN_ERROR("waitpid failed");
        exit(1);
    }
    if (!WIFEXITED(wstatus)) {
        PEN_ERROR("execl failed with unknown status");
        return false;
    }

    wstatus = WEXITSTATUS(wstatus);
    return wstatus == 0;
}

static inline void
_send_auth_data(pen_event_base_t *eb)
{
    pen_aes_data_t data;
    data.llu_[0] = time(NULL);
    data.u_[2] = 0x1c2b8695;
    data.u_[3] = 0;

    pen_crypt_aes_encrypt(enkey, &data);
    pen_assert2(write(eb->fd_, (void*)&data, sizeof(data)) == sizeof(data));
}

static bool
_on_write(pen_event_base_t *eb)
{
    if (eb->user_ == NULL) {
        PEN_INFO("connect to server.");
        pen_assert2(pen_set_keepalive(eb->fd_, 3, 60, 20));
        _send_auth_data(eb);
        eb->user_ = (void*)1;
    }
    return true;
}

static void
_on_event(pen_event_base_t *eb, uint16_t pe)
{
    uint64_t buf[3];
    pen_aes_data_t *data;

    if (pe == PEN_EVENT_CLOSE)
        return _on_close(eb);

    if (pe & PEN_EVENT_WRITE)
        _on_write(eb);

    if ((pe & PEN_EVENT_READ) == 0)
        return;

    int ret = read(eb->fd_, buf, sizeof(buf));
    if (ret == 0)
        goto error;
    if (ret != sizeof(uint64_t) * 2) {
        PEN_WARN("unknown data received!");
        goto error;
    }
    data = (pen_aes_data_t*)buf;
    pen_crypt_aes_decrypt(dekey, data);
    if (data->u_[2] != 0)
        goto error;

    while (!_on_ip_changed(data->u_[3]))
        sleep(1);
    return;
error:
    _on_close(eb);
}

static void
_stop_connector(pen_event_base_t *eb)
{
    if (eb->fd_ != PEN_INVALID_SOCK) {
        close(eb->fd_);
        eb->fd_ = PEN_INVALID_SOCK;
    }
}

static void
_start_connector(pen_event_base_t *eb)
{
    PEN_INFO("start connector.");
    memset(eb, 0, sizeof(*eb));
    pen_assert2(pen_connect_tcp(eb, host, port));

    pen_assert2(pen_set_sockopt(eb->fd_, SO_RCVLOWAT, sizeof(uint64_t) * 2));
    pen_assert2(pen_set_sockopt(eb->fd_, SO_RCVBUF, sizeof(uint64_t) * 3));
    pen_assert2(pen_set_sockopt(eb->fd_, SO_SNDBUF, sizeof(uint64_t) * 3));

    eb->on_event_ = _on_event;

    pen_assert2(pen_event_add_rw(ev, eb));
}

static void
_on_signal(int sig PEN_UNUSED)
{
    fflush(NULL);
    running = false;
}

static void
_on_timer(void *user)
{
    _start_connector((pen_event_base_t*)user);
}

static void
start_server(void)
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
    pen_event_base_t conn;

    _init_options(argc, argv);

    pen_assert2(pen_log_init());
    pen_assert2(passwd != NULL);

    enkey = pen_crypt_aes_encrypt_init((uint8_t*)passwd);
    pen_assert2(enkey != NULL);
    dekey = pen_crypt_aes_decrypt_init((uint8_t*)passwd);
    pen_assert2(dekey != NULL);

    ev = pen_event_init(8);
    pen_assert2(ev != NULL);

    pen_assert2(pen_signal_init(ev));
    pen_assert2(pen_signal(SIGTERM, _on_signal));
    pen_assert2(pen_signal(SIGINT, _on_signal));

    timer = pen_timer_init(ev, _on_timer, &conn);
    pen_assert2(timer != NULL);

    _start_connector(&conn);

    start_server();

    pen_timer_destroy(timer);
    _stop_connector(&conn);
    pen_signal_destroy();
    pen_event_destroy(ev);
    pen_crypt_aes_destroy(enkey);
    pen_crypt_aes_destroy(dekey);
    pen_log_destroy();
    return 0;
}

