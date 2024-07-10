#include "uffd.h"

static struct child_pf_log_entry children_pf_log[MAX_CHILDREN];
static int nentries = 0;

void add_log_entry(pid_t pid, int parent_read, int parent_write) {
    struct child_pf_log_entry child_log_entry = {
        .child_pid = pid,
        .fault_cnt = 0,
        .ipc_fds = {
            .parent_read = parent_read,
            .parent_write = parent_write
        }
    };
    children_pf_log[nentries++] = child_log_entry;
}

// Get parent_write given parent_read
struct child_pf_log_entry *get_log_entry(int parent_read) {
    for (int i = 0; i < nentries; ++i) {
        if (children_pf_log[i].ipc_fds.parent_read == parent_read)
            return &children_pf_log[i];
    }
    return NULL;
}