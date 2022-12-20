#include "../fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    const char *file_path = "/f1";
    const char *link_path = "/l1";

    assert(tfs_init(NULL) != -1);

    // Create file
    int fd = tfs_open(file_path, TFS_O_CREAT);
    assert(fd != -1);

    int a;

    a = tfs_link(file_path, link_path);
    assert(a != -1);

    a = (tfs_unlink(file_path));
    assert(a == -1);

    printf("Successful test.\n");
}