#ifndef _PIPE_H
#define _PIPE_H

#include "common/memory.h"

void pipe_init(const struct gun_struct *this_gun);
void pipe_cleanup(void);
void pipe_www_out(const struct gun_struct *this_gun);
void pipe_audio(const char *filename);
void pipe_update(struct gun_struct *this_gun);
int pipe_www_in(struct gun_struct *this_gun);
int pipe_button_out(int button, bool gordon);
void pipe_laser_pwr(bool laser_request,struct gun_struct *this_gun);

#endif
