#include "uffd.h"

int page_size;
unsigned long glob_new_vma;
unsigned long glob_code_vma_start_addr;
unsigned long glob_code_vma_end_addr;

static struct uffdio_copy prepare_page(struct uffd_msg msg) {
    /* Create a page that will be copied into the faulting region */

    long page_offset = msg.arg.pagefault.address - glob_code_vma_start_addr;
    unsigned long page = (glob_new_vma + page_offset) & ~(page_size - 1);
    mprotect((void *)page, page_size, PROT_READ | PROT_EXEC);

    /* Copy the page pointed to by 'page' into the faulting
    region. Vary the contents that are copied in, so that it
    is more obvious that each fault is handled separately. */

    struct uffdio_copy uffdio_copy = {
        .src = page,

        /* We need to handle page faults in units of pages(!).
        So, round faulting address down to page boundary */

        .dst = (unsigned long)msg.arg.pagefault.address & ~(page_size - 1),
        .len = page_size,
        .mode = 0,
        .copy = 0
    };

    return uffdio_copy;
}

void *fault_handler_thread(void *arg) {
    struct uffd_msg msg;           /* Data read from userfaultfd */
    int fault_cnt = 0;             /* Number of faults so far handled */
    uffd_t this_uffd = *((uffd_t *)arg); /* userfaultfd file descriptor */
    ssize_t nread;

    printf(MAGENTA "\nfault_handler_thread spawned! PID = %d, uffd = %d\n\n" RESET, getpid(), this_uffd);

    /* Loop, handling incoming events on the userfaultfd
       file descriptor */

    while (1) {
        /* See what poll() tells us about the userfaultfd */

        int nready;
        struct pollfd pollfd = {
            .fd = this_uffd,
            .events = POLLIN
        };
        nready = poll(&pollfd, 1, -1);
        if (nready == -1)
            errExit("poll");

        printf(MAGENTA "%6d. " RESET "poll() returns: "
                "nready = %d; POLLIN = %d; POLLERR = %d\n",
                ++fault_cnt, nready, (pollfd.revents & POLLIN) != 0,
                (pollfd.revents & POLLERR) != 0);

        /* Read an event from the userfaultfd */

        nread = read(this_uffd, &msg, sizeof(msg));
        if (nread == 0) {
            printf("EOF on userfaultfd!\n");
            exit(EXIT_FAILURE);
        } else if (nread == -1)
            errExit(RED "read from uffd");

        /* We expect only one kind of event; verify that assumption */

        assert(msg.event == UFFD_EVENT_PAGEFAULT);

        /* Display info about the page-fault event */

        printf("        PAGEFAULT event: flags = %#llx; "
               BLUE "address = " RED "%#llx\n" RESET,
               msg.arg.pagefault.flags, msg.arg.pagefault.address);

        /* Serve the page */

        struct uffdio_copy uffdio_copy = prepare_page(msg);
        printf(BLUE "        Page source = " GREEN "%llx" RESET " (PID: %d)\n"
               BLUE "        Page content = " CYAN "%08lx\n\n" RESET,
               uffdio_copy.src, getpid(), *(long *)uffdio_copy.src);
        if (ioctl(this_uffd, UFFDIO_COPY, &uffdio_copy) == -1)
            errExit("ioctl-UFFDIO_COPY");
    }
}

void start_fht(uffd_t *uffd) {
    pthread_t thr; /* ID of thread that handles page faults */
    int s = pthread_create(&thr, NULL, fault_handler_thread, (void *)uffd);
    if (s != 0) {
        errno = s;
        errExit(RED "pthread_create -> handler" RESET);
    }
}

// Tell the loader to run this function once the library is loaded
__attribute__((constructor)) int uffd_init() {
    page_size = sysconf(_SC_PAGE_SIZE);
    //setup_sigsegv_handler();

    /* Create and enable userfaultfd object */

    uffd_t uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (uffd == -1)
        errExit(RED "syscall -> userfaultfd" RESET);

    struct uffdio_api uffdio_api = {
        .api = UFFD_API,
        .features = 0
    };
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
        errExit(RED "ioctl -> UFFDIO_API" RESET);

    /* Register the memory range of the code pages for handling by the
       userfaultfd object. In mode, we request to track missing pages
       (i.e., pages that have not yet been faulted in). */
    
    get_code_vma_bounds(&glob_code_vma_start_addr, &glob_code_vma_end_addr);
    glob_new_vma = (unsigned long)file_backed_to_dontneed_anon(glob_code_vma_start_addr,
                                                               glob_code_vma_end_addr);

    struct uffdio_register uffdio_register = {
        .range = {
            .start = glob_code_vma_start_addr,
            .len = glob_code_vma_end_addr - glob_code_vma_start_addr,
        },
        .mode = UFFDIO_REGISTER_MODE_MISSING
    };
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
        errExit(RED "ioctl -> UFFDIO_REGISTER" RESET);

    /* Create a thread that will process the userfaultfd events */
    start_fht(&uffd);


    /* Block for userfaultfd events on the separate created thread,
       and let this one exit and call main in the target program */
    return 0;
}