#include "portal.h"
#include <string.h> // memcpy
#include <sys/shm.h>  
#include <stdlib.h>     //exit
#include <stdio.h>  //perror
#include <time.h> //CLOCK_MONOTONIC_RAW
#include <sched.h>

int shmid;	
struct this_gun_struct *cleanup_pointer;

void shm_setup(this_gun_struct**this_gun,bool init){
	
	key_t key;
	
	/* make the key: */
    if ((key = ftok("/home/pi/portal", 'R')) == -1) {
        perror("ftok");
        exit(1);
    }

    /* connect to (and possibly create) the segment: */
    if ((shmid = shmget(key, sizeof(struct this_gun_struct), 0644 | IPC_CREAT)) == -1) {
        perror("shmget");
        exit(1);
    }

    /* attach to the segment to get a pointer to it: */
    *this_gun = (struct this_gun_struct *)shmat(shmid, (void *)0, 0);
    if (*this_gun == (struct this_gun_struct *)(-1)) {
        perror("shmat");
        exit(1);
    }
	
    /* optionally copy init values into shared memory: */	
	if (init){
		struct this_gun_struct temp_gun;
		memcpy(*this_gun,&temp_gun,sizeof(temp_gun));
	}
	
	/* save the pointer for later: */
    cleanup_pointer = *this_gun;
	
	/* set name: */
	if(getenv("GORDON")) 		(*this_gun)->gordon = true;
	else if(getenv("CHELL")) 	(*this_gun)->gordon = false;
	else {
		printf("SET THE GORDON OR CHELL ENVIRONMENT VARIABLE!");
		exit(1);
	}
}

void shm_cleanup(){
	
	/* detatch from the segment: */
	if (shmdt(cleanup_pointer) == -1) {
        perror("shmdt");
        exit(1);
    }
	
	/* remove the segment: */
	if ((shmid = shmctl(shmid, IPC_RMID, 0)) == -1) {
        perror("shmctl");
        exit(1);
	}
}

unsigned int millis (void){
  struct  timespec ts ;
  clock_gettime (CLOCK_MONOTONIC_RAW, &ts) ;
  uint64_t now  = (uint64_t)ts.tv_sec * (uint64_t)1000 + (uint64_t)(ts.tv_nsec / 1000000L) ;
  return (uint32_t)now ;
}

int piHiPri (const int pri){
  struct sched_param sched ;

  memset (&sched, 0, sizeof(sched)) ;

  if (pri > sched_get_priority_max (SCHED_RR))
    sched.sched_priority = sched_get_priority_max (SCHED_RR) ;
  else
    sched.sched_priority = pri ;

  return sched_setscheduler (0, SCHED_RR, &sched) ;
}

