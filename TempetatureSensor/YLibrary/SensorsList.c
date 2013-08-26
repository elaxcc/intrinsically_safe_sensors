#include "SensorsList.h"

#include "stdlib.h"
#include "YDebug.h"
#include "LPC23xx.h"
#include "ff.h"

uint32_t sensors_amount_ = 0;
struct Sensor *list_begin_ = NULL, *list_end_ = NULL;

YBOOL use_sd_card = YFALSE;

void CreateFileName(uint8_t* id, char* file_name, enum FileType file_type)
{
	uint32_t iter, id_iter, iter_file_name = 0;
	uint8_t buf[2];
	
	for (id_iter = 0; id_iter < 8; ++ id_iter)
	{
		buf[0] = id[id_iter] >> 4;
		buf[1] = id[id_iter] & 0x0F;
		
		for(iter = 0; iter < 2; iter++)
		{
			if((buf[iter] >= 0x00) && (buf[iter] <= 0x09))
			{
				buf[iter] += 0x30;
			}
			else if ((buf[iter] >= 0x0A) && (buf[iter] <= 0x0F))
			{
				buf[iter] += 0x37;
			}
			file_name[iter_file_name] = buf[iter];
			iter_file_name++;
		}
	}
	
	file_name[iter_file_name] = '.';
	file_name[iter_file_name + 1] = file_type == FileTypeCfg ? 't' : 'd';
	file_name[iter_file_name + 2] = '\0';
}

void CreateNewSensorInfoFile(struct Sensor* sensor, char* file_name)
{
	uint32_t bytes;
	static const char unknown_place[10] = {0x19, 0x31, 0x35, 0x34, 0x2E, 0x31, 0x3E, 0x3F, 0x3A, 0x3B};
	FRESULT f_result;
	
	sensor->place_size_ = 10;
	sensor->place_ = (char*) malloc(sizeof(char) * sensor->place_size_);
	memcpy(sensor->place_, unknown_place, sensor->place_size_ );
	sensor->threshold_ = 0;
	sensor->valid_threshold_ = YFALSE;
	
	f_result = f_open(&(sensor->file_), file_name, FA_CREATE_NEW | FA_READ | FA_WRITE);
	if (f_result != FR_OK)
	{
		YDebugSendMessage("Can't create file\r\n", 19);
		return;
	}
		
	// save sensor id to file
	f_result = f_write(&(sensor->file_), sensor->id_, 8, &bytes);
	
	// save threshold
	f_write(&(sensor->file_), (void*) &(sensor->valid_threshold_), 4, &bytes);
	f_write(&(sensor->file_), (void*) &(sensor->threshold_), 4, &bytes);
	
	// save sensor place lenght
	f_write(&(sensor->file_), (void*) &(sensor->place_size_), 4, &bytes);
	// save sensor place text
	f_write(&(sensor->file_), sensor->place_, sensor->place_size_, &bytes);
	
	f_close(&(sensor->file_));
}

