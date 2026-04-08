#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include "worker.h"
#include "common.h"

typedef struct {
    int      num_workers;
    uint64_t trial_number;
    uint64_t chunk_size;
} user_input;

user_input parse_args(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <worker_number> <trial_number> <chunk_size>\n", argv[0]);
        exit(1);
    }
    user_input input;
    input.num_workers  = atoi(argv[1]);
    input.trial_number = strtoull(argv[2], NULL, 10);
    input.chunk_size   = strtoull(argv[3], NULL, 10);
    if (input.num_workers <= 0 || input.chunk_size == 0 || input.trial_number == 0) {
        fprintf(stderr, "Invalid Input\n");
        exit(1);
    }
    return input;
}

void create_workers(worker_t *workers, int n)
{
    for (int i = 0; i < n; i++) {
        int p2c[2]; /* parent writes jobs → worker reads  */
        int c2p[2]; /* worker writes results → parent reads */

        if (pipe(p2c) < 0 || pipe(c2p) < 0) {
            perror("pipe");
            exit(1);
        }
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }
        if (pid == 0) {
            for (int j = 0; j < i; j++)
            {
                if (close(workers[j].from_worker_fd)<0 || close(workers[j].to_worker_fd) < 0)
                {
                    perror("close");
                    exit(1);
                }
                
            }
            
            free(workers);
            
            /* child only reads jobs and writes results — close the other ends */
            if (close(p2c[1]) < 0 || close(c2p[0]) < 0) {
                perror("close");
                exit(1);
            }
            worker_loop(p2c[0], c2p[1], i);
            if (close(p2c[0]) < 0 || close(c2p[1]) < 0) {
                perror("close");
                exit(1);
            }
            exit(0);
        } else {
            /* parent only writes jobs and reads results — close the other ends */
            if (close(p2c[0]) < 0 || close(c2p[1]) < 0) {
                perror("close");
                exit(1);
            }
            worker_t worker;
            worker.id             = i;
            worker.from_worker_fd = c2p[0]; /* parent reads results here */
            worker.to_worker_fd   = p2c[1]; /* parent writes jobs here   */
            worker.pid            = pid;
            workers[i]            = worker;
        }
    }
}

void send_job(worker_t *worker, uint64_t trial, int job_id)
{
    job_msg_type job = {0};
    job.type        = msg_job;
    job.random_seed = 42 + job_id * 11; /* unique seed per job */
    job.trials      = trial;
    job.job_id      = job_id;

    if (write_full(worker->to_worker_fd, &job, sizeof(job)) != (ssize_t)sizeof(job)) {
        perror("failure to write job");
        exit(1);
    }
}

void send_shutdown(worker_t *worker)
{
    job_msg_type job = {0};
    job.type   = stop_job;
    job.job_id = -1;

    if (write_full(worker->to_worker_fd, &job, sizeof(job)) != (ssize_t)sizeof(job)) {
        perror("failure to shutdown");
        exit(1);
    }
}

/* Wait for any worker to finish using select(), then read its result */
int collect_one_result(worker_t *workers, int n, result_msg_type *out)
{
    fd_set readfds;
    FD_ZERO(&readfds);
    int maxfd = -1;

    for (int i = 0; i < n; i++) {
        if (workers[i].from_worker_fd < 0)
            continue;
        FD_SET(workers[i].from_worker_fd, &readfds);
        if (workers[i].from_worker_fd >= maxfd)
            maxfd = workers[i].from_worker_fd;
    }
    if (maxfd < 0) {
        fprintf(stderr, "no worker result pipes remain open\n");
        return -1;
    }

    int ready;
    do {
        ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
    } while (ready < 0 && errno == EINTR); /* retry if interrupted by a signal */

    if (ready < 0) {
        perror("select");
        exit(1);
    }

    for (int i = 0; i < n; i++) {
        if (!FD_ISSET(workers[i].from_worker_fd, &readfds))
            continue;

        ssize_t bytes = read_full(workers[i].from_worker_fd, out, sizeof(result_msg_type));

        if (bytes == 0) {
            fprintf(stderr, "worker %d closed pipe unexpectedly\n", i);
            close(workers[i].from_worker_fd);
            workers[i].from_worker_fd = -1;
            return -1;
        }
        if (bytes < 0) {
            perror("failure to read from worker");
            close(workers[i].from_worker_fd);
            workers[i].from_worker_fd = -1;
            return -1;
        }
        if (bytes != (ssize_t)sizeof(result_msg_type)) {
            fprintf(stderr, "partial result message from worker %d\n", i);
            close(workers[i].from_worker_fd);
            workers[i].from_worker_fd = -1;
            return -1;
        }

        return i;
    }
    fprintf(stderr, "select returned, but no fd ready");
    exit(1);
}

