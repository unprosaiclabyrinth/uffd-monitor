#include "uffd.h"

static struct child_pf_log_entry children_pf_log[MAX_CHILDREN];
static int nentries = 0;

void add_log_entry(pid_t child_pid, uffd_t child_uffd, int parent_read) {
    struct child_pf_log_entry child_log_entry = {
        .child_pid = child_pid,
        .child_uffd = child_uffd,
        .parent_read = parent_read,
        .fault_cnt = 0
    };
    children_pf_log[nentries++] = child_log_entry;
}

// Get parent_write given parent_read
struct child_pf_log_entry *get_log_entry(int parent_read) {
    for (int i = 0; i < nentries; ++i) {
        if (children_pf_log[i].parent_read == parent_read)
            return &children_pf_log[i];
    }
    return NULL;
}