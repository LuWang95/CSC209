#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct 
{
    int num_workers;
    uint64_t trial_number;
    uint64_t chunk_size
    
}user_input;


user_input parse_args(int argc,char* argv[]){
    if(argc != 4){
        fprintf(stderr,"Usage:%s,<worker_number><trial_number><chunk_number>");
    }

    user_input input;
    input.num_workers = argv[1];
    input.trial_number = argv[2];
    input.chunk_size = argv[3];

    if(input.num_workers <=0 || input.chunk_size <=0 || input.trial_number<=0){
        fprintf(stderr,"Invalid Input");
    }



}


