#include "memory.h"
#include "../common/effects.h"
#include <string.h> // memcpy
#include <sys/shm.h>  
#include <stdlib.h> //exit
#include <stdio.h> //perror
#include <sched.h> //piHiPri
#include <time.h> //millis

static int shmid;	
static struct gun_struct *cleanup_pointer;

static void shared_init_gun(struct gun_struct *this_gun)
{
    memset(this_gun,0,sizeof(struct gun_struct));
	
	this_gun->ir_pwm = 1024;
	this_gun->fan_pwm = 1024;
	
	this_gun->playlist_solo[0]=GST_LIBVISUAL_INFINITE;
	this_gun->playlist_solo[1]=GST_LIBVISUAL_JESS;
	this_gun->playlist_solo[2]=GST_GOOM;
	this_gun->playlist_solo[3]=GST_GOOM2K1;
	this_gun->playlist_solo[4]=GST_LIBVISUAL_JAKDAW;
	this_gun->playlist_solo[5]=GST_LIBVISUAL_OINKSIE;
	this_gun->playlist_solo[6]=-1;
	this_gun->playlist_solo[7]=-1;
	this_gun->playlist_solo[8]=-1;
	this_gun->playlist_solo[9]=-1;
	
	this_gun->playlist_solo_index = 1;
	this_gun->effect_solo = GST_VIDEOTESTSRC;

	this_gun->playlist_duo[0]=GST_NORMAL;
	this_gun->playlist_duo[1]=GST_EDGETV;
	this_gun->playlist_duo[2]=GST_GLHEAT;
	this_gun->playlist_duo[3]=GST_REVTV;
	this_gun->playlist_duo[4]=GST_GLCUBE;
	this_gun->playlist_duo[5]=GST_AGINGTV;
	this_gun->playlist_duo[6]=-1;
	this_gun->playlist_duo[7]=-1;
	this_gun->playlist_duo[8]=-1;
	this_gun->playlist_duo[9]=-1;

	this_gun->playlist_duo_index = 1;
	this_gun->effect_duo = GST_NORMAL;
}

void shared_init(struct gun_struct **this_gun,bool init)
{	
	key_t key;
	
	/* make the key: */
    if ((key = ftok("/home/pi/portal/portal", 'R')) == -1) {
        perror("ftok");
        exit(1);
    }

    /* connect to (and possibly create) the segment: */
    if ((shmid = shmget(key, sizeof(struct gun_struct), 0644 | IPC_CREAT)) == -1) {
        perror("shmget");
        exit(1);
    }

    /* attach to the segment to get a pointer to it: */
    *this_gun = (struct gun_struct *)shmat(shmid, (void *)0, 0);
    if (*this_gun == (struct gun_struct *)(-1)) {
        perror("shmat");
        exit(1);
    }
	
    /* optionally copy init values into shared memory: */	
	if (init) shared_init_gun(*this_gun);
	
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

void shared_cleanup(void)
{
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

uint32_t millis(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  uint64_t now  = (uint64_t)ts.tv_sec * (uint64_t)1000 + (uint64_t)(ts.tv_nsec / 1000000L);
  return (uint32_t)now ;
}

int piHiPri(const int pri)
{
  struct sched_param sched;
  memset (&sched, 0, sizeof(sched));
  if (pri > sched_get_priority_max (SCHED_RR))
    sched.sched_priority = sched_get_priority_max (SCHED_RR);
  else
    sched.sched_priority = pri;
  return sched_setscheduler (0, SCHED_RR, &sched);
}


uint32_t micros(void)
{
  uint64_t now ;
  struct  timespec ts ;
  clock_gettime (CLOCK_MONOTONIC_RAW, &ts) ;
  now  = (uint64_t)ts.tv_sec * (uint64_t)1000000 + (uint64_t)(ts.tv_nsec / 1000) ;
  return (uint32_t)now;
}
