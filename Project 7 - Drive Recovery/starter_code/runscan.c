#include "ext2_fs.h"
#include "read_ext2.h"
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_SIZE = 1024

int main(int argc, char **argv) 
{
    if (argc != 3) 
    {
        printf("expected usage: ./runscan inputfile outputfile\n");
        exit(0);
    }

    /* This is some boilerplate code to help you get started, feel free to modify
       as needed! */

    int fd;
    fd = open(argv[1], O_RDONLY);    /* open disk image */

    ext2_read_init(fd);

    struct ext2_super_block super;
    struct ext2_group_desc *group_descs = malloc(sizeof(struct ext2_group_desc) * num_groups);

    // example read first the super-block and group descriptors
    read_super_block(fd, &super);
    read_group_descs(fd, group_descs, num_groups);

    //find offset of inode table in group_descs containing specified inode
    int root_inode_num = EXT2_ROOT_INO;
    int block_group = (root_inode_num - 1) / inodes_per_group;
    off_t inode_table = locate_inode_table(block_group, group_descs);

    //read root inode from disk
    struct ext2_inode root_inode;
    read_inode(fd, inode_table, block_group, &root_inode, super.s_inode_size);

    // verify that the inode corresponds to a directory
    if ((root_inode.i_mode & S_IFMT) != S_IFDIR) {
        printf("Error: specified inode is not a directory\n");
        exit(0);
    }

    // read the first directory block from disk
    int block_num = root_inode.i_block[0];

    // calculate the block offset in bytes
    int block_offset = (block_num - 1) * block_size + BASE_OFFSET;

    char *block_data = malloc(block_size);
    pread(fd, block_data, block_size, block_offset);

    // iterate through the directory entries and print their inode numbers and names
    struct ext2_dir_entry_2 *dir_entry;
    unsigned int dir_entry_offset = 0;
    while (dir_entry_offset < block_size) {
        dir_entry = (struct ext2_dir_entry_2 *)(block_data + dir_entry_offset);
        printf("Inode number: %u\n", dir_entry->inode);
        printf("Name: %.*s\n", dir_entry->name_len, dir_entry->name);
        dir_entry_offset += dir_entry->rec_len;
    }

    //clean up
    free(block_data);
    free(group_descs);
    close(fd);

    return 0;
}