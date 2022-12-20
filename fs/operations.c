#include "operations.h"
#include "config.h"
#include "state.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>



#include "betterassert.h"

tfs_params tfs_default_params() {
    tfs_params params = {
        .max_inode_count = 64,
        .max_block_count = 1024,
        .max_open_files_count = 16,
        .block_size = 1024,
    };
    return params;
}

int tfs_init(tfs_params const *params_ptr) {
    tfs_params params;
    if (params_ptr != NULL) {
        params = *params_ptr;
    } else {
        params = tfs_default_params();
    }

    if (state_init(params) != 0) {
        return -1;
    }

    // create root inode
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    if (state_destroy() != 0) {
        return -1;
    }
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

/**
 * Looks for a file.
 *
 * Note: as a simplification, only a plain directory space (root directory only)
 * is supported.
 *
 * Input:
 *   - name: absolute path name
 *   - root_inode: the root directory inode
 * Returns the inumber of the file, -1 if unsuccessful.
 */
static int tfs_lookup(char const *name, inode_t const *root_inode) {
    // TODO: assert that root_inode is the root directory



    if (!valid_pathname(name)) {

        return -1;
    }


    // skip the initial '/' character
    name++;


    return find_in_dir(root_inode, name);
}

int tfs_open(char const *name, tfs_file_mode_t mode) {
    // Checks if the path name is valid
    if (!valid_pathname(name)) {
        return -1;
    }

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    pthread_rwlock_wrlock(&root_dir_inode->trinco);


    if(root_dir_inode == NULL) {
        pthread_rwlock_unlock(&root_dir_inode->trinco);
        return -1;
    }
    int inum = tfs_lookup(name, root_dir_inode);
    size_t offset;

    if (inode_get(inum) -> i_node_type == T_LINK) {
        pthread_rwlock_unlock(&root_dir_inode->trinco);
        return tfs_open(inode_get(inum) -> path,mode);
    }

    else if (inum >= 0) {
        // The file already exists
        inode_t *inode = inode_get(inum);
        if(inode == NULL) {
            pthread_rwlock_unlock(&root_dir_inode->trinco);
            return -1;
        }

        // Truncate (if requested)
        if (mode & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                data_block_free(inode->i_data_block);
                inode->i_size = 0;
            }
        }
        // Determine initial offset
        if (mode & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
    } else if (mode & TFS_O_CREAT) {
        // The file does not exist; the mode specified that it should be created
        // Create inode
        inum = inode_create(T_FILE);
        if (inum == -1) {
            pthread_rwlock_unlock(&root_dir_inode->trinco);
            return -1; // no space in inode table
        }

        // Add entry in the root directory
        if (add_dir_entry(root_dir_inode, name + 1, inum) == -1) {
            inode_delete(inum);
            pthread_rwlock_unlock(&root_dir_inode->trinco);
            return -1; // no space in directory
        }

        offset = 0;
    } else {
        pthread_rwlock_unlock(&root_dir_inode->trinco);
        return -1;
    }
    pthread_rwlock_unlock(&root_dir_inode->trinco);

    // Finally, add entry to the open file table and return the corresponding
    // handle
    return add_to_open_file_table(inum, offset);

    // Note: for simplification, if file was created with TFS_O_CREAT and there
    // is an error adding an entry to the open file table, the file is not
    // opened but it remains created
}

int tfs_sym_link(char const *target, char const *link_name) {
    int inumber = inode_create(T_LINK);
    inode_t* inode_link = inode_get(inumber);
    inode_t* inode = inode_get(ROOT_DIR_INUM);

    pthread_rwlock_wrlock(&inode_link->trinco);
    pthread_rwlock_wrlock(&inode->trinco);

    if(inumber == -1) {
        pthread_rwlock_unlock(&inode->trinco);
        pthread_rwlock_unlock(&inode_link->trinco);

        return -1;
    }
    

    if(add_dir_entry(inode,link_name + 1,inumber) == -1) {

        pthread_rwlock_unlock(&inode->trinco);
        pthread_rwlock_unlock(&inode_link->trinco);

        return -1;
    }

    strcpy(inode_link -> path, target);
    
    pthread_rwlock_unlock(&inode->trinco);
    pthread_rwlock_unlock(&inode_link->trinco);

    return 0;
}

int tfs_link(char const *target, char const *link_name) {
    inode_t* inode = inode_get(ROOT_DIR_INUM);
    int inum_target = tfs_lookup(target,inode);
    inode_t* inode_target = inode_get(inum_target);

    pthread_rwlock_wrlock(&inode->trinco);
    pthread_rwlock_wrlock(&inode_target->trinco);
    
    if(inum_target < 0) {
        pthread_rwlock_unlock(&inode_target->trinco);
        pthread_rwlock_unlock(&inode->trinco);
        return -1;
    }

    if(inode_target -> i_node_type == T_LINK) {
        pthread_rwlock_unlock(&inode_target->trinco);
        pthread_rwlock_unlock(&inode->trinco);
        return -1;
    }


    if(add_dir_entry(inode,link_name+1,inum_target) == -1) {
        pthread_rwlock_unlock(&inode_target->trinco);
        pthread_rwlock_unlock(&inode->trinco);
        return -1;
    }

    inode_target->i_count_hard++;
    pthread_rwlock_unlock(&inode_target->trinco);
    pthread_rwlock_unlock(&inode->trinco);

    return 0;
}

int tfs_close(int fhandle) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1; // invalid fd
    }

    remove_from_open_file_table(fhandle);

    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    //  From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber);

    pthread_rwlock_wrlock(&inode->trinco);

    if (inode == NULL) {
        pthread_rwlock_unlock(&inode->trinco);
        return -1;
    }

    // Determine how many bytes to write
    size_t block_size = state_block_size();
    if (to_write + file->of_offset > block_size) {
        to_write = block_size - file->of_offset;
    }

    if (to_write > 0) {
        if (inode->i_size == 0) {
            // If empty file, allocate new block
            int bnum = data_block_alloc();
            if (bnum == -1) {
                pthread_rwlock_unlock(&inode->trinco);
                return -1; // no space
            }

            inode->i_data_block = bnum;
        }

        void *block = data_block_get(inode->i_data_block);
        if(block == NULL) {
            pthread_rwlock_unlock(&inode->trinco);
            return -1;
        }

        // Perform the actual write
        memcpy(block + file->of_offset, buffer, to_write);

        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }
    pthread_rwlock_unlock(&inode->trinco);
    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    pthread_mutex_lock(&file -> mutex3);
    if (file == NULL) {
        pthread_mutex_unlock(&file -> mutex3);
        return -1;
    }

    // From the open file table entry, we get the inode
    inode_t const *inode = inode_get(file->of_inumber);
    if(inode == NULL) {
        pthread_mutex_unlock(&file -> mutex3);
        return -1;
    }

    // Determine how many bytes to read
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    if (to_read > 0) {
        void *block = data_block_get(inode->i_data_block);
        if(block == NULL) {
            pthread_mutex_unlock(&file -> mutex3);
            return -1;
        }

        // Perform the actual read
        memcpy(buffer, block + file->of_offset, to_read);
        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_read;
    }
    pthread_mutex_unlock(&file -> mutex3);

    return (ssize_t)to_read;
}

