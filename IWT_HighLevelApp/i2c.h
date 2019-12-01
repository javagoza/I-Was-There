#pragma once

#include <stdbool.h>
#include "epoll_timerfd_utilities.h"


#define VCNL4040_ID            0x6C   // register value
#define VCNL4040_ADDRESS	   0x60	  // I2C Address

int initI2c(void);
void closeI2c(void);

// Export to use I2C in other file
extern int i2cFd;
