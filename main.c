#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include<sys/select.h>
#include "worker.h"
#include "common.h"
typedef struct 
{
    int num_workers;
    uint64_t trial_number;
    uint64_t chunk_size;
    
}user_input;


user_input parse_args(int argc,char* argv[]){
    if(argc != 4){
        fprintf(stderr, "Usage: %s <worker_number> <trial_number> <chunk_size>\n", argv[0]);
        exit(1);
    }

    user_input input;
    input.num_workers = atoi(argv[1]);
    input.trial_number = strtoull(argv[2], NULL, 10);
    input.chunk_size = strtoull(argv[3], NULL, 10);
    if(input.num_workers <=0 || input.chunk_size ==0 || input.trial_number==0){
        fprintf(stderr,"Invalid Input\n");
        exit(1);
    }
    return input;
}


void create_workers(worker_t *workers, int n){
    for (int i = 0; i < n; i++)
    {
        int p2c[2];
        int c2p[2];
        if(pipe(p2c)<0 || pipe(c2p)<0){
            perror("pipe");
            exit(1);
        }
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            exit(1);
        }
        if (pid == 0)
        {
            if (close(p2c[1]) < 0 || close(c2p[0])<0)
            {
                perror("close");
                exit(1);
            }
            worker_loop(p2c[0],c2p[1],i);
             if (close(p2c[0]) < 0 || close(c2p[1])<0)
            {
                perror("close");
                exit(1);
            }
            exit(0);
        }
        else{
            if (close(p2c[0]) < 0 || close(c2p[1])<0)
            {
                perror("close");
                exit(1);
            }
            worker_t worker;
            worker.id = i;
            worker.from_worker_fd = c2p[0];
            worker.to_worker_fd = p2c[1];
            worker.pid = pid;
            workers[i] = worker;
            
        }
        
    }
    

}

void send_job(worker_t *worker, uint64_t trial, int job_id){
    job_msg_type job;
    job.type = msg_job;
    job.random_seed = 42 + job_id * 11;
    job.trials = trial;
    job.job_id = job_id;

    if (write_full(worker->to_worker_fd, &job, sizeof(job)) != (ssize_t)sizeof(job)) {
        fprintf(stderr, "failure to write job\n");
        exit(1);
    }
}

void send_shutdown(worker_t *worker){
    job_msg_type job;
    job.type = stop_job;
    job.job_id = -1;
    job.trials = 0;
    job.random_seed = 0;

    if (write_full(worker->to_worker_fd, &job, sizeof(job)) != (ssize_t)sizeof(job)) {
        fprintf(stderr, "failure to shutdown\n");
        exit(1);
    }
    
}

int collect_one_result(worker_t *workers,int n, result_msg_type *out){
    fd_set readfds;
    FD_ZERO(&readfds);

    int maxfd = -1;
    for(int i = 0; i < n; i++){
        FD_SET(workers[i].from_worker_fd,&readfds);
        if(workers[i].from_worker_fd >= maxfd){
            maxfd = workers[i].from_worker_fd;
        }
    }
    int ready = select(maxfd+1,&readfds,NULL,NULL,NULL);
    if(ready < 0){
        perror("select");
        exit(1);
    }
    for(int i = 0;i < n; i++){
        if(FD_ISSET(workers[i].from_worker_fd,&readfds)){
            if (read_full(workers[i].from_worker_fd,out,sizeof(result_msg_type))<= 0)
            {
                fprintf(stderr,"failure to read from worker");
                exit(1);
            }
            return i;
        }
    }
    fprintf(stderr,"select returned, but no fd ready");
    exit(1);

}


double run_simulation(worker_t *workers,user_input *input){
    uint64_t trials_sent = 0;
    uint64_t trials_done = 0;
    int next_job = 0;
    uint64_t global_count = 0;
    for(int i = 0; i < input->num_workers && trials_sent < input->trial_number; i++){
        uint64_t chunk = input ->chunk_size;
        uint64_t remaining = input ->trial_number - trials_sent;
        if(chunk > remaining){
            chunk = remaining;
        }
        printf("Sending job %d to worker %d\n",next_job,i);
        send_job(&workers[i],chunk,next_job);
        next_job++;
        trials_sent += chunk;
    }
    while(trials_done < input ->trial_number){
        result_msg_type one_result;
        int worker_id;
        worker_id = collect_one_result(workers,input->num_workers,&one_result);
        trials_done += one_result.trials_done;
        global_count += one_result.count;
        
        if (trials_sent < input -> trial_number)
        {
            uint64_t chunk = input ->chunk_size;
            uint64_t remaining = input ->trial_number - trials_sent;
            if(chunk > remaining){
                chunk = remaining;
            }
            printf("Worker %d finished previous job, sending chunk %d to the worker right now. Remaining trials is  %lld\n.",worker_id,next_job,remaining);
            send_job(&workers[worker_id],chunk,next_job);
            trials_sent += chunk;
            next_job ++;
        }    
    }

    for (int i = 0; i < input->num_workers; i++)
    {
        send_shutdown(&workers[i]);
    }
    
    return 4.0 * (double) global_count / (double) trials_done;
}

int main(int argc, char *argv[]){
    user_input input;
    input = parse_args(argc,argv);
    int worker_num = input.num_workers;
    worker_t *workers = malloc(sizeof(worker_t) * worker_num);
    if (workers == NULL)
    {
        perror("malloc");
        exit(-1);
    }
    create_workers(workers,worker_num);

    double pai = run_simulation(workers,&input);
    for (int i = 0; i < worker_num; i++)
    {
        if (close(workers[i].from_worker_fd) < 0 || close(workers[i].to_worker_fd) < 0)
        {
            perror("close");
        }
        
    }

    printf("Estimated pi = %f\n", pai);
    free(workers);

    return 0;
}
