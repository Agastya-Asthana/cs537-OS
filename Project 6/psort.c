#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>

#define RECORD_SIZE 100

typedef struct {
    int key;
    char value[96];
} Record;

typedef struct {
    Record* records;
    int start;
    int end;
} MergeArgs;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void merge_sort(Record* records, int start, int end) {
    if (start < end) {
        int mid = (start + end) / 2;
        merge_sort(records, start, mid);
        merge_sort(records, mid + 1, end);

        int i, j, k;
        int n1 = mid - start + 1;
        int n2 = end - mid;
        Record L[n1], R[n2];

        for (i = 0; i < n1; i++)
            L[i] = records[start + i];

        for (j = 0; j < n2; j++)
            R[j] = records[mid + 1 + j];

        i = 0;
        j = 0;
        k = start;

        while (i < n1 && j < n2) {
            if (L[i].key <= R[j].key) {
                records[k] = L[i];
                i++;
            }
            else {
                records[k] = R[j];
                j++;
            }
            k++;
        }

        while (i < n1) {
            records[k] = L[i];
            i++;
            k++;
        }

        while (j < n2) {
            records[k] = R[j];
            j++;
            k++;
        }
    }
}

void* merge_worker(void* args) {
    MergeArgs* mergeArgs = (MergeArgs*) args;
    int start = mergeArgs->start;
    int end = mergeArgs->end;
    Record* records = mergeArgs->records;

    merge_sort(records, start, end);

    pthread_mutex_lock(&lock);

    int i, j, k;
    int n1 = end - start + 1;
    int n2 = end + 1;
    Record L[n1], R[n2];

    for (i = 0; i < n1; i++)
        L[i] = records[start + i];

    for (j = 0; j < n2; j++)
        R[j] = records[j];

    i = 0;
    j = 0;
    k = start;

    while (i < n1 && j < n2) {
        if (L[i].key <= R[j].key) {
            records[k] = L[i];
            i++;
        }
        else {
            records[k] = R[j];
            j++;
        }
        k++;
    }

    while (i < n1) {
        records[k] = L[i];
        i++;
        k++;
    }

    while (j < n2) {
        records[k] = R[j];
        j++;
        k++;
    }

    pthread_mutex_unlock(&lock);

    return NULL;
}

int main(int argc, char* argv[]) {
    printf("In main atleast\n");
    if (argc != 4) {
        fprintf(stderr, "Usage: %s input_file output_file num_threads\n", argv[0]);
        return 1;
    }

    char* input_file = argv[1];
    printf("Input file: %s\n", input_file);
    char* output_file = argv[2];
    int num_threads = atoi(argv[3]);

    // Open input file
    int input_fd = open(input_file, O_RDONLY);
    if (input_fd == -1) {
        perror("open input file");
        return 1;
    }

    // Determine file size
    off_t input_size = lseek(input_fd, 0, SEEK_END);
    if (input_size == -1) {
        perror("lseek input file");
        return 1;
    }

    // Map input file to memory
    char* input_data = mmap(NULL, input_size, PROT_READ, MAP_PRIVATE, input_fd, 0);
    if (input_data == MAP_FAILED) {
        perror("mmap input file");
        return 1;
    }
    printf("mmap %s\n", input_data);

    // Close input file
    if (close(input_fd) == -1) {
        perror("close input file");
        return 1;
    }

    // Determine number of records
    int num_records = input_size / sizeof(Record);

    // Initialize record array
    Record* records = (Record*) input_data;

    // Initialize threads and arguments
    pthread_t threads[num_threads];
    MergeArgs mergeArgs[num_threads];

    // Calculate chunk size and start/end indices for each thread
    int chunk_size = num_records / num_threads;
    int start = 0;
    int end = chunk_size - 1;
    for (int i = 0; i < num_threads; i++) {
        // Adjust end index for last thread to avoid rounding errors
        if (i == num_threads - 1) {
            end = num_records - 1;
        }

        // Set up arguments for current thread
        mergeArgs[i].start = start;
        mergeArgs[i].end = end;
        mergeArgs[i].records = records;

        // Spawn thread
        if (pthread_create(&threads[i], NULL, merge_worker, &mergeArgs[i])) {
            perror("pthread_create");
            return 1;
        }

        // Update start and end indices for next thread
        start = end + 1;
        end += chunk_size;
    }
    // Wait for threads to finish this is fucked
    for (int i = 0; i < num_threads; i++) {
        if (pthread_join(threads[i], NULL)) {
            perror("pthread_join");
            return 1;
        }
    }
    printf("here\n");
    // Open output file
    int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (output_fd == -1) {
        perror("open output file");
        return 1;
    }

    // Write records to output file
    for (int i = 0; i < num_records; i++) {
        if (write(output_fd, &records[i], sizeof(Record)) == -1) {
            perror("write output file");
            return 1;
        }
    }

    // Force writes to disk
    if (fsync(output_fd) == -1) {
        perror("fsync output file");
        return 1;
    }

    // Close output file
    if (close(output_fd) == -1) {
        perror("close output file");
        return 1;
    }
    // Unmap input file from memory
    if (munmap(records, input_size) == -1) {
        perror("munmap input_file");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

