#include <stdio.h>   
#include <stdlib.h>  
#include <unistd.h> 
#include <stdint.h>  

#include "worker.h"  
#include "common.h"  


/**
 * Monte Carlo simulation function.
 * Generates random points in [0,1] x [0,1] and counts how many fall
 * inside the quarter unit circle.
 *
 * @param trials number of random points to generate
 * @param seed random seed 
 * @return number of points inside the circle
 */
uint64_t count_inside_circle(uint64_t trials, uint32_t seed) {
    uint64_t inside = 0;

    srand(seed);

    // generate random points
    for (uint64_t i = 0; i < trials; i++) {
        double x = (double) rand() / (double) RAND_MAX;
        double y = (double) rand() / (double) RAND_MAX;

        // check if point is inside circle
        if (x * x + y * y <= 1.0) {
            inside++;
        }
    }

    return inside;
}


/**
 * Continuously receives jobs from parent through pipe,
 * processes them, and sends results back.
 *
 * @param read_fd  file descriptor to read jobs from parent
 * @param write_fd file descriptor to send results back to parent
 * @param worker_id ID of this worker
 */
void worker_loop(int read_fd, int write_fd, int worker_id) {
    job_msg_type job;       
    result_msg_type result; 

    while (1) {

        // read a full job message from parent
        ssize_t bytes_read = read_full(read_fd, &job, sizeof(job_msg_type));

        // case 1: failed to read full message (error or partial read)
        if (bytes_read != sizeof(job_msg_type)) {
            perror("worker read job failed");
            close(read_fd);
            close(write_fd);
            _exit(1);
        }

        // case 2: parent closed pipe or send stop signal (no more data)
        if ((bytes_read == 0) || (job.type == stop_job)) {
            close(read_fd);
            close(write_fd);
            _exit(0);
        }

        // perform Monte Carlo simulation for this chunk
        uint64_t inside = count_inside_circle(job.trials, job.random_seed);

        // package result to send back
        result.worker_id = worker_id;      
        result.chunk_id = job.chunk_id;    
        result.trials_done = job.trials;   
        result.count = inside;            

        // send result back to parent
        if (write_full(write_fd, &result, sizeof(result_msg_type)) != sizeof(result_msg_type)) {
            perror("worker write result failed");
            close(read_fd);
            close(write_fd);
            _exit(1);
        }

        // loop continues: worker waits for next chunk
    }
}