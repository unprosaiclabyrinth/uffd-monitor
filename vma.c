#include "uffd.h"

void get_code_vma_bounds(unsigned long *code_vma_start_addr,
                         unsigned long *code_vma_end_addr) {
    FILE *proc_maps = fopen("/proc/self/maps", "r");
    if (proc_maps == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    
    char line[256];
    __attribute__((unused)) char *throwaway = fgets(line, sizeof(line), proc_maps); // read line 1
    throwaway = fgets(line, sizeof(line), proc_maps); // read line 2 (.text)
    sscanf(line, "%lx-%lx", code_vma_start_addr, code_vma_end_addr);
    fclose(proc_maps);

    printf(BLUE "                PID: " YELLOW "%d\n" RESET , getpid());
    printf(BLUE "Code VMA start addr: " RESET "%#lx\n", *code_vma_start_addr);
    printf("  Code VMA end addr: %#lx\n", *code_vma_end_addr);
}

/*
    1. Copy code pages to a new VMA (private and anonymous)
    2. Munmap original code VMA, and again mmap it as private and anon.
    3. Copy code pages from new VMA to old VMA (with correct permisions).
    4. Drop all code pages.
    5. Return pointer to new VMA.
*/
void *setup_code_monitor(unsigned long code_vma_start_addr,
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

    printf(BLUE "    Code VMA length: " YELLOW "%ld\n" RESET, len);
    printf(BLUE "  Code monitor addr: " RESET "%p\n\n", new_vma);
    // Drop all code pages
    madvise(old_vma, len, MADV_DONTNEED);

    return new_vma;
}