void LoadSensorInfoFormFile(struct Sensor* sensor)
{
	uint32_t iter;
	YBOOL valid_id = YTRUE;
	
	FRESULT f_result;
	char file_name[19];
	char file_sensor_id[8];
	uint32_t bytes;
	
	// create file name
	CreateFileName(sensor->id_, file_name, FileTypeCfg);
	f_result = f_open(&(sensor->file_), file_name, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
	if (f_result != FR_OK)
	{
		// info-file don't exist, maybe it is new sensor,
		// create file for new sensor and set sensor status SENSOR_NOT_PLACED
		
		CreateNewSensorInfoFile(sensor, file_name);
		
		sensor->status_ = SENSOR_NOT_PLACED;
	}
	else
	{
		// sensor file exist, load his info
		
		// read sensor id and check it
		f_read(&(sensor->file_), file_sensor_id, 8, &bytes);
		for (iter = 0; iter < 8; ++iter)
		{
			if (sensor->id_[iter] != file_sensor_id[iter])
			{
				valid_id = YFALSE;
				break;
			}
		}
		if (valid_id == YFALSE)
		{
			// file invalid for this sensor, delete it and create new
			f_close(&(sensor->file_));
			f_unlink(file_name);
			CreateNewSensorInfoFile(sensor, file_name);
		}
		else
		{
			// load threshold
			f_read(&(sensor->file_), (void*) &(sensor->valid_threshold_), 4, &bytes);
			f_read(&(sensor->file_), (void*) &(sensor->threshold_), 4, &bytes);
			
			// load sensor place size
			f_read(&(sensor->file_), (void*) &(sensor->place_size_), 4, &bytes);
			// load sensor place
			sensor->place_ = (char*) malloc(sizeof(char) * sensor->place_size_);
			f_read(&(sensor->file_), sensor->place_, sensor->place_size_, &bytes);
			
			sensor->status_ = SENSOR_OK;
			
			f_close(&(sensor->file_));
		}
	}
}

void SensorSaveThresholdToFile(struct Sensor* sensor)
{
	char file_name[19];
	uint32_t bytes;
	
	// create file name
	CreateFileName(sensor->id_, file_name, FileTypeCfg);
	f_open(&(sensor->file_), file_name, FA_OPEN_EXISTING | FA_WRITE);
	
	f_lseek(&(sensor->file_), 8);
	
	f_write(&(sensor->file_), (void*) &(sensor->valid_threshold_), 4, &bytes);
	f_write(&(sensor->file_), (void*) &(sensor->threshold_), 4, &bytes);
	
	f_close(&(sensor->file_));
}

void SensorSavePlaceToFile(struct Sensor* sensor)
{
	char file_name[19];
	uint32_t bytes;
	
	// create file name
	CreateFileName(sensor->id_, file_name, FileTypeCfg);
	f_open(&(sensor->file_), file_name, FA_OPEN_EXISTING | FA_WRITE);
	
	f_lseek(&(sensor->file_), 16);
	
	f_write(&(sensor->file_), (void*) &(sensor->place_size_), 4, &bytes);
	f_write(&(sensor->file_), sensor->place_, sensor->place_size_, &bytes);
	
	f_close(&(sensor->file_));
}

void SensorSaveStatusToFile(struct Sensor* sensor)
{
	
}

void SensorAdd(uint8_t* id)
{
	struct Sensor *new_sensor;

	sensors_amount_++;

	if (list_begin_ == NULL)
	{
		list_begin_ = (struct Sensor*) malloc(sizeof(struct Sensor));

		list_begin_->next_ = NULL;
		list_begin_->prev_ = NULL;

		// save sensor id
		memcpy(list_begin_->id_, id, sizeof(uint8_t) * 8);
		// load sensor info from file
		LoadSensorInfoFormFile(list_begin_);

		list_end_ = list_begin_;
		list_end_->next_ = NULL;
		list_end_->prev_ = NULL;
		
		return;
	}

	new_sensor = (struct Sensor*) malloc(sizeof(struct Sensor));

	list_end_->next_ = new_sensor;
	new_sensor->next_ = NULL;
	new_sensor->prev_ = list_end_;

	memcpy(new_sensor->id_, id, sizeof(uint8_t) * 8);
	// load sensor info from file
	LoadSensorInfoFormFile(new_sensor);

	list_end_ = new_sensor;
}

YBOOL SensorFind(uint8_t* id, struct Sensor** sensor)
{
	struct Sensor *device_iter = list_begin_;
	uint32_t id_iter;
	YBOOL device_found;

	for(; device_iter != NULL; device_iter = device_iter->next_)
	{
		device_found = YTRUE;
		for (id_iter = 0; id_iter < 8; ++id_iter)
		{
			if (id[id_iter] != device_iter->id_[id_iter])
			{
				device_found = YFALSE;
				break;
			}
		}
		if (device_found == YTRUE)
		{
			(*sensor) = device_iter;
			return YTRUE;
		}
	}
	return YFALSE;
}

YBOOL SensorRemove(uint8_t* id)
{
	struct Sensor* found_device;
	struct Sensor* found_device_prev;

	if (SensorFind(id, &found_device) == YTRUE)
	{
		if (found_device->prev_ == NULL)
		{
			// delete first
			found_device->next_->prev_ = NULL;
			list_begin_ = found_device->next_;
		}
		else if (found_device->next_ == NULL)
		{
			// delete last
			found_device->prev_->next_ = NULL;
			list_end_ = found_device->prev_;
		}
		else
		{
			// between first and last
			found_device_prev = found_device->prev_;
			found_device->prev_->next_ = found_device->next_;
			found_device->next_->prev_ = found_device_prev;
		}

		// close file
		free(found_device->place_);
		free(found_device);

		sensors_amount_--;

		return YTRUE;
	}
	return YFALSE;
}

uint32_t SensorsAmount(void)
{
	return sensors_amount_;
}

void SensorUseSDCard(YBOOL use)
{
	use_sd_card = use;
}

void GetCurrentDate(struct Date *date)
{
	date->sec = RTC_SEC;
	date->min = RTC_MIN;
	date->hour = RTC_HOUR;
	date->day_of_month = RTC_DOM;
	date->month = RTC_MONTH;
	date->year = RTC_YEAR;
}

void SetCurrentDate(struct Date date)
{
	RTC_SEC = date.sec;
	RTC_MIN = date.min;
	RTC_HOUR = date.hour;
	RTC_DOM = date.day_of_month;
	RTC_MONTH = date.month;
	RTC_YEAR = date.year;
}
