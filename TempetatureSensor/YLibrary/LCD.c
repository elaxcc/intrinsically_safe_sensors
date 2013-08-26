#include "LCD.h"

#include "YDebug.h"

#include "ff.h"

#include "LPC24xx.h"

#include "stdlib.h"

#include "WD.h"

#define C_GLCD_H_SIZE           800
#define C_GLCD_H_PULSE          4
#define C_GLCD_H_FRONT_PORCH    17
#define C_GLCD_H_BACK_PORCH     33

#define C_GLCD_V_SIZE           480
#define C_GLCD_V_PULSE          10
#define C_GLCD_V_FRONT_PORCH    22
#define C_GLCD_V_BACK_PORCH     23

#define C_GLCD_PWR_ENA_DIS_DLY  10000

const uint8_t loading_text[] = {0x13, 0x2C, 0x2F, 0x3D, 0x40, 0x34, 0x37, 0x2C}; // size 8

void LoadMainScreenToMemory(void);
void LoadFontToMemory(void);
void LoadSelectToMemory(void);
void LoadSensorCfgScreenToMemory(void);
void LoadThresholdKeyboardToMemory(void);
void LoadAlphabetKeyboardToMemory(void);
void LoadKeyboardSelecterToMemory(void);
void LoadGraphicsScreenToMemory(void);
void CleanScreen(void);
void DrawText(uint32_t color, uint8_t* text, uint32_t text_size, uint32_t x_pos, uint32_t y_pos);

void LCDinit(void)
{
	volatile uint32_t i;
	
	// init SDRAM
	SDRAMinit();
	#ifdef YDEBUG
	if (SDRAMtest() == YTRUE)
	{
		YDebugSendMessage("SDRAM init ok\r\n", 15);
	}
	else
	{
		YDebugSendMessage("SDRAM init faild\r\n", 18);
	}
	#endif // YDEBUG
	
	CleanScreen();
	
	// Assign pin
	PINSEL0 &= 0xFFF000FF;
	PINSEL0 |= 0x00055500;
	PINMODE0&= 0xFFFC00FF;
	PINMODE0|= 0x0002AA00;
	PINSEL3 &= 0xF00000FF;
	PINSEL3 |= 0x05555500;
	PINMODE3&= 0xF00000FF;
	PINMODE3|= 0x0AAAAA00;
	PINSEL4 &= 0xF0300000;
	PINSEL4 |= 0x054FFFFF;
	PINMODE4&= 0xF0300000;
	PINMODE4|= 0x0A8AAAAA;
	PINSEL9 &= 0xF0FFFFFF;
	PINSEL9 |= 0x0A000000;
	PINMODE9&= 0xF0FFFFFF;
	PINMODE9|= 0x0A000000;
	PINSEL11&= 0xFFFFFFF0;
	PINSEL11|= 0x0000000F;
	
	// Init GLCD cotroller
	PCONP |= 1 << 20;      // enable LCD controller clock
	CRSR_CTRL = 0; // Disable cursor
	LCD_CTRL = 0;   // disable GLCD controller
	LCD_CTRL |= 5 << 1;  //2bpp // 24 bpp

	LCD_CTRL |= 1 << 5;   // TFT panel
	//LCD_CTRL_bit.LcdDual=0;   // single panel
	//LCD_CTRL_bit.BGR   = 0;   // normal output
	//LCD_CTRL_bit.BEBO  = 0;   // little endian byte order
	//LCD_CTRL_bit.BEPO  = 0;   // little endian pix order
	//LCD_CTRL_bit.LcdPwr= 0;   // disable power

	// LCD_CTRL_bit.WATERMARK = 0;                                                   // added
	// init pixel clock
	LCD_CFG |= 2;
	//LCD_POL |= 1 << 26;   // bypass inrenal clk divider
	//LCD_POL_bit.CLKSEL = 0;   // clock source for the LCD block is HCLK
	LCD_POL |= 1 << 11;   // LCDFP pin is active LOW and inactive HIGH
	LCD_POL |= 1 << 12;   // LCDLP pin is active LOW and inactive HIGH
	// LCD_POL_bit.IPC    = 1;   // data is driven out into the LCD on the falling edge
	LCD_POL &= ~1;   // data is driven out into the LCD on the rising edge
	
	LCD_POL |= 2;

	//LCD_POL_bit.PCD_HI = 0;  //  //
	//LCD_POL_bit.PCD_LO = 0;  //

	//LCD_POL_bit.IOE    = 0;   // active high
	LCD_POL |= (C_GLCD_H_SIZE-1) << 16;
	// init Horizontal Timing
	LCD_TIMH |= (C_GLCD_H_BACK_PORCH - 1) << 24;
	LCD_TIMH |= (C_GLCD_H_FRONT_PORCH - 1) << 16;
	LCD_TIMH |= (C_GLCD_H_PULSE - 1) << 8;
	LCD_TIMH |= ((C_GLCD_H_SIZE/16) - 1) << 2;
	// init Vertical Timing
	LCD_TIMV |= C_GLCD_V_BACK_PORCH << 24;
	LCD_TIMV |= C_GLCD_V_FRONT_PORCH << 16;
	LCD_TIMV |= C_GLCD_V_PULSE << 10;
	LCD_TIMV |= C_GLCD_V_SIZE - 1;
	// Frame Base Address doubleword aligned
	LCD_UPBASE = SDRAM_BASE_ADDR;//LCD_VRAM_BASE_ADDR & ~7UL ;
	
	LCD_CTRL |= 1;
	for(i = C_GLCD_PWR_ENA_DIS_DLY; i; i--);
	LCD_CTRL |= 1 << 11;   // enable power
	
	LoadFontToMemory();
	
	DrawText(0x00FFFFFF, (uint8_t *) loading_text, 8, 320, 222);
	
	WatchDogReset();
	LoadMainScreenToMemory();
	WatchDogReset();
	LoadSelectToMemory();
	WatchDogReset();
	LoadSensorCfgScreenToMemory();
	WatchDogReset();
	LoadThresholdKeyboardToMemory();
	WatchDogReset();
	LoadAlphabetKeyboardToMemory();
	WatchDogReset();
	LoadKeyboardSelecterToMemory();
	WatchDogReset();
	LoadGraphicsScreenToMemory();
	WatchDogReset();
}

