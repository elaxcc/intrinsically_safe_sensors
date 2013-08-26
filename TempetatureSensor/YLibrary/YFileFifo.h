#ifndef YFILEFIFO_H
#define YFILEFIFO_H

#include <stdint.h>
#include <SensorsList.h>
#include <YOneWire.h>

#define FIFO_SIZE 700

void YFileFifoPush(struct Sensor *sensor, struct Temperature *temperature);

uint32_t YFileFifoElementsAmount(struct Sensor *sensor);

void YFileFifoGetElement(FIL *file, struct Date *date,
	struct Temperature *temperature, uint32_t elem_num);

#endif // YFILEFIFO_H
