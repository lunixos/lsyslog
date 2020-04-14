#pragma once

#ifndef MAX_CLIENTS
#define MAX_CLIENTS 8
#endif

struct lsyslog_s {
    int epoll_fd;
    int tcp_fd;
};
