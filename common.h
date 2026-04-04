#ifndef COMMON
#define COMMON

#include <sys/types.h>
#include <stdint.h>


typedef enum{
    msg_job = 1,
    stop_job = 2
} job_type_t;

typedef struct 
{
    int worker_id;
    int chunk_id;
    u_int64_t trials;
    u_int32_t random_seed;
} job_msg_type;

typedef struct
{
    int worker_id;
    int chunk_id;
    u_int64_t trials_done;
    u_int64_t count;
};

ssize_t read_full(int fd, void *buf, size_t count);
ssize_t write_full(int fd, const void *buf,size_t count);

#endif