void LCDOn(void)
{
	LCD_CTRL |= 1 << 11;
}

void LCDOff(void)
{
	LCD_CTRL &= ~(1 << 11);
}

void LoadMainScreenToMemory(void)
{
	FIL file;
	FRESULT f_result;
	uint32_t pic_part;
	uint32_t iter1, iter2;
	uint32_t bytes_read;
	uint32_t read_val, rotate_val;
	
	f_result = f_open(&file, "system/main.bmp", FA_OPEN_EXISTING | FA_READ);
	if (f_result == FR_OK)
	{
		f_lseek(&file, 54); // skip bmp header
		
		// load data
		for (iter1 = 480; iter1 != 0; --iter1)
		{
			for (iter2 = 0; iter2 < 800; ++iter2)
			{
				f_read(&file, (void*) &pic_part, 3, &bytes_read);
				
				read_val = (uint32_t) (0x00FFFFFF & pic_part);
				rotate_val = (read_val & 0x00FF0000) >> 16;
				rotate_val |= (read_val & 0x0000FF00);
				rotate_val |= (read_val & 0x000000FF) << 16;
				rotate_val |= 0xFF000000;
				
				SDRAM_VAL(MAIN_PICTURE_ADDR + ((iter1 - 1) * 800 + iter2) * 4) = rotate_val;
			}
		}
		
		f_close(&file);
	}
}

void LoadFontToMemory(void)
{
	FIL file;
	FRESULT f_result;
	uint32_t pic_part;
	uint32_t iter1, iter2;
	uint32_t bytes_read;
	
	f_result = f_open(&file, "system/font.bmp", FA_OPEN_EXISTING | FA_READ);
	if (f_result == FR_OK)
	{
		f_lseek(&file, 54); // skip bmp header
		
		// load data
		for (iter1 = 35; iter1 != 0; --iter1)
		{
			for (iter2 = 0; iter2 < FONT_FILE_X_SIZE; ++iter2)
			{
				f_read(&file, (void*) &pic_part, 3, &bytes_read);
				
				SDRAM_VAL(FONT_ADDR + ((iter1 - 1) * FONT_FILE_X_SIZE + iter2) * 4) = (uint32_t) (0x00FFFFFF & pic_part);
			}
			f_lseek(&file, f_tell(&file) + 3);
		}
		
		f_close(&file);
	}
}

