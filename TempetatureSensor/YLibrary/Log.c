#include "Log.h"

#include "YDebug.h"

#include "ff.h"

#include "FreeRTOS.h"
#include "task.h"

void LogSave(uint8_t type)
{
	FRESULT f_result;
	FIL file;
	uint32_t cnt, tmp;
	struct Date date;
	
	f_result = f_open(&file, "system/log.f", FA_OPEN_EXISTING | FA_WRITE | FA_READ);
	if (f_result != FR_OK)
	{
		f_open(&file, "system/log.f", FA_CREATE_NEW | FA_WRITE);
		cnt = 1;
	}
	else
	{
		f_read(&file, (void *) &cnt, 4, &tmp);
		cnt++;
	}
	
	f_lseek(&file, 0);
	// save log file size 
	f_write(&file, (void *) &cnt, 4, &tmp);
	vTaskDelay(100);
	// go to end of file
	f_lseek(&file, f_size(&file));
	// save log data
	GetCurrentDate(&date);
	f_write(&file, (void *) (void *) (&date), sizeof(struct Date), &tmp);
	vTaskDelay(100);
	// save log type
	f_write(&file, (void *) (void *) (&type), 1, &tmp);
	vTaskDelay(100);
	
	f_close(&file);
}

uint32_t LogAmount(void)
{
	FRESULT f_result;
	FIL file;
	uint32_t cnt, tmp;
	
	f_result = f_open(&file, "system/log.f", FA_OPEN_EXISTING | FA_READ);
	if (f_result != FR_OK)
	{
		return 0;
	}
	else
	{
		f_read(&file, (void *) &cnt, 4, &tmp);
		return cnt;
	}
}

YBOOL LogRead(uint32_t index, uint8_t *log_type, struct Date *date)
{
	FRESULT f_result;
	FIL file;
	uint32_t cnt, tmp;
	
	f_result = f_open(&file, "system/log.f", FA_OPEN_EXISTING | FA_READ);
	if (f_result != FR_OK)
	{
		return YFALSE;
	}

	f_read(&file, (void *) &cnt, 4, &tmp);
	if (cnt < index)
	{
		f_close(&file);
		return YFALSE;
	}
	
	f_lseek(&file, 4 + (sizeof(struct Date) + sizeof(uint8_t)) * index);
	
	f_read(&file, (void *) (void *) date, sizeof(struct Date), &tmp);
	f_read(&file, (void *) (void *) log_type, 1, &tmp);
	
	f_close(&file);
	
	return YTRUE;
}

void EraseLog(void)
{
	f_unlink("system/log.f");
}
