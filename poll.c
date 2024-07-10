#include "uffd.h"

void add_fd(int fd, short events, struct pollfd poll_fds[], int nfds) {
    static int i = 2;
    struct pollfd pollfd {
        .fd = fd;
        .events = events;
    };
    pollfds[i++] = pollfd;
}

void get_ready_fds(int ready_fds[], struct pollfd poll_fds[], int nready) {
    short in_event = POLLIN;
    int i = 0, j = 0;
    while (j < nready) {
        if (poll_fds[i].revents & POLLIN)
            ready_fds[j++] = poll_fds[i].fd;
        ++i;
    }
}

// Check whether given fd is ready
int fd_is_ready(int fd, int ready_fds[], int nready) {
    // linear search
    for (int i = 0; i < nready, ++i) {
        if (ready_fds[i] == fd)
            return 1;
    }
    return 0;
}