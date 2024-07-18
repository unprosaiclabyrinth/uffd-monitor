// _GNU_SOURCE enables the RTLD_NEXT handle used in dlysm
#define _GNU_SOURCE

// Provide the prototypes for dlysm
#include <dlfcn.h>
#include <stddef.h>

#include <inttypes.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>

// ANSI color codes
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define MAX_CHILDREN 1000

// Function pointer to hold the address of the original fork function
// used in fork hijack
typedef pid_t (*fork_t)(void);
typedef int uffd_t;

// extern uffd_t uffd;            /* userfaultfd file descriptor */
extern unsigned long glob_new_vma;
extern unsigned long glob_code_vma_start_addr;
extern unsigned long glob_code_vma_end_addr;

void *fault_handler_thread(void *);
void start_fht(uffd_t *);

// Custom fork to hijack calls to libc fork
pid_t fork();

// Functions to manipulate code VMA
void get_code_vma_bounds(unsigned long *, unsigned long *);
void *setup_code_monitor(unsigned long, unsigned long);

// SIGSEGV handler
void sigsegv_handler(__attribute__((unused)) int, siginfo_t *,
                     __attribute__((unused)) void *);
void setup_sigsegv_handler();