#ifndef _PROJECTOR_LOGIC_H
#define _PROJECTOR_LOGIC_H

#include <stdbool.h>

void logic_update(int gst_state, int portal_state);
void logic_init(volatile bool *video_done_flag_p);

#endif