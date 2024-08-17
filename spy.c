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

int infect(int pid, void *mru_page) {
    #define err_and_ret(msg)      \
        do {                      \
            fprintf(stderr, msg); \
            return -1;            \
        } while (0)

    int state;
    long ret = -1000;
    struct parasite_ctl *ctl;

    compel_log_init(print_vmsg, COMPEL_LOG_DEBUG);

    printf(BRIGHT_RED "Stopping task\n" RESET);
    state = compel_stop_task(pid);
    if (state < 0)
        err_and_ret(RED "Can't stop task" RESET);

    printf(BRIGHT_RED "Preparing parasite ctl\n" RESET);
    ctl = compel_prepare(pid);
    if (!ctl)
        err_and_ret(RED "Can't prepare for infection" RESET);

    printf(BRIGHT_BLUE "Calling madvise\n" RESET);
    if (compel_syscall(ctl, __NR_madvise, &ret, (unsigned long)mru_page, PAGE_SIZE, MADV_DONTNEED, 0, 0, 0) < 0)
		err_and_ret("Can't run rmadvise");
	printf(BRIGHT_BLUE "Remote madvise returned %ld" RESET "\n", ret);

    /*
    * Done. Cure and resume the task.
    */
    printf(BRIGHT_RED "Curing\n" RESET);
    if (compel_cure(ctl))
        err_and_ret(RED "Can't cure victim" RESET);

    if (compel_resume_task(pid, state, state))
        err_and_ret(RED "Can't unseize task" RESET);

    printf(BRIGHT_RED "Done!" RESET "\n");
    return 0;
}