#include "uffd.h"
#include <sys/socket.h>

static fork_t real_fork = NULL;

__attribute__((unused))
static void *fault_notifier_thread(void *arg) {
    struct uffd_msg msg;
    int child_write = *(int *)arg;
    ssize_t nread;
    while (1) {
        int nready;
        struct pollfd pollfd = {
            .fd = uffd,
            .events = POLLIN
        };

        nready = poll(&pollfd, 1, -1);
        if (nready == -1)
            errExit(RED "notifier -> poll" RESET);

        nread = read(uffd, &msg, sizeof(msg));
        if (nread == 0) {
            printf(RED "notifier -> read -> uffd: EOF!\n" RESET);
            exit(EXIT_FAILURE);
        } else if (nread == -1)
            errExit(RED "notifier -> read -> uffd" RESET);

        assert(msg.event == UFFD_EVENT_PAGEFAULT);

        // Send messsge to parent
        int nwritten = write(child_write, &msg, sizeof(msg));
        if (nwritten == -1)
            errExit(RED "notifier -> write -> child_write" RESET);
        printf(CYAN "        %d: fault_notifier_thread() -> Notified %d bytes\n\n" RESET, getpid(), nwritten);
    }
}

// Our custom fork function
pid_t fork() {
    // Load the original fork function if not already loaded
    if (!real_fork) {
        real_fork = (fork_t)dlsym(RTLD_NEXT, "fork");
        if (!real_fork) {
            fprintf(stderr, RED "Error in `dlsym`: %s\n" RESET, dlerror());
            exit(EXIT_FAILURE);
        }
    }
    
    // Setup child->parent IPC using a pipe
    int child_to_parent[2]; // read @ parent, write @ child
    if (pipe(child_to_parent) == -1)
        errExit(RED "fork -> pipe -> c2p" RESET);
    int parent_read = child_to_parent[0];
    int child_write = child_to_parent[1];

    // Setup UDS for passing uffd
    int uds[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, uds) == -1)
        errExit(RED "fork -> socketpair" RESET);

    // Call the original fork function
    // printf(CYAN "Intercepted: Trying to fork, are we?\n" RESET);
    pid_t child_pid = real_fork();
    if (child_pid == 0) {
        close(parent_read);
        close(uds[0]);

        // Send uffd to parent
        char buf[CMSG_SPACE(sizeof(uffd_t))];
        char dummy = ' ';
        struct iovec iov = {
            .iov_base = &dummy,
            .iov_len = sizeof(dummy)
        };
        struct msghdr msg = {
            .msg_iov = &iov,
            .msg_iovlen = 1,
            .msg_control = buf,
            .msg_controllen = sizeof(buf)
        };

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(uffd_t));
        *((uffd_t *)CMSG_DATA(cmsg)) = uffd;
        if (sendmsg(uds[1], &msg, 0) == -1)
            errExit("sendmsg");
        close(uds[1]);

        // pthread_t thr;
        // int s = pthread_create(&thr, NULL, fault_notifier_thread, (void *)&child_write);
        // if (s != 0) {
        //     errno = s;
        //     errExit(RED "pthread_create -> notifier" RESET);
        // }
    } else {
        close(child_write);
        close(uds[1]);

        // Receive uffd from child
        char buf[CMSG_SPACE(sizeof(uffd_t))];
        char dummy = ' ';
        struct iovec iov = {
            .iov_base = &dummy,
            .iov_len = sizeof(dummy)
        }; 
        struct msghdr msg = {
            .msg_iov = &iov,
            .msg_iovlen = 1,
            .msg_control = buf,
            .msg_controllen = sizeof(buf)
        };

        if (recvmsg(uds[0], &msg, 0) == -1)
            errExit("recvmsg");
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        uffd_t child_uffd = *((uffd_t *)CMSG_DATA(cmsg));

        add_log_entry(child_pid, child_uffd, parent_read);
        // if (write(self_pipe_fds[1], &child_uffd, sizeof(child_uffd)) == -1)
        //     errExit(RED "fork -> write -> self_pipe" RESET);

        pthread_t thr;
        int s = pthread_create(&thr, NULL, fault_handler_thread, (void *)(long)child_uffd);
        if (s != 0) {
            errno = s;
            errExit(RED "fork -> pthread_create -> handler" RESET);
        }
        printf(CYAN "\nfault_handler_thread spawned! PID = %d, uffd = %d\n\n" RESET, child_pid, child_uffd);
    }

    // Call the original fork function
    return child_pid;
}