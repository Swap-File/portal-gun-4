#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>


bool i2c_write(int i2c_fd, uint8_t address, uint8_t * buffer, uint8_t length)
{

  struct i2c_msg message = { address, 0, length, buffer };
  struct i2c_rdwr_ioctl_data ioctl_data = { &message, 1 };
  int result = ioctl(i2c_fd, I2C_RDWR, &ioctl_data);
  if (result != 1)
  {
    printf("failed to set target");
    return false;
  }
  return true;
}


bool i2c_read(int i2c_fd,uint8_t address, uint8_t command, uint8_t * buffer, uint8_t length)
{
  struct i2c_msg messages[] = {
    { address,  0, 1, &command },
    { address,  I2C_M_RD, length, buffer },
  };
  struct i2c_rdwr_ioctl_data ioctl_data = { messages, 2 };
  int result = ioctl(i2c_fd, I2C_RDWR, &ioctl_data);
  if (result != 2)
  {
    printf("failed to get variables");
    return false;
  }
  return true;
}

bool i2c_read_only(int i2c_fd,uint8_t address,  uint8_t * buffer, uint8_t length)
{
  struct i2c_msg messages[] = {
    { address,  I2C_M_RD, length, buffer },
  };
  struct i2c_rdwr_ioctl_data ioctl_data = { messages, 1 };
  int result = ioctl(i2c_fd, I2C_RDWR, &ioctl_data);
  if (result != 1)
  {
    printf("failed to get variables");
    return false;
  }
  return true;
}
