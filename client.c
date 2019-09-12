#define BUF_SIZE 100


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <errno.h>

#include <time.h>
#include <sys/time.h>


typedef struct request_struct{
	int index;
	char keyword[128];
} request;

	struct shared_data_struct{
		request request_queue[10];
		int queue_state[10];
		int result_queue[10][BUF_SIZE];
		int out[10];
		int in[10];
		int in_request;
		int out_request;
		int count[10];
	};

struct shared_data_struct *shared_data_ptr;


#define SHARED_MEMORY_SIZE sizeof(struct shared_data_struct)


int fd;

void *shm_start;
struct  stat statbuf;
int N = 10;
request rq;

sem_t *sem_mutex_request;
sem_t *sem_empty_request;
sem_t *sem_full_request;

sem_t *sem_mutex_result[10];
sem_t *sem_empty_result[10];
sem_t *sem_full_result[10];

sem_t* sem_mutex_qs;

void client(char *shm_name, char *keyword, char *semName){


  fd = shm_open(shm_name,O_RDWR, 0660 );
  if(fd < 0)
    {
      printf("FAIL ");
      exit(1);
  }
  
  fstat(fd, &statbuf );  // get status of shared mem
  
  shm_start = mmap(NULL, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // map the shared memory into the address space 
  if(shm_start < 0) {
    
     printf(" cannot map shared memory");
  }

  close(fd);
  
  shared_data_ptr = (struct shared_data_struct *) shm_start;

    
	//sem_unlink(SEM_NAME_QS);
	//sem_unlink(SEM_NAME_REQ_Q_E);
	//sem_unlink(SEM_NAME_REQ_Q_F);

	char sem_name[128];
	char sem_name_res_q[256];
	char sem_name_res_q_e[256];
	char sem_name_res_q_f[256];
	char sem_name_qs[256];
	char sem_name_req[256];
	char sem_name_req_e[256];
	char sem_name_req_f[256];
	
	sprintf(sem_name, "%s", semName);
	//printf("NAME %s\n", sem_name);


	sprintf(sem_name_res_q,"%s%s", sem_name,"_resultQueue" );
	sprintf(sem_name_res_q_e,"%s%s", sem_name,"_resultQueue_empty" );
	sprintf(sem_name_res_q_f,"%s%s", sem_name,"_resultQueue_full" );
	sprintf(sem_name_qs,"%s%s", sem_name,"_queue_state" );
	sprintf(sem_name_req,"%s%s", sem_name, "_request_queue" );
	sprintf(sem_name_req_e,"%s%s", sem_name,"_request_queue_empty" );
	sprintf(sem_name_req_f,"%s%s", sem_name, "_request_queue_full" );



	sem_mutex_qs = sem_open(sem_name_qs, O_RDWR );
	sem_empty_request = sem_open(sem_name_req_e, O_RDWR);
	
	sem_full_request = sem_open(sem_name_req_f, O_RDWR);
	sem_mutex_request = sem_open(sem_name_req, O_RDWR);
	char name[30][128];
	for(int i = 0; i < 10 ; i++)	
	{	
		sprintf(name[i], "%s%d", sem_name_res_q, i);
		//sem_unlink(name[i]);
		sem_mutex_result[i] = sem_open(name[i], O_RDWR);
		if (sem_mutex_result[i] < 0){
			perror("cant create semaphore");
			exit(1);
		}
	}

	for(int i = 0; i < 10 ; i++)	
	{	
		sprintf(name[10+i], "%s%d", sem_name_res_q_e, i);
		//sem_unlink(name[10+i]);
		sem_empty_result[i] = sem_open(name[10+i], O_RDWR);
		if (sem_empty_result[i] < 0){
			perror("cant create semaphore");
			exit(1);
		}
	}
	for(int i = 0; i < 10 ; i++)	
	{	
		sprintf(name[20+i], "%s%d", sem_name_res_q_f, i);
		//sem_unlink(name[20+i]);
		sem_full_result[i] = sem_open(name[20+i], O_RDWR);
		if (sem_full_result[i] < 0){
			perror("cant create semaphore");
			exit(1);
		}
	}

  int indexOfRequest = -1;
   for(int i = 0; i < 10; i++)
	{
		sem_wait(sem_mutex_qs);
		if(shared_data_ptr->queue_state[i] == 0)
		{
			indexOfRequest = i;
			shared_data_ptr->queue_state[i] = 1;
			sem_post(sem_mutex_qs);
			break;
		}
		sem_post(sem_mutex_qs);
	}
	if (indexOfRequest == -1){
		printf("too many cients started\n");
		exit(0);
	}

	//printf("index of request client  %d\n", indexOfRequest);

	sem_wait(sem_empty_request);
	sem_wait(sem_mutex_request);
   	shared_data_ptr->request_queue[ shared_data_ptr->in_request].index = indexOfRequest;

   	strcpy(shared_data_ptr->request_queue[ shared_data_ptr->in_request].keyword, keyword);
        //printf("%s\n",shared_data_ptr->request_queue[ shared_data_ptr->in_request ].keyword);
	
	shared_data_ptr->in_request = (shared_data_ptr->in_request + 1) % 10;

	sem_post(sem_mutex_request);
	sem_post(sem_full_request);


	int item;
	do{
			sem_wait(sem_full_result[indexOfRequest]);
			sem_wait(sem_mutex_result[indexOfRequest]);


			item = shared_data_ptr->result_queue[indexOfRequest][shared_data_ptr->out[indexOfRequest]];
			shared_data_ptr->out[indexOfRequest] = (shared_data_ptr->out[indexOfRequest] + 1) % BUF_SIZE;
			if(item != -1 )
				printf("%d\n", item);

			sem_post(sem_mutex_result[indexOfRequest]);
			sem_post(sem_empty_result[indexOfRequest]);
	}while(item != -1);

  	sem_wait(sem_mutex_qs);
	shared_data_ptr->queue_state[indexOfRequest] = 0;
	sem_post(sem_mutex_qs);

}

int main(int argc, char *argv[]){
	// shm_name keyword sem_name
	
	//struct timeval start,end;
	//gettimeofday(&start, NULL);
	client( argv[1], argv[2], argv[3] );
	//gettimeofday(&end, NULL);
	//printf("Time: %ld\n", ((end.tv_sec * 1000000 + end.tv_usec) -(start.tv_sec * 1000000 + start.tv_usec)));
}

