#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {


    char *path_copied_file = "/f1";
    char *path_src = "tests/file_to_copy_out_of_bounds.txt";

    assert(tfs_init(NULL) != -1);

    int f;

    //fail if path_src contains more than 1024 bytes(1 block)
    f = tfs_copy_from_external_fs(path_src, path_copied_file);
    assert(f == -1);

    printf("Successful test.\n");

    return 0;

}
