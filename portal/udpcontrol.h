#ifndef _UDPCONTROL_H
#define _UDPCONTROL_H
#include <stdint.h>
void udpcontrol_setup(void);
int udp_send_state(int state, uint32_t offset);
int udp_receive_state(int * state, uint32_t * offset);
#endif
