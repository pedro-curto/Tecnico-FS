#include "fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

uint8_t const file_contents[] = "AAA!";
char const link_path1[] = "/l1";
char const link_path2[] = "/l2";
char const link_path3[] = "/l3";
char const target_path1[] = "/f1";
char const target_path2[] = "/f2";
char const target_path3[] = "/f3";
char const symlink_path1[] = "/s1";
char const symlink_path2[] = "/s2";
char const symlink_path3[] = "/s3";
char buffer1[1024];
char *str1 = "AAA!";

void assert_contents_ok(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    uint8_t buffer[sizeof(file_contents)];
    assert(tfs_read(f, buffer, sizeof(buffer)) == sizeof(buffer));
    assert(memcmp(buffer, file_contents, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void assert_empty_file(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    uint8_t buffer[sizeof(file_contents)];
    assert(tfs_read(f, buffer, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void write_contents(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    assert(tfs_write(f, file_contents, sizeof(file_contents)) ==
           sizeof(file_contents));

    assert(tfs_close(f) != -1);
}

void read_contents(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    memset(buffer1, 0, sizeof buffer1);
    assert(tfs_read(f, buffer1, sizeof(buffer1)) == sizeof(file_contents));

    assert(tfs_close(f) != -1);
}

void assert_error(char const *path) {
    int f = tfs_open(path, 0);
    assert(f == -1);
}

int main() {
    assert(tfs_init(NULL) != -1);

    // Write to symlink and read original file
    {
        int f = tfs_open(target_path1, TFS_O_CREAT);
        assert(f != -1);
        assert(tfs_close(f) != -1);

        assert_empty_file(target_path1); // sanity check
    }

    assert(tfs_link(target_path1, link_path1) != -1);
    assert(tfs_link(link_path1, link_path2) != -1);
    assert(tfs_link(link_path2, link_path1) == -1);
    assert(tfs_link(link_path2, link_path3) != -1);
    assert(tfs_sym_link(link_path3, symlink_path1) != -1);
    assert(tfs_sym_link(symlink_path1, symlink_path2) != -1);

    assert(tfs_unlink(link_path1) != -1);
    assert(tfs_link(symlink_path1, link_path1) == -1);

    write_contents(link_path3);
    assert_contents_ok(symlink_path1);
    read_contents(link_path2);

    assert(tfs_unlink(link_path3) != -1);

    assert_error(symlink_path1);
    assert_error(symlink_path2);
    assert_error(link_path3);

    assert(tfs_link(link_path2, link_path3) != -1);

    write_contents(symlink_path1);
    read_contents(symlink_path2);

    assert(tfs_unlink(symlink_path1) != -1);
    assert(tfs_sym_link(symlink_path1, symlink_path3) != -1);

    assert(tfs_sym_link(link_path2, symlink_path3) == -1);
    assert(tfs_sym_link(link_path3, symlink_path3) == -1);
    assert(tfs_sym_link(symlink_path3, symlink_path3) == -1);

    assert(tfs_unlink(link_path3) != -1);

    assert(tfs_link(link_path2, link_path3) != -1);

    assert(tfs_unlink(link_path1) == -1);
    assert(tfs_unlink(link_path3) != -1);
    assert(tfs_unlink(link_path2) != -1);
    assert(tfs_unlink(target_path1) != -1);
    assert(tfs_unlink(link_path3) == -1);

    assert_error(symlink_path1);
    assert_error(symlink_path2);
    assert_error(symlink_path3);
    assert_error(target_path1);
    assert_error(target_path2);
    assert_error(target_path3);

    assert(tfs_destroy() != -1);

    printf("Successful test.\n");

    return 0;
}
