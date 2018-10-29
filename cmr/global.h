#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

/* Sleep duration in seconds
 * This can be any length since both monitor timer expiring and
 * child process termination will wake this process up
 */
#define SLEEP_DURATION 1

/* Monitor sample duration in nanoseconds
 * Length of duration decides the monitoring overhead
 * Set to 4 ms
 */
//#define SAMPLE_DURATION 2000000
#define SAMPLE_DURATION 5000000
//#define SAMPLE_DURATION 500000000 // for DEBUG

#define MAX_PROCESS_NUM 8
#define MAX_ARGV_LEN 65536
#define CORE_NUM 16


/* Shared memory configuration */
#define SM_SIZE 256
//#define PRINT_SAMPLES

#define ROTATE_PROB 0.05

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define MAX_FREQ_IDX 9
#define MIN_FREQ_IDX 0

#define EXEC_RECORD_LEN 10
#define MIN_PARTITION_NUM 2
#define MAX_PARTITION_NUM 18

extern bool use_freq_scale;
extern bool use_partitioning;
extern bool change_partition;
extern unsigned int init_partition_num;

#endif
