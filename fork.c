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

    // Call the original fork function
    pid_t child_pid = real_fork();
    if (child_pid == 0) {
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

        start_fht(&uffd); 
    }

    // Call the original fork function
    return child_pid;
}