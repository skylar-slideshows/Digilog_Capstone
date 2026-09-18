#ifndef I2C_SCHEDULER_H
#define I2C_SCHEDULER_H

#include <stdint.h>

/**
 * @brief Start a single frame of the I2C communication for the given bucket channel
 */
void start_i2c_frame (uint8_t channel);

#endif
