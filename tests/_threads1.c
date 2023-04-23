#include "fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char *file1 = "/f1";
char *file2 = "/f2";
char *file3 = "/f3";
char *file4 = "/f4";

char *to_write = "AAAA";
char buffer[50];

void *thread1() {

    int f = tfs_open(file1, TFS_O_CREAT);
    assert(f != -1);

    ssize_t w = tfs_write(f, to_write, strlen(to_write));
    assert(w == strlen(to_write));

    ssize_t r = tfs_read(f, buffer, sizeof(buffer));
    assert(r == strlen(buffer));

    assert(tfs_close(f) != -1);

    f = tfs_open(file2, TFS_O_CREAT);
    assert(f != -1);

    ssize_t w1 = tfs_write(f, buffer, sizeof(buffer));
    assert(w1 == sizeof(buffer));

    assert(tfs_close(f) != -1);
    return NULL;
}

int main() {

    assert(tfs_init(NULL) != -1);

    pthread_t t1, t2, t3;

    assert(pthread_create(&t1, NULL, thread1, NULL) == 0);
    assert(pthread_create(&t2, NULL, thread1, NULL) == 0);
    assert(pthread_create(&t3, NULL, thread1, NULL) == 0);

    assert(pthread_join(t1, NULL) == 0);
    assert(pthread_join(t2, NULL) == 0);
    assert(pthread_join(t3, NULL) == 0);

    printf("Successful test.\n");
    return 0;
}