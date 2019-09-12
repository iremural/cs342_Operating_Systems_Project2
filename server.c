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
	#include <signal.h>
	
	

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

	struct param{
		request rq;
		char filename[128];
	};
	
	#define SHARED_MEMORY_SIZE sizeof(struct shared_data_struct)	


	
	void server(char *, char *, char *);

	struct shared_data_struct *shared_data_ptr;

	int fd;

	void *shm_start;
	struct stat statbuf;
	
	sem_t *sem_mutex_request;
	sem_t *sem_empty_request;
	sem_t *sem_full_request;

	sem_t *sem_mutex_result[10];
	sem_t *sem_empty_result[10];
	sem_t *sem_full_result[10];

	sem_t *sem_mutex_qs;

	pthread_t tid;
	
	char shared_mem_name[128];
	char sem_name[128];
	char sem_name_res_q[256];
	char sem_name_res_q_e[256];
	char sem_name_res_q_f[256];
	char sem_name_qs[256];
	char sem_name_req[256];
	char sem_name_req_e[256];
	char sem_name_req_f[256];
	char name[30][128];
	static void signal_handler()
	{
		//close semaphores
		sem_close(sem_mutex_request);
		sem_close(sem_full_request);
		sem_close(sem_empty_request);
		
		for(int i = 0; i < 10 ; i++)
		{
			sem_close(sem_mutex_result[i]);
			sem_close(sem_empty_result[i]);
			sem_close(sem_full_result[i]);
		}
		//unlink semaphores
		sem_unlink(sem_name_res_q);
		sem_unlink(sem_name_res_q_e);
		sem_unlink(sem_name_res_q_f);
		sem_unlink(sem_name_qs);
		sem_unlink(sem_name_req);
		sem_unlink(sem_name_req_e);
		sem_unlink(sem_name_req_f);
		for( int i = 0; i < 30 ; i++)
		{
			sem_unlink(name[i]);
		}
		//remove shared memory
		shm_unlink(shared_mem_name);
		exit(0);

	}
	static void *search(void * parameter)//request rqptr, char *inputFileName) // static dene
	{

		struct param *p = (struct param *) parameter;		

		int indexOfRequest;

		if(p->rq.index != -1){
			indexOfRequest = p->rq.index;
    		}
	
		//search keyword 
		FILE * fp;
		fp = fopen(p->filename,"r");
		if(fp == NULL)
			printf("%s\n","file null");
		char line[1024];
		int index = 1;
		//int counter = 0;
		while( fgets(line, sizeof(line), fp) != NULL ) // newline character e kadar oku
		{
			if (strstr(line, p->rq.keyword) != NULL){
				//printf("SEARCH  %d\n",index);
                                
                                sem_wait(sem_empty_result[indexOfRequest]);
				sem_wait(sem_mutex_result[indexOfRequest]);

				shared_data_ptr->result_queue[indexOfRequest][ shared_data_ptr->in[indexOfRequest] ] = index;
				//printf("IN: %d\n", shared_data_ptr->result_queue[indexOfRequest][ shared_data_ptr->in[indexOfRequest] ] );
				shared_data_ptr->in[indexOfRequest] = (shared_data_ptr->in[indexOfRequest] + 1) % BUF_SIZE;
				//counter++;
				
				sem_post(sem_mutex_result[indexOfRequest]);
				sem_post(sem_full_result[indexOfRequest]);
  
			}
				
			index++;
			
		}
		sem_wait(sem_empty_result[indexOfRequest]);
		sem_wait(sem_mutex_result[indexOfRequest]);

		shared_data_ptr->result_queue[indexOfRequest][ shared_data_ptr->in[indexOfRequest] ] = -1; // indicator of end 
		shared_data_ptr->in[indexOfRequest] = (shared_data_ptr->in[indexOfRequest] + 1) % BUF_SIZE;

		sem_post(sem_mutex_result[indexOfRequest]);
		sem_post(sem_full_result[indexOfRequest]);

		fclose(fp);
		//printf("END %d\n", getpid() );
		free(p);
		pthread_exit(NULL);

	}


	void server(char *shm_name, char *inputFileName, char *semName){
	  
	  sprintf(shared_mem_name,"%s", shm_name);
	  fd = shm_open(shm_name, O_RDWR | O_CREAT, 0660 );
	  if(fd < 0)
	    {
	      printf("FAIL ");
	      exit(1);
	  }
	  ftruncate(fd, SHARED_MEMORY_SIZE); // set size
	  fstat(fd, &statbuf );  // get status of shared mem
	  
	  shm_start = mmap(NULL, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // map the shared memory into the address space 
	  if(shm_start < 0) {
	    
	     printf(" cannot map shared memory");
	  }

	  close(fd);
	  
	  shared_data_ptr = (struct shared_data_struct *) shm_start;

	  // initialize shared data
	  for(int i = 0; i < 10 ; i++) {
	  
	     shared_data_ptr->in[i] = 0;
	     shared_data_ptr->out[i] = 0;
	     shared_data_ptr->queue_state[i] = 0;
	     shared_data_ptr->request_queue[i].index = -1;
             shared_data_ptr->count[i] = 0;
	  }
	  shared_data_ptr->in_request = 0;
	  shared_data_ptr->out_request = 0;
	   //printf("dsuccess");

	


	
	sprintf(sem_name, "%s", semName);
	//printf("NAME %s\n", sem_name);


	sprintf(sem_name_res_q,"%s%s", sem_name,"_resultQueue" );
	sprintf(sem_name_res_q_e,"%s%s", sem_name,"_resultQueue_empty" );
	sprintf(sem_name_res_q_f,"%s%s", sem_name,"_resultQueue_full" );
	sprintf(sem_name_qs,"%s%s", sem_name,"_queue_state" );
	sprintf(sem_name_req,"%s%s", sem_name, "_request_queue" );
	sprintf(sem_name_req_e,"%s%s", sem_name,"_request_queue_empty" );
	sprintf(sem_name_req_f,"%s%s", sem_name, "_request_queue_full" );

        sem_unlink(sem_name_qs);
	sem_unlink(sem_name_req);
	sem_unlink(sem_name_req_e);

	sem_mutex_qs = sem_open(sem_name_qs, O_RDWR | O_CREAT, 0660, 1);
	if (sem_mutex_qs < 0){
			perror("cant create semaphore");
			exit(1);
	}

	sem_empty_request = sem_open(sem_name_req_e, O_RDWR | O_CREAT, 0660, 10);
	if (sem_empty_request < 0){
			perror("cant create semaphore");
			exit(1);
	}

	sem_mutex_request = sem_open(sem_name_req, O_RDWR | O_CREAT, 0660, 1);

	sem_unlink(sem_name_req_f);
	sem_full_request = sem_open(sem_name_req_f, O_RDWR | O_CREAT, 0660, 0);
	if (sem_full_request < 0){
			perror("cant create semaphore");
			exit(1);
	}

	
	for(int i = 0; i < 10 ; i++)	
	{	
		sprintf(name[i], "%s%d", sem_name_res_q, i);
		sem_unlink(name[i]);
		sem_mutex_result[i] = sem_open(name[i], O_RDWR | O_CREAT, 0660, 1);
		if (sem_mutex_result[i] < 0){
			perror("cant create semaphore");
			exit(1);
		}
	}

	for(int i = 0; i < 10 ; i++)	
	{	
		sprintf(name[10+i], "%s%d", sem_name_res_q_e, i);
		sem_unlink(name[10+i]);
		sem_empty_result[i] = sem_open(name[10+i], O_RDWR | O_CREAT, 0660, BUF_SIZE);
		if (sem_empty_result[i] < 0){
			perror("cant create semaphore");
			exit(1);
		}
	}
	for(int i = 0; i < 10 ; i++)	
	{	
		sprintf(name[20+i], "%s%d", sem_name_res_q_f, i);
		sem_unlink(name[20+i]);
		sem_full_result[i] = sem_open(name[20+i], O_RDWR | O_CREAT, 0660, 0);
		if (sem_full_result[i] < 0){
			perror("cant create semaphore");
			exit(1);
		}
	}
	int ret;
	struct param paramStruct;
	  while(1){	
			sem_wait(sem_full_request);
			sem_wait(sem_mutex_request);
			//search(shared_data_ptr->request_queue[shared_data_ptr->out_request], "input.txt");

			struct param* paramStruct = malloc(sizeof(struct param));
			paramStruct->rq = shared_data_ptr->request_queue[shared_data_ptr->out_request];
		
			strcpy(paramStruct->filename, inputFileName);
			ret = pthread_create(&(tid), NULL, &search, (void*) paramStruct);
			if(ret != 0)
			{
				printf("thread creation failed.");
				exit(1);
			}
			//ret = pthread_join(tid,NULL);

			shared_data_ptr->out_request = (shared_data_ptr->out_request + 1) % 10;

			sem_post(sem_mutex_request);
			sem_post(sem_empty_request);
	    }

	}

	int main(int argc, char **argv){

		signal(SIGINT, signal_handler);
		server(argv[1], argv[2], argv[3] );
	}




