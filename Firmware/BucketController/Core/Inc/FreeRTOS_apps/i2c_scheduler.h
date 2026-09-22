#ifndef I2C_SCHEDULER_H
#define I2C_SCHEDULER_H

/**
 * @brief Initialize I2C Scheduler timer and tasks
 */
void init_i2c_scheduler (void);

/**
 * @brief Trigger one frame of the I2C scheduler on all channels
 */
void i2c_scheduler_trigger_frame (void);

#endif
