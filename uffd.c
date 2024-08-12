#include "uffd.h"

int PAGE_SIZE;
unsigned long glob_new_vma;
unsigned long glob_code_vma_start_addr;
unsigned long glob_code_vma_end_addr;

static struct uffdio_copy prepare_page(struct uffd_msg msg) {
    /* Create a page that will be copied into the faulting region */

    long page_offset = msg.arg.pagefault.address - glob_code_vma_start_addr;
    unsigned long page = (glob_new_vma + page_offset) & ~(PAGE_SIZE - 1);
    mprotect((void *)page, PAGE_SIZE, PROT_READ | PROT_EXEC);

    /* Copy the page pointed to by 'page' into the faulting
    region. Vary the contents that are copied in, so that it
    is more obvious that each fault is handled separately. */

    struct uffdio_copy uffdio_copy = {
        .src = page,

        /* We need to handle page faults in units of pages(!).
        So, round faulting address down to page boundary */

        .dst = (unsigned long)msg.arg.pagefault.address & ~(PAGE_SIZE - 1),
        .len = PAGE_SIZE,
        .mode = 0,
        .copy = 0
    };

    return uffdio_copy;
}

void *fault_handler_thread(void *arg) {
    struct uffd_msg msg;             /* Data read from userfaultfd */
    int fault_cnt = 0;               /* Number of faults so far handled */
    uffd_t uffd = (uffd_t)(long)arg; /* userfaultfd file descriptor */
    void *mru_page = NULL;           /* Address of most recently used page */
    ssize_t nread;

    printf(MAGENTA "fault_handler_thread spawned for "
           BLUE "PID = " YELLOW "%d" MAGENTA ", "
           BLUE "uffd = " YELLOW "%d\n" RESET,
           getpid(), uffd);

    /* Loop, handling incoming events on the userfaultfd
       file descriptor */

    while (1) {
        /* See what poll() tells us about the userfaultfd */

        int nready;
        struct pollfd pollfd = {
            .fd = uffd,
            .events = POLLIN
        };
        nready = poll(&pollfd, 1, -1);
        if (nready == -1) {
            if (errno == EINTR)
                continue;
            else
                errExit("poll");
        }

        /* Read an event from the userfaultfd */

        nread = read(uffd, &msg, sizeof(msg));
        if (nread == 0) {
            printf("EOF on userfaultfd!\n");
            exit(EXIT_FAILURE);
        } else if (nread == -1)
            errExit("read from uffd");

        /* We expect only one kind of event; verify that assumption */

        assert(msg.event == UFFD_EVENT_PAGEFAULT);

        /* Serve the page */

        struct uffdio_copy uffdio_copy = prepare_page(msg);
        if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
            errExit("ioctl-UFFDIO_COPY");

        /* Display info about the page-fault event */

        fprintf(stderr, MAGENTA "[" YELLOW "%6d" MAGENTA "/" YELLOW "%d" MAGENTA "/" CYAN "%6d" MAGENTA "] "
                        RESET "addr: " RED "%#llx" RESET ", "
                        RESET "src: " GREEN "%#llx" RESET ", "
                        RESET "code: " RESET "%lx\n" RESET, getpid(), uffd, ++fault_cnt,
                        msg.arg.pagefault.address, uffdio_copy.src, *(long *)uffdio_copy.src);
        
        /* Drop previously loaded code page to restrict visibility to one page */

        #if PARASITE
            struct child_fhl_entry *fhle = get_fhl_entry_by_uffd(uffd);
            if (fhle == NULL) {
                // parent case
                if (mru_page != NULL)
                    madvise(mru_page, PAGE_SIZE, MADV_DONTNEED);
                mru_page = (void *)uffdio_copy.dst;
            } else {
                // child case
                if (fhle->mru_page != NULL)
                    infect(fhle->pid, (void *)uffdio_copy.dst); // parasite invocation
                fhle->mru_page = (void *)uffdio_copy.dst;
            }
        #else
            if (mru_page != NULL)
                madvise(mru_page, PAGE_SIZE, MADV_DONTNEED);
            mru_page = (void *)uffdio_copy.dst;
        #endif
    }
}

void start_fht(long long_uffd) {
    pthread_t thr; /* ID of thread that handles page faults */
    int s = pthread_create(&thr, NULL, fault_handler_thread, (void *)long_uffd);
    if (s != 0) {
        errno = s;
        errExit("pthread_create -> handler");
    }
}

// Tell the loader to run this function once the library is loaded
__attribute__((constructor)) int uffd_init() {
    setup_sigchld_handler();
    PAGE_SIZE = sysconf(_SC_PAGE_SIZE);

    /* Create and enable userfaultfd object */

    uffd_t uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (uffd == -1)
        errExit("syscall -> userfaultfd");

    struct uffdio_api uffdio_api = {
        .api = UFFD_API,
        .features = 0
    };
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
        errExit("ioctl -> UFFDIO_API");

    /* Register the memory range of the code pages for handling by the
       userfaultfd object. In mode, we request to track missing pages
       (i.e., pages that have not yet been faulted in). */
    
    get_code_vma_bounds(&glob_code_vma_start_addr, &glob_code_vma_end_addr);
    glob_new_vma = (unsigned long)setup_code_monitor(glob_code_vma_start_addr,
                                                     glob_code_vma_end_addr);

    struct uffdio_register uffdio_register = {
        .range = {
            .start = glob_code_vma_start_addr,
            .len = glob_code_vma_end_addr - glob_code_vma_start_addr,
        },
        .mode = UFFDIO_REGISTER_MODE_MISSING
    };
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
        errExit("ioctl -> UFFDIO_REGISTER");

    /* Create a thread that will process the userfaultfd events */

    start_fht(uffd);

    /* Block for userfaultfd events on the separate thread, and let
       this one exit and call main in the application program */
    return 0;
}