#include <stdint.h>
#include <stdbool.h>

bool i2c_write(int i2c_fd, uint8_t address, uint8_t * buffer, uint8_t length);
bool i2c_read(int i2c_fd,uint8_t address, uint8_t command, uint8_t * buffer, uint8_t length);
bool i2c_read_only(int i2c_fd,uint8_t address,  uint8_t * buffer, uint8_t length);
