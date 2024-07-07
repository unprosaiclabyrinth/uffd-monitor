#include "uffd.h"

static fork_t real_fork = NULL;

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

    // Print a message before calling the original fork function
    printf("Intercepted call to fork!\n");

    // Call the original fork function
    return real_fork();
}