void LoadSelectToMemory(void)
{
	FIL file;
	FRESULT f_result;
	uint32_t pic_part;
	uint32_t iter1, iter2;
	uint32_t bytes_read;
	uint32_t read_val, rotate_val;
	
	f_result = f_open(&file, "system/select.bmp", FA_OPEN_EXISTING | FA_READ);
	if (f_result == FR_OK)
	{
		f_lseek(&file, 56); // skip bmp header
		
		// load data
		for (iter1 = SELECT_Y_SIZE; iter1 != 0; --iter1)
		{
			for (iter2 = 0; iter2 < SELECT_X_SIZE; ++iter2)
			{
				f_read(&file, (void*) &pic_part, 3, &bytes_read);
				
				read_val = (uint32_t) (0x00FFFFFF & pic_part);
				rotate_val = (read_val & 0x00FF0000) >> 16;
				rotate_val |= (read_val & 0x0000FF00);
				rotate_val |= (read_val & 0x000000FF) << 16;
				rotate_val |= 0xFF000000;
				
				SDRAM_VAL(SELECT_ADDR + ((iter1 - 1) * SELECT_X_SIZE + iter2) * 4) = rotate_val;
			}
		}
		
		f_close(&file);
	}
}

void LoadSensorCfgScreenToMemory(void)
{
	FIL file;
	FRESULT f_result;
	uint32_t pic_part;
	uint32_t iter1, iter2;
	uint32_t bytes_read;
	uint32_t read_val, rotate_val;
	
	f_result = f_open(&file, "system/sensor_cfg.bmp", FA_OPEN_EXISTING | FA_READ);
	if (f_result == FR_OK)
	{
		f_lseek(&file, 54); // skip bmp header
		
		// load data
		for (iter1 = 480; iter1 != 0; --iter1)
		{
			for (iter2 = 0; iter2 < 800; ++iter2)
			{
				f_read(&file, (void*) &pic_part, 3, &bytes_read);
				
				read_val = (uint32_t) (0x00FFFFFF & pic_part);
				rotate_val = (read_val & 0x00FF0000) >> 16;
				rotate_val |= (read_val & 0x0000FF00);
				rotate_val |= (read_val & 0x000000FF) << 16;
				rotate_val |= 0xFF000000;
				
				SDRAM_VAL(SENSOR_CFG_ADDR + ((iter1 - 1) * 800 + iter2) * 4) = rotate_val;
			}
		}
		
		f_close(&file);
	}
}

void LoadThresholdKeyboardToMemory(void)
{
	FIL file;
	FRESULT f_result;
	uint32_t pic_part;
	uint32_t iter1, iter2;
	uint32_t bytes_read;
	uint32_t read_val, rotate_val;
	
	f_result = f_open(&file, "system/threshold_keyboard.bmp", FA_OPEN_EXISTING | FA_READ);
	if (f_result == FR_OK)
	{
		f_lseek(&file, 56); // skip bmp header
		
		// load data
		for (iter1 = THRESHOLD_KEYBOARD_Y_SIZE; iter1 != 0; --iter1)
		{
			for (iter2 = 0; iter2 < THRESHOLD_KEYBOARD_X_SIZE; ++iter2)
			{
				f_read(&file, (void*) &pic_part, 3, &bytes_read);
				
				read_val = (uint32_t) (0x00FFFFFF & pic_part);
				rotate_val = (read_val & 0x00FF0000) >> 16;
				rotate_val |= (read_val & 0x0000FF00);
				rotate_val |= (read_val & 0x000000FF) << 16;
				rotate_val |= 0xFF000000;
				
				SDRAM_VAL(THRESHOLD_KEYBOARD_ADDR + ((iter1 - 1) * THRESHOLD_KEYBOARD_X_SIZE + iter2) * 4) =
					rotate_val;
			}
			f_lseek(&file, f_tell(&file) + 3);
		}
		
		f_close(&file);
	}
}

