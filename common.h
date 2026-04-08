#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    msg_job  = 1,   /* assign a chunk of work to a worker */
    stop_job = 2    /* tell the worker to exit            */
} job_type_t;

/* Message 1: Parent --> Worker */
typedef struct {
    job_type_t type;
    int        job_id;
    uint64_t   trials;
    uint32_t   random_seed; /* unique per chunk so samples stay independent */
} job_msg_type;

/* Message 2: Worker --> Parent */
typedef struct {
    int      worker_id;
    uint64_t trials_done;
    uint64_t count;         /* points that fell inside the quarter circle */
} result_msg_type;

typedef struct {
    int    id;
    pid_t  pid;
    int    to_worker_fd;    /* parent writes jobs here   */
    int    from_worker_fd;  /* parent reads results here */
} worker_t;

ssize_t read_full (int fd,       void *buf, size_t count);
ssize_t write_full(int fd, const void *buf, size_t count);

#endif
