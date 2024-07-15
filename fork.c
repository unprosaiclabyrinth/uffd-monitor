#include "uffd.h"
#include <sys/socket.h>

static fork_t real_fork = NULL;

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

    // Setup UDS for passing uffd
    int uds[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, uds) == -1)
        errExit(RED "fork -> socketpair" RESET);

    // Call the original fork function
    pid_t child_pid = real_fork();
    if (child_pid == 0) {
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

        struct uffdio_register uffdio_register = {
            .range = {
                .start = glob_code_vma_start_addr,
                .len = glob_code_vma_end_addr - glob_code_vma_start_addr,
            },
            .mode = UFFDIO_REGISTER_MODE_MISSING
        };
        if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
            errExit(RED "ioctl -> UFFDIO_REGISTER" RESET);
    } else {
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
        printf(CYAN "child_uffd: %d\n" RESET, child_uffd);

        close(uds[0]);
        pthread_t thr;
        uffd_t arg[1] = { child_uffd };
        int s = pthread_create(&thr, NULL, fault_handler_thread, (void *)arg);
        if (s != 0) {
            errno = s;
            errExit(RED "fork -> pthread_create -> handler" RESET);
        }
    }

    // Call the original fork function
    return child_pid;
}