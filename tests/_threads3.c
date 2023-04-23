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

char *link1 = "l1";
char *link2 = "l2";
char *link3 = "l3";

char *exp_output = "";
char buffer[50];

void *thread1() {
    int f1 = tfs_open(file1, TFS_O_CREAT);
    assert(f1 != -1);
    assert(tfs_link(file1, link1) != -1);
    return NULL;
}

void *thread2() {
    int f1 = tfs_open(file2, TFS_O_CREAT);
    assert(f1 != -1);
    assert(tfs_link(file2, link2) != -1);
    return NULL;
}

void *thread3() {
    int f1 = tfs_open(file3, TFS_O_CREAT);
    assert(f1 != -1);
    assert(tfs_link(file3, link3) != -1);
    return NULL;
}

int main() {
    tfs_params params = tfs_default_params();
    params.max_open_files_count = 3;
    assert(tfs_init(&params) != -1);

    pthread_t t1, t2, t3;

    assert(pthread_create(&t1, NULL, thread1, NULL) == 0);
    assert(pthread_create(&t2, NULL, thread2, NULL) == 0);
    assert(pthread_create(&t3, NULL, thread3, NULL) == 0);

    assert(pthread_join(t1, NULL) == 0);
    assert(pthread_join(t2, NULL) == 0);
    assert(pthread_join(t3, NULL) == 0);

    int f = tfs_open(file4, TFS_O_CREAT);
    assert(f == -1);

    printf("Successful test.\n");
    return 0;
}