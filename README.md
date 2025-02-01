# Multithreaded Word Frequency Counter with Thread Affinity

## Overview
This program reads a very large text file, counts the total number of unique words, and calculates their frequencies using multiple threads. It also has an option to enable thread affinity for better CPU utilization.

## Features
- Multithreaded processing for efficiency.
- Thread affinity option to bind threads to specific CPUs.
- Efficient hash table-based word storage.
- Handles large text files efficiently by dividing them into chunks.
- Merges results from multiple threads into a final frequency count.

## Compilation
To compile the program, use:
```bash
gcc -o word_counter word_counter.c -pthread
```

## Usage
```bash
./word_counter <input_file> <num_threads> <affinity>
```
where:
- `<input_file>` is the path to the text file to be processed.
- `<num_threads>` is the number of threads to use.
- `<affinity>` is `1` to enable thread affinity and `0` to disable it.

## Example
```bash
./word_counter large_text.txt 4 1
```
This runs the program with 4 threads and enables thread affinity.

## How It Works
1. The file size is determined, and the input file is divided into equal chunks based on the number of threads.
2. Each thread processes its chunk by:
   - Adjusting to the nearest word boundary.
   - Reading words and storing them in a local hash table.
3. After all threads finish execution, their results are merged into a global hash table.
4. The program prints:
   - The total number of unique words.
   - The frequency of each word.
   - The execution time.

## Performance Considerations
- Using multiple threads improves performance, especially on large files.
- Enabling thread affinity can enhance cache locality and performance.
- A hash table with linked lists is used to manage collisions efficiently.

## Sample Output
```
the: 5000
and: 3200
of: 2900
...
Total unique words: 100000
Execution time: 2.34 seconds
```

## Dependencies
- POSIX threads (`pthread`)
- GCC or any compatible C compiler

## Notes
- Ensure the input file is large enough to benefit from multithreading.
- Too many threads may degrade performance due to contention.
- Memory usage scales with the number of unique words in the input file.


