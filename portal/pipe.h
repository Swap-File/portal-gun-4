#ifndef _PIPE_H 
#define _PIPE_H

#include "shared.h"

void pipe_init(bool gordon);
void pipe_cleanup(void);
void pipe_www_out(const struct gun_struct *this_gun);
void pipe_audio(const char *filename);
void pipe_update(struct gun_struct *this_gun);
int pipe_www_in(struct gun_struct *this_gun);
uint32_t pipe_laser_pwr(bool pwr);

#endif