//
// LLRPC: Link-level RPC router
// Copyright (c) 2025, Dmitry Sednev <dmitry@sednev.ru>
//
// SPDX-License-Identifier: BSD-3-Clause
//
#include <sys/socket.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <netinet/in.h>

#define IPPROTO_LLPRC 0xFC

int main(void)
{
    int fd;
    struct sockaddr_in sin;

    fd = socket(AF_INET, SOCK_RAW, IPPROTO_LLPRC);
    if (fd < 0) {
        fprintf(stderr, "Unable to create LLRPC socket: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    memset(&sin, 0, sizeof(sin));

    sin.sin_len         = sizeof(struct sockaddr_in);
    sin.sin_family      = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        fprintf(stderr, "Unable to bind LLRPC socket: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    close(fd);

    return EXIT_SUCCESS;
}
