#include "memory.h"
#include "../common/effects.h"
#include <string.h> // memcpy
#include <sys/shm.h>
#include <stdlib.h> //exit
#include <stdio.h> //perror
#include <sched.h> //piHiPri
#include <time.h> //millis
#include "opengl.h" //max and min

static int shmid;
static struct gun_struct *cleanup_pointer;

static void shared_init_gun(struct gun_struct *this_gun)
{
    memset(this_gun,0,sizeof(struct gun_struct));

    this_gun->ir_pwm = 1024;
    this_gun->fan_pwm = 1024;

    if(getenv("GORDON")) {
        this_gun->playlist_solo[0] = GST_MOVIE1;
        this_gun->playlist_solo[1] = GST_MOVIE2;
        this_gun->playlist_solo[2] = GST_MOVIE3;
        this_gun->playlist_solo[3] = GST_MOVIE4;
        this_gun->playlist_solo[4] = GST_MOVIE5;
        this_gun->playlist_solo[5] = GST_MOVIE6;
        this_gun->playlist_solo[6] = GST_MOVIE7;
        this_gun->playlist_solo[7] = GST_MOVIE8;
        this_gun->playlist_solo[8] = GST_MOVIE9;
        this_gun->playlist_solo[9] = GST_MOVIE10;
    } else {
        this_gun->playlist_solo[0] = GST_MOVIE10;
        this_gun->playlist_solo[1] = GST_MOVIE9;
        this_gun->playlist_solo[2] = GST_MOVIE8;
        this_gun->playlist_solo[3] = GST_MOVIE7;
        this_gun->playlist_solo[4] = GST_MOVIE6;
        this_gun->playlist_solo[5] = GST_MOVIE5;
        this_gun->playlist_solo[6] = GST_MOVIE4;
        this_gun->playlist_solo[7] = GST_MOVIE3;
        this_gun->playlist_solo[8] = GST_MOVIE2;
        this_gun->playlist_solo[9] = GST_MOVIE1;
    }

    this_gun->playlist_solo_index = 1;
    this_gun->effect_solo = this_gun->playlist_solo[0];

    this_gun->playlist_duo[0] = GST_NORMAL;
    this_gun->playlist_duo[1] = GST_GLHEAT;
    this_gun->playlist_duo[2] = GST_EDGETV;
    this_gun->playlist_duo[3] = GST_REVTV;
    this_gun->playlist_duo[4] = GST_AATV;
    this_gun->playlist_duo[5] = GST_AGINGTV;
    this_gun->playlist_duo[6] = GST_CACATV;
    this_gun->playlist_duo[7] = GST_GLMIRROR;
    this_gun->playlist_duo[8] = GST_RIPPLETV;
    this_gun->playlist_duo[9] = GST_RADIOACTV;

    this_gun->playlist_duo_index = 1;
    this_gun->effect_duo = this_gun->playlist_duo[0];

    this_gun->servo_bypass = SERVO_NORMAL;
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
        printf("\nSET THE GORDON OR CHELL ENVIRONMENT VARIABLE!\n");
        exit(1);
    }

    /* check if tethered to a desk via ethernet: */
    if(getenv("TETHERED"))	(*this_gun)->tethered = true;
    else					(*this_gun)->tethered = false;
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

uint32_t micros(void)
{
    uint64_t now ;
    struct  timespec ts ;
    clock_gettime (CLOCK_MONOTONIC_RAW, &ts) ;
    now  = (uint64_t)ts.tv_sec * (uint64_t)1000000 + (uint64_t)(ts.tv_nsec / 1000) ;
    return (uint32_t)now;
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
void fps_counter(char * title,uint32_t start_time,int skip)
{
    static const uint32_t interval = 1000; //in milliseconds
    static uint32_t render_time = 0;
    static uint32_t time_fps = 0;
    static uint32_t frame_time_min = interval * 1000;
    static uint32_t frame_time_max = 0;
    static int fps = 0;
    static int frameskip = 0;
    frameskip += skip;
    uint32_t frame_time = micros() - start_time;   //in microseconds
    frame_time_max = MAX2(frame_time_max,frame_time);
    frame_time_min = MIN2(frame_time_min,frame_time);
    render_time += frame_time;
    fps++;
    if (time_fps < millis()) {
        float ms_per_frame = (float)render_time/(fps * interval);
        int load_percent = (float)render_time/(interval * 10.0);
        float jitter = (float)(frame_time_max-frame_time_min)/100.0;
        printf("%s fps:%d\t%.2fms frame\t%.2fms jitter\t%d%% load %d skipped\n",title,fps, ms_per_frame,jitter,load_percent,frameskip);
        fps = 0;
        render_time = 0;
        frameskip = 0;
        time_fps += interval;
        frame_time_min = interval * 1000;
        frame_time_max = 0;
        /* readjust counter if we missed a cycle */
        if (time_fps < millis()) time_fps = millis() + interval;
    }
}

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

float map(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}