void LoadAlphabetKeyboardToMemory(void)
{
	FIL file;
	FRESULT f_result;
	uint32_t pic_part;
	uint32_t iter1, iter2;
	uint32_t bytes_read;
	uint32_t read_val, rotate_val;
	
	f_result = f_open(&file, "system/alphabet_keyboard.bmp", FA_OPEN_EXISTING | FA_READ);
	if (f_result == FR_OK)
	{
		f_lseek(&file, 56); // skip bmp header
		
		// load data
		for (iter1 = ALPHABET_KEYBOARD_Y_SIZE; iter1 != 0; --iter1)
		{
			for (iter2 = 0; iter2 < ALPHABET_KEYBOARD_X_SIZE; ++iter2)
			{
				f_read(&file, (void*) &pic_part, 3, &bytes_read);
				
				read_val = (uint32_t) (0x00FFFFFF & pic_part);
				rotate_val = (read_val & 0x00FF0000) >> 16;
				rotate_val |= (read_val & 0x0000FF00);
				rotate_val |= (read_val & 0x000000FF) << 16;
				rotate_val |= 0xFF000000;
				
				SDRAM_VAL(ALPHABET_KEYBOARD_ADDR + ((iter1 - 1) * ALPHABET_KEYBOARD_X_SIZE + iter2) * 4) =
					rotate_val;
			}
			f_lseek(&file, f_tell(&file) + 3);
		}
		
		f_close(&file);
	}
}

void LoadKeyboardSelecterToMemory(void)
{
	FIL file;
	FRESULT f_result;
	uint32_t pic_part;
	uint32_t iter1, iter2;
	uint32_t bytes_read;
	uint32_t read_val, rotate_val;
	
	f_result = f_open(&file, "system/keyboard_selecter.bmp", FA_OPEN_EXISTING | FA_READ);
	if (f_result == FR_OK)
	{
		f_lseek(&file, 56); // skip bmp header
		
		// load data
		for (iter1 = KEYBOARD_SELECTER_Y_SIZE; iter1 != 0; --iter1)
		{
			for (iter2 = 0; iter2 < KEYBOARD_SELECTER_X_SIZE; ++iter2)
			{
				f_read(&file, (void*) &pic_part, 3, &bytes_read);
				
				read_val = (uint32_t) (0x00FFFFFF & pic_part);
				rotate_val = (read_val & 0x00FF0000) >> 16;
				rotate_val |= (read_val & 0x0000FF00);
				rotate_val |= (read_val & 0x000000FF) << 16;
				rotate_val |= 0xFF000000;
				
				SDRAM_VAL(KEYBOARD_SELECTER_ADDR + ((iter1 - 1) * KEYBOARD_SELECTER_X_SIZE + iter2) * 4) = rotate_val;
			}
			f_lseek(&file, f_tell(&file) + 3);
		}
		
		f_close(&file);
	}
}

void LoadGraphicsScreenToMemory(void)
{
	FIL file;
	FRESULT f_result;
	uint32_t pic_part;
	uint32_t iter1, iter2;
	uint32_t bytes_read;
	uint32_t read_val, rotate_val;
	
	f_result = f_open(&file, "system/graphic.bmp", FA_OPEN_EXISTING | FA_READ);
	if (f_result == FR_OK)
	{
		f_lseek(&file, 54); // skip bmp header
		
		// load data
		for (iter1 = 480; iter1 != 0; --iter1)
		{
			for (iter2 = 0; iter2 < 800; ++iter2)
			{
				f_read(&file, (void*) &pic_part, 3, &bytes_read);
				
				read_val = (uint32_t) (0x00FFFFFF & pic_part);
				rotate_val = (read_val & 0x00FF0000) >> 16;
				rotate_val |= (read_val & 0x0000FF00);
				rotate_val |= (read_val & 0x000000FF) << 16;
				rotate_val |= 0xFF000000;
				
				SDRAM_VAL(GRAPHICS_ADDR + ((iter1 - 1) * 800 + iter2) * 4) = rotate_val;
			}
		}
		
		f_close(&file);
	}
}

void CleanScreen(void)
{
	uint32_t iter;
	
	for (iter = 0; iter < 384000; ++iter)
	{
		WatchDogReset();
		SDRAM_VAL(iter * 4) = 0x00000000;
	}
}

