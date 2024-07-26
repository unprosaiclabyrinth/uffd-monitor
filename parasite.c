#include <sys/mman.h>
#include <errno.h>

#include <compel/plugins/std.h>
#include <infect-rpc.h>

/*
 * Stubs for std compel plugin.
 */
int parasite_trap_cmd(int cmd, void *args) {
	return 0;
}

void parasite_cleanup(void) {}

#define PARASITE_CMD_MADV PARASITE_USER_CMDS
// #define PAGE_SIZE sys_sysconf(_SC_PAGE_SIZE)

int parasite_daemon_cmd(int cmd, void *arg) {
	if (cmd == PARASITE_CMD_MADV && arg != NULL)
		return sys_madvise(*((unsigned long *)arg), 4096, MADV_DONTNEED);
	return 0;
}