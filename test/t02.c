#include <stdio.h>
#include <unistd.h>

int main() {
    for (int i = 0; i < 10000; ++i) {
        printf("%d\n", i);
        sleep(1);
    }
}