void DrawPicture(unsigned int picture, unsigned int x_size, unsigned int y_size,
	unsigned int x_pos, unsigned int y_pos)
{
	uint32_t iter1, iter2, pic_iter = 0;
	
	for (iter1 = y_pos; iter1 < y_pos + y_size; ++iter1)
	{
		for (iter2 = x_pos; iter2 < x_pos + x_size; ++iter2)
		{
			SDRAM_VAL((iter1 * 800 + iter2) * 4) = SDRAM_VAL(picture + pic_iter * 4);
			pic_iter++;
		}
	}
}

void DrawText(uint32_t color, uint8_t* text, uint32_t text_size, uint32_t x_pos, uint32_t y_pos)
{
	uint32_t iter, iter1, iter2, pict_iter_x, pict_iter_y;
	uint32_t symbol_pos_in_font_in_text;
	uint32_t symbol_pos_in_screen = 0;
	uint32_t delta = 0;
	
	for(iter = 0; iter < text_size; ++iter)
	{
		symbol_pos_in_font_in_text = text[iter];
		
		if (!(text[iter] >= 0x0B && text[iter] <= 0x2B))
		{
			delta = 13;
		}
		else if (text[iter] == 0x12 || text[iter] == 0x24
			|| text[iter] == 0x25 || text[iter] == 0x27
			|| text[iter] == 0x2A)
		{
			delta = 0;
		}
		else
		{
			delta = 10;
		}
		
		pict_iter_y = 0;
		for (iter1 = y_pos; iter1 < y_pos + FONT_Y_SIZE; ++iter1)
		{
			pict_iter_x = 0;
			for (iter2 = x_pos; iter2 < x_pos + FONT_X_SIZE; ++iter2)
			{
				if (SDRAM_VAL(FONT_ADDR + (pict_iter_y * FONT_FILE_X_SIZE + pict_iter_x
						+ symbol_pos_in_font_in_text * FONT_X_SIZE) * 4) != 0x00FFFFFF)
				{
					SDRAM_VAL((iter1 * 800 + iter2 + symbol_pos_in_screen) * 4) = color;
				}
				
				pict_iter_x++;
			}
			pict_iter_y++;
		}
		
		symbol_pos_in_screen += FONT_X_SIZE - delta;
	}
}

void DrawWidthText(uint32_t color, uint8_t* text, uint32_t text_size, uint32_t x_pos, uint32_t y_pos)
{
	uint32_t iter, iter1, iter2, pict_iter_x, pict_iter_y;
	uint32_t symbol_pos_in_font_in_text;
	uint32_t symbol_pos_in_screen = 0;
	
	for(iter = 0; iter < text_size; ++iter)
	{
		symbol_pos_in_font_in_text = text[iter];
		
		pict_iter_y = 0;
		for (iter1 = y_pos; iter1 < y_pos + FONT_Y_SIZE; ++iter1)
		{
			pict_iter_x = 0;
			for (iter2 = x_pos; iter2 < x_pos + FONT_X_SIZE; ++iter2)
			{
				if (SDRAM_VAL(FONT_ADDR + (pict_iter_y * FONT_FILE_X_SIZE + pict_iter_x
						+ symbol_pos_in_font_in_text * FONT_X_SIZE) * 4) != 0x00FFFFFF)
				{
					SDRAM_VAL((iter1 * 800 + iter2 + symbol_pos_in_screen * (FONT_X_SIZE)) * 4) = color;
				}
				
				pict_iter_x++;
			}
			pict_iter_y++;
		}
		
		symbol_pos_in_screen++;
	}
}

void ConvertNumToText(int32_t value, uint8_t buffer[], uint32_t* text_size)
{
	uint8_t digitals_cnt = 0;
	uint8_t digitals_buffer[10];
	uint32_t iter = 0;
	
	if (value == 0)
	{
		buffer[0] = 0x01;
		*text_size = 1;
		return;
	}
	
	if (value < 0)
	{
		digitals_buffer[0] = 0x50;
		digitals_cnt++;
	}
	
	// prepare value
	while(value)
	{
		digitals_buffer[digitals_cnt] = (uint8_t)(value % 10);
		value /= 10;
		digitals_cnt++;
	}
	
	*text_size = digitals_cnt;
	
	for (iter = 0; digitals_cnt > 0; --digitals_cnt, ++iter)
	{
		buffer[iter] = digitals_buffer[digitals_cnt - 1] + 1;
	}
}

