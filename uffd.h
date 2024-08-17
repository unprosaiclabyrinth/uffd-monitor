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
#define BRIGHT_RED "\033[91m"
#define BRIGHT_GREEN "\033[92m"
#define BRIGHT_YELLOW "\033[93m"
#define BRIGHT_BLUE "\033[94m"
#define BRIGHT_MAGENTA "\033[95m"
#define BRIGHT_CYAN "\033[96m"

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define MAX_CHILDREN 1000

// Function pointer to hold the address of the original fork function
// used in fork hijack
typedef pid_t (*fork_t)(void);
typedef int uffd_t;

extern int PAGE_SIZE;
extern unsigned long glob_new_vma;
extern unsigned long glob_code_vma_start_addr;
extern unsigned long glob_code_vma_end_addr;

// Starts the fault handler thread with given uffd 
void start_fht(long);

// Custom fork hook to hijack calls to libc fork
pid_t fork(void);

// Functions to manipulate code VMA
void get_code_vma_bounds(unsigned long *, unsigned long *);
void *setup_code_monitor(unsigned long, unsigned long);

// SIGCGLD handler
void sigchld_handler(int);
void setup_sigchld_handler(void);

// Parasite commands
void print_vmsg(unsigned int, const char *, va_list);
int infect(int, void *);

// Structure and functions for logging
struct child_proc_info {
    int pid;
    uffd_t uffd; // in parent
    void *mru_page;
};

void add_log_entry(pid_t, uffd_t);
struct child_proc_info *get_proc_info_by_uffd(uffd_t);
struct child_proc_info *get_proc_info_by_pid(pid_t);
void dump_log(void);
void mark_as_removed(pid_t *, int);