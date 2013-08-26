#include "YFileFifo.h"

struct YFileFifo
{
	uint32_t head_ptr_;
	uint32_t tail_ptr_;
	uint32_t size_;
};

void YFileFifoPush(struct Sensor *sensor, struct Temperature *temperature)
{
	FRESULT f_result;
	char file_name[19];
	struct Date date;
	struct YFileFifo fifo_info;
	uint32_t bytes;
	uint32_t next_position_for_tail_elm;
	uint32_t old_tail;
	
	CreateFileName(sensor->id_, file_name, FileTypeData);
	f_result = f_open(&(sensor->file_), file_name, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
	if (f_result != FR_OK)
	{
		f_result = f_open(&(sensor->file_), file_name, FA_CREATE_NEW | FA_READ | FA_WRITE);
		fifo_info.head_ptr_ = 0;
		fifo_info.tail_ptr_ = 1;
		fifo_info.size_ = FIFO_SIZE;
	}
	else
	{
		f_read(&(sensor->file_), (void *) (&fifo_info), sizeof(struct YFileFifo), &bytes);
	}
	
	next_position_for_tail_elm = fifo_info.tail_ptr_ + 1;
	old_tail = fifo_info.tail_ptr_;
	if (next_position_for_tail_elm == fifo_info.size_)
	{
		next_position_for_tail_elm = 0;
	}
	if (next_position_for_tail_elm == fifo_info.head_ptr_)
	{
		fifo_info.head_ptr_++;
		if (fifo_info.head_ptr_ == fifo_info.size_)
		{
			fifo_info.head_ptr_ = 0;
		}
	}
	fifo_info.tail_ptr_ = next_position_for_tail_elm;
	
	// save FIFO info
	f_lseek(&(sensor->file_), 0);
	f_write(&(sensor->file_), (void*) (&fifo_info), sizeof(struct YFileFifo), &bytes);
	// save point
	f_lseek(&(sensor->file_), sizeof(struct YFileFifo)
		+ (sizeof(struct Date) + sizeof(struct Temperature)) * old_tail);
	GetCurrentDate(&date);
	f_write(&(sensor->file_), (void*) (&date), sizeof(struct Date), &bytes);
	f_write(&(sensor->file_), (void*) (temperature), sizeof(struct Temperature), &bytes);
	
	f_close(&(sensor->file_));
}

uint32_t YFileFifoElementsAmount(struct Sensor *sensor)
{
	FRESULT f_result;
	char file_name[19];
	struct YFileFifo fifo_info;
	uint32_t bytes;
	
	CreateFileName(sensor->id_, file_name, FileTypeData);
	f_result = f_open(&(sensor->file_), file_name, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
	if (f_result != FR_OK)
	{
		return 0;
	}

	f_read(&(sensor->file_), (void *) (&fifo_info), sizeof(struct YFileFifo), &bytes);
	f_close(&(sensor->file_));
	
	if (fifo_info.tail_ptr_ > fifo_info.head_ptr_)
	{
		return fifo_info.tail_ptr_ - fifo_info.head_ptr_ - 1;
	}
	return FIFO_SIZE - 2;
}

void YFileFifoGetElement(FIL *file, struct Date *date,
	struct Temperature *temperature, uint32_t elem_num)
{
	struct YFileFifo fifo_info;
	uint32_t read_head, tmp;
	FRESULT f_result;
	
	f_lseek(file, 0);
	f_result =f_read(file, (void *) (&fifo_info), sizeof(struct YFileFifo), &tmp);
	if (f_result != FR_OK)
	{
		return;
	}
	read_head = fifo_info.head_ptr_ + elem_num;
	if (read_head >= FIFO_SIZE)
	{
		read_head -= FIFO_SIZE;
	}
	f_result = f_lseek(file, sizeof(struct YFileFifo) + (sizeof(struct Date) + sizeof(struct Temperature)) * read_head);
	if (f_result != FR_OK)
	{
		return;
	}
	f_result = f_read(file, (void *) date, sizeof(struct Date), &tmp);
	if (f_result != FR_OK)
	{
		return;
	}
	f_result = f_read(file, (void *) temperature, sizeof(struct Temperature), &tmp);
	if (f_result != FR_OK)
	{
		return;
	}
}
