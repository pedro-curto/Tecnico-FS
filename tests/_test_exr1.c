#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    assert(tfs_init(NULL) != -1);

    char *str1 = "HELLO WORLD\n";
    char *empty_str = "";
    char *file1 = "/file0";
    char *src_path = "tests/_text_file1.txt";
    char *src_path_2 = "tests/_text_file2.txt";
    char *new_path = "/4C";
    char *second_path = "/4c";
    char *multi_path = "f1";
    // char *output = "TTAA";
    char buffer[1100];

    // cria novo file
    int f = tfs_open(file1, TFS_O_CREAT);
    assert(f != -1);
    int a, b, c;
    a = tfs_open(file1, 0);
    b = tfs_open(file1, 0);
    c = tfs_open(file1, TFS_O_CREAT);
    assert(a != f && a != b && a != c);

    assert(tfs_copy_from_external_fs(empty_str, empty_str) == -1);

    assert(tfs_copy_from_external_fs(src_path, src_path) == -1);

    assert(tfs_copy_from_external_fs(src_path, empty_str) == -1);

    assert(tfs_copy_from_external_fs(src_path, multi_path) == -1);

    assert(tfs_copy_from_external_fs(src_path, new_path) != -1);

    assert(tfs_copy_from_external_fs(src_path_2, second_path) != -1);

    int f2 = tfs_open(new_path, 0);
    assert(f2 != -1);
    int f3 = tfs_open(second_path, 0);
    assert(f3 != -1);

    ssize_t r = tfs_read(f2, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str1));

    ssize_t r1 = tfs_read(f3, buffer, sizeof(buffer) - 1);
    assert(r1 == 1024);

    assert(tfs_write(f, buffer, sizeof(buffer) - 1) == 1024);
    memset(buffer, 0, sizeof(buffer) - 1);
    tfs_close(f);

    tfs_open(file1, 0);
    assert(tfs_read(f, buffer, sizeof(buffer) - 1) == 1024);

    tfs_close(f);
    assert(tfs_write(f, buffer, sizeof(buffer) - 1) == -1);

    printf("Successful test.\n");
    return 0;
}