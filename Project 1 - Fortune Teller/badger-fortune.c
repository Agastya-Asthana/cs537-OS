#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char** ParseFortune(char* filename);
int* ParseBatch(char* filename, int maxFort);

int main(int argc, char** argv) {
    if (argc < 5){
        printf("USAGE: \n\tbadger-fortune -f <file> -n <number> (optionally: -o <output file>) \n\t\t OR \n\tbadger-fortune -f <file> -b <batch file> (optionally: -o <output file>)\n");
        return -1;
    }
    char** fortune = NULL;
    int* batchData  = NULL;
    char* batchFile = NULL;
    char* outputFile = NULL;
    int nNumber = 0;
    short nFlag = 0;
    short bFlag = 0;
    for (int i = 1; i < argc; i+=2) {
        if (!(strcmp(argv[i],"-f") == 0 || strcmp(argv[i],"-n") == 0 || strcmp(argv[i],"-b") == 0 || strcmp(argv[i],"-o") == 0)){
            printf("ERROR: Invalid Flag Types\n");
            return -1;
        }
        if (strcmp(argv[i],"-f") == 0){   // for file flag
            if (strcmp(argv[i+1],"") == 0){
                printf("ERROR: No fortune file was provided\n");
                return -1;
            }
            char* filename = argv[i+1];
            fortune = ParseFortune(filename);
            if (fortune == NULL) return -1;
        }
        else if (strcmp(argv[i],"-b") == 0){ // for batchName file flag
            if (nFlag){
                printf("ERROR: You can't use batch mode when specifying a fortune number using -n\n");
                return -1;
            }
            bFlag++;
            batchFile = argv[i + 1];
            FILE *file = fopen(batchFile, "r");
            if (file == NULL){
                printf("ERROR: Can't open batch file\n");
                return -1;
            }
        }
        else if (strcmp(argv[i],"-n") == 0){
            if (bFlag){
                printf("ERROR: You can't specify a specific fortune number in conjunction with batch mode\n");
                return -1;
            }
            nFlag++;
            nNumber = atoi(argv[i+1]);
            if (nNumber < 0){
                printf("ERROR: Invalid Fortune Number\n");
                return -1;
            }
            if (fortune != NULL && fortune[0][0] < nNumber){
                printf("ERROR: Invalid Fortune Number\n");
                return -1;
            }
        }
        else if (strcmp(argv[i],"-o") == 0){
            outputFile = argv[i+1];
            if (strcmp(outputFile, "") == 0){
                printf("ERROR: No output file specified\n");
                return -1;
            }
        }

    }

    if (nFlag){ //an n number was provided
        if (fortune[0][0] < nNumber){
            printf("ERROR: Invalid Fortune Number\n");
            return -1;
        }
        if (outputFile != NULL){ //output to file
            FILE *file = fopen(outputFile, "w");
            fprintf(file, "%s", fortune[nNumber]);
            fclose(file);
        } else{ //output to STDOUT
            printf("%s", fortune[nNumber]);
        }
    } else { //using batch mode
        batchData = ParseBatch(batchFile, fortune[0][0]);
        if (outputFile != NULL){ //output to file
            FILE *file = fopen(outputFile, "w");
            for (int i = 1; i < batchData[0]; i++){
                if (batchData[i] < 0 || batchData[i] > fortune[0][0]){
                    fprintf(file,"ERROR: Invalid Fortune Number\n\n");
                    continue;
                }
                fprintf(file, "%s\n\n", fortune[batchData[i]]);
            }
            fclose(file);
        } else{ //output to STDOUT
            for (int i = 1; i < batchData[0]; i++){
                if (batchData[i] < 0 || batchData[i] > fortune[0][0]){
                    printf("ERROR: Invalid Fortune Number\n\n");
                    continue;
                }
                printf("%s\n\n", fortune[batchData[i]]);
            }
        }
    }
    free(fortune);
    free(batchData);

    return 0;
}


char** ParseFortune(char* filename){
    FILE *file;
    file = fopen(filename, "r");
    if(file == NULL){
        printf("ERROR: Can't open fortune file\n");
        return NULL;
    }
    char buf[100];

    char* res = fgets(buf, 100, file);
    if (res == NULL){
        printf("ERROR: Fortune File Empty\n");
        return NULL;
    }
    int numF = atoi(buf); //max number of fortune
    fgets(buf, 100, file);
    int maxLF = atoi(buf); //max length of fortune

    char** data = malloc(sizeof(char*) * (numF+1)); //allocating space by size of pointer * number of fortunes plus additional
    for(int i = 0; i < (numF+1); i++){
        data[i] = malloc(sizeof(char) * maxLF); //for each fortune allocating max number of chars it could be
    }
    data[0][0] = numF;//storing the number of fortunes here
    fgets(buf, 100, file); // first % symbol
    for (int i = 1; i < (numF+1); ){
        char* res = fgets(buf, 100, file);
        if (strcmp(buf, "%\n") != 0 && res != NULL)
            strcat(data[i], buf);
        else
            i++;
    }
    fclose(file);
    return data;
}

int* ParseBatch(char* filename, int maxFort){
    FILE *file = fopen(filename, "r");
    if (file == NULL){
        printf("ERROR: Can't open batch file\n");
        return NULL;
    }
    char buf[100];
    int* batches = malloc(sizeof(int) * (maxFort+1));

    char* res = fgets(buf, 100, file);
    if (res == NULL){
        printf("ERROR: Batch File Empty\n");
        return NULL;
    }
    batches[1] = atoi(buf);
    int i;
    for (i = 2; fgets(buf, 100, file) != NULL; ++i) {
        batches[i] = atoi(buf);
    }
    batches[0] = i;
    return batches;
}