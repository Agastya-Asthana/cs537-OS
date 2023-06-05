#include <stdio.h>
#include "ext2_fs.h"
#include "read_ext2.h"
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    char buffer[1024];                  // buffer that reads in contents of the actual data blocks
    int is_jpg;                         // flag for if a file is jpg
    int file_size;                      // size of the file in the inode
    FILE *inode_file;                   // file pointer for the file created in the output directory
    char inode_filename[100];           // inode's file name variable
    char strnum[100];                   // inode number integer to string type variable
    int inode_list[1024];               // list of the inode numbers of all the jpg files
    int inode_list_count = 0;               // size of the list of jpg inode numbers
    char filename[1024];             // name of the actual jpg file
    int ind_buff[256];                  // indirect pointer buffer for the 256 pointers
    int dbl_buff1[256];                 // double indirect pointer buffer
    int data_block_num;                 // keep track of what data block you are in
    FILE *final_file;                   // file pointer for the final file


    // check if the arguments are valid
    if (argc != 3) {
        printf("expected usage: ./runscan inputfile outputfile\n");
        exit(0);
    }

    // check if the output directory already exists. if so, exit the program. if not create the directory
    DIR *dp = opendir(argv[2]);
    if(dp != NULL) {
        printf("Directory already exists!\n");
        exit(0);
    } else {
        mkdir(argv[2], S_IRWXU | S_IRWXG | S_IRWXO);
    }

    int fd;
    fd = open(argv[1], O_RDONLY);    /* open disk image */

    ext2_read_init(fd);

    struct ext2_super_block super;
    struct ext2_group_desc group;

    printf("There are %u inodes in an inode table block and %u blocks in the idnode table\n", inodes_per_block, itable_blocks);

    //calculate the start of the inode table
    off_t start_inode_table;
    //iterate through each group in the disk image to go through all inodes
    for(uint g = 0; g < num_groups; g++){
        // Debugging: Block group #
        printf("block group %u in total block groups: %u\n", g, num_groups);
        read_super_block(fd, &super);
        read_group_descs(fd, &group, num_groups);
        start_inode_table = locate_inode_table(g, &group);
        for (unsigned int i = 1; i <= inodes_per_block * itable_blocks; i++) {
            // Debugging: Inode #
            // printf("inode %u: \n", i);
            is_jpg = 0;
            file_size = 0;
            data_block_num = 0;
            struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
            read_inode(fd, start_inode_table, i, inode, super.s_inode_size);
            // unsigned int i_blocks = inode->i_blocks/(2<<super.s_log_block_size);
            // Debugging: # of data blocks in the inode
            // printf("number of blocks %u\n", i_blocks);
            //  printf("Is directory? %s \n Is Regular file? %s\n",
            //     S_ISDIR(inode->i_mode) ? "true" : "false",
            //     S_ISREG(inode->i_mode) ? "true" : "false");

            //this shit invalid if index 0 is 0
            printf("Made it this far1\n");
            if(inode->i_block[0] == 0){
                continue;
            }
            printf("Made it this far2\n");
            // if the inode is a regular file, check the firsts data block in the inode to check for jpg file
            if(S_ISREG(inode->i_mode)){
                memset(buffer, 0, 1024);
                lseek(fd, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
                read(fd, buffer, sizeof(char[1024]));
                if (buffer[0] == (char)0xff &&
                    buffer[1] == (char)0xd8 &&
                    buffer[2] == (char)0xff &&
                    (buffer[3] == (char)0xe0 ||
                     buffer[3] == (char)0xe1 ||
                     buffer[3] == (char)0xe8)) {
                    is_jpg = 1;
                }
            }else{
                continue;
            }

            if(is_jpg){
                memset(buffer, 0, 1024);
                // create filename
                memset(inode_filename, 0, 100);
                strcat(inode_filename, argv[2]);
                strcat(inode_filename, "/file-");
                sprintf(strnum, "%d", i);
                strcat(inode_filename, strnum);
                strcat(inode_filename, ".jpg");

                inode_list[inode_list_count] = i;
                inode_list_count++;
                printf("Inode num: %d\n", inode_list[inode_list_count-1]);

                // save the size of the file in a variable and decrement as you read through a block
                file_size = inode -> i_size;

                // create and open a file in the output directory for this node
                inode_file = fopen(inode_filename, "w");

                memset(buffer, 0, 1024);

                while(file_size > 0){
                    if(data_block_num < 12){
                        lseek(fd, BLOCK_OFFSET(blocks_per_group * g + inode->i_block[data_block_num]), SEEK_SET);
                        if(file_size > 1024){
                            read(fd, buffer, sizeof(char[1024]));
                            fwrite(buffer, 1, sizeof(buffer), inode_file);
                            file_size -= 1024;
                        }else{
                            read(fd, buffer, sizeof(char[file_size]));
                            fwrite(buffer, 1, sizeof(buffer), inode_file);
                            file_size -= file_size;
                        }
                    } else if (data_block_num == 12){
                        lseek(fd, BLOCK_OFFSET(blocks_per_group * g + inode->i_block[data_block_num]), SEEK_SET);
                        read(fd, ind_buff, sizeof(int[256]));
                        for(int j = 0; j < 256; j++){
                            if(file_size > 0 && file_size >= 1024){
                                lseek(fd, BLOCK_OFFSET(blocks_per_group * g + ind_buff[j]), SEEK_SET);
                                read(fd, buffer, sizeof(char[1024]));
                                fwrite(buffer, 1, sizeof(buffer), inode_file);
                                file_size -= 1024;
                            }else if(file_size > 0 && file_size < 1024){
                                lseek(fd, BLOCK_OFFSET(blocks_per_group * g + ind_buff[j]), SEEK_SET);
                                read(fd, buffer, sizeof(char[file_size]));
                                fwrite(buffer, 1, sizeof(buffer), inode_file);
                                file_size -= file_size;
                            }else{
                                break;
                            }
                        }
                    } else if (data_block_num == 13){
                        lseek(fd, BLOCK_OFFSET(blocks_per_group * g + inode->i_block[data_block_num]), SEEK_SET);
                        read(fd, dbl_buff1, sizeof(int[256]));
                        for(int j = 0; j < 256; j++){
                            lseek(fd, BLOCK_OFFSET(blocks_per_group * g + dbl_buff1[j]), SEEK_SET);
                            read(fd, ind_buff, sizeof(int[256]));
                            for(int k = 0; k < 256; k++){
                                if(file_size > 0 && file_size >= 1024){
                                    lseek(fd, BLOCK_OFFSET(blocks_per_group * g + ind_buff[k]), SEEK_SET);
                                    read(fd, buffer, sizeof(char[1024]));
                                    fwrite(buffer, 1, sizeof(buffer), inode_file);
                                    file_size -= 1024;
                                }else if(file_size > 0 && file_size < 1024){
                                    lseek(fd, BLOCK_OFFSET(blocks_per_group * g + ind_buff[k]), SEEK_SET);
                                    read(fd, buffer, sizeof(char[file_size]));
                                    fwrite(buffer, 1, sizeof(buffer), inode_file);
                                    file_size -= file_size;
                                }else{
                                    break;
                                }
                            }
                        }
                    }
                    data_block_num++;
                }
                fclose(inode_file);
            }
            free(inode);
        }
    }

    // int temp[] = {12, 13, 14};

    // PART 2: Look for the directory and file names:
    for(uint g = 0; g < num_groups; g++){
        read_super_block(fd, &super);
        read_group_descs(fd, &group, num_groups);
        start_inode_table = locate_inode_table(g, &group);

        //iterate each inode in the inode table
        for (unsigned int i = 1; i <= inodes_per_block * itable_blocks; i++) {
            is_jpg = 0;
            struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
            read_inode(fd, start_inode_table, i, inode, super.s_inode_size);

            if(inode->i_block[0] == 0) {
                continue;
            }

            if(S_ISDIR(inode->i_mode)) {
                memset(buffer, 0, 1024);
                lseek(fd, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
                read(fd, buffer, sizeof(char[1024]));
                int offset = 0;
                while(buffer[offset] != 0) {
                    struct ext2_dir_entry_2 *dentry;
                    dentry = (struct ext2_dir_entry_2 *) & (buffer[offset]);
                    int name_len = dentry->name_len & 0xFF;
                    char name [EXT2_NAME_LEN];
                    strncpy(name, dentry->name, name_len);
                    name[name_len] = '\0';
                    for(int j = 0; j < inode_list_count; j++) {
                        if((int) dentry->inode == inode_list[j]) {

                            // Debugging: prints out names
                            printf("Inode number for file is %d\n", dentry->inode);
                            printf("Entry name is --%s--\n", name);

                            // temp file name
                            memset(inode_filename, 0, 100);
                            memset(strnum, 0, 100);
                            strcat(inode_filename, argv[2]);
                            strcat(inode_filename, "/file-");
                            sprintf(strnum, "%d", inode_list[j]);
                            strcat(inode_filename, strnum);
                            strcat(inode_filename, ".jpg");
                            inode_file = fopen(inode_filename, "r");
                            printf("%s\n", inode_filename);

                            // final file name
                            memset(filename, 0, 1024);
                            strcat(filename, argv[2]);
                            strcat(filename, "/");
                            strcat(filename, name);
                            final_file = fopen(filename, "w");
                            printf("%s\n", filename);

                            int c;
                            c = fgetc(inode_file);
                            while (c != EOF)
                            {
                                fputc(c, final_file);
                                c = fgetc(inode_file);
                            }
                            fclose(inode_file);
                            fclose(final_file);

                        }
                    }
                    int padding = 0;
                    if(dentry->name_len % 4 != 0) {
                        padding = 4 - (dentry->name_len % 4);
                    }
                    offset += 8 + dentry->name_len + padding;
                }
            }
        }
    }
    close(fd);
    closedir(dp);
}