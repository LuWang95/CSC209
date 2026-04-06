#ifndef WORKER
#define WORKER

#include <stdint.h>

void worker_loop(int read_fd,int write_fd,int worker_id);
uint64_t count_inside_circle(uint64_t trials, uint32_t seed);

#endif
