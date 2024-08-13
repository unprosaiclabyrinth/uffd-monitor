#include <signal.h>
#include <sys/wait.h>
#include "uffd.h"

void sigchld_handler(__attribute__((unused)) int signo) {
    pid_t pid;
    int status;

    // Use waitpid to collect terminated child processes
    pid_t dead_children[MAX_CHILDREN];
    int ndead = 0;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            dead_children[ndead++] = pid;
        }
    }

    scan_and_clean_log(dead_children, ndead);
}

void setup_sigchld_handler() {
    struct sigaction sa = {
        .sa_handler = sigchld_handler,
        .sa_flags = SA_RESTART | SA_NOCLDSTOP
    };
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}