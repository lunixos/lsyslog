#include <sys/epoll.h>
#include <sys/socket.h>
#include <syslog.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#include "lsyslog_parser.h"
#include "lsyslog.h"

#define EPOLL_NUM_EVENTS 8
#define BUF_LEN 1025

static int lsyslog_epoll_event_tcp_fd (
    struct lsyslog_s * lsyslog,
    struct epoll_event * event
)
{
    int ret = 0;
    int client_fd = 0;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;

    printf("client connecting\n");

    sin_size = sizeof(their_addr);
    client_fd = accept(lsyslog->tcp_fd, (struct sockaddr *)&their_addr, &sin_size);
    if (-1 == client_fd) {
        warn("accept");
        return -1;
    }


    // Add the client to epoll.
    ret = epoll_ctl(
        lsyslog->epoll_fd,
        EPOLL_CTL_ADD,
        client_fd,
        &(struct epoll_event){
            .events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLONESHOT,
            .data = {
                .fd = client_fd
            }
        }
    );
    if (-1 == ret) {
        fprintf(stderr, "%s:%d:%s: epoll_ctl: %s\n", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }


    // Re-arm EPOLLONESHOT file descriptor in epoll
    ret = epoll_ctl(
        lsyslog->epoll_fd,
        EPOLL_CTL_MOD,
        event->data.fd,
        &(struct epoll_event){
            .events = EPOLLIN |  EPOLLERR | EPOLLHUP | EPOLLONESHOT,
            .data = {
                .fd = event->data.fd
            }
        }
    );
    if (-1 == ret) {
        fprintf(stderr, "%s:%d: epoll_ctl: %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    // We're done.
    return 0;
}


static int lsyslog_epoll_event_client_fd (
    struct lsyslog_s * lsyslog,
    struct epoll_event * event
)
{
    int ret = 0;
    int bytes_read;

    char buf[BUF_LEN];
    bytes_read = read(event->data.fd, buf, BUF_LEN);
    if (-1 == bytes_read) {
        err(1, "read");
    }
    if (0 == bytes_read) {
        // Client disconnected, clear out the client from the client list
        // Remember to EPOLL_CTL_DEL *before* closing the file descriptor, see
        // https://idea.popcount.org/2017-03-20-epoll-is-fundamentally-broken-22/
        ret = epoll_ctl(
            lsyslog->epoll_fd,
            EPOLL_CTL_DEL,
            event->data.fd,
            NULL
        );
        if (-1 == ret) {
            syslog(LOG_ERR, "%s:%d: epoll_ctl: %s", __func__, __LINE__, strerror(errno));
            return -1;
        }
        
        printf("client disconnected\n");
        return 0;
    }

    printf("read: %.*s\n", bytes_read, buf);

    buf[bytes_read] = 0;
    ret = lsyslog_parser_parse(buf, bytes_read+1);

    // Re-arm EPOLLONESHOT file descriptor in epoll
    ret = epoll_ctl(
        lsyslog->epoll_fd,
        EPOLL_CTL_MOD,
        event->data.fd,
        &(struct epoll_event){
            .events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLONESHOT,
            .data = {
                .fd = event->data.fd
            }
        }
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d: epoll_ctl: %s", __func__, __LINE__, strerror(errno));
        return -1;
    }

    return 0;
}


static int lsyslog_epoll_event_dispatch (
    struct lsyslog_s * lsyslog,
    struct epoll_event * event
)
{
    int ret = 0;

    if (event->data.fd == lsyslog->tcp_fd)
        return lsyslog_epoll_event_tcp_fd(lsyslog, event);


    // If no match, it's probably a client.
    ret = lsyslog_epoll_event_client_fd(lsyslog, event);
    if (-1 == ret) {
        fprintf(stderr, "lsyslog_epoll_event_client_fd returned -1\n");
    }

    return 0;
}


static int lsyslog_epoll_handle_events (
    struct lsyslog_s * lsyslog,
    struct epoll_event epoll_events[EPOLL_NUM_EVENTS],
    int ep_events_len
)
{
    int ret = 0;
    for (int i = 0; i < ep_events_len; i++) {
        // (snippet: epdisp)
        ret = lsyslog_epoll_event_dispatch(lsyslog, &epoll_events[i]);
        if (0 != ret) {
            return ret;
        }
    }
    return 0;
}


int main(int argc, char *argv[])
{
    int ret = 0;
    struct lsyslog_s lsyslog = {0};
    struct addrinfo *servinfo, *p;
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };

    // Create the epoll instance
    lsyslog.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (-1 == lsyslog.epoll_fd) {
        fprintf(stderr, "%s:%d:%s: epoll_create1: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    ret = getaddrinfo("0.0.0.0", "1514", &hints, &servinfo);
    if (0 != ret) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        lsyslog.tcp_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (-1 == lsyslog.tcp_fd) {
            warn("socket");
            continue;
        }

        int yes = 1;
        if (-1 == setsockopt(lsyslog.tcp_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) {
            err(EXIT_FAILURE, "setsockopt");
        }

        if (-1 == bind(lsyslog.tcp_fd, p->ai_addr, p->ai_addrlen)) {
            close(lsyslog.tcp_fd);
            warn("bind");
            continue;
        }

        break;

    }
    freeaddrinfo(servinfo);
    if (NULL == p) {
        err(EXIT_FAILURE, "bind");
    }

    if (-1 == listen(lsyslog.tcp_fd, 8)) {
        err(EXIT_FAILURE, "listen");
    }


    // Ok, we have a socket ready, add it to the epoll.
    ret = epoll_ctl(
        lsyslog.epoll_fd,
        EPOLL_CTL_ADD,
        lsyslog.tcp_fd,
        &(struct epoll_event){
            .events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLONESHOT,
            .data = {
                .fd = lsyslog.tcp_fd
            }
        }
    );
    if (-1 == ret) {
        fprintf(stderr, "%s:%d:%s: epoll_ctl: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    
    // Time for the epoll wait loop.
    int ep_events_len = 0;
    struct epoll_event ep_events[EPOLL_NUM_EVENTS];
    for (ep_events_len = epoll_wait(lsyslog.epoll_fd, ep_events, EPOLL_NUM_EVENTS, -1);
         ep_events_len > 0;
         ep_events_len = epoll_wait(lsyslog.epoll_fd, ep_events, EPOLL_NUM_EVENTS, -1))
    {
        // (snippet: epev)
        ret = lsyslog_epoll_handle_events(&lsyslog, ep_events, ep_events_len);
        if (-1 == ret) {
            break;
        }
    }
    if (-1 == ep_events_len) {
        fprintf(stderr, "%s:%d:%s: epoll_wait: %s", __FILE__, __LINE__, __func__, strerror(errno));
        exit(EXIT_FAILURE);
    }
    


    return 0;
}
