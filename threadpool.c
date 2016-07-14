//Sultan Khalaily
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "threadpool.h"

threadpool* create_threadpool(int num_threads_in_pool){
	if(num_threads_in_pool<0||num_threads_in_pool>MAXT_IN_POOL)
		return NULL;
	
	threadpool* poolthread = (threadpool*) malloc(sizeof(threadpool));
	if(poolthread==NULL)
		return NULL;
	
	
	poolthread->num_threads=num_threads_in_pool;	//number of active threads
	poolthread->qsize=0;	        //number in the queue
	
	pthread_t *threads = (pthread_t*) malloc(sizeof(pthread_t)*(num_threads_in_pool));
	if(threads==NULL)
		return NULL;
	poolthread->threads=threads;	//pointer to threads
	
	poolthread->qhead=NULL;		//queue head pointer
	poolthread->qtail=NULL;		//queue tail pointer
	
	int res;
	res=pthread_mutex_init(&(poolthread->qlock), NULL);  //lock on the queue list
	if(res){ 
		perror("error: \n"); 
		exit(-1);
	} 
	res=pthread_cond_init(&(poolthread->q_not_empty), NULL); //non empty and empty condidtion vairiables
	if(res){ 
		perror("error: \n"); 
		exit(-1);
	} 
	res=pthread_cond_init(&(poolthread->q_empty), NULL); 
	if(res){ 
		perror("error: \n"); 
		exit(-1);
	} 
	
	poolthread->shutdown=0;            //1 if the pool is in distruction process     
	poolthread->dont_accept=0;       //1 if destroy function has begun
	
	int i=0;
	
	for(;i<num_threads_in_pool;i++)
	{
		res=pthread_create(threads+i, NULL, do_work, (void*)poolthread);
		if(res){ 
			perror("error: \n"); 
			exit(-1);
		} 
	}
	return poolthread;
}

void* do_work(void* p){
	threadpool* thpool = (threadpool*) p;

	while(1){
		if(thpool->shutdown==1){
			pthread_mutex_unlock (&(thpool->qlock));
			return (NULL);
		}
		pthread_mutex_lock (&(thpool->qlock));
		
		if(thpool->qsize==0){
			pthread_cond_wait(&thpool->q_empty, &thpool->qlock);
		}
		
		if(thpool->shutdown==1){
			pthread_mutex_unlock (&(thpool->qlock));
			return (NULL);
		}

		thpool->qsize--; 
		work_t *work_st = thpool->qhead; 
		thpool->qhead = work_st->next; 
		
		pthread_mutex_unlock (&(thpool->qlock));
		
		(work_st->routine)(work_st->arg);
		free (work_st); 
		work_st = NULL; 
		
		if(thpool->qsize==0&&thpool->dont_accept==1){
			pthread_cond_signal(&thpool->q_not_empty);
			return (NULL);
		}
	}
	return(NULL);
}

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
	if(from_me->dont_accept==1)
		return;
	
	pthread_mutex_lock (&(from_me->qlock)); 
	work_t* work = (work_t*) malloc(sizeof(work_t));
	
	work->routine=dispatch_to_here;  //the threads process function
	work->arg=arg;  //argument to the function
	work->next=NULL;  
	from_me->qsize++;
	if(from_me->qhead==NULL)
	{	
		from_me->qhead=work;
		from_me->qtail=work;
	}
	else{
		from_me->qtail->next=work;
		from_me->qtail=work;
	}
	pthread_mutex_unlock (&(from_me->qlock));
	pthread_cond_signal (&(from_me->q_empty));
}

void destroy_threadpool(threadpool* destroyme){
	pthread_mutex_lock (&(destroyme->qlock));
	destroyme->dont_accept=1;
	if(destroyme->qsize!=0)
		pthread_cond_wait(&destroyme->q_not_empty, &destroyme->qlock);
	
	destroyme->shutdown=1;
	
	int i;
	for(i=0;i<destroyme->num_threads;i++)
		pthread_cond_signal (&(destroyme->q_empty));
	pthread_mutex_unlock (&(destroyme->qlock));
	int res;
	for (i = 0; i < destroyme->num_threads; i++){
		res=pthread_join(destroyme->threads[i], NULL); 
		if(res){ 
		      perror("error: \n"); 
		      exit(-1);
		}	
	}
	
	pthread_mutex_destroy(&(destroyme->qlock));
	pthread_cond_destroy(&(destroyme->q_not_empty));
	pthread_cond_destroy(&(destroyme->q_empty));
	free(destroyme->threads);
	free(destroyme);
}