void ConvertTextToNum(int32_t *val, uint8_t* buffer, uint32_t size)
{
	uint32_t i = 0;
	uint32_t digit = 10;
	YBOOL below_zero = YFALSE;
	
	*val = 0;
	
	if (buffer[i] == 0x50)
	{
		below_zero = YTRUE;
	}

	// integer part
	for (; i < size; ++i)
	{
		*val *= digit;
		*val += (int32_t) (buffer[i] - 0x01);
	}

	if (below_zero == YTRUE)
	{
		*val *= -1;
	}
}

void ConvertFloatToText(float val, uint8_t* buffer, uint32_t* size)
{
	int32_t integer_part = (int32_t) val;
	int32_t fractional_part;
	unsigned cnt = 0;

	*size = 0;

	if (integer_part < 0)
	{
		buffer[0] = 0x50;
		cnt++;
		*size += 1;
		integer_part *= -1;
		val *= -1.0f;
	}

	fractional_part = (int) ((val - integer_part) * 10);

	ConvertNumToText(integer_part, &buffer[cnt], &cnt);
	*size += cnt;

	buffer[*size] = 0x4F;
	*size += 1;

	ConvertNumToText(fractional_part, &buffer[*size], &cnt);
	*size += cnt;
}

void ConvertTextToFloat(uint8_t* text, uint32_t size, float* val, YBOOL below_zero)
{
	uint32_t integer = 0;
	uint32_t fractional = 0;
	uint32_t i, j = 0;
	uint32_t digit = 10;
	uint32_t fractional_divider = 1;
	float tmp_fractional;

	// integer part
	for (i = j; text[i] != 0x4F && i < size; ++i)
	{
		integer *= digit;
		integer += (uint32_t) (text[i] - 0x01);
	}

	// fractional part
	for (j = i + 1; j < size; ++j)
	{
		fractional *= digit;
		fractional += (uint32_t) (text[j] - 0x01);
		fractional_divider *= 10;
	}

	tmp_fractional = ((float) (fractional)) / ((float) fractional_divider);
	*val = (float) integer + tmp_fractional;
	if (below_zero == YTRUE)
	{
		*val *= -1;
	}
}

void DrawFloat(uint32_t color, float value, uint32_t x_pos, uint32_t y_pos)
{
	uint8_t buffer[10];
	uint32_t buf_size;
	uint32_t iter;
	
	ConvertFloatToText(value, buffer, &buf_size);
	
	// find point symbol position
	for (iter = 0; iter < buf_size; ++iter)
	{
		if (buffer[iter] == 0x4F)
		{
			break;
		}
	}
	
	// drwa integer part
	DrawText(color, buffer, iter, x_pos, y_pos);
	// draw point
	DrawText(color, &buffer[iter], 1, x_pos + (FONT_X_SIZE - 15)* (iter + 1) - 10, y_pos);
	iter++;
	// draw fractional part
	DrawText(color, &buffer[iter], buf_size - iter, x_pos + (FONT_X_SIZE -15) * (iter + 1) -30, y_pos);
}

void DrawPoint(uint32_t color, uint32_t x, uint32_t y)
{
	SDRAM_VAL((y * 800 + x) * 4) = color;
	SDRAM_VAL((y * 800 + x + 1) * 4) = color;
	SDRAM_VAL(((y + 1) * 800 + x) * 4) = color;
	SDRAM_VAL(((y + 1) * 800 + x + 1) * 4) = color;
}

void DrawLine(uint32_t color, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2)
{
	const int deltaX = abs(x2 - x1);
	const int deltaY = abs(y2 - y1);
	const int signX = x1 < x2 ? 1 : -1;
	const int signY = y1 < y2 ? 1 : -1;
	
	int error = deltaX - deltaY;
	int error2;
	
	DrawPoint(color, x2, y2);
	while(x1 != x2 || y1 != y2)
	{
		DrawPoint(color, x1, y1);
		error2 = error * 2;
		
		if(error2 > -deltaY)
		{
			error -= deltaY;
			x1 += signX;
		}
		if(error2 < deltaX)
		{
			error += deltaX;
			y1 += signY;
		}
	}
}
