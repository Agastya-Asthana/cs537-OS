//
// Created by Agastya Asthana on 1/26/23.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(){
    char *filename = "testBatch.txt";
    FILE *file = fopen(filename, "r");
    if (file == NULL){
        printf("file not found fuck!\n");
        return -1;
    }
    char *res;
    char buf[100];
    res = fgets(buf, 100, file);
    if (res == NULL){
        printf("EOF\n");
    }
    printf("%s\n", buf);
}
