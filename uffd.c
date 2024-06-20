// _GNU_SOURCE enables the RLD_NEXT handle used in dysm
#define _GNU_SOURCE

// Provide the prototypes for dlysm
#include <dlfcn.h>
#include <stddef.h>

#include <inttypes.h>
#include <sys/types.h>
#include <stdio.h>
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

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)

static int page_size;

static void *fault_handler_thread(void *arg) {
    static struct uffd_msg msg;   /* Data read from userfaultfd */
    static int fault_cnt = 0;     /* Number of faults so far handled */
    long uffd;                    /* userfaultfd file descriptor */
    static char *page = NULL;
    ssize_t nread;

    uffd = (long) arg;

    /* Create a page that will be copied into the faulting region */

    if (page == NULL) {
        page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (page == MAP_FAILED)
            errExit("mmap");
    }

    /* Loop, handling incoming events on the userfaultfd
        file descriptor */

    while (1) {

        /* See what poll() tells us about the userfaultfd */

        struct pollfd pollfd;
        int nready;
        pollfd.fd = uffd;
        pollfd.events = POLLIN;
        nready = poll(&pollfd, 1, -1);
        if (nready == -1)
            errExit("poll");
        
        printf("\nfault_handler_thread():\n");
        printf("    poll() returns: nready = %d; "
                "POLLIN = %d; POLLERR = %d\n", nready,
                (pollfd.revents & POLLIN) != 0,
                (pollfd.revents & POLLERR) != 0);

        /* Read an event from the userfaultfd */

        nread = read(uffd, &msg, sizeof(msg));
        if (nread == 0) {
            printf("EOF on userfaultfd!\n");
            exit(EXIT_FAILURE);
        }

        if (nread == -1)
            errExit("read");

        /* We expect only one kind of event; verify that assumption */

        if (msg.event != UFFD_EVENT_PAGEFAULT) {
            fprintf(stderr, "Unexpected event on userfaultfd\n");
            exit(EXIT_FAILURE);
        }

        /* Display info about the page-fault event */

        printf("    UFFD_EVENT_PAGEFAULT event: ");
        printf("flags = %"PRIx64"; ", msg.arg.pagefault.flags);
        printf("address = %"PRIx64"\n", msg.arg.pagefault.address);

        /* Copy the page pointed to by 'page' into the faulting
            region. Vary the contents that are copied in, so that it
            is more obvious that each fault is handled separately. */

        memset(page, 'A' + fault_cnt % 20, page_size);
        fault_cnt++;

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

        printf("        (uffdio_copy.copy returned %"PRId64")\n",
                uffdio_copy.copy);
    }
}

void get_code_addrs(unsigned long addrs[]) {
    char filename[128];
    snprintf(filename, sizeof(filename), "/proc/%d/maps", getpid());

    FILE *proc_maps = fopen(filename, "r");
    if (proc_maps == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    
    char line[256];
    fgets(line, sizeof(line), proc_maps); // read line 1
    fgets(line, sizeof(line), proc_maps); // read line 2 (.text)
    sscanf(line, "%lx-%lx", &addrs[0], &addrs[1]);
    fclose(proc_maps);
}

void *fBacked2Anon(unsigned long addrs[]) {
    size_t len = (size_t)(addrs[1] - addrs[0]);

    // Copy code pages to new VMA
    void *new_vma = mmap(NULL, len, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memcpy(new_vma, (void *)addrs[0], len);

    // Map anonymous VMA in place of old VMA
    munmap((void *)addrs[0], len); // Unmap the unavailable code VMA first
    void *old_vma = mmap((void *)addrs[0], len, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    // Copy back code pages to old VMA
    memcpy(old_vma, new_vma, len);
    munmap(new_vma, len);
    printf("%lx, %lx, %lx\n", new_vma, old_vma, addrs[0]);
    return old_vma;
}

// Tell the loader to run this function once the library is loaded
__attribute__((constructor))
int uffd_init() {
    long uffd;          /* userfaultfd file descriptor */
    pthread_t thr;      /* ID of thread that handles page faults */
    page_size = sysconf(_SC_PAGE_SIZE);

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
    
    unsigned long addrs[2];
    get_code_addrs(addrs); // Get start and end addresses of the code section
    printf("%d: %lx, %lx\n", getpid(), addrs[0], addrs[1]);
    void *old_vma = fBacked2Anon(addrs);

    struct uffdio_register uffdio_register = {
        .range = {
            .start = addrs[0],
            .len = addrs[1] - addrs[0]
        },
        .mode = UFFDIO_REGISTER_MODE_MISSING
    };
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
        errExit("ioctl-UFFDIO_REGISTER");

    /* Create a thread that will process the userfaultfd events */

    int s = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
    if (s != 0) {
        errno = s;
        errExit("pthread_create");
    }

    /* Block for userfaultfd events on the separate created thread,
       and let this one exit and call main in the target program */
    return 0;
}