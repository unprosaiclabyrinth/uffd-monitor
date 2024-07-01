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

#include <signal.h>
#include <execinfo.h>
#include <ucontext.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)

static int page_size;

static void *fault_handler_thread(void *args) {
    static struct uffd_msg msg;                   /* Data read from userfaultfd */
    static int fault_cnt = 0;                     /* Number of faults so far handled */
    long uffd = ((long *)args)[0];                /* userfaultfd file descriptor */
    long code_offset = ((long *)args)[1];         /* offset of code VMA in executable */
    long code_vma_start_addr = ((long *)args)[2]; /* start address of code VMA */
    char *page = NULL;
    ssize_t nread;

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
        if (nready == -1)
            errExit("poll");
        printf("\nfault_handler_thread():-\n");
        printf("   %d. poll() returns: nready = %d; POLLIN = %d; POLLERR = %d\n",
                ++fault_cnt, nready, (pollfd.revents & POLLIN) != 0,
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

        printf("      UFFD_EVENT_PAGEFAULT event: ");
        printf("flags = %#llx; ", msg.arg.pagefault.flags);
        printf("address = %#llx\n", msg.arg.pagefault.address);

        /* Create a page that will be copied into the faulting region */

        if (page == NULL) {
            int exefd = open("/proc/self/exe", O_RDONLY);
            off_t page_offset = (off_t)((code_offset + (msg.arg.pagefault.address -
                                        code_vma_start_addr)) & ~(page_size - 1));
            page = mmap(NULL, page_size, PROT_READ | PROT_EXEC,
                        MAP_PRIVATE, exefd, page_offset);
            printf("      Offset in executable: %ld; Code page address: %#lx\n",
                    page_offset, (unsigned long)page);
            close(exefd);
            if (page == MAP_FAILED)
                errExit("mmap");
        }

        /* Copy the page pointed to by 'page' into the faulting
            region. Vary the contents that are copied in, so that it
            is more obvious that each fault is handled separately. */

        struct uffdio_copy uffdio_copy = {
            .src = (unsigned long)page,

            /* We need to handle page faults in units of pages(!).
               So, round faulting address down to page boundary */

            .dst = (unsigned long)msg.arg.pagefault.address & ~(page_size - 1),
            .len = page_size,
            .mode = 0,
            .copy = 0
        };
        if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
            errExit("ioctl-UFFDIO_COPY");

        printf("      (uffdio_copy.copy returned %lld)\n", uffdio_copy.copy);
    }
}

void get_code_addrs_and_offset(unsigned long *code_vma_start_addr, 
                               unsigned long *code_vma_end_addr,long *code_offset) {
    FILE *proc_maps = fopen("/proc/self/maps", "r");
    if (proc_maps == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    
    char line[256];
    fgets(line, sizeof(line), proc_maps); // read line 1
    fgets(line, sizeof(line), proc_maps); // read line 2 (.text)
    sscanf(line, "%lx-%lx r-xp %lx", code_vma_start_addr, code_vma_end_addr, code_offset);
    fclose(proc_maps);

    printf("                PID: %d\n", getpid());
    printf("Code VMA start addr: %#lx\n", *code_vma_start_addr);
    printf("  Code VMA end addr: %#lx\n", *code_vma_end_addr);
    printf("             Offset: %ld\n", *code_offset);
}

void *file_backed_to_dontneed_anon(unsigned long code_vma_start_addr,
                                   unsigned long code_vma_end_addr) {
    size_t len = (size_t)(code_vma_end_addr - code_vma_start_addr);

    // Copy code pages to new VMA
    void *new_vma = mmap(NULL, len, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_vma == MAP_FAILED)
        errExit("new_vma-mmap");
    memcpy(new_vma, (void *)code_vma_start_addr, len);

    // Map anonymous VMA in place of old VMA
    munmap((void *)code_vma_start_addr, len); // Unmap the unavailable code VMA first
    void *old_vma = mmap((void *)code_vma_start_addr, len, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (old_vma == MAP_FAILED)
        errExit("old_vma-mmap");
    assert((unsigned long)old_vma == code_vma_start_addr);
    
    // Copy code pages back to old VMA
    memcpy(old_vma, new_vma, len);
    mprotect(old_vma, len, PROT_READ | PROT_EXEC);
    munmap(new_vma, len);

    printf("Code section length: %ld\n", len);
    printf("       New VMA addr: %p\n", new_vma);
    printf("       Old VMA addr: %p\n", old_vma);
    // Drop all code pages
    printf("        madvise ret: %d\n", madvise(old_vma, len, MADV_DONTNEED));

    return old_vma;
}

void sigsegv_handler(int sig __attribute__((unused)), siginfo_t *si,
                     void *unused __attribute__((unused))) {
    printf("Caught SIGSEGV at address: %p\n", si->si_addr);

    // Optionally, print the stack trace
    void *buffer[30];
    int nptrs = backtrace(buffer, 30);
    backtrace_symbols_fd(buffer, nptrs, STDERR_FILENO);

    exit(EXIT_FAILURE);
}

void setup_sigsegv_handler() {
    struct sigaction sa = {
        .sa_flags = SA_SIGINFO,
        .sa_sigaction = sigsegv_handler
    };
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

// Tell the loader to run this function once the library is loaded
__attribute__((constructor))
int uffd_init() {
    long uffd; /* userfaultfd file descriptor */
    pthread_t thr;      /* ID of thread that handles page faults */
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
    long code_offset; // Offset of the code VMA in the executable file
    get_code_addrs_and_offset(&code_vma_start_addr, &code_vma_end_addr, &code_offset);
    void *old_vma = file_backed_to_dontneed_anon(code_vma_start_addr, code_vma_end_addr);

    struct uffdio_register uffdio_register = {
        .range = {
            .start = (unsigned long)old_vma,
            .len = code_vma_end_addr - code_vma_start_addr,
        },
        .mode = UFFDIO_REGISTER_MODE_MISSING
    };
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
        errExit("ioctl-UFFDIO_REGISTER");

    /* Create a thread that will process the userfaultfd events */

    long args[3] = {uffd, code_offset, (long)code_vma_start_addr};
    int s = pthread_create(&thr, NULL, fault_handler_thread, (void *)args);
    if (s != 0) {
        errno = s;
        errExit("pthread_create");
    }
    printf(" pthread_create ret: %d\n", s);

    /* Block for userfaultfd events on the separate created thread,
       and let this one exit and call main in the target program */
    return 0;
}