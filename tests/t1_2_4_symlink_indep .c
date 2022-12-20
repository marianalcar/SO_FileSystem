#include "fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PATH_FORMAT "/f%ld_%ld"
#define FILE_COUNT 5
#define MAX_PATH_SIZE 32

uint8_t const file_contents[] = "AAA!";

void _format_path(char *dest, size_t max_size, size_t rep, size_t file_idx) {
    int ret = snprintf(dest, max_size, PATH_FORMAT, rep, file_idx);
    assert(ret > 0);
    assert(ret <= max_size);
}

int _open(size_t rep, size_t file_idx, tfs_file_mode_t mode) {
    char path[MAX_PATH_SIZE];
    _format_path(path, MAX_PATH_SIZE, rep, file_idx);
    return tfs_open(path, mode);
}

int _link(size_t rep_target, size_t file_idx_target, size_t rep_name,
          size_t file_idx_name) {
    char target[MAX_PATH_SIZE];
    _format_path(target, MAX_PATH_SIZE, rep_target, file_idx_target);

    char name[MAX_PATH_SIZE];
    _format_path(name, MAX_PATH_SIZE, rep_name, file_idx_name);

    return tfs_unlink(target);
}

int _unlink(size_t rep, size_t file_idx) {
    char path[MAX_PATH_SIZE];
    _format_path(path, MAX_PATH_SIZE, rep, file_idx);
    return tfs_unlink(path);
}

void assert_contents_ok(size_t rep, size_t file_idx) {
    int f = _open(rep, file_idx, 0);
    assert(f != -1);

    uint8_t buffer[sizeof(file_contents)];
    assert(tfs_read(f, buffer, sizeof(buffer)) == sizeof(buffer));
    assert(memcmp(buffer, file_contents, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void assert_empty_file(size_t rep, size_t file_idx) {
    int f = _open(rep, file_idx, 0);
    assert(f != -1);

    uint8_t buffer[sizeof(file_contents)];
    assert(tfs_read(f, buffer, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void write_contents(size_t rep, size_t file_idx) {
    int f = _open(rep, file_idx, 0);
    assert(f != -1);

    assert(tfs_write(f, file_contents, sizeof(file_contents)) ==
           sizeof(file_contents));

    assert(tfs_close(f) != -1);
}

void run_test(size_t rep) {
    // _open(rep, <number from 0 to FILE_COUNT>, mode) opens one of the
    // alternative names to the same file

    // confirm that all are empty
    for (size_t i = 0; i < FILE_COUNT; i++) {
        assert_empty_file(rep, i);
    }

    // delete half the links
    for (size_t i = FILE_COUNT / 2; i < FILE_COUNT; i++) {
        assert(_unlink(rep, i) != -1);
    }

    // write in one of the links
    write_contents(rep, 0);

    // confirm that the others see the write
    for (size_t i = 0; i < FILE_COUNT / 2; i++) {
        assert_contents_ok(rep, i);
    }

    // finish removing links
    for (size_t i = 0; i < FILE_COUNT / 2; i++) {
        assert(_unlink(rep, i) != -1);
    }
}

int main() {
    assert(tfs_init(NULL) != -1);

    for (size_t rep = 0; rep < FILE_COUNT; rep++) {
        // create original file
        {
            int fd = _open(rep, rep, TFS_O_CREAT);
            assert(fd != -1);
            assert(tfs_close(fd) != -1);
        }

        // create links
        for (size_t i = 0; i < FILE_COUNT; i++) {
            if (i == rep) {
                continue;
            }

            // instead of linking to the original always, let's create a link to
            // the previously linked name
            if (i == 0) {
                // no links exist yet, link to the original
                assert(_link(rep, rep, rep, i) != -1);
            } else {
                // link to the link created in the previous iteration
                assert(_link(rep, i - 1, rep, i) != -1);
            }
        }

        run_test(rep);
    }

    printf("Successful test.\n");

    return 0;
}
