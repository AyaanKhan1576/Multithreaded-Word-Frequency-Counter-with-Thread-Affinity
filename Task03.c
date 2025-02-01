#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>

#define HASH_TABLE_SIZE 10000000

typedef struct Entry {
    char *word;
    int count;
    struct Entry *next;
} Entry;

typedef struct {
    Entry **buckets;
} HashTable;

typedef struct {
    char *filename;
    long start_offset;
    long end_offset;
    HashTable *hash_table;
} ThreadArg;

HashTable *createHT() {
    HashTable *ht = (HashTable *)malloc(sizeof(HashTable));
    ht->buckets = (Entry **)calloc(HASH_TABLE_SIZE, sizeof(Entry *));
    return ht;
}

unsigned long hashFunc(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HASH_TABLE_SIZE;
}

void add_word(HashTable *ht, const char *word) {
    unsigned long index = hashFunc(word);
    Entry *entry = ht->buckets[index];

    while (entry != NULL) {
        if (strcmp(entry->word, word) == 0) {
            entry->count++;
            return;
        }
        entry = entry->next;
    }

    Entry *new_entry = (Entry *)malloc(sizeof(Entry));
    new_entry->word = strdup(word);
    new_entry->count = 1;
    new_entry->next = ht->buckets[index];
    ht->buckets[index] = new_entry;
}

void mergeHash(HashTable *dest, HashTable *src) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        Entry *src_entry = src->buckets[i];
        while (src_entry != NULL) {
            add_word(dest, src_entry->word);
            src_entry = src_entry->next;
        }
    }
}

long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) != 0) {
        perror("Error getting file size");
        exit(EXIT_FAILURE);
    }
    return st.st_size;
}

long findStart(FILE *file, long original_start) {
    if (original_start == 0) return 0;

    fseek(file, original_start - 1, SEEK_SET);
    int c = fgetc(file);
    if (isspace(c)) return original_start;

    long pos = original_start - 1;
    while (pos >= 0) {
        fseek(file, pos, SEEK_SET);
        c = fgetc(file);
        if (isspace(c)) return pos + 1;
        pos--;
    }
    return 0;
}

long findEnd(FILE *file, long original_end, long file_size) {
    if (original_end >= file_size) return file_size;

    fseek(file, original_end, SEEK_SET);
    int c;
    long pos = original_end;
    while (pos < file_size) {
        c = fgetc(file);
        if (isspace(c)) return pos;
        pos++;
    }
    return file_size;
}

void *process_chunk(void *arg) {
    ThreadArg *t_arg = (ThreadArg *)arg;
    const char *filename = t_arg->filename;
    long original_start = t_arg->start_offset;
    long original_end = t_arg->end_offset;

    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    long file_size = get_file_size(filename);
    long adjusted_start = findStart(file, original_start);
    long adjusted_end = findEnd(file, original_end, file_size);

    HashTable *hash_table = createHT();

    fseek(file, adjusted_start, SEEK_SET);
    char word[256];

    while (fscanf(file, "%255s", word) == 1) {
        add_word(hash_table, word);
    }

    fclose(file);
    t_arg->hash_table = hash_table;
    return NULL;
}

void free_hash_table(HashTable *ht) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        Entry *entry = ht->buckets[i];
        while (entry) {
            Entry *temp = entry;
            entry = entry->next;
            free(temp->word);
            free(temp);
        }
    }
    free(ht->buckets);
    free(ht);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <input_file> <num_threads> <affinity:0|1>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    int num_threads = atoi(argv[2]);
    int use_affinity = atoi(argv[3]);

    long file_size = get_file_size(filename);
    long chunk_size = file_size / num_threads;

    ThreadArg *thread_args = (ThreadArg *)malloc(num_threads * sizeof(ThreadArg));
    pthread_t *threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);

    for (int i = 0; i < num_threads; i++) {
        thread_args[i].filename = filename;
        thread_args[i].start_offset = i * chunk_size;
        thread_args[i].end_offset = (i + 1) * chunk_size;
        thread_args[i].hash_table = NULL;
    }
    thread_args[num_threads - 1].end_offset = file_size;

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, process_chunk, &thread_args[i]);
        if (use_affinity) {
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(i % num_cpus, &cpuset);
            pthread_setaffinity_np(threads[i], sizeof(cpu_set_t), &cpuset);
        }
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Merge all thread-local hash tables
    HashTable *global_ht = createHT();
    for (int i = 0; i < num_threads; i++) {
        if (thread_args[i].hash_table) {
            mergeHash(global_ht, thread_args[i].hash_table);
            free_hash_table(thread_args[i].hash_table);
        }
    }

    // Calculate total unique words
    int unique_words = 0;
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        Entry *entry = global_ht->buckets[i];
        while (entry) {
            unique_words++;
            printf("%s: %d\n", entry->word, entry->count);
            entry = entry->next;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double execution_time = (end_time.tv_sec - start_time.tv_sec) +
                            (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    printf("\nTotal unique words: %d\n", unique_words);
    printf("Execution time: %.2f seconds\n", execution_time);

    free_hash_table(global_ht);
    free(thread_args);
    free(threads);
    
    return 0;
}
