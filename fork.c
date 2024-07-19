#include "uffd.h"
#include <sys/socket.h>

static fork_t real_fork = NULL;

// Called in child to send uffd to parent
static void send_uffd(int uffd, int send_sock) {
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
    if (sendmsg(send_sock, &msg, 0) == -1)
        errExit("sendmsg");
}

// Called in parent to receive uffd from child
static uffd_t recv_uffd(int recv_sock) {
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

    if (recvmsg(recv_sock, &msg, 0) == -1) // Block
        errExit("recvmsg");
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    uffd_t uffd = *((uffd_t *)CMSG_DATA(cmsg));

    return uffd;
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

    // Setup UDS to pass uffd
    int uds[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, uds) == -1)
        errExit("fork -> socketpair");

    // Call the original fork function
    pid_t child_pid = real_fork();
    if (child_pid == 0) {
        // Setup uffd in child
        uffd_t uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
        if (uffd == -1)
            errExit("syscall -> userfaultfd");

        struct uffdio_api uffdio_api = {
            .api = UFFD_API,
            .features = 0
        };
        if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
            errExit("ioctl -> UFFDIo_API");

        struct uffdio_register uffdio_register = {
            .range = {
                .start = glob_code_vma_start_addr,
                .len = glob_code_vma_end_addr - glob_code_vma_start_addr,
            },
            .mode = UFFDIO_REGISTER_MODE_MISSING
        };
        if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
            errExit("ioctl -> UFFDIO_REGISTER");

        // Send uffd to parent
        close(uds[0]);
        send_uffd(uffd, uds[1]);
        close(uds[1]);
    } else {
        // Receive uffd from child
        close(uds[1]);
        uffd_t child_uffd = recv_uffd(uds[0]);
        close(uds[0]);
        start_fht(&child_uffd);
    }

    // Call the original fork function
    return child_pid;
}