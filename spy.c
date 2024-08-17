#include "uffd.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/syscall.h>

#include <compel/log.h>
#include <compel/infect.h>

void print_vmsg(unsigned int lvl, const char *fmt, va_list parms) {
    printf("\tLC%u: ", lvl);
    vprintf(fmt, parms);
}

int infect(int pid, void *addr) {
    #define err_and_ret(msg)      \
        do {                      \
            fprintf(stderr, msg); \
            return -1;            \
        } while (0)

    int state;
    long ret = -1000;
    struct parasite_ctl *ctl;

    compel_log_init(print_vmsg, COMPEL_LOG_DEBUG);

    #if VERBOSE_PARASITE
        printf(BRIGHT_RED "Stopping task\n" RESET);
    #endif
    state = compel_stop_task(pid);
    if (state < 0)
        err_and_ret(RED "Can't stop task" RESET "\n");

    #if VERBOSE_PARASITE
        printf(BRIGHT_RED "Preparing parasite ctl" RESET "\n");
    #endif
    ctl = compel_prepare(pid);
    if (!ctl)
        err_and_ret(RED "Can't prepare for infection" RESET "\n");

    #if VERBOSE_PARASITE
        printf(BRIGHT_BLUE "Calling madvise..." RESET);
    #endif
    if (compel_syscall(ctl, __NR_madvise, &ret, (unsigned long)addr, PAGE_SIZE, MADV_DONTNEED, 0, 0, 0) < 0)
		err_and_ret(RED "Can't run rmadvise" RESET "\n");
    #if VERBOSE_PARASITE
	    printf(BRIGHT_BLUE "Remote madvise returned %ld" RESET "\n", ret);
    #endif

    /*
    * Done. Cure and resume the task.
    */
    #if VERBOSE_PARASITE
        printf(BRIGHT_RED "Curing\n" RESET);
    #endif
    if (compel_cure(ctl))
        err_and_ret(RED "Can't cure victim" RESET "\n");

    if (compel_resume_task(pid, state, state))
        err_and_ret(RED "Can't unseize task" RESET "\n");

    #if VERBOSE_PARASITE
        printf(BRIGHT_RED "Done!" RESET "\n");
    #endif
    return 0;
}