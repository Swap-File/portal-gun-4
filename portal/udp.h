#ifndef _UDP_H
#define _UDP_H

#include <stdbool.h>
#include <stdint.h>

void udp_init(bool gordon);
int udp_send_state(int state, uint32_t offset);
int udp_receive_state(int *state, uint32_t *offset);

#endif