#include <stdio.h>
#include "ext2_fs.h"
#include "read_ext2.h"
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

int main(int argc, char **argv) {

    if (argc != 3) {
        printf("expected usage: ./runscan inputfile outputfile\n");
        exit(0);
    }

    // Creates output directory if non-existent, otherwise error
    if (opendir(argv[2])) {
        //ERROR - already exists
        printf("ERROR - Output directory already exists\n");
        exit(1);
    }
    mkdir(argv[2], 0700); // Creates the new output directory

    // Opens disk images
    int input_file;
    input_file = open(argv[1], O_RDONLY);

    ext2_read_init(input_file); // Prints info about the blocks and nodes in input_file


    struct ext2_super_block super;
    struct ext2_group_desc group;

    read_super_block(input_file, &super); //prints uper block info
    read_group_descs(input_file, &group, num_groups); //prints group desc info

    int inode_is_jpg[inodes_per_group];

    for (uint q = 0; q < inodes_per_group; q++) {
        inode_is_jpg[q] = 0;
    }

    // JPG PHOTO HANDLING CODE

    //printf("There are %u inodes in an inode table block and %u blocks in the idnode table\n", inodes_per_block, itable_blocks);
    //iterate the first inode block
    off_t start_inode_table = locate_inode_table(0, &group);
    for (unsigned int i = 0; i < inodes_per_group; i++) { //inodes_per_group

        //printf("inode %u: \n", i);
        struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
        read_inode(input_file, 0, start_inode_table, inode, i);
        /* the maximum index of the i_block array should be computed from i_blocks / ((1024<<s_log_block_size)/512)
             * or once simplified, i_blocks/(2<<s_log_block_size)
             * https://www.nongnu.org/ext2-doc/ext2.html#i-blocks
             */
        // unsigned int i_blocks = inode->i_blocks/(2<<super.s_log_block_size);
        //printf("number of blocks %u\n", i_blocks);
        /*printf("Is directory? %s \n Is Regular file? %s\n",
           S_ISDIR(inode->i_mode) ? "true" : "false",
           S_ISREG(inode->i_mode) ? "true" : "false");*/


        // Read the first block of every node - check is_jpg
        char buffer[block_size];
        lseek(input_file,BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
        read(input_file,buffer,block_size);

        int is_jpg = 0;
        if (buffer[0] == (char)0xff &&
            buffer[1] == (char)0xd8 &&
            buffer[2] == (char)0xff &&
            (buffer[3] == (char)0xe0 ||
             buffer[3] == (char)0xe1 ||
             buffer[3] == (char)0xe8)) {
            is_jpg = 1;
        }

        if (is_jpg == 1) {

            // sizing for multi-digit inode nums
            int num_digits = 1;
            int count = i;
            while (count /= 10) {
                num_digits++;
            }

            // Set up new file
            int filename_chars = (strlen(argv[2]) + strlen("/") + strlen("file-") + num_digits + strlen(".jpg") + 1);
            size_t name_len = filename_chars * sizeof(char);
            char* filename = malloc(name_len);
            snprintf(filename, name_len, "%s/file-%u.jpg", argv[2], i);
            int file_ptr = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0666);

            // And a copy to rename later
            int new_filename_chars = (strlen(argv[2]) + strlen("/") + strlen("file-") + num_digits + strlen("ex.jpg") + 1);
            size_t new_name_len = new_filename_chars * sizeof(char);
            char* new_filename = malloc(new_name_len);
            snprintf(new_filename, new_name_len, "%s/file-%uex.jpg", argv[2], i);
            int new_file_ptr = open(new_filename, O_WRONLY | O_TRUNC | O_CREAT, 0666);

            uint filesize = inode->i_size; // Get filesize from the inode
            //char file_arr[filesize];

            uint block_id;
            uint bytes_read;
            for (bytes_read = 0, block_id = 0; bytes_read < filesize && bytes_read < block_size * EXT2_NDIR_BLOCKS; bytes_read = bytes_read + block_size) {

                uint j;
                for (j = 0; j < block_size && bytes_read + j < filesize; j++) {
                    // file_arr[bytes_read + j] = buffer[j];
                }

                write(file_ptr, buffer, j);
                write(new_file_ptr, buffer, j);

                if (bytes_read + j < filesize) {
                    block_id++;
                    lseek(input_file,BLOCK_OFFSET(inode->i_block[block_id]), SEEK_SET);
                    read(input_file,buffer,block_size);
                }
            }


            if (bytes_read < filesize) { // Single Indirect Points

                uint ind_buffer[block_size / sizeof(uint)];

                lseek(input_file,BLOCK_OFFSET(inode->i_block[EXT2_IND_BLOCK]), SEEK_SET);
                read(input_file,ind_buffer,block_size);

                lseek(input_file,BLOCK_OFFSET(ind_buffer[0]), SEEK_SET);
                read(input_file,buffer,block_size);

                for (block_id = 0; bytes_read < filesize && block_id < (block_size / sizeof(uint)); bytes_read = bytes_read + block_size) {

                    uint j;
                    for (j = 0; j < block_size && bytes_read + j < filesize; j++) {
                        // file_arr[bytes_read + j] = buffer[j];
                    }

                    write(file_ptr, buffer, j);
                    write(new_file_ptr, buffer, j);

                    if (bytes_read + j < filesize) {
                        block_id++;
                        lseek(input_file,BLOCK_OFFSET(ind_buffer[block_id]), SEEK_SET);
                        read(input_file,buffer,block_size);
                    }
                }
                //put single block handling code here
                //inode->i_block[EXT2_IND_BLOCK];
            } //*/

            if (bytes_read < filesize && 1 == 1) { // Double Indirect Points

                uint first_buffer[block_size / sizeof(uint)];
                uint second_buffer[block_size / sizeof(uint)];

                lseek(input_file,BLOCK_OFFSET(inode->i_block[EXT2_DIND_BLOCK]), SEEK_SET);
                read(input_file,first_buffer,block_size);

                uint first_layer_id;
                for (first_layer_id = 0; first_layer_id < (block_size / sizeof(int)) && bytes_read < filesize; first_layer_id++) {
                    lseek(input_file,BLOCK_OFFSET(first_buffer[first_layer_id]), SEEK_SET);
                    read(input_file,second_buffer,block_size);

                    lseek(input_file,BLOCK_OFFSET(second_buffer[0]), SEEK_SET);
                    read(input_file,buffer,block_size);
                    for (block_id = 0; bytes_read < filesize && block_id < (block_size / sizeof(int)); bytes_read = bytes_read + block_size) {

                        uint j;
                        for (j = 0; j < block_size && bytes_read + j < filesize; j++) {
                            //file_arr[bytes_read + j] = buffer[j];
                        }

                        write(file_ptr, buffer, j);
                        write(new_file_ptr, buffer, j);

                        if (bytes_read + j < filesize) {
                            block_id++;
                            lseek(input_file,BLOCK_OFFSET(second_buffer[block_id]), SEEK_SET);
                            read(input_file,buffer,block_size);
                        }
                    }
                }
            }

            // write(file_ptr, file_arr, filesize); // Writes file to directory
            // write(new_file_ptr, file_arr, filesize); // Writes file to directory

            inode_is_jpg[i] = 42;
        }

        // print i_block numbers
        // for(unsigned int z=0; z<EXT2_N_BLOCKS; z++)
        // {       if (z < EXT2_NDIR_BLOCKS)                  				  /* direct blocks */
        // 			printf("Block %2u : %u\n", z, inode->i_block[z]);
        // 		else if (z == EXT2_IND_BLOCK)                             /* single indirect block */
        // 				printf("Single   : %u\n", inode->i_block[z]);
        // 		else if (z == EXT2_DIND_BLOCK)                            /* double indirect block */
        // 				printf("Double   : %u\n", inode->i_block[z]);
        // 		else if (z == EXT2_TIND_BLOCK)                            /* triple indirect block */
        // 				printf("Triple   : %u\n", inode->i_block[z]);
        // }
        free(inode);
    }

    // DIRECTORY TRAVESAL LOOP

    start_inode_table = locate_inode_table(0, &group);
    for (unsigned int i = 0; i < inodes_per_group; i++) { //inodes_per_group

        struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
        read_inode(input_file, 0, start_inode_table, inode, i);
        //unsigned int i_blocks = inode->i_blocks/(2<<super.s_log_block_size);


        if (S_ISDIR(inode->i_mode)) { // inode is a directory

            char dir_buffer[block_size];

            lseek(input_file,BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
            read(input_file,dir_buffer,block_size);


            uint curr_offset = 24;

            struct ext2_dir_entry_2* dentry = (struct ext2_dir_entry_2*) & (dir_buffer[curr_offset]);

            // what is inside a directory?
            // printf("name_len: %u\n", dentry->name_len); //0
            // printf("directory inode number: %u\n", dentry->inode);

            for (; curr_offset < block_size; dentry = (struct ext2_dir_entry_2*) & (dir_buffer[curr_offset])) {

                int name_len = dentry->name_len & 0xFF; // convert 2 bytes to 4 bytes properly

                if (name_len <= 0) { break; }

                char name [EXT2_NAME_LEN];
                strncpy(name, dentry->name, name_len);
                name[name_len] = '\0';

                //printf("Entry inode is -%d-, Entry name is --%s--\n", dentry->inode, name);

                struct ext2_inode *curr_inode = malloc(sizeof(struct ext2_inode));
                read_inode(input_file, 0, start_inode_table, curr_inode, i);

                if (inode_is_jpg[dentry->inode] == 42) {
                    // Rename the ex.jpg file
                    int num_digits = 1;
                    int count = dentry->inode;
                    while (count /= 10) {
                        num_digits++;
                    }

                    //printf("THE CODE IS ACTUALLY RUNNING\n");

                    int directory_chars = (strlen(argv[2]) + strlen("/"));

                    int oldname_chars = (directory_chars + strlen("file-") + num_digits + strlen("ex.jpg") + 1);
                    size_t oldname_len = oldname_chars * sizeof(char);
                    char* oldname = malloc(oldname_len);
                    snprintf(oldname, oldname_len, "%s/file-%uex.jpg", argv[2], dentry->inode);

                    int newname_chars = directory_chars + dentry->name_len + 1;
                    size_t newname_len = newname_chars * sizeof(char);
                    char*newname = malloc(newname_len);
                    snprintf(newname, newname_len, "%s/%s", argv[2], dentry->name);

                    //printf("%s->%s\n", oldname, newname);

                    rename(oldname, newname);
                }

                /*// do we need to look through directories
                if  (!(S_ISDIR(curr_inode->i_mode))) {
                    //char tmp_buffer[block_size];
                    //lseek(input_file,BLOCK_OFFSET(curr_inode->i_block[0]), SEEK_SET);
                    //read(input_file,tmp_buffer,block_size);
                    int curr_jpg = 0;
                    if (tmp_buffer[0] == (char)0xff &&
                        tmp_buffer[1] == (char)0xd8 &&
                        tmp_buffer[2] == (char)0xff &&
                        (tmp_buffer[3] == (char)0xe0 ||
                        tmp_buffer[3] == (char)0xe1 ||
                        tmp_buffer[3] == (char)0xe8)) {
                        curr_jpg = 1;
                    }
                }*/

                curr_offset = curr_offset + name_len + sizeof(dentry->inode) + sizeof(dentry->rec_len) + sizeof(dentry->name_len) + sizeof(dentry->file_type);

                // byte aligment
                if (curr_offset % 4 != 0) {
                    curr_offset = curr_offset + (4 - (curr_offset % 4));
                }

            }
        }
    }
}
