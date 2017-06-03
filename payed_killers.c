#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/time.h>
#include "struct_const.h"

MPI_Datatype mpi_message_type;
/**
   Metoda tworzy customowy typ wiadomosci wysyłanej przez MPI
**/
void create_custom_message_type();
/**
   F-cja inicjalizacyjna która losuje startowe wartości
**/
void init_companies(int tid,int size,int companyCount,struct Company *companies);
/**
   Tworzenie nowego wątku
**/
struct ThreadParams* createNewThread(int tid,int size,struct Company* companies, bool *threadIsAlive, pthread_mutex_t *mutexCompany,pthread_cond_t *changeCondition);
/**
   Wrapper na MPI_Send któremu podajemy wszystkie parametry i in już buduje strukturę do przesłania
**/
void send_mpi_message(int sender_id, int company_id, int info_type, int timestamp, int data, int tag, int receiver);
/**
   Wrapper na MPI_recv który od razu zwraca nam naszą strukturę wiadomości
**/
struct Message reveive_mpi_message(int tag);
/**
   Usuwa klienta z kolejki danej firmy
**/
void remove_client_from_queue(struct Company *companies, int size, int sender_id, int company_id);
/**
   Dodaje klienta do kolejki sortując ją po timestampie. Jeżeli czasy są takie same pierwszeństwo ma niższe ID
**/
void add_client_to_queue(struct Company * companies, int size, int sender_id, int company_id, int timestamp);
/**
   Zmiania lokalnie reputację firmy oraz rozsyła polecenie zmiany do innych firm
**/
void update_company_reputation(struct Company * companies, int tid, int size, int company_id, int reputation_change){
   int i=0;
   companies[company_id].reputation+=reputation_change;
   for(i=0;i<size;i++){
      if(i!=tid){
         send_mpi_message(tid,company_id, CHANGE_REPUTATION_TYPE, -1, reputation_change, MESSAGE_TAG, i);
      }
   }

}
/**
   Metoda wysyła żądania zwolnienia z kolejek do wszystkich innych klientów oraz zwalnia z lokalnej kolejki dla danej firmy
**/
void free_request_to_company(int tid, int size, int company, struct Company * companies){
   int i=0;
   remove_client_from_queue(companies, size, tid, company);
   for(i=0;i<size;i++)
      if (i!=tid)
         send_mpi_message(tid,company,REMOVE_FROM_COMPANY_QUEUE,-1,-1,MESSAGE_TAG, i);
}
/**
   Dodaje wybraną firmę do lokalnej kolejki oraz wysyła requesty do innych firm oraz odbiera ACK
**/
int request_company(int tid, int size, int company, struct Company * companies){
   int i=0;
   int CURR_TIME = (int)time(NULL);
   add_client_to_queue(companies, size, tid, company, CURR_TIME);
   for(i=0;i<size;i++){
      if(i!=tid){
         send_mpi_message(tid,company, REQUEST_KILLER, CURR_TIME, -1, MESSAGE_TAG, i);
         //TODO RECEIVE AND DO STH
      }
   }
}





void mainThread(bool* threadIsAlive, int tid,int size, struct Company *companies) {
   int i=0;
   srand(tid);
   while(*threadIsAlive){
      sleep(rand()%SLEEP_RAND_MAX);

      //TODO NAJLEPSZA FIRMA - DO POPRAWKI
      int best_company = 0, best_reputation=companies[0].reputation;
      for(i=1;i<size;i++){
         if (companies[i].reputation>best_reputation){
            best_reputation = companies[i].reputation;
            best_company=i;
         }
      }
      printf("%d \n", best_company);

      int result = request_company(tid, size, best_company, companies);

      //TODO SEND ADD TO queue
      //TODO SEND REQUESTS TO ALL CLIENTS
      //TODO WAIT FOR RESPONSES
      //TODO DECIDE TO USE KILLER OR REQUEST NEXT COMPANY OR WAIT

      //Main thread
      sleep(3);
      puts("SENDING MAIN\n");
      send_mpi_message(tid,0,REQUEST_KILLER,13,14,MESSAGE_TAG, tid);
      puts("SENDED MAIN\n");
      puts("WAITING MAIN\n");
      struct Message data = reveive_mpi_message(ACK_TAG);
      puts("RECEIVED MAIN\n");

      //TODO FREE COMPANY AND CHANGE REPUTATION
      update_company_reputation(companies, tid, size, best_company, (rand()%(REPUTATION_CHANGE_MIN_MAX*2)-REPUTATION_CHANGE_MIN_MAX ));


   }
}

