#include "YOneWire.h"

#include "YDebug.h"

#define YDELAY_F_CPU 72000000

#include "YDelay.h"
#include "SensorsList.h"

#include "LPC23xx.H"

#include "ff.h"

const uint8_t c_reset_command_ = 0xC1;
const uint8_t c_single_bit_1_command_ = 0x91;
const uint8_t c_single_bit_0_command_ = 0x81;
const uint8_t c_match_rom_command_ = 0x55;
const uint8_t c_temperature_convert_command_ = 0x44;
const uint8_t c_read_scratchpad_command_ = 0xBE;

void (*YOneWireSendFunctionPtr_)(uint8_t) = 0;
YBOOL (*YOneWireSendCompleteFunctionPtr_)(void) = 0;
uint8_t (*YOneWireReadFunctionPtr_)(void) = 0;
YBOOL (*YOneWireReadCompletePtr_)(void) = 0;

// global search state
unsigned char ROM_NO[8];
int LastDiscrepancy;
int LastFamilyDiscrepancy;
YBOOL LastDeviceFlag;
unsigned char crc8;

static unsigned char dscrc_table[] = {
	0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
	157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
	35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
	190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
	70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
	219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
	101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
	248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
	140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
	17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
	175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
	50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
	202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
	87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
	233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
	116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

//--------------------------------------------------------------------------
// Calculate the CRC8 of the byte value provided with the current 
// global 'crc8' value. 
// Returns current global crc8 value
//
unsigned char docrc8(unsigned char value)
{
	// See Application Note 27

	// TEST BUILD
	crc8 = dscrc_table[crc8 ^ value];
	return crc8;
}

// if found devices return YTRUE
uint8_t YOneWireResetPulse(void)
{
	uint8_t reset_pulse_result;
	
	uint32_t iter_cnt = 0;
	
	YOneWireSendFunctionPtr_(c_reset_command_);
	while(!YOneWireSendCompleteFunctionPtr_() && iter_cnt < 10000)
	{
		iter_cnt++;
	}
	iter_cnt = 0;
	while(!YOneWireReadCompletePtr_() && iter_cnt < 10000)
	{
		iter_cnt++;
	}
	reset_pulse_result = YOneWireReadFunctionPtr_();
	
	if ((reset_pulse_result & 0x03) == 0x01)
	{
		return YTRUE;
	}
	return YFALSE;
}

void YOneWireSendBit1(void)
{
	uint8_t responce;
	uint32_t iter_cnt = 0;
	
	YOneWireSendFunctionPtr_(c_single_bit_1_command_);
	while(!YOneWireSendCompleteFunctionPtr_() && iter_cnt < 10000)
	{
		iter_cnt++;
	}
	iter_cnt = 0;
	while(!YOneWireReadCompletePtr_() && iter_cnt < 10000)
	{
		iter_cnt++;
	}
	responce = YOneWireReadFunctionPtr_();
	
	if (responce)
	{
		// check responce
	}
}

void YOneWireSendBit0(void)
{
	uint8_t responce;
	uint32_t iter_cnt = 0;
	
	YOneWireSendFunctionPtr_(c_single_bit_0_command_);
	while(!YOneWireSendCompleteFunctionPtr_() && iter_cnt < 10000)
	{
		iter_cnt++;
	}
	iter_cnt = 0;
	while(!YOneWireReadCompletePtr_() && iter_cnt < 10000)
	{
		iter_cnt++;
	}
	responce = YOneWireReadFunctionPtr_();
	
	if (responce)
	{
		// check responce
	}
}

void YOneWireSendBit(uint8_t bit)
{
	if (bit)
	{
		YOneWireSendBit1();
	}
	else
	{
		YOneWireSendBit0();
	}
}

void YOneWireSendByte(uint8_t byte)
{
	uint8_t iter;
	
	for(iter = 0; iter < 8; ++iter)
	{
		if ((byte >> iter) & 0x01)
		{
			YOneWireSendBit1();
		}
		else
		{
			YOneWireSendBit0();
		}
	}
}

uint8_t YOneWireReadBit(void)
{
	uint8_t responce;
	
	YOneWireSendFunctionPtr_(c_single_bit_1_command_);
	while(!YOneWireSendCompleteFunctionPtr_());
	while(!YOneWireReadCompletePtr_());
	responce = YOneWireReadFunctionPtr_();
	return (0x01 & responce);
}

uint8_t YOneWireReadByte(void)
{
	uint8_t responce_byte = 0;
	uint8_t iter;
	
	for(iter = 0; iter < 8; ++iter)
	{
		responce_byte |= ((0x01 &YOneWireReadBit()) << iter);
	}
	
	return responce_byte;
}

YBOOL YOneWireSearch()
{
	volatile int id_bit_number;
	volatile int last_zero, rom_byte_number, search_result;
	volatile int id_bit, cmp_id_bit;
	volatile unsigned char rom_byte_mask, search_direction;

	// initialize for search
	id_bit_number = 1;
	last_zero = 0;
	rom_byte_number = 0;
	rom_byte_mask = 1;
	search_result = 0;
	crc8 = 0;

	// if the last call was not the last one
	if (!LastDeviceFlag)
	{
		// 1-Wire reset
		if (YOneWireResetPulse() == YFALSE)
		{
			// reset the search
			LastDiscrepancy = 0;
			LastDeviceFlag = YFALSE;
			LastFamilyDiscrepancy = 0;
			return YFALSE;
		}

		// issue the search command
		YOneWireSendByte(0xF0);

		// loop to do the search
		do
		{
			// read a bit and its complement
			id_bit = YOneWireReadBit();
			cmp_id_bit = YOneWireReadBit();

			// check for no devices on 1-wire
			if ((id_bit == 1) && (cmp_id_bit == 1))
			{
				break;
			}
			else
			{
				// all devices coupled have 0 or 1
				if (id_bit != cmp_id_bit)
				{
					search_direction = id_bit;  // bit write value for search
				}
				else
				{
					// if this discrepancy if before the Last Discrepancy
					// on a previous next then pick the same as last time
					if (id_bit_number < LastDiscrepancy)
					{
						search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
					}
					else
					{
						// if equal to last pick 1, if not then pick 0
						search_direction = (id_bit_number == LastDiscrepancy);
					}

					// if 0 was picked then record its position in LastZero
					if (search_direction == 0)
					{
						last_zero = id_bit_number;

						// check for Last discrepancy in family
						if (last_zero < 9)
						{
							LastFamilyDiscrepancy = last_zero;
						}
					}
				}

				// set or clear the bit in the ROM byte rom_byte_number
				// with mask rom_byte_mask
				if (search_direction == 1)
				{
					ROM_NO[rom_byte_number] |= rom_byte_mask;
				}
				else
				{
					ROM_NO[rom_byte_number] &= ~rom_byte_mask;
				}

				// serial number search direction write bit
				YOneWireSendBit(search_direction);

				// increment the byte counter id_bit_number
				// and shift the mask rom_byte_mask
				id_bit_number++;
				rom_byte_mask <<= 1;

				// if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
				if (rom_byte_mask == 0)
				{
					docrc8(ROM_NO[rom_byte_number]);  // accumulate the CRC
					rom_byte_number++;
					rom_byte_mask = 1;
				}
			}
		}
		while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7

		// if the search was successful then
		if (!((id_bit_number < 65) || (crc8 != 0)))
		{
			// search successful so set LastDiscrepancy,LastDeviceFlag,search_result
			LastDiscrepancy = last_zero;

			// check for last device
			if (LastDiscrepancy == 0)
				LastDeviceFlag = YTRUE;

			search_result = YTRUE;
		}
	}

	// if no device found then reset counters so next 'search' will be like a first
	if (!search_result || !ROM_NO[0])
	{
		LastDiscrepancy = 0;
		LastDeviceFlag = YFALSE;
		LastFamilyDiscrepancy = 0;
		search_result = YFALSE;
	}

	return search_result;
}

YBOOL YOneWireFirst()
{
	LastDiscrepancy = 0;
	LastDeviceFlag = YFALSE;
	LastFamilyDiscrepancy = 0;

	return YOneWireSearch();
}

YBOOL YOneWireNext()
{
	return YOneWireSearch();
}

void YOneWireInit(void (*_send_func)(uint8_t), YBOOL (*_send_complete_func)(void),
	uint8_t (*_read_func)(void), YBOOL (*_read_complete_func)(void))
{
	YOneWireSendFunctionPtr_ = _send_func;
	YOneWireSendCompleteFunctionPtr_ = _send_complete_func;
	YOneWireReadFunctionPtr_ = _read_func;
	YOneWireReadCompletePtr_ = _read_complete_func;
}

// return number of found devices
void YOneWireFindDevices(void)
{
	struct Sensor* sensor;
	YBOOL have_first_device;
	YBOOL have_next_device = YTRUE;
	struct Temperature temperature;
	
	YDebugSendMessage("first search\r\n", 14);
	have_first_device = YOneWireFirst();
	YDebugSendMessage("after search\r\n", 14);
	
	if (have_first_device == YTRUE)
	{
		// try to save first device in list
		if (SensorFind(ROM_NO, &sensor) == YFALSE)
		{
			// can't find device in list, it is new device
			SensorAdd(ROM_NO);
			
			// dummy temperature read
			YOneWireReadDeviceTemperature(ROM_NO, &temperature);
			
			YDebugSendMessage("Add new sensor\r\n", 16);
		}
		else
		{
			if (sensor->status_ == SENSOR_LOST)
			{
				sensor->status_ = SENSOR_OK;
			}
		}
		
		while(have_next_device == YTRUE)
		{
			have_next_device = YOneWireNext();
			if (have_next_device == YTRUE)
			{
				// try to save first device in list
				if (SensorFind(ROM_NO, &sensor) == YFALSE)
				{
					// can't find device in list, it is new device
					SensorAdd(ROM_NO);
					
					// dummy temperature read
					YOneWireReadDeviceTemperature(ROM_NO, &temperature);
					
					YDebugSendMessage("Add new sensor\r\n", 16);
				}
				else
				{
					if (sensor->status_ == SENSOR_LOST)
					{
						sensor->status_ = SENSOR_OK;
					}
				}
			}
		}
	}
}

YBOOL YOneWireReadDeviceTemperature(uint8_t* device_id, struct Temperature* temperature)
{
	uint8_t iter;
	uint8_t device_data[9];
	
	// send reset pulse
	YOneWireResetPulse();
	
	// send match ROM command
	YOneWireSendByte(c_match_rom_command_);
	
	// send device ID
	for (iter = 0; iter < 8; ++iter)
	{
		YOneWireSendByte(device_id[iter]);
	}
	
	// send temperature convert command
	YOneWireSendByte(c_temperature_convert_command_);
	
	// wait 500 ms
	YDelay(600);
	
	// send reset pulse
	YOneWireResetPulse();
	
	// send match ROM command
	YOneWireSendByte(c_match_rom_command_);
	
	// send device ID
	for (iter = 0; iter < 8; ++iter)
	{
		YOneWireSendByte(device_id[iter]);
	}
	
	// send read scratchpad command
	YOneWireSendByte(c_read_scratchpad_command_);
	
	// read temperature
	for (iter = 0; iter < 9; ++iter)
	{
		device_data[iter] = YOneWireReadByte();
	}
	
	// send reset pulse
	YOneWireResetPulse();
	
	// check incoming data
	crc8 = 0;
	for (iter = 0; iter < 8; ++iter)
	{
		docrc8(device_data[iter]);
	}
	if (crc8 != device_data[8])
	{
		return YFALSE;
	}
	
	temperature->t_lsb = device_data[0];
	temperature->t_msb = device_data[1];
	temperature->t_cnt_remain = device_data[6];
	temperature->t_cnt_per_c = device_data[7];
	
	return YTRUE;
}

float ConvertTemperatureToFloat(struct Temperature* temperature)
{
	uint16_t tmp1, tmp2;
	
	tmp1 = (uint16_t)(temperature->t_msb );
	tmp1 = ((tmp1 << 8) | ((uint16_t)(temperature->t_lsb))) & 0xFFFE;
	tmp2 = (uint16_t)(temperature->t_cnt_per_c - temperature->t_cnt_remain);
	return ((float)tmp1 + ((float)tmp2) / ((float)temperature->t_cnt_per_c)) / 2.0;
}
