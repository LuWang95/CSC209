#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>

typedef enum{
    msg_job = 1,
    stop_job = 2
} job_type_t;

typedef struct 
{   job_type_t type;
    int job_id;
    uint64_t trials;
    uint32_t random_seed;
} job_msg_type;

typedef struct
{
    int worker_id;
    uint64_t trials_done;
    uint64_t count;
} result_msg_type;

typedef struct {
    int id;
    pid_t pid;
    int to_worker_fd;
    int from_worker_fd;
} worker_t;

ssize_t read_full(int fd, void *buf, size_t count);
ssize_t write_full(int fd, const void *buf,size_t count);

#endif