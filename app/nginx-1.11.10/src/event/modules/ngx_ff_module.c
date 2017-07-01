/*
 * Inspired by nginx_ofp's ngx_ofp_module.c.
 * https://github.com/OpenFastPath/nginx_ofp.
 * But According to #42, nginx_ofp's ngx_ofp_module.c may be derived from https://github.com/opendp/dpdk-nginx/blob/master/src/event/modules/ans_module.c,
 * so add a license of opendp/dpdk-nginx.
 */

/*-
BSD LICENSE
Copyright(c) 2015-2017 Ansyun anssupport@163.com. All rights reserved.
All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.
Neither the name of Ansyun anssupport@163.com nor the names of its
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
Author: JiaKai (jiakai1000@gmail.com) and Bluestar (anssupport@163.com)
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <netinet/in.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/select.h>

#include "ff_api.h"
#include "ff_config.h"

#define _GNU_SOURCE
#define __USE_GNU

#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <dlfcn.h>

#define FF_FD_BITS 16
#define CHK_FD_BIT(fd)          (fd & (1 << FF_FD_BITS))
#define CLR_FD_BIT(fd)          (fd & ~(1 << FF_FD_BITS))

static int (*real_close)(int);
static int (*real_socket)(int, int, int);
static int (*real_bind)(int, const struct sockaddr*, socklen_t);
static int (*real_connect)(int, const struct sockaddr*, socklen_t);
static int (*real_listen)(int, int);
static int (*real_setsockopt)(int, int, int, const void *, socklen_t);

static int (*real_accept)(int, struct sockaddr *, socklen_t *);
static int (*real_accept4)(int, struct sockaddr *, socklen_t *, int);
static ssize_t (*real_recv)(int, void *, size_t, int);
static ssize_t (*real_send)(int, const void *, size_t, int);
static ssize_t (*real_writev)(int, const struct iovec *, int);

static int (*real_ioctl)(int, int, void *);

static int (*real_select) (int, fd_set *, fd_set *, fd_set *, struct timeval *);

void
ff_mod_init(int argc, char * const *argv) {
    int rc;

#define INIT_FUNCTION(func) \
    real_##func = dlsym(RTLD_NEXT, #func); \
    assert(real_##func)

    INIT_FUNCTION(socket);
    INIT_FUNCTION(bind);
    INIT_FUNCTION(connect);
    INIT_FUNCTION(close);
    INIT_FUNCTION(listen);
    INIT_FUNCTION(setsockopt);
    INIT_FUNCTION(accept);
    INIT_FUNCTION(accept4);
    INIT_FUNCTION(recv);
    INIT_FUNCTION(send);
    INIT_FUNCTION(writev);

    INIT_FUNCTION(ioctl);
    INIT_FUNCTION(select);

#undef INIT_FUNCTION

    assert(argc >= 2);

    rc = ff_init(argv[1], argc, argv);
    assert(0 == rc);
}

int
socket(int domain, int type, int protocol)
{
    int rc;

    if ((AF_INET != domain) || (SOCK_STREAM != type && SOCK_DGRAM != type)) {
        rc = real_socket(domain, type, protocol);
        return rc;
    }

    rc = ff_socket(domain, type, protocol);
    if(rc >= 0)
        rc |= 1 << FF_FD_BITS;

    return rc;
}

int
bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (CHK_FD_BIT(sockfd)) {
        sockfd = CLR_FD_BIT(sockfd);
        return ff_bind(sockfd, (struct linux_sockaddr *)addr, addrlen);
    } else {
        return real_bind(sockfd, addr, addrlen);
    }
}

int
connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (CHK_FD_BIT(sockfd)) {
        sockfd = CLR_FD_BIT(sockfd);
        return ff_connect(sockfd, (struct linux_sockaddr *)addr, addrlen);
    } else {
        return real_connect(sockfd, addr, addrlen);
    }
}

ssize_t
send(int sockfd, const void *buf, size_t len, int flags)
{
    if (CHK_FD_BIT(sockfd)) {
        sockfd = CLR_FD_BIT(sockfd);
        return ff_send(sockfd, buf, len, flags);
    } else {
        return real_send(sockfd, buf, len, flags);
    }
}

ssize_t
recv(int sockfd, void *buf, size_t len, int flags)
{
    if (CHK_FD_BIT(sockfd)) {
        sockfd = CLR_FD_BIT(sockfd);
        return ff_recv(sockfd, buf, len, flags);
    } else {
        return real_recv(sockfd, buf, len, flags);
    }
}

int
listen(int sockfd, int backlog)
{
    if (CHK_FD_BIT(sockfd)) {
        sockfd = CLR_FD_BIT(sockfd);
        return ff_listen(sockfd, backlog);
    } else {
        return real_listen(sockfd, backlog);
    }
}

int
setsockopt (int sockfd, int level, int optname,
    const void *optval, socklen_t optlen)
{
    if (CHK_FD_BIT(sockfd)) {
        sockfd = CLR_FD_BIT(sockfd);
        return ff_setsockopt(sockfd, level, optname, optval, optlen);
    } else {
        return real_setsockopt(sockfd, level, optname, optval, optlen);
    }
}

int
accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    if (CHK_FD_BIT(sockfd)) {
        sockfd = CLR_FD_BIT(sockfd);
        int fd = ff_accept(sockfd, (struct linux_sockaddr *)addr, addrlen);
        if (fd < 0) {
            return fd;
        }
        fd |= 1 << FF_FD_BITS;
        return fd;
    } else {
        return real_accept(sockfd, addr, addrlen);
    }
}

int
accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
    if (CHK_FD_BIT(sockfd)) {
        sockfd = CLR_FD_BIT(sockfd);
        return ff_accept(sockfd, (struct linux_sockaddr *)addr, addrlen);
    } else {
        return real_accept4(sockfd, addr, addrlen, flags);
    }
}

int
close(int sockfd)
{
    if (CHK_FD_BIT(sockfd)) {
        sockfd = CLR_FD_BIT(sockfd);
        return ff_close(sockfd);
    } else {
        if (__builtin_expect(!!(real_close == NULL), 0)) {
            real_close = dlsym(RTLD_NEXT, "close");
        }
        return real_close(sockfd);
    }
}

ssize_t
writev(int sockfd, const struct iovec *iov, int iovcnt)
{
    if (CHK_FD_BIT(sockfd)) {
        sockfd = CLR_FD_BIT(sockfd);
        return ff_writev(sockfd, iov, iovcnt);
    } else {
        return real_writev(sockfd, iov, iovcnt);
    }
}

int
ioctl(int sockfd, int request, void *p)
{
    if (CHK_FD_BIT(sockfd)) {
        sockfd = CLR_FD_BIT(sockfd);
        return ff_ioctl(sockfd, request, p);
    } else {
        return real_ioctl(sockfd, request, p);
    }
}

int
select(int nfds, fd_set *readfds, fd_set *writefds,
    fd_set *exceptfds, struct timeval *timeout)
{
    if (CHK_FD_BIT(nfds)) {
        nfds = CLR_FD_BIT(nfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        return ff_select(nfds, readfds, writefds, exceptfds, &tv);
    } else {
        return real_select(nfds, readfds, writefds, exceptfds, timeout);
    }
}

int
kqueue(void)
{
    return ff_kqueue();
}

int
kevent(int kq, const struct kevent *changelist, int nchanges, 
    struct kevent *eventlist, int nevents, const struct timespec *timeout)
{
    return ff_kevent(kq, changelist, nchanges, eventlist, nevents, timeout);
}
