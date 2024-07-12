#include "uffd.h"

static int page_size;
uffd_t uffd;
int self_pipe_fds[2];

static struct uffdio_copy prepare_page(struct uffd_msg msg, long new_vma,
                                       long code_vma_start_addr) {
    /* Create a page that will be copied into the faulting region */

    long page_offset = msg.arg.pagefault.address - code_vma_start_addr;
    unsigned long page = (new_vma + page_offset) & ~(page_size - 1);
    printf(BLUE "        Page source = " GREEN "%lx " RESET, page);
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

static void *fault_handler_thread(void *args) {
    struct uffd_msg msg;                          /* Data read from userfaultfd */
    static int fault_cnt = 0;                     /* Number of faults so far handled */
    long new_vma = ((long *)args)[0];             /* offset of code VMA in executable */
    long code_vma_start_addr = ((long *)args)[1]; /* start address of code VMA */
    ssize_t nread;
    int nfds = 0;
    if (pipe(self_pipe_fds) == -1)
        errExit(RED "handler -> pipe -> self_pipe" RESET);

    struct pollfd poll_fds[MAX_CHILDREN];
    // Add read end of the self pipe
    poll_fds[0] = (struct pollfd){
        .fd = self_pipe_fds[0],
        .events = POLLIN
    }; ++nfds;
    // Add uffd
    poll_fds[1] = (struct pollfd){
        .fd = uffd,
        .events = POLLIN
    }; ++nfds;

    printf(MAGENTA "fault_handler_thread():-\n" RESET);

    /* Loop, handling incoming events on the userfaultfd
       file descriptor */

    while (1) {
        /* See what poll() tells us about the userfaultfd */

        int nready;
        nready = poll(poll_fds, nfds, -1);
        if (nready == -1)
            errExit(RED "handler -> poll" RESET);
        int ready_fds[nready];
        get_ready_fds(ready_fds, nready, poll_fds, nfds);

        // Poll fds update mechanism: check for self pipe read
        if (fd_is_ready(self_pipe_fds[0], ready_fds, nready)) {
            int parent_read;
            if (read(self_pipe_fds[0], &parent_read, sizeof(parent_read)) == -1)
                errExit(RED "handler -> read -> self_pipe" RESET);
            add_fd(parent_read, POLLIN, poll_fds, nfds++);
        }
        
        // Handle page faults of the parent process
        if (fd_is_ready(uffd, ready_fds, nready)) {
            struct pollfd pollfd = *get_pollfd(uffd, poll_fds, nfds);
            printf(MAGENTA "\n%6d. " RESET "poll() returns: "
                   "nready = %d; POLLIN = %d; POLLERR = %d\n",
                   ++fault_cnt, nready, (pollfd.revents & POLLIN) != 0,
                   (pollfd.revents & POLLERR) != 0);

            /* Read an event from the userfaultfd */

            nread = read(uffd, &msg, sizeof(msg));
            if (nread == 0) {
                printf(RED "handler -> read -> uffd: EOF!\n" RESET);
                exit(EXIT_FAILURE);
            } else if (nread == -1)
                errExit(RED "handler -> read -> uffd" RESET);

            /* We expect only one kind of event; verify that assumption */

            assert(msg.event == UFFD_EVENT_PAGEFAULT);

            /* Display info about the page-fault event */

            printf("        PAGEFAULT event: ");
            printf("flags = %#llx; ", msg.arg.pagefault.flags);
            printf(BLUE "address = " RED "%#llx\n" RESET, msg.arg.pagefault.address);

            struct uffdio_copy uffdio_copy = prepare_page(msg, new_vma,
                                                          code_vma_start_addr);
            // Serve the page
            if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy))
                errExit(RED "ioctl -> UFFDIO_COPY" RESET);
            printf("(read fd = %d, copy fd = %d)\n\n", uffd, uffd);
        }

        // Handle page faults of child(ren)
        for (int i = 0; i < nready; ++i) {
            if (ready_fds[i] != uffd && ready_fds[i] != self_pipe_fds[0]) {
                int parent_read = ready_fds[i];
                struct pollfd pollfd = *get_pollfd(parent_read, poll_fds, nfds);
                printf(MAGENTA "\n%6d. " RESET "poll() returns: "
                       "nready = %d; POLLIN = %d; POLLERR = %d\n",
                       ++fault_cnt, nready, (pollfd.revents & POLLIN) != 0,
                       (pollfd.revents & POLLERR) != 0);

                nread = read(parent_read, &msg, sizeof(msg));
                if (nread == 0) {
                    printf(RED "handler -> parent_read: EOF!\n" RESET);
                    exit(EXIT_FAILURE);
                } else if (nread == -1)
                    errExit(RED "handler -> read -> parent_read" RESET);

                assert(msg.event == UFFD_EVENT_PAGEFAULT);

                printf("        PAGEFAULT event: ");
                printf("flags = %#llx; ", msg.arg.pagefault.flags);
                printf(BLUE "address = " RED "%#llx\n" RESET, msg.arg.pagefault.address);

                struct child_pf_log_entry *child_info = get_log_entry(parent_read);
                struct uffdio_copy uffdio_copy = prepare_page(msg, new_vma,
                                                              code_vma_start_addr);
                // Serve the page
                if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy))
                    errExit(RED "ioctl -> UFFDIO_COPY" RESET);
                printf("(read fd = %d, copy fd = %d)\n\n", parent_read, child_info->child_uffd);
                child_info->fault_cnt++;
            }
        }
    }
}

// Tell the loader to run this function once the library is loaded
__attribute__((constructor)) int uffd_init() {
    pthread_t thr; /* ID of thread that handles page faults */
    page_size = sysconf(_SC_PAGE_SIZE);
    //setup_sigsegv_handler();

    /* Create and enable userfaultfd object */

    uffd = syscall(__NR_userfaultfd, O_NONBLOCK);
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
    
    unsigned long code_vma_start_addr, code_vma_end_addr;
    get_code_vma_bounds(&code_vma_start_addr, &code_vma_end_addr);
    void *new_vma = file_backed_to_dontneed_anon(code_vma_start_addr,
                                                 code_vma_end_addr);

    struct uffdio_register uffdio_register = {
        .range = {
            .start = code_vma_start_addr,
            .len = code_vma_end_addr - code_vma_start_addr,
        },
        .mode = UFFDIO_REGISTER_MODE_MISSING
    };
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
        errExit(RED "ioctl -> UFFDIO_REGISTER" RESET);

    /* Create a thread that will process the userfaultfd events */

    long args[2] = {(long)new_vma, (long)code_vma_start_addr};
    int s = pthread_create(&thr, NULL, fault_handler_thread, (void *)args);
    if (s != 0) {
        errno = s;
        errExit(RED "pthread_create -> handler" RESET);
    }
    printf(" pthread_create ret: %d\n\n", s);

    /* Block for userfaultfd events on the separate created thread,
       and let this one exit and call main in the target program */
    return 0;
}