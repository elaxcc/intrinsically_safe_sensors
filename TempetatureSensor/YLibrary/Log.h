#ifndef __LOG_H_
#define __LOG_H_

#include "YBool.h"

#include "SensorsList.h"

#include <stdint.h>

#define LOG_CHANGE_THRESHOLD 1
#define LOG_CHANGE_PLACE 2
#define LOG_THRESHOLD_ABOVE 3
#define LOG_SENSOR_LOST 4

void LogSave(uint8_t type);
uint32_t LogAmount(void);
YBOOL LogRead(uint32_t index, uint8_t *log_type, struct Date *date);
void EraseLog(void);

#endif // __LOG_H_
