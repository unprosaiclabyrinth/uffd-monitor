#include "uffd.h"

static fork_t real_fork = NULL;

static void *fault_notifier_thread(void *args) {
    struct uffd_msg msg;
    __attribute__((unused)) int child_read = ((int *)args)[0];
    int child_write = ((int *)args)[1];
    ssize_t nread;
    while (1) {
        int nready;
        struct pollfd pollfd = {
            .fd = uffd,
            .events = POLLIN
        };
        nready = poll(&pollfd, 1, -1);
        if (nready == -1)
            errExit("poll");

        nread = read(uffd, &msg, sizeof(msg));
        if (nread == 0) {
            printf("EOF on userfaultfd!\n");
            exit(EXIT_FAILURE);
        } else if (nread == -1)
            errExit("read");

        if (msg.event != UFFD_EVENT_PAGEFAULT) {
            fprintf(stderr, "Unexpected event on userfaultfd\n");
            exit(EXIT_FAILURE);
        }

        // Send messsge to parent
        write(child_write, &msg, sizeof(msg));
    }
}

// Our custom fork function
pid_t fork() {
    // Load the original fork function if not already loaded
    if (!real_fork) {
        real_fork = (fork_t)dlsym(RTLD_NEXT, "fork");
        if (!real_fork) {
            fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
            exit(1);
        }
    }
    
    // Setup 2-way IPC using 2 pipes
    int child_to_parent[2]; // read @ parent, write @ child
    int parent_to_child[2]; // read @ child, write @ parent
    if (pipe(child_to_parent) == -1)
        errExit("pipe-child_to_parent");
    if (pipe(parent_to_child) == -1)
        errExit("pipe-parent_to_child");
    int parent_read = child_to_parent[0];
    int child_write = child_to_parent[1];
    int child_read = parent_to_child[0];
    int parent_write = parent_to_child[1];

    // Call the original fork function
    printf(CYAN "HIjacked! Trying to fork, are we?\n" RESET);
    pid_t child_pid = real_fork();
    if (child_pid == 0) {
        close(parent_read);
        close(parent_write);
        pthread_t thr;
        int args[2] = {child_read, child_write};
        pthread_create(&thr, NULL, fault_notifier_thread, (void *)args);
    } else {
        close(child_read);
        close(child_write);
        add_log_entry(child_pid, parent_read, parent_write);
        write(self_pipe_fds[1], &parent_read, sizeof(parent_read));
    }

    // Call the original fork function
    return child_pid;
}