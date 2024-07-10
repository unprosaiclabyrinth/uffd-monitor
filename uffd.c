#include "uffd.h"

static int page_size;
long uffd;
int self_pipe_fds[2];

static long long int serve_page(int fd, struct uffd_msg msg, long new_vma,
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
    if (ioctl(fd, UFFDIO_COPY, &uffdio_copy) == -1)
        errExit("ioctl-UFFDIO_COPY");

    return uffdio_copy.copy;
}

static void *fault_handler_thread(void *args) {
    struct uffd_msg msg;                          /* Data read from userfaultfd */
    static int fault_cnt = 0;                     /* Number of faults so far handled */
    long new_vma = ((long *)args)[0];             /* offset of code VMA in executable */
    long code_vma_start_addr = ((long *)args)[1]; /* start address of code VMA */
    ssize_t nread;
    int nfds = 0;
    if (pipe(self_pipe_fds) == -1)
        errExit("self pipe");

    printf(MAGENTA "fault_handler_thread():-\n" RESET);

    /* Loop, handling incoming events on the userfaultfd
       file descriptor */

    while (1) {
        /* See what poll() tells us about the userfaultfd */

        int nready;
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
        nready = poll(poll_fds, nfds, -1);
        if (nready == -1)
            errExit("poll");
        int ready_fds[nready];
        get_ready_fds(ready_fds, poll_fds, nready);

        // Poll fds update mechanism: check for self pipe read
        if (fd_is_ready(self_pipe_fds[0], ready_fds, nready)) {
            int parent_read;
            read(self_pipe_fds[0], &parent_read, sizeof(parent_read));
            add_fd(parent_read, POLLIN, poll_fds, nfds++);
            continue;
        }
        
        // Handle page faults of the parent process
        if (fd_is_ready(uffd, ready_fds, nready)) {
            struct pollfd pollfd = *get_pollfd(uffd, poll_fds, nfds);
            printf(MAGENTA "\n%6d. " RESET "poll() returns:"
                   "nready = %d; POLLIN = %d; POLLERR = %d\n",
                   fault_cnt, nready, (pollfd.revents & POLLIN) != 0,
                   (pollfd.revents & POLLERR) != 0);

            /* Read an event from the userfaultfd */

            nread = read(uffd, &msg, sizeof(msg));
            if (nread == 0) {
                printf("EOF on userfaultfd!\n");
                exit(EXIT_FAILURE);
            } else if (nread == -1)
                errExit("read");

            /* We expect only one kind of event; verify that assumption */

            if (msg.event != UFFD_EVENT_PAGEFAULT) {
                fprintf(stderr, "Unexpected event on userfaultfd\n");
                exit(EXIT_FAILURE);
            }

            /* Display info about the page-fault event */

            printf("        PAGEFAULT event: ");
            printf("flags = %#llx; ", msg.arg.pagefault.flags);
            printf(BLUE "address = " RED "%#llx\n" RESET, msg.arg.pagefault.address);

            long long int uffdio_copy_copy = serve_page(uffd, msg, new_vma,
                                                        code_vma_start_addr);
            printf("(uffdio_copy.copy -> %lld)\n\n", uffdio_copy_copy);
        }

        // Handle page faults of child(ren)
        for (int i = 0; i < nready; ++i) {
            if (ready_fds[i] != uffd && ready_fds[i] != self_pipe_fds[0]) {
                int parent_read = ready_fds[i];

                nread = read(parent_read, &msg, sizeof(msg));
                if (nread == 0) {
                    printf("EOF on userfaultfd!\n");
                    exit(EXIT_FAILURE);
                } else if (nread == -1)
                    errExit("read");

                if (msg.event != UFFD_EVENT_PAGEFAULT) {
                    fprintf(stderr, "Unexpected event on userfaultfd\n");
                    exit(EXIT_FAILURE);
                }

                struct child_pf_log_entry *child_info = get_log_entry(parent_read);
                serve_page(child_info->ipc_fds.parent_write, msg, new_vma,
                           code_vma_start_addr);
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

    uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (uffd == -1)
        errExit("userfaultfd");

    struct uffdio_api uffdio_api = {
        .api = UFFD_API,
        .features = 0
    };
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
        errExit("ioctl-UFFDIO_API");

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
        errExit("ioctl-UFFDIO_REGISTER");

    /* Create a thread that will process the userfaultfd events */

    long args[2] = {(long)new_vma, (long)code_vma_start_addr};
    int s = pthread_create(&thr, NULL, fault_handler_thread, (void *)args);
    if (s != 0) {
        errno = s;
        errExit("pthread_create");
    }
    printf(" pthread_create ret: %d\n\n", s);

    /* Block for userfaultfd events on the separate created thread,
       and let this one exit and call main in the target program */
    return 0;
}