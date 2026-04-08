#include <stdio.h>   
#include <stdlib.h>  
#include <unistd.h> 
#include <stdint.h>  
#include <errno.h>

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
    /* Zero-init so write() does not copy uninitialized struct padding. */
    result_msg_type result = {0};

    while (1) {

        // read a full job message from parent
        ssize_t bytes_read = read_full(read_fd, &job, sizeof(job_msg_type));

        // case 1: parent closed pipe cleanly
        if (bytes_read == 0) {
            close(read_fd);
            close(write_fd);
            _exit(0);
        }
        
        // case 2: actual read error
        if (bytes_read < 0) {
            perror("worker read job failed");
            close(read_fd);
            close(write_fd);
            _exit(1);
        }
        
        // case 3: partial / malformed message
        if (bytes_read != (ssize_t)sizeof(job_msg_type)) {
            fprintf(stderr, "worker received partial job message\n");
            close(read_fd);
            close(write_fd);
            _exit(1);
        }
        
        // case 4: explicit stop command
        if (job.type == stop_job) {
            close(read_fd);
            close(write_fd);
            _exit(0);
        }
        // case 5: normal job message
        if (job.type != msg_job) {
            fprintf(stderr, "worker received unknown job type %d\n", job.type);
            close(read_fd);
            close(write_fd);
            _exit(1);
        }
        
        // validate job payload
        if (job.job_id < 0 || job.trials == 0) {
            fprintf(stderr, "worker received invalid job payload\n");
            close(read_fd);
            close(write_fd);
            _exit(1);
        }        
        
        // perform Monte Carlo simulation for this chunk
        printf("Worker %d received job %d\n",worker_id,job.job_id);
        uint64_t inside = count_inside_circle(job.trials, job.random_seed);

        // package result to send back
        result.worker_id = worker_id;         
        result.trials_done = job.trials;   
        result.count = inside;            

        // send result back to parent
        if (write_full(write_fd, &result, sizeof(result_msg_type)) != (ssize_t)sizeof(result_msg_type)) {
            if (errno == EPIPE) {
                fprintf(stderr, "worker write failed: parent closed result pipe\n");
            } else {
                perror("worker write result failed");
            }
            close(read_fd);
            close(write_fd);
            _exit(1);
        }

        // loop continues: worker waits for next chunk
    }
}
