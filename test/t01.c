/**
 * t01.c: A super simple I/O test case.
 * */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>         /* mmap() constants */
#include <stdlib.h>

#define PAGESIZE 4096

void *pages = NULL;

int main()
{
	char name[4096];
	int ret = 0;
	int loop = 1;

	/* Allocate two pages. */
	if ((pages = mmap(NULL, PAGESIZE * 2, PROT_READ,
			  MAP_PRIVATE | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED) {
		fprintf(stderr, "++ mmap failed: %m\n");
		exit(1);
	}

    /* Touch the two pages. */
	printf("page address: %p, contents:\n", pages);
    fprintf(stderr, "%s", (char *)pages);
	fprintf(stderr, "%s", &((char *)pages)[PAGESIZE]);

	while (loop) {
		printf("Input your name ('quit' to exit): \n");
		fflush(stdout);
		ret = scanf("%s", name);
		printf("pid: %d. Hi, %s. scanf ret: %d\n",
		       getpid(), name, ret);
		if (!strncmp(name, "quit", 4)) {
			loop = 0;
		}
	}
	return 0;
}