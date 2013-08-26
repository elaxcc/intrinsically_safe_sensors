#ifndef __SEBSORLIST_H_
#define __SEBSORLIST_H_

#include "YBool.h"
#include "ff.h"
#include "string.h"

#define SENSOR_OK 0
#define SENSOR_LOST 1
#define SENSOR_NOT_PLACED 2
#define SENSOR_TEMPERATURE_ABOVE 3

struct Sensor
{
	uint8_t id_[8];
	
	char* place_;
	uint32_t place_size_;
	
	float threshold_;
	YBOOL valid_threshold_;
	
	float cuttenr_temperature_;

	FIL file_;
	uint32_t status_;

	struct Sensor* next_;
	struct Sensor* prev_;
};

extern struct Sensor *list_begin_, *list_end_;

void SensorAdd(uint8_t* id);
YBOOL SensorFind(uint8_t* id, struct Sensor** sensor);
YBOOL SensorRemove(uint8_t* id);
uint32_t SensorsAmount(void);
void SensorUseSDCard(YBOOL use);
void SensorSaveThresholdToFile(struct Sensor* sensor);
void SensorSavePlaceToFile(struct Sensor* sensor);

struct Date
{
	int sec;
	int min;
	int hour;
	int day_of_month;
	int month;
	int year;
};
void GetCurrentDate(struct Date *date);
void SetCurrentDate(struct Date date);

enum FileType
{
	FileTypeCfg,
	FileTypeData
};

void CreateFileName(uint8_t* id, char* file_name, enum FileType file_type);

#endif // __SEBSORLIST_H_
