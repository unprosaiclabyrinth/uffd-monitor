#include "uffd.h"
#define MAX_CHILDREN 1000

struct child_fhl_entry children_fault_handler_log[MAX_CHILDREN];
int nentries = 0;

void add_fhl_entry(pid_t child_pid, uffd_t child_uffd) {
    // initialize log to zero if first enry is being added
    if (nentries == 0)
        memset(children_fault_handler_log, 0, sizeof(children_fault_handler_log));

    // if uffd already exists in log (reused),
    // update log entry
    for (int i = 0; i < nentries; ++i) {
        if (children_fault_handler_log[i].uffd == child_uffd) {
            children_fault_handler_log[i].pid = child_pid;
            children_fault_handler_log[i].mru_page = NULL;
            return;
        }
    }
    // else, add new entry
    struct child_fhl_entry new_entry = {
        .pid = child_pid,
        .uffd = child_uffd,
        .mru_page = NULL
    };
    children_fault_handler_log[nentries++] = new_entry;
}

struct child_fhl_entry *get_fhl_entry_by_uffd(uffd_t child_uffd) {
    for (int i = 0; i < nentries; ++i) {
        if (children_fault_handler_log[i].uffd == child_uffd)
            return &children_fault_handler_log[i];
    }
    return NULL;
}

void scan_and_clean_fhl(pid_t dead_children[], int ndead) {
    int j = 0;
    int found;

    for (int e = 0; e < nentries; ++e) {
        found = 0;

        for (int d = 0; d < ndead; ++d) {
            if (children_fault_handler_log[e].pid == dead_children[d]) {
                found = 1;
                break;
            }
        }

        if (!found)
            children_fault_handler_log[j++] = children_fault_handler_log[e];
    }

    for (int i = j; i < nentries; ++i) {
        memset(&children_fault_handler_log[i], 0, sizeof(struct child_fhl_entry));
    }
    nentries = j;
}

void dump_fhl() {
    if (nentries > 0) {
        printf("(%d,%d)", children_fault_handler_log[0].pid, children_fault_handler_log[0].uffd);
        for (int i = 1; i < nentries; ++i) {
            printf(", (%d,%d)", children_fault_handler_log[i].pid, children_fault_handler_log[i].uffd);
        }
        printf("\n");
    } else {
        printf("<EMPTY FHL>\n");
    }
}