int tfs_unlink(char const *target) {
    inode_t* inode_dir = inode_get(ROOT_DIR_INUM);
    int inumber = tfs_lookup(target,inode_dir);
    inode_t* inode = inode_get(inumber);
    pthread_rwlock_wrlock(&inode_dir->trinco);
    pthread_rwlock_wrlock(&inode->trinco);

    if (inode->i_node_type == T_LINK){
        pthread_rwlock_unlock(&inode->trinco);
        pthread_rwlock_unlock(&inode_dir->trinco);
        clear_dir_entry(inode_dir, target+1);
        inode_delete(inumber);

        return 0;
    }

    inode->i_count_hard--;

    if (inode->i_count_hard == 0){
        pthread_rwlock_unlock(&inode->trinco);
        pthread_rwlock_unlock(&inode_dir->trinco);
        clear_dir_entry(inode_dir, target+1);
        inode_delete(inumber);
        return 0;
    }
    else{
        clear_dir_entry(inode_dir, target+1);
    }
    pthread_rwlock_unlock(&inode->trinco);
    pthread_rwlock_unlock(&inode_dir->trinco);
    return 0;
}

int tfs_copy_from_external_fs(char const *source_path, char const *dest_path) {
    FILE * source_file;
    source_file = fopen(source_path, "r");
    int dest_file = tfs_open(dest_path, TFS_O_CREAT|TFS_O_APPEND|TFS_O_TRUNC);
    unsigned long bytes_read = 0;

    if (source_file == NULL || dest_file < 0){
        fprintf(stderr, "open error: %s\n", strerror(errno));
        tfs_close(dest_file);
        return -1;
    }


    else if(dest_file < 0){
        fprintf(stderr, "open error: %s\n", strerror(errno));
        fclose(source_file);
        return -1;
    }


    char buffer[128];

    memset(buffer,0,sizeof(buffer));

   /* read the contents of the file */

    while( 0 < (bytes_read = fread(buffer, 1, sizeof(buffer),source_file))){

        if(tfs_write(dest_file,buffer,bytes_read) != bytes_read) {
            fclose(source_file);
            tfs_close(dest_file);
            return -1;
        }

        memset(buffer,0,sizeof(buffer));
    }
    if(fclose(source_file) == EOF || tfs_close(dest_file) == -1) {
        return -1;
    }

    return 0;
}

