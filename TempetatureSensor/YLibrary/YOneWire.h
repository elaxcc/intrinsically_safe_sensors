#ifndef __YONEWIRE_H_
#define __YONEWIRE_H_

#include "YBool.h"

#include <stdint.h>

/*!
 * \brief This module use DS2480B chip for comunication
 */

#define MAX_DEVICES_AMOUNT 10

extern uint8_t YOneWireDevicesRoms_[MAX_DEVICES_AMOUNT][8];

struct Temperature
{
	uint8_t t_lsb;
	uint8_t t_msb;
	uint8_t t_cnt_remain;
	uint8_t t_cnt_per_c;
};

void YOneWireInit(void (*_send_func)(uint8_t), YBOOL (*_send_complete_func)(void),
	uint8_t (*_read_func)(void), YBOOL (*_read_complete_func)(void));
void YOneWireFindDevices(void);
YBOOL YOneWireReadDeviceTemperature(uint8_t* device_id, struct Temperature* temperature);

float ConvertTemperatureToFloat(struct Temperature* temperature);

void SaveTemperatureToFile(uint8_t* device_id, struct Temperature* temperature);

#endif // __YONEWIRE_H_
