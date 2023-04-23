#include "operations.h"
#include "config.h"
#include "state.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
    if (root_inode->i_node_type != T_DIRECTORY)
        return -1;

    if (!valid_pathname(name))
        return -1;

    // skip the initial '/'
    name++;
    int inumber = find_in_dir(root_inode, name);
    return inumber;
}

int tfs_open(char const *name, tfs_file_mode_t mode) {
    // Checks if the path name is valid
    if (!valid_pathname(name)) {
        return -1;
    }
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_open: root dir inode must exist");
    int inum = tfs_lookup(name, root_dir_inode);
    size_t offset;

    if (inum >= 0) {
        // The file already exists

        inode_t *inode = inode_get(inum);
        ALWAYS_ASSERT(inode != NULL,
                      "tfs_open: directory files must have an inode");
        while (inode->i_node_type == T_SYM_LINK) { // TODO change code
            char *contents = (char *)data_block_get(inode->i_data_block);
            inum = tfs_lookup(contents, root_dir_inode);
            if (inum == -1)
                return -1;
            inode = inode_get(inum);
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
            return -1; // no space in inode table
        }
        // Add entry in the root directory
        if (add_dir_entry(root_dir_inode, name + 1, inum) == -1) {
            inode_delete(inum);
            return -1; // no space in directory
        }
        offset = 0;

    } else {
        return -1;
    }

    // Finally, add entry to the open file table and return the corresponding
    // handle
    return add_to_open_file_table(inum, offset);

    // Note: for simplification, if file was created with TFS_O_CREAT and there
    // is an error adding an entry to the open file table, the file is not
    // opened but it remains created
}

int tfs_sym_link(char const *target, char const *link_name) {
    // cannot create a link if it doesn't exist
    if (tfs_lookup(link_name, inode_get(ROOT_DIR_INUM)) != -1)
        return -1;
    // creates inode
    int inode_num = inode_create(T_SYM_LINK);
    if (inode_num == -1)
        return -1;
    inode_t *inode = inode_get(inode_num);
    // fill data block contents with target contents (a path)
    char *contents = (char *)data_block_get(inode->i_data_block);
    strcpy(contents, target);
    if (add_dir_entry(inode_get(ROOT_DIR_INUM), link_name + 1, inode_num) ==
        -1) {
        inode_delete(inode_num);
        return -1;
    }
    return 0;
}

int tfs_link(char const *target, char const *link_name) {
    // doesn't allow a link to a non-existent file
    int file_inumber = tfs_lookup(target, inode_get(ROOT_DIR_INUM));
    if (file_inumber == -1)
        return -1;
    // doesn't allow a non-existing file to establish a link
    if (tfs_lookup(link_name, inode_get(ROOT_DIR_INUM)) != -1)
        return -1;
    // creates the link and adds it to the directory
    inode_t *inode = inode_get(file_inumber);
    if (inode->i_node_type != T_FILE)
        return -1;
    if (add_dir_entry(inode_get(ROOT_DIR_INUM), ++link_name, file_inumber) ==
        -1)
        return -1;
    inode->hard_links++;
    return 0;
}

int tfs_close(int fhandle) {
    lock_open_file_table();
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1; // invalid fd
    }

    remove_from_open_file_table(fhandle);
    unlock_open_file_table();
    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }
    //  From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_write: inode of open file deleted");

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
                return -1; // no space
            }

            inode->i_data_block = bnum;
        }
        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_write: data block deleted mid-write");
        lock_datablocks();
        // Perform the actual write
        memcpy(block + file->of_offset, buffer, to_write);
        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }
    unlock_datablocks();
    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }
    lock_datablocks();
    // From the open file table entry, we get the inode
    inode_t const *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_read: inode of open file deleted");

    // Determine how many bytes to read
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    if (to_read > 0) {
        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_read: data block deleted mid-read");

        // Perform the actual read
        memcpy(buffer, block + file->of_offset, to_read);
        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_read;
    }
    unlock_datablocks();
    return (ssize_t)to_read;
}

int tfs_unlink(char const *target) {
    // obtém o inumber
    int inumber = tfs_lookup(target, inode_get(ROOT_DIR_INUM));
    if (inumber == -1)
        return -1;
    inode_t *inode = inode_get(inumber);
    // verifica se o ficheiro a dar unlink está aberto
    if (search_in_open_file_table(inumber) != -1)
        return -1;
    // um caso para file e outro para soft links
    if (inode->i_node_type == T_FILE) {
        if (--inode->hard_links == 0)
            inode_delete(inumber);
    } else if (inode->i_node_type == T_SYM_LINK) {
        inode_delete(inumber);
    }
    // remove o ficheiro da diretoria
    if (clear_dir_entry(inode_get(ROOT_DIR_INUM), target + 1) == -1)
        return -1;
    return 0;
}

int tfs_copy_from_external_fs(char const *source_path, char const *dest_path) {
    // lê de um ficheiro externo
    FILE *cont = fopen(source_path, "r");
    if (cont == NULL)
        return -1;
    char buffer[state_block_size()];
    memset(buffer, 0, sizeof buffer);
    // copia o conteúdo para o buffer
    size_t nread = fread(buffer, sizeof *buffer, sizeof buffer, cont);
    if (nread == -1)
        return -1;
    fclose(cont);
    // abre/cria um ficheiro do tfs
    int fhandle = tfs_open(dest_path, TFS_O_TRUNC | TFS_O_CREAT);
    if (nread == 0)
        return 0;
    if (fhandle == -1)
        return -1;
    // escreve nesse ficheiro
    ssize_t err_write = tfs_write(fhandle, buffer, strlen(buffer));
    if (err_write == -1)
        return -1;
    return 0;
}
