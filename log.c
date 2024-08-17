#include "uffd.h"

struct child_proc_info children_fault_handler_log[MAX_CHILDREN];
static int initialized = 0;
static int nelems = 0;   // number of elements (of type struct child_proc_info) in the log
static int nentries = 0; // number of nonempty entries (not available for reuse)

void add_log_entry(pid_t child_pid, uffd_t child_uffd) {
    // initialize log to zero if first enry is being added
    if (!initialized) {
        memset(children_fault_handler_log, 0, sizeof(children_fault_handler_log));
        initialized = 1;
    }

    // if capacity is reached, return error
    if (nelems == MAX_CHILDREN) {
        fprintf(stderr, "Fault handler log FULL!\n");
        exit(EXIT_FAILURE);
    }

    // reuse existing empty slot by updating log entry
    for (int i = 0; i < nelems; ++i) {
        if (children_fault_handler_log[i].pid == -1) {
            children_fault_handler_log[i].pid = child_pid;
            children_fault_handler_log[i].uffd = child_uffd;
            children_fault_handler_log[i].mru_page = NULL;
            ++nentries;
            return;
        }
    }

    // else, add new entry
    struct child_proc_info new_entry = {
        .pid = child_pid,
        .uffd = child_uffd,
        .mru_page = NULL
    };
    children_fault_handler_log[nelems++] = new_entry;
    ++nentries;
}

struct child_proc_info *get_proc_info_by_uffd(uffd_t child_uffd) {
    for (int i = 0; i < nelems; ++i) {
        if (children_fault_handler_log[i].uffd == child_uffd)
            return &children_fault_handler_log[i];
    }
    return NULL;
}

struct child_proc_info *get_proc_info_by_pid(pid_t child_pid) {
    for (int i = 0; i < nelems; ++i) {
        if (children_fault_handler_log[i].pid == child_pid)
            return &children_fault_handler_log[i];
    }
    return NULL;
}

void mark_as_removed(pid_t dead_children[], int ndead) {
    for (int i = 0; i < ndead; ++i) {
        struct child_proc_info *proc_info = get_proc_info_by_pid(dead_children[i]);
        proc_info->pid = -1;
        proc_info->uffd = -1;
        proc_info->mru_page = NULL;
        --nentries;
    }
}

void dump_log() {
    if (nelems > 0) {
        printf("(%d,%d)", children_fault_handler_log[0].pid, children_fault_handler_log[0].uffd);
        for (int i = 1; i < nelems; ++i) {
            printf(", (%d,%d)", children_fault_handler_log[i].pid, children_fault_handler_log[i].uffd);
        }
        printf("\n");
    } else {
        printf("<EMPTY FHL>\n");
    }
}