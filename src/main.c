//
// LLRPC: Link-level RPC router
// Copyright (c) 2025, Dmitry Sednev <dmitry@sednev.ru>
//
// SPDX-License-Identifier: BSD-3-Clause
//
#include <sys/socket.h>

#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <poll.h>
#include <errno.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define IPPROTO_LLPRC 0xFC

enum rpc_message_type
{
    RPC_MESSAGE_ECHO_REQ,    ///< Echo request
    RPC_MESSAGE_ECHO_RESP,   ///< Echo response
    RPC_MESSAGE_COMMAND_REQ, ///< Command request
    RPC_MESSAGE_COMMAND_RESP ///< Command response
};

struct rpc_message_header
{
    uint16_t type;        ///< Message type
    uint32_t endpoint_id; ///< Source endpoint identifier
    uint32_t sequence_id; ///< Message sequence identifier
    uint16_t length;      ///< Total length
    uint64_t timestamp;   ///< Source endpoint time
    uint32_t crc32;       ///< Checksum
} __attribute__((packed));

static int server(struct in_addr local_addr, struct in_addr remote_addr);
static void timer(int sig);
static int endpoint_open(struct in_addr local_addr);
static void endpoint_close(int fd);
static void message_init(struct rpc_message_header *header, enum rpc_message_type type);
static void message_dump(struct sockaddr_in *addr, struct rpc_message_header *header);

static volatile bool terminate = false;
static volatile bool heartbeat = false;

int main(int argc, char *argv[])
{
    int ch;
    struct in_addr local_addr = { .s_addr = htonl(INADDR_LOOPBACK) };
    struct in_addr remote_addr = { .s_addr = htonl(INADDR_LOOPBACK) };

    while (true) {
        ch = getopt(argc, argv, "l:r:");
        if (ch < 0)
            break;

        switch (ch) {
        case 'l':
            if (!inet_aton(optarg, &local_addr)) {
                fprintf(stderr, "Invalid local address: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;

        case 'r':
            if (!inet_aton(optarg, &remote_addr)) {
                fprintf(stderr, "Invalid remote address: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;

        default:
            return EXIT_FAILURE;
        }
    }

    printf("Local: %s\n", inet_ntoa(local_addr));
    printf("Remote: %s\n", inet_ntoa(remote_addr));

    return server(local_addr, remote_addr);
}

static int server(struct in_addr local_addr, struct in_addr remote_addr)
{
    int fd;

    fd = endpoint_open(local_addr);
    if (fd < 0)
        return EXIT_FAILURE;

    struct sockaddr_in dst, src;
    socklen_t src_len;
    struct rpc_message_header msg;

    memset(&dst, 0, sizeof(dst));

    dst.sin_len    = sizeof(struct sockaddr_in);
    dst.sin_family = AF_INET;
    dst.sin_addr   = remote_addr;

    signal(SIGALRM, timer);
    alarm(1);

    while (!terminate) {
        struct pollfd fds[1] = {
            { .fd = fd, .events = POLLIN, .revents = 0 }
        };

        if (heartbeat)
            fds[0].events |= POLLOUT;

        int rv = poll(fds, 1, 1);
        if (rv < 0)
            continue;

        if (fds[0].revents & POLLIN) {
            src_len = sizeof(src);
            ssize_t rv = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr *)&src, &src_len);
            if (rv < 0) {
                fprintf(stderr, "Failed to receive message: %s\n", strerror(errno));
            } else {
                fprintf(stderr, "receviced %zd bytes:\n", rv);
                message_dump(&src, &msg);
            }
        }

        if (fds[0].revents & POLLOUT) {
            heartbeat = false;

            message_init(&msg, RPC_MESSAGE_ECHO_REQ);

            ssize_t rv = sendto(fd, &msg, sizeof(msg), 0, (struct sockaddr *)&dst, sizeof(dst));
            if (rv < 0) {
                fprintf(stderr, "Failed to receive message: %s\n", strerror(errno));
            } else {
                fprintf(stderr, "sent %zd bytes:\n", rv);
                message_dump(&dst, &msg);
            }
        }

        sleep(1);
    }

    endpoint_close(fd);

    return EXIT_SUCCESS;
}

static void timer(int sig)
{
    printf("tick, arming heartbeat\n");
    heartbeat = true;
    alarm(1);
}

static int endpoint_open(struct in_addr local_addr)
{
    int fd;
    struct sockaddr_in sin;

    fd = socket(AF_INET, SOCK_RAW, IPPROTO_LLPRC);
    if (fd < 0) {
        fprintf(stderr, "Unable to create LLRPC socket: %s\n", strerror(errno));
        return -1;
    }

    memset(&sin, 0, sizeof(sin));

    sin.sin_len    = sizeof(struct sockaddr_in);
    sin.sin_family = AF_INET;
    sin.sin_addr   = local_addr;

    if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        close(fd);
        fprintf(stderr, "Unable to bind LLRPC socket: %s\n", strerror(errno));
        return -1;
    }

    return fd;
}

static void endpoint_close(int fd)
{
    close(fd);
}

static void message_init(struct rpc_message_header *header, enum rpc_message_type type)
{
    static uint32_t seq = 1;

    header->type        = type;
    header->endpoint_id = 0;
    header->sequence_id = seq++;
    header->length      = sizeof(*header);
    header->timestamp   = time(NULL);
    header->crc32       = 0;
}

static void message_dump(struct sockaddr_in *addr, struct rpc_message_header *header)
{
    printf("[%s] type=%u length=%u endpoint_id=%u sequence_id=%u timestamp=%llu\n",
           inet_ntoa(addr->sin_addr),
           header->type, header->length, header->endpoint_id, header->sequence_id, header->timestamp);
}
