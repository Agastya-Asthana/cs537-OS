#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

struct KeyValue {
    int key;
    char value[96];
};

struct KeyValue* inputRecords;
struct KeyValue* sortedKeys;
int numRecords;
int numThreads;
pthread_mutex_t mutex;

int partition(struct KeyValue* arr, int low, int high) {
    struct KeyValue pivot = arr[low];
    int i = low;
    int j = high;

    while (1) {
        while (arr[i].key < pivot.key) {
            i++;
        }
        while (arr[j].key > pivot.key) {
            j--;
        }
        if (i >= j) {
            return j;
        }
        struct KeyValue temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
        i++;
        j--;
    }
}

void quicksort(struct KeyValue* arr, int low, int high) {
    if (low < high) {
        int pivotIndex = partition(arr, low, high);
        quicksort(arr, low, pivotIndex);
        quicksort(arr, pivotIndex + 1, high);
    }
}

void* parallelKeySort(void* arg) {
    int threadID = *((int*)arg);
    int start = threadID * (numRecords / numThreads);
    int end = (threadID == numThreads - 1) ? numRecords : (threadID + 1) * (numRecords / numThreads);

    // Create a local copy of the inputRecords array
    struct KeyValue* localInputRecords = (struct KeyValue*)malloc((end - start) * sizeof(struct KeyValue));
    if (localInputRecords == NULL) {
        perror("Error allocating memory for local input records");
        exit(1);
    }
    memcpy(localInputRecords, &inputRecords[start], (end - start) * sizeof(struct KeyValue));

    // Sort the local copy
    quicksort(localInputRecords, 0, end - start - 1);

    // Allocate memory for sortedKeys if not already allocated
    if (sortedKeys == NULL) {
        sortedKeys = (struct KeyValue*)malloc(numRecords * sizeof(struct KeyValue));
        if (sortedKeys == NULL) {
            perror("Error allocating memory for sorted keys");
            exit(1);
        }
    }

    // Copy the sorted keys to the sortedKeys array
    pthread_mutex_lock(&mutex);
    memcpy(&sortedKeys[start], localInputRecords, (end - start) * sizeof(struct KeyValue));
    pthread_mutex_unlock(&mutex);

    // Free the local copy
    free(localInputRecords);

    return NULL;
}

int main(int argc, char* argv[]) {
    // Get command line inputs
    char *inputFile = argv[1];
    char *outputFile = argv[2];
    numThreads = atoi(argv[3]);

    // Open input file
    int fd = open(inputFile, O_RDONLY);
    if (fd == -1) {
        perror("Error opening input file");
        exit(1);
    }

    // Get input file size
    struct stat st;
    fstat(fd, &st);
    int fileSize = st.st_size;

    // Map input file to memory
    inputRecords = (struct KeyValue *) mmap(NULL, fileSize, PROT_READ, MAP_SHARED, fd, 0);
    if (inputRecords == MAP_FAILED) {
        perror("Error mapping input file to memory");
        exit(1);
    }

    // Calculate number of records int he input file
    numRecords = fileSize / sizeof(struct KeyValue);
    // Initialize mutex
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("Error initializing mutex");
        exit(1);
    }

// Create threads for parallel sorting
    pthread_t threads[numThreads];
    int threadIDs[numThreads];
    for (int i = 0; i < numThreads; i++) {
        threadIDs[i] = i;
        if (pthread_create(&threads[i], NULL, parallelKeySort, &threadIDs[i]) != 0) {
            perror("Error creating thread");
            exit(1);
        }
    }

// Wait for all threads to finish
    for (int i = 0; i < numThreads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("Error joining thread");
            exit(1);
        }
    }

// Unmap input file from memory
    if (munmap(inputRecords, fileSize) == -1) {
        perror("Error unmapping input file from memory");
        exit(1);
    }

// Sort the sortedKeys array
    quicksort(sortedKeys, 0, numRecords - 1);

// Open output file
    FILE *out = fopen(outputFile, "wb");
    if (out == NULL) {
        perror("Error opening output file");
        exit(1);
    }

    // Write sorted keys to output file
    for (int i = 0; i < numRecords; i++) {
        fwrite(&sortedKeys[i], sizeof(struct KeyValue), 1, out);
    }

    // Close output file
    fclose(out);

    // Clean up
    free(sortedKeys);
    pthread_mutex_destroy(&mutex);

    return 0;
}
