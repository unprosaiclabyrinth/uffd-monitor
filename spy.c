#include "uffd.h"
#include <compel/log.h>
#include <compel/infect-rpc.h>
#include <compel/infect-util.h>

#include "parasite.h"
#define PARASITE_CMD_MADV PARASITE_USER_CMDS

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
    struct parasite_ctl *ctl;
    struct infect_ctx *ictx;
    unsigned long *arg;

    compel_log_init(print_vmsg, COMPEL_LOG_DEBUG);

    printf(BRIGHT_RED "Stopping task\n" RESET);
    state = compel_stop_task(pid);
    if (state < 0)
        err_and_ret(RED "Can't stop task" RESET);

    printf(BRIGHT_RED "Preparing parasite ctl\n" RESET);
    ctl = compel_prepare(pid);
    if (!ctl)
        err_and_ret(RED "Can't prepare for infection" RESET);

    printf(BRIGHT_RED "Configuring contexts\n" RESET);

    /*
    * First -- the infection context. Most of the stuff
    * is already filled by compel_prepare(), just set the
    * log descriptor for parasite side, library cannot
    * live w/o it.
    */
    ictx = compel_infect_ctx(ctl);
    ictx->log_fd = STDERR_FILENO;

    parasite_setup_c_header(ctl);

    printf(BRIGHT_RED "Infecting\n" RESET);
    if (compel_infect(ctl, 1, sizeof(int)))
        err_and_ret(RED "Can't infect victim" RESET);

    /*
    * Now get the area with arguments and run
    * command to call madvise in victim
    */
	arg = compel_parasite_args(ctl, unsigned long);
    *arg = (unsigned long)mru_page;

    printf(BRIGHT_BLUE "Calling madvise\n" RESET);
    if (compel_rpc_call(PARASITE_CMD_MADV, ctl))
        err_and_ret(RED "Can't run cmd" RESET);

    /*
    * Done. Cure and resume the task.
    */
    printf(BRIGHT_RED "Curing\n" RESET);
    if (compel_cure(ctl))
        err_and_ret(RED "Can't cure victim" RESET);

    if (compel_resume_task(pid, state, state))
        err_and_ret(RED "Can't unseize task" RESET);

    printf(BRIGHT_RED "Done\n" RESET);
    return 0;
}