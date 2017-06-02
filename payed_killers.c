#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include "struct_const.h"

MPI_Datatype mpi_message_type;

void create_custom_message_type();

void init_companies(int tid,int size,int companyCount,struct Company *companies);

struct ThreadParams* createNewThread(int tid,int size,struct Company* companies, int* proc_time, bool *threadIsAlive, pthread_mutex_t *mutexCompany,pthread_cond_t *changeCondition);

void send_mpi_message(int sender_id, int company_id, int info_type, int timestamp, int data, int tag, int receiver);

struct Message reveive_mpi_message(int tag);

void mainThread(bool* threadIsAlive, int tid,int size, struct Company *companies, int* proc_time) {
   while(*threadIsAlive){
      //Main thread
      sleep(3);
      puts("SENDING\n");
      send_mpi_message(10,11,CHANGE_REPUTATION_TYPE,13,14,MESSAGE_TAG, tid);
      puts("SENDED\n");
   }
}

void *additionalThread(void *thread){
  struct ThreadParams *pointer = (struct ThreadParams*) thread;
  struct Company *companies = pointer->companies;
  int tid = pointer->tid;
  int size = pointer->size;
  int *proc_time = pointer->proc_time;
  bool* threadIsAlive = pointer->threadIsAlive;
  pthread_mutex_t *mutexCompany = pointer->mutexCompany;
  pthread_cond_t *changeCondition = pointer->changeCondition;

  while(*threadIsAlive){
    //WAIT ON MASSAGE
    struct Message data = reveive_mpi_message(MESSAGE_TAG);
    switch (data.info_type) {
       case CHANGE_REPUTATION_TYPE:
         companies[data.company_id].reputation += data.data;
         break;
       case REMOVE_FROM_COMPANY_QUEUE:
         //TODO just remove from queue
         break;
       case REQUEST_KILLER:
         //TODO ADD sender_id to local queue, send response with timestamp
         break;
    }
  }
  pthread_exit(NULL);
}

int main(int argc,char **argv){
   int companyCount = COMPANY_COUNT;
   if (argc > 1){
      companyCount = (int) strtol(argv[1], (char **)NULL, 10);
   }
   int size, tid, provided;   //size - number of processes ; tid - current process id ; provided - to check mpi threading
   MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
   if (provided < MPI_THREAD_MULTIPLE){
      printf("ERROR: The MPI library does not have full thread support\n");
      MPI_Abort(MPI_COMM_WORLD, 1);
   }
   MPI_Comm_size(MPI_COMM_WORLD, &size);
   create_custom_message_type();
   MPI_Comm_rank( MPI_COMM_WORLD, &tid );

   struct Company *companies = (struct Company*)malloc(sizeof(struct Company)*companyCount);
   init_companies(tid,size,companyCount,companies);
   int *proc_time = (int*)malloc(sizeof(int)*1);
   *proc_time = -1;

   //  Create additional thread
   pthread_mutex_t *mutexCompany = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
   pthread_mutex_init(mutexCompany,NULL);
   pthread_attr_t *attr = (pthread_attr_t*)malloc(sizeof(pthread_attr_t));
   pthread_attr_init(attr);
   pthread_attr_setdetachstate(attr, PTHREAD_CREATE_JOINABLE);
   pthread_t *thread = (pthread_t*)malloc(sizeof(pthread_t));
   bool *threadIsAlive = (bool*)malloc(sizeof(bool));
   *threadIsAlive = true;
   pthread_cond_t *changeCondition = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
   pthread_cond_init(changeCondition,NULL);
   struct ThreadParams* param = createNewThread(tid,size,companies,proc_time,threadIsAlive,mutexCompany,changeCondition);
   int err = pthread_create(thread, attr, additionalThread, (void *)param);
   if (err){
      fprintf(stderr,"ERROR Pthread_create: %d\n",err);
      MPI_Abort(MPI_COMM_WORLD, 1);
   }

   mainThread(threadIsAlive, tid, size, companies, proc_time);

   pthread_join(*thread, NULL);
   pthread_attr_destroy(attr);
   pthread_mutex_destroy(mutexCompany);
   pthread_cond_destroy(changeCondition);
   MPI_Type_free(&mpi_message_type);
   MPI_Finalize();
   free(thread);
   free(attr);
   free(changeCondition);
   free(mutexCompany);
   //TODO FREE ALL DATA
   free(companies);
   pthread_exit(NULL);
}

