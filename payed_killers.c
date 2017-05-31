#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <math.h>

//Const to companies generator
#define COMPANY_KILLERS_MIN = 2
#define COMPANY_KILLERS_MAX = 6
#define COMPANY_REPUTATION_MIN = 10
#define COMPANY_REPUTATION_MAX = 30

//TODO DELETE NOT NEEDED CONSTS
#define ROOT 0
#define MSG_TAG 100
#define Points 20000000
#define R 200000

struct message {
  int sender_id;
  int company_id;
  int info_type;
  int timestamp;
}

int main(int argc,char **argv)
{
    //TODO load number of companies from params
    int size,tid;   //size - number of processes ; tid - current process id
    MPI_Init(&argc, &argv);
    MPI_Comm_size( MPI_COMM_WORLD, &size );
    MPI_Comm_rank( MPI_COMM_WORLD, &tid );

    if ( tid == 0) {
      //TODO Random companies killers and reputation
      //TODO Create queue for process 1, reputation and killers table.
      //TODO Send data to other processes (Sync)
      //TODO SEND START ALLOW TO ALL processess (async)
    } else {
      //TODO RECEIVE DATA
      //TODO Create queue, reputation and killers table for each process
      //TODO Wait for the rest of processes (wait for start allow signal)
    }

     //PROCESSING START
    while(True) {
      //TODO Implement this

    }


    MPI_Finalize();
}