double run_simulation(worker_t *workers, user_input *input, int *next_job_io,
                      uint64_t *cumulative_count, uint64_t *cumulative_trials,
                      double *cumulative_time)
{
    struct timespec start_time, end_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
        perror("clock_gettime start");
        exit(1);
    }

    uint64_t trials_sent = 0;
    uint64_t trials_done = 0;
    int      next_job    = *next_job_io;
    uint64_t round_count = 0;

    /* Phase 1: send first batch — one chunk per worker to get all of them busy */
    for (int i = 0; i < input->num_workers && trials_sent < input->trial_number; i++) {
        uint64_t chunk     = input->chunk_size;
        uint64_t remaining = input->trial_number - trials_sent;
        if (chunk > remaining) chunk = remaining;
        printf("Sending job %d to worker %d\n", next_job, i);
        send_job(&workers[i], chunk, next_job);
        next_job++;
        trials_sent += chunk;
    }

    /* Phase 2: as each worker finishes, immediately send it the next chunk */
    while (trials_done < input->trial_number) {
        result_msg_type one_result;
        int worker_id = collect_one_result(workers, input->num_workers, &one_result);

        if (worker_id < 0) {
            fprintf(stderr, "failed to collect a valid result from any worker\n");
            exit(1);
        }

        trials_done += one_result.trials_done;
        round_count += one_result.count;

        if (trials_sent < input->trial_number) {
            uint64_t chunk     = input->chunk_size;
            uint64_t remaining = input->trial_number - trials_sent;
            if (chunk > remaining) chunk = remaining;

            printf("Worker %d finished previous job, sending chunk %d to the worker right now. Remaining trials is  %llu\n.",
                   worker_id, next_job, (unsigned long long)remaining);

            if (workers[worker_id].to_worker_fd < 0) {
                fprintf(stderr, "worker %d command pipe is closed\n", worker_id);
                exit(1);
            }
            send_job(&workers[worker_id], chunk, next_job);
            trials_sent += chunk;
            next_job++;
        }
    }

    /* Phase 3: all work done — send stop_job to every worker */
    for (int i = 0; i < input->num_workers; i++) {
        if (workers[i].to_worker_fd < 0) {
            fprintf(stderr, "worker %d shutdown pipe already closed\n", i);
            continue;
        }
        printf("Worker %d finished all the jobs\n", i);
        send_shutdown(&workers[i]);
    }

    *next_job_io        = next_job;
    *cumulative_count  += round_count;
    *cumulative_trials += trials_done;

    if (clock_gettime(CLOCK_MONOTONIC, &end_time) != 0) {
        perror("clock_gettime end");
        exit(1);
    }
    double elapsed = (double)(end_time.tv_sec  - start_time.tv_sec) +
                     (double)(end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    *cumulative_time += elapsed;
    printf("\nThis round took %.6f seconds\n", elapsed);
    printf("Cumulative time: %.6f seconds\n\n", *cumulative_time);

    return 4.0 * (double)round_count / (double)trials_done;
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN); /* turn broken-pipe into a write() error instead of a crash */

    uint64_t cumulative_count  = 0;
    uint64_t cumulative_trials = 0;
    double   cumulative_time   = 0.0;
    int      next_job_id       = 0;

    user_input input  = parse_args(argc, argv);
    int worker_num    = input.num_workers;
    worker_t *workers = malloc(sizeof(worker_t) * worker_num);
    if (workers == NULL) {
        perror("malloc");
        exit(-1);
    }

    create_workers(workers, worker_num);
    double pai = run_simulation(workers, &input, &next_job_id,
                                &cumulative_count, &cumulative_trials, &cumulative_time);

    for (int i = 0; i < worker_num; i++) {
        if (close(workers[i].from_worker_fd) < 0 || close(workers[i].to_worker_fd) < 0)
            perror("close");
    }

    printf("This round: pi = %f\n", pai);
    printf("Cumulative: pi = %f (total trials %llu)\n",
           4.0 * cumulative_count / cumulative_trials,
           (unsigned long long)cumulative_trials);

    for (;;) {
        printf("Want to run again? (y/n): ");
        fflush(stdout);
        char answer;
        if (scanf(" %c", &answer) != 1) break;

        if (answer != 'y' && answer != 'Y' && answer != 'n' && answer != 'N') {
            printf("Please enter y or n to run again or quit\n");
            continue;
        }
        if (answer == 'n' || answer == 'N') break;

        printf("Enter workers trials chunk_size: ");
        unsigned long long t, c;
        if (scanf("%d %llu %llu", &input.num_workers, &t, &c) != 3) {
            printf("Please enter valid input\n");
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            continue;
        }
        input.trial_number = (uint64_t)t;
        input.chunk_size   = (uint64_t)c;
        if (input.num_workers <= 0 || input.trial_number == 0 || input.chunk_size == 0) {
            fprintf(stderr, "Invalid Input\n");
            continue;
        }
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {}

        /* Reap old workers before spawning new ones — no zombies */
        for (int i = 0; i < worker_num; i++)
            waitpid(workers[i].pid, NULL, 0);
        free(workers);

        worker_num = input.num_workers;
        workers    = malloc(sizeof(worker_t) * worker_num);
        if (workers == NULL) {
            perror("malloc");
            exit(1);
        }

        create_workers(workers, worker_num);
        pai = run_simulation(workers, &input, &next_job_id,
                             &cumulative_count, &cumulative_trials, &cumulative_time);

        for (int i = 0; i < worker_num; i++) {
            if (close(workers[i].from_worker_fd) < 0 || close(workers[i].to_worker_fd) < 0)
                perror("close");
        }
        printf("This round: pi = %f\n", pai);
        printf("Cumulative: pi = %f (total trials %llu)\n",
               4.0 * cumulative_count / cumulative_trials,
               (unsigned long long)cumulative_trials);
    }

    for (int i = 0; i < worker_num; i++)
        waitpid(workers[i].pid, NULL, 0);
    free(workers);
    return 0;
}
