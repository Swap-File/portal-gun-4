#ifndef _SHAREDMEM_H
#define _SHAREDMEM_H

#include "portal.h"

void shm_setup(this_gun_struct** this_gun ,bool init);
void shm_cleanup(void);
unsigned int millis (void);
int piHiPri (const int pri);

#endif