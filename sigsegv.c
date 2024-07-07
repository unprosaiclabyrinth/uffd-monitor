#include <signal.h>
#include <execinfo.h>
#include <ucontext.h>
#include "uffd.h"

void sigsegv_handler(__attribute__((unused)) int sig, siginfo_t *si,
                     __attribute__((unused)) void *unused) {
    printf("Caught SIGSEGV at address: %p\n", si->si_addr);
    // Print the stack trace
    void *buffer[30];
    int nptrs = backtrace(buffer, 30);
    backtrace_symbols_fd(buffer, nptrs, STDERR_FILENO);
    errExit("Segmentation fault");
}

void setup_sigsegv_handler() {
    struct sigaction sa = {
        .sa_flags = SA_SIGINFO,
        .sa_sigaction = sigsegv_handler
    };
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        errExit("sigaction");
    }
}