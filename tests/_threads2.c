#include "fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char *path_copied_file = "/f1";
char *path_t1 = "tests/_thread1.txt";
char *final = "Thread 2";
char buffer[50];

void *thread1() {
    int f = tfs_copy_from_external_fs(path_t1, path_copied_file);
    assert(f != -1);
    return NULL;
}

void *thread2() {
    int f = tfs_copy_from_external_fs(path_t1, path_copied_file);
    assert(f != -1);
    return NULL;
}

void *thread3() {
    int f = tfs_copy_from_external_fs(path_t1, path_copied_file);
    assert(f != -1);
    return NULL;
}

int main() {
    assert(tfs_init(NULL) != -1);

    pthread_t t1, t2, t3;
    assert(pthread_create(&t1, NULL, thread1, NULL) == 0);
    assert(pthread_create(&t2, NULL, thread2, NULL) == 0);
    assert(pthread_create(&t3, NULL, thread3, NULL) == 0);

    int f = tfs_open(path_copied_file, TFS_O_CREAT);
    assert(f != -1);

    assert(pthread_join(t1, NULL) == 0);
    assert(pthread_join(t2, NULL) == 0);
    assert(pthread_join(t3, NULL) == 0);

    ssize_t read = tfs_read(f, buffer, sizeof(buffer));
    assert(read == strlen(final));

    printf("Successful test.\n");
    return 0;
}