void *additionalThread(void *thread){
  int i=0;
  struct ThreadParams *pointer = (struct ThreadParams*) thread;
  struct Company *companies = pointer->companies;
  int tid = pointer->tid;
  int size = pointer->size;
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
         remove_client_from_queue(companies, size, data.sender_id, data.company_id);
         //TODO NOTIFY OTHER THREAD?
         break;
       case REQUEST_KILLER: //Dodajemy do kolejki
         add_client_to_queue(companies, size, data.sender_id, data.company_id, data.timestamp);
         //Jeżeli nie ubiegamy się o firmę lub nie ubiegamy się o tą konktretną wysyłamy -1. W przeciwnym wypadku wysyłamy czas w którym robiliśmy request.
         int myRequestTimeStamp = -1;
         for(i=0;i<size;i++){
            if(companies[data.company_id].queue[i][0] == tid){
               myRequestTimeStamp = companies[data.company_id].queue[i][1];
               break;
            }
         }
         send_mpi_message(tid, data.company_id,ACK_TYPE,myRequestTimeStamp,-1, ACK_TAG, data.sender_id);
         break;
    }

    for(i=0;i<size;i++){
      printf("%d %d\n", companies[0].queue[i][0], companies[0].queue[i][1]);
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
   struct ThreadParams* param = createNewThread(tid,size,companies,threadIsAlive,mutexCompany,changeCondition);
   int err = pthread_create(thread, attr, additionalThread, (void *)param);
   if (err){
      fprintf(stderr,"ERROR Pthread_create: %d\n",err);
      MPI_Abort(MPI_COMM_WORLD, 1);
   }

   mainThread(threadIsAlive, tid, size, companies);

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
      companies[i].queue[j][0] = -1;   //Client ID
      companies[i].queue[j][1] = -1;   //Request time
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

struct ThreadParams* createNewThread(int tid,int size,struct Company* companies, bool *threadIsAlive, pthread_mutex_t *mutexCompany,pthread_cond_t *changeCondition){
    struct ThreadParams* thread = (struct ThreadParams*)malloc(sizeof(struct ThreadParams));
    thread->tid = tid;
    thread->size = size;
    thread->companies = companies;
    thread->threadIsAlive = threadIsAlive;
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
   MPI_Recv(&data, 1, mpi_message_type, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
   printf("RECEIVED: sender_id:%d company_id:%d info_type:%d time_stamp:%d data:%d\n", data.sender_id, data.company_id, data.info_type, data.timestamp, data.data);
   return data;
}

void remove_client_from_queue(struct Company *companies, int size, int sender_id, int company_id){
   int i;
   for(i=0; i<size; i++){
      if(companies[company_id].queue[i][0] == sender_id){
         companies[company_id].queue[i][0] = -1;
         companies[company_id].queue[i][1] = -1;
         while(i<size-1 && companies[company_id].queue[i+1][0] !=-1){
            companies[company_id].queue[i][0] = companies[company_id].queue[i+1][0];
            companies[company_id].queue[i][1] = companies[company_id].queue[i+1][1];
            companies[company_id].queue[i+1][0]=-1;
            companies[company_id].queue[i+1][1]=-1;
            i++;
         }
         break;
      }
   }
}

void add_client_to_queue(struct Company * companies, int size, int sender_id, int company_id, int timestamp){
   int i=0;

   for(i=0;i<size;i++){
      if(timestamp <= companies[company_id].queue[i][1] || companies[company_id].queue[i][0] == -1){
         if(timestamp == companies[company_id].queue[i][1] && sender_id>companies[company_id].queue[i][0])
            continue;

         int tmp1=companies[company_id].queue[i][0];
         int tmp2=companies[company_id].queue[i][1];
         companies[company_id].queue[i][0] = sender_id;
         companies[company_id].queue[i][1] = timestamp;

         while(i<size-1 && tmp1!=-1){
            int tmp3=companies[company_id].queue[i+1][0];
            int tmp4=companies[company_id].queue[i+1][1];
            companies[company_id].queue[i+1][0] = tmp1;
            companies[company_id].queue[i+1][1] = tmp2;
            tmp1 = tmp3;
            tmp2 = tmp4;
         }
         break;
      }
   }
}