void create_custom_message_type(){
   int blocklengths[5] = {1,1,1,1,1};
   MPI_Datatype types[5] = {MPI_INT,MPI_INT,MPI_INT,MPI_INT,MPI_INT};
   MPI_Aint offsets[5];
   offsets[0] = offsetof(struct Message, sender_id);
   offsets[1] = offsetof(struct Message, company_id);
   offsets[2] = offsetof(struct Message, info_type);
   offsets[3] = offsetof(struct Message, timestamp);
   offsets[4] = offsetof(struct Message, data);

   MPI_Type_create_struct(5, blocklengths, offsets, types, &mpi_message_type);
   MPI_Type_commit(&mpi_message_type);
}

void init_companies(int tid,int size,int companyCount,struct Company *companies){
  srand(time(NULL));
  int i,j;
  for (i = 0; i < companyCount; i++) {
    companies[i].queue = (int**)malloc(sizeof(int*)*size);
    for (j = 0; j < size; j++) {
      companies[i].queue[j] = (int*)malloc(sizeof(int)*2);
      companies[i].queue[j][0] = -1;
      companies[i].queue[j][1] = -1;
    }
    companies[i].killers = -1;
    companies[i].reputation = -1;
    int randComp[2];
    if(tid == ROOT){
      randComp[0] = (rand()%(COMPANY_KILLERS_MAX - COMPANY_KILLERS_MIN) + COMPANY_KILLERS_MIN);
      randComp[1] = (rand()%(COMPANY_REPUTATION_MAX - COMPANY_REPUTATION_MIN) + COMPANY_REPUTATION_MIN);
      printf("Company %d\tkillers: %d\treputation: %d\n", i,randComp[0],randComp[1]);
    }
    MPI_Bcast(&randComp, 2, MPI_INT, ROOT, MPI_COMM_WORLD);
    companies[i].killers = randComp[0];
    companies[i].reputation = randComp[1];
  }
  MPI_Barrier(MPI_COMM_WORLD);
}

struct ThreadParams* createNewThread(int tid,int size,struct Company* companies,int *proc_time, bool *threadIsAlive, pthread_mutex_t *mutexCompany,pthread_cond_t *changeCondition){
    struct ThreadParams* thread = (struct ThreadParams*)malloc(sizeof(struct ThreadParams));
    thread->tid = tid;
    thread->size = size;
    thread->companies = companies;
    thread->threadIsAlive = threadIsAlive;
    thread->proc_time = proc_time;
    thread->changeCondition = changeCondition;
    thread->mutexCompany = mutexCompany;
    return thread;
}

void send_mpi_message(int sender_id, int company_id, int info_type, int timestamp, int data, int tag, int receiver){
   struct Message send_data;
   send_data.sender_id=sender_id;
   send_data.company_id=company_id;
   send_data.info_type=info_type;
   send_data.timestamp=timestamp;
   send_data.data=data;
   sleep(3);
   puts("SENDING\n");
   MPI_Send(&send_data, 1, mpi_message_type, receiver, tag, MPI_COMM_WORLD);
}

struct Message reveive_mpi_message(int tag){
   struct Message data;
   MPI_Status status;
   //TODO HANDLE STATUS
   MPI_Recv(&data, 1, mpi_message_type, MPI_ANY_SOURCE, MESSAGE_TAG, MPI_COMM_WORLD, &status);
   printf("RECEIVED: sender_id:%d company_id:%d info_type:%d time_stamp:%d data:%d\n", data.sender_id, data.company_id, data.info_type, data.timestamp, data.data);
   return data;
}
