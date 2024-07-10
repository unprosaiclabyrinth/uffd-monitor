#include "uffd.h"

void add_fd(int fd, short events, struct pollfd poll_fds[], int nfds) {
    struct pollfd pollfd = {
        .fd = fd,
        .events = events
    };
    poll_fds[nfds] = pollfd;
}

struct pollfd *get_pollfd(int fd, struct pollfd poll_fds[], int nfds) {
    for (int i = 0; i < nfds; ++i) {
        if (poll_fds[i].fd == fd)
            return &poll_fds[i];
    }
    return NULL;
}

void get_ready_fds(int ready_fds[], struct pollfd poll_fds[], int nready) {
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
    for (int i = 0; i < nready; ++i) {
        if (ready_fds[i] == fd)
            return 1;
    }
    return 0;
}

void dump_poll_fds(struct pollfd poll_fds[], int nfds) {
    printf(WHITE "Going for the poll are %d fds...:-\n", nfds);
    for (int i = 0; i < nfds; ++i) {
        printf("%d\n", poll_fds[i].fd);
    }
    printf(RESET);
}

void dump_ready_fds(int ready_fds[], int nready) {
    printf(WHITE "Ooh...%d fds are READY!!!:-\n", nready);
    for (int i = 0; i < nready; ++i) {
        printf("%d\n", ready_fds[i]);
    }
    printf(RESET);
}