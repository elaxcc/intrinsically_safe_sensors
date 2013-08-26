/* Standard includes. */
#include <stdlib.h>

#include "LPC23xx.H"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#define YDELAY_F_CPU 72000000

#include "YDebug.h"
#include "YDelay.h"
#include "YOneWire.h"
#include "SensorsList.h"
#include "SDRAM.h"
#include "LCD.h"
#include "YFileFifo.h"
#include "Log.h"
#include "WD.h"

#include "diskio.h"
#include "ff.h"

#include "stdlib.h"

#define Fcclk 72000000
#define Fpclk (Fcclk / 4)

xSemaphoreHandle xSemaphore = NULL;
uint32_t semaphore_timeout = 1000000;

DSTATUS d_status;
YBOOL have_devices = YFALSE;

FRESULT f_result;
FATFS fat_fs;
YBOOL sd_card_inited = YFALSE;

int32_t current_selected_sensor_num = 0;
int8_t current_selected_cfg_part = 0;
int8_t current_selected_keyboard_key_x = 0;
int8_t current_selected_keyboard_key_y = 0;
int32_t current_string_lenght = 0;

#define MAIN_SCREEN 0
#define SENSOR_CFG_SCREEN 1
#define THRESHOLD_KEYBOARD 2
#define ALPHABET_KEYBOARD 3
#define TIME_EDITOR 4
#define GRAPHIC 5
#define PASSWORD 6
#define CHANGE_PASSWORD 7
#define LOG_SCREEN 8
uint32_t current_screen = MAIN_SCREEN;
uint32_t next_screen = MAIN_SCREEN;

#define KEYS_BUFFER_SIZE 50
uint8_t keyboard_entered_keys_buffer[KEYS_BUFFER_SIZE];
uint8_t entered_kyes_cnt = 0;
YBOOL is_below_zero = YFALSE;
YBOOL point_kye_is_pressed = YFALSE;

const int8_t max_sensor_draw = 7;
int8_t current_sensor_page_num = 0;
int8_t current_sensor_page_size = 0;
int8_t page_cnt = 0;
YBOOL first_page_init = YTRUE;

YBOOL is_login_on = YFALSE;
struct Password
{
	uint8_t *pass;
	uint8_t pass_size;
} password;

const uint32_t log_page_size = 9;
int32_t current_log_page_num = 0;
int32_t log_page_cnt = 0;

const uint8_t main_wnd_sensor_finding_title[] = {0x1B, 0x3B, 0x35, 0x3E, 0x37, 0x00, 0x30, 0x2C, 0x3F,
	0x44, 0x35, 0x37, 0x3B, 0x2E}; // size 13
const uint8_t main_wnd_title[] = {0x1E, 0x31, 0x39, 0x3C, 0x31, 0x3D, 0x2C, 0x3F, 0x40, 0x3D, 0x2C}; // size 11
const uint8_t main_wnd_find_text[] = {0x19, 0x2C, 0x36, 0x30, 0x31, 0x3A, 0x3B, 0x53, 0x00}; // size 9
const uint8_t cfg_wnd_title[] = {0x16, 0x3B, 0x3A, 0x41, 0x35, 0x2F, 0x40, 0x3D, 0x2C, 0x43, 0x35, 0x4C,
	0x00, 0x30, 0x2C, 0x3F, 0x44, 0x35, 0x37, 0x2C}; // size 20
const uint8_t cfg_wnd_sensor_text[] = {0x0F, 0x2C, 0x3F, 0x44, 0x35, 0x37, 0x00}; //size 7
const uint8_t cfg_wnd_sensor_threshold[] = {0x1F, 0x3E, 0x3F, 0x2C, 0x2E, 0x37, 0x2C, 0x53}; // size 8
const uint8_t cfg_wnd_sensor_info[] = {0x1A, 0x3C, 0x35, 0x3E, 0x2C, 0x3A, 0x35, 0x31, 0x53};

uint8_t alphabet_keyboard_wnd_keys_low_names[] =
	{
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x050, 0x4F, 0x55,
		0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
		0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
		0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x53, 0x4E, 0x51, 0x52, 0x57, 0x58
	};
uint8_t alphabet_keyboard_wnd_keys_high_names[] =
	{
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x050, 0x4F, 0x55,
		0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
		0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x53, 0x4E, 0x51, 0x52, 0x57, 0x54
	};
uint8_t* current_alphabet_keyboard = (uint8_t *) alphabet_keyboard_wnd_keys_low_names;

const uint8_t threshold_keyboard_wnd_keys_names[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
	0x56, 0x4F, 0x55}; // size 13

const uint8_t error_mes_wrong_tempareture[] = 
	{0x0D, 0x3B, 0x34, 0x39, 0x3B, 0x33, 0x3A, 0x3B, 0x00, 0x3B, 0x3F, 0x00,
	0x50, 0x06, 0x06, 0x00, 0x30, 0x3B, 0x00, 0x02, 0x03, 0x06, 0x00, 0x4D, 0x1D}; // size 25

const uint8_t time_editor_wnd_text[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x55}; // size 11

const uint8_t change_password_text[] = {0x50, 0x3E, 0x39, 0x31, 0x3A, 0x35, 0x3F,
	0x49, 0x00, 0x3C, 0x2C, 0x3D, 0x3B, 0x38, 0x49, 0x4E, 0x06, 0x50,
	0x02, 0x03, 0x00, 0x3E, 0x35, 0x39, 0x2E, 0x3B, 0x38, 0x3B, 0x2E}; // size 15

const uint8_t wrong_password_text[] = {0x19, 0x31, 0x2E, 0x31, 0x3D, 0x3A, 0x48,
	0x36, 0x00, 0x3C, 0x2C, 0x3D, 0x3B, 0x38, 0x49}; // size 29

const uint8_t graphic_text[] = {0x0E, 0x3D, 0x2C, 0x41, 0x35, 0x37};

const uint8_t log_wnd_main_text[] = {0x12, 0x40, 0x3D, 0x3A, 0x2C, 0x38,
	0x00, 0x3E, 0x3B, 0x2D, 0x48, 0x3F, 0x35, 0x36};

const uint8_t enter_pass_text[] = {0x0D, 0x2E, 0x31, 0x30, 0x35, 0x3F, 0x31,
	0x00, 0x3C, 0x2C, 0x3D, 0x3B, 0x38, 0x49}; // size 14

const uint8_t log_change_threshold_text[] = {0x3E, 0x39, 0x31, 0x3A, 0x2C, 0x00, 0x40,
	0x3E, 0x3F, 0x2C, 0x2E, 0x37, 0x35}; // size 13

const uint8_t log_change_place_text[] = {0x3E, 0x39, 0x31, 0x3A, 0x2C, 0x00,
	0x3C, 0x3B, 0x38, 0x3B, 0x33, 0x4F}; // size 12

const uint8_t log_sensor_lost_text[] = {0x30, 0x2C, 0x3F, 0x44, 0x35, 0x37,
	0x00, 0x3C, 0x3B, 0x3F, 0x31, 0x3D, 0x4C, 0x3A}; // 14

const uint8_t log_threshold_above_text[] = {0x2E, 0x48, 0x3E, 0x3B, 0x37, 0x2C, 0x4C,
	0x00, 0x3F, 0x31, 0x39, 0x3C, 0x4F}; // size 13

const uint32_t BackGroundMainColor = 0x00FFFFFF;

uint8_t default_pass[] = {0x02, 0x03, 0x04, 0x05, 0x06}; // size 5
uint32_t default_pass_size = 5;
void LoadPassword(void)
{
	FIL file;
	FRESULT f_result;
	uint32_t tmp;
	
	f_result = f_open(&file, "system/pass.cfg", FA_OPEN_EXISTING | FA_READ);
	if (f_result != FR_OK)
	{
		// set default password
		password.pass_size = 5;
		password.pass = (uint8_t *) malloc(sizeof(uint8_t) * password.pass_size);
		memcpy(password.pass, default_pass, 5);
		
		f_open(&file, "system/pass.cfg", FA_CREATE_NEW | FA_WRITE);
		f_write(&file, (void *) &default_pass_size, 1, &tmp);
		f_write(&file, (void *) default_pass, 5, &tmp);
	}
	else
	{
		f_read(&file, (void *) &password.pass_size, 1, &tmp);
		password.pass = (uint8_t *) malloc(sizeof(uint8_t) * password.pass_size);
		f_read(&file, (void *) password.pass, password.pass_size, &tmp);
	}
	
	f_close(&file);
}

void KeysInit()
{
	IODIR0 |= (1 << 28);
}

#define KyesSetAccessPinLow() IOCLR0 |= (1 << 28)
#define KyesSetAccessPinHigh() IOSET0 |= (1 << 28)

void DateDraw(uint32_t color, struct Date date, uint32_t x, uint32_t y)
{
	uint32_t shift = 0;
	
	uint8_t digits_buffer[5];
	uint32_t digits_cnt;
	
	// day of month
	if (date.day_of_month < 10)
	{
		digits_buffer[0] = 0x01;
		DrawText(color, digits_buffer, 1, x, y);
		shift += 30;
	}
	ConvertNumToText(date.day_of_month, digits_buffer, &digits_cnt);
	DrawText(color, digits_buffer, digits_cnt, x + shift, y);
	shift += digits_cnt * 30 - 5;
	digits_buffer[0] = 0x4F;
	DrawText(color, digits_buffer, 1, x + shift, y);
	shift += 5;
	// month
	if (date.month < 10)
	{
		digits_buffer[0] = 0x01;
		DrawText(color, digits_buffer, 1, x + shift, y);
		shift += 30;
	}
	ConvertNumToText(date.month, digits_buffer, &digits_cnt);
	DrawText(color, digits_buffer, digits_cnt, x + shift, y); 
	shift += digits_cnt * 30 - 5;
	digits_buffer[0] = 0x4F;
	DrawText(color, digits_buffer, 1, x + shift, y);
	shift += 5;
	// year
	ConvertNumToText(date.year, digits_buffer, &digits_cnt);
	DrawText(color, digits_buffer, digits_cnt, x + shift, y); 
	shift += (digits_cnt - 1) * 30 + 10;
	digits_buffer[0] = 0x50;
	DrawText(color, digits_buffer, 1, x - 10 + shift, y);
	shift += 10;
	// hour
	if (date.hour < 10)
	{
		digits_buffer[0] = 0x01;
		DrawText(color, digits_buffer, 1, x + shift, y);
		shift += 30;
	}
	ConvertNumToText(date.hour, digits_buffer, &digits_cnt);
	DrawText(color, digits_buffer, digits_cnt, x + shift, y); 
	shift += digits_cnt * 30;
	digits_buffer[0] = 0x53;
	DrawText(color, digits_buffer, 1, x - 10 + shift, y);
	shift += 7;
	// minutes
	if (date.min < 10)
	{
		digits_buffer[0] = 0x01;
		DrawText(color, digits_buffer, 1, x + shift, y);
		shift += 30;
	}
	ConvertNumToText(date.min, digits_buffer, &digits_cnt);
	DrawText(color, digits_buffer, digits_cnt, x + shift, y); 
}

void MainWndDraw()
{
	uint32_t iter1;
	uint32_t devices_amount = 0;
	struct Sensor* sensor_iter;
	
	uint8_t digits_buffer[5];
	uint32_t digits_cnt;
	
	uint32_t text_color;
	
	struct Date date;
	
	WatchDogReset();
	
	if (current_screen != MAIN_SCREEN)
	{
		return;
	}
	
	devices_amount = SensorsAmount();
	
	// draw main screen
	DrawPicture((unsigned int) MAIN_PICTURE_ADDR, 800, 480, 0, 0);
	// draw main screen text
	DrawText(0xFFFFFFFF, (uint8_t *) main_wnd_title, 11, 7, 7);
	// draw sensor amount
	DrawText(0x00000000, (uint8_t *) main_wnd_find_text, 9, 7, 440);
	ConvertNumToText(devices_amount, digits_buffer, &digits_cnt);
	DrawText(0x00000000, digits_buffer, digits_cnt, 180, 440);
	// draw data
	GetCurrentDate(&date);
	DateDraw(0xFFFFFFFF, date, 435, 7);
	
	if (devices_amount != 0)
	{
		// draw selecter screen
		DrawPicture((unsigned int) SELECT_ADDR, 800, 35, 0,
			MAIN_PICTURE_UP_STEP + current_selected_sensor_num * SELECT_Y_SIZE);
		
		// find current sensor
		for (iter1 = 0, sensor_iter = list_begin_;
			iter1 < current_sensor_page_num * max_sensor_draw && sensor_iter != NULL;
			++iter1)
		{
			sensor_iter = sensor_iter->next_;
		}

		for (iter1 = 0; sensor_iter != NULL && iter1 < max_sensor_draw; sensor_iter = sensor_iter->next_, ++iter1)
		{
			WatchDogReset();
			
			if (sensor_iter->status_ == SENSOR_LOST)
			{
				text_color = 0x005F5F5F;
			}
			else if (sensor_iter->status_ == SENSOR_TEMPERATURE_ABOVE)
			{
				text_color = 0x000000FF;
			}
			else
			{
				text_color = 0x00000000;
			}
			
			// draw sensor number
			ConvertNumToText(iter1 + 1 + current_sensor_page_num * max_sensor_draw, digits_buffer, &digits_cnt);
			DrawText(text_color, digits_buffer, digits_cnt, 0,
				MAIN_PICTURE_UP_STEP + iter1 * MAIN_PICTURE_LINE_Y_STEP);
			// draw sensor place
			DrawText(text_color, (uint8_t *) sensor_iter->place_, sensor_iter->place_size_, 50,
				MAIN_PICTURE_UP_STEP + iter1 * MAIN_PICTURE_LINE_Y_STEP);
			// draw threshold
			if (sensor_iter->valid_threshold_ == YFALSE)
			{
				digits_buffer[0] = 0x50;
				DrawText(text_color, digits_buffer, 1, 575,
					MAIN_PICTURE_UP_STEP + iter1 * MAIN_PICTURE_LINE_Y_STEP);
			}
			else
			{
				DrawFloat(text_color, sensor_iter->threshold_, 520,
					MAIN_PICTURE_UP_STEP + iter1 * MAIN_PICTURE_LINE_Y_STEP);
			}
			// draw current temperature
			if (sensor_iter->status_ == SENSOR_LOST)
			{
				digits_buffer[0] = 0x50;
				DrawText(text_color, digits_buffer, 1, 715,
					MAIN_PICTURE_UP_STEP + iter1 * MAIN_PICTURE_LINE_Y_STEP);
			}
			else
			{
				DrawFloat(text_color, sensor_iter->cuttenr_temperature_, 660,
					MAIN_PICTURE_UP_STEP + iter1 * MAIN_PICTURE_LINE_Y_STEP);
			}
		}
	}
}

void CfgWndDraw()
{
	uint8_t digits_buffer[5];
	uint32_t digits_cnt;
	struct Sensor* sensor_ptr = list_begin_;
	uint32_t iter;
	
	WatchDogReset();
	
	// draw main screen
	DrawPicture((unsigned int) SENSOR_CFG_ADDR, 800, 480, 0, 0);
	// draw main screen text
	DrawText(0xFFFFFFFF, (uint8_t *) cfg_wnd_title, 20, 7, 7);
	
	// draw selecter
	DrawPicture((unsigned int) SELECT_ADDR, 800, 35, 0, 165 + current_selected_cfg_part * 90);
	
	// draw sensor number
	DrawText(0x000000FF, (uint8_t *)cfg_wnd_sensor_text, 7, 40, 80);
	ConvertNumToText(current_selected_sensor_num + 1 + current_sensor_page_num * max_sensor_draw, digits_buffer, &digits_cnt);
	DrawText(0x000000FF, digits_buffer, digits_cnt, 180, 80);
	
	for (iter = 0; iter < current_selected_sensor_num + current_sensor_page_num * max_sensor_draw; ++iter)
	{
		sensor_ptr = sensor_ptr->next_;
	}
	
	// draw threshold
	DrawText(0x00000000, (uint8_t *) cfg_wnd_sensor_threshold, 8, 10, 165);
	if (sensor_ptr->valid_threshold_ == YFALSE)
	{
		digits_buffer[0] = 0x50;
		DrawText(0x00000000, digits_buffer, 1, 185, 165);
	}
	else
	{
		DrawFloat(0x00000000, sensor_ptr->threshold_, 185, 165);
	}
	
	// draw sensor place
	DrawText(0x00000000, (uint8_t *) cfg_wnd_sensor_info, 9, 10, 255);
	DrawText(0x00000000, (uint8_t *) sensor_ptr->place_, sensor_ptr->place_size_, 10, 295);
	
	// draw "F"
	digits_buffer[0] = 0x0E;
	digits_buffer[1] = 0x50;
	DrawText(0x00000000, (uint8_t *) &(digits_buffer[0]), 1, 7, 440);
	DrawText(0x00000000, (uint8_t *) &(digits_buffer[1]), 1, 7, 435);
	// draw help text
	DrawText(0x00000000, (uint8_t *) change_password_text, 29, 32, 440);
}

void ThresholdKeyboardWndDraw(void)
{
	uint32_t text_pos = 188;
	uint8_t minus = 0x50;
	
	WatchDogReset();
	
	// draw main picture
	DrawPicture((unsigned int) THRESHOLD_KEYBOARD_ADDR,
		THRESHOLD_KEYBOARD_X_SIZE, THRESHOLD_KEYBOARD_Y_SIZE, 170, 203);
	
	// draw selecter
	DrawPicture((unsigned int) KEYBOARD_SELECTER_ADDR, 35, 35,
		171 + 35 * current_selected_keyboard_key_x, 242);
	
	// draw keys's names
	DrawWidthText(0x00000000, (uint8_t *) threshold_keyboard_wnd_keys_names, 13, 172, 241);
	
	// draw minus
	if (is_below_zero == YTRUE)
	{
		DrawText(0x00000000, &minus, 1, text_pos, 203);
		text_pos += 35;
	}
	
	// draw entered string
	DrawWidthText(0x00000000, keyboard_entered_keys_buffer, entered_kyes_cnt, text_pos, 203);
}

void AlphabetKeyboardWndDraw(void)
{
	WatchDogReset();
	
	// draw main picture
	DrawPicture((unsigned int) ALPHABET_KEYBOARD_ADDR,
		ALPHABET_KEYBOARD_X_SIZE, ALPHABET_KEYBOARD_Y_SIZE, 170, 146);
	
	// draw selecter
	DrawPicture((unsigned int) KEYBOARD_SELECTER_ADDR, 35, 35,
		172 + 35 * current_selected_keyboard_key_x,
		185 + 37 * current_selected_keyboard_key_y);
	
	// draw keys's names
	DrawWidthText(0x00000000, (uint8_t *) (current_alphabet_keyboard),
		13, 172, 185);
	DrawWidthText(0x00000000, (uint8_t *) (current_alphabet_keyboard + 13 * sizeof(uint8_t)),
		13, 172, 221);
	DrawWidthText(0x00000000, (uint8_t *) (current_alphabet_keyboard + 26 * sizeof(uint8_t)),
		13, 172, 258);
	DrawWidthText(0x00000000, (uint8_t *) (current_alphabet_keyboard + 39 * sizeof(uint8_t)),
		13, 172, 294);
	
	// draw entered text
	DrawText(0x00000000, (uint8_t *) keyboard_entered_keys_buffer, entered_kyes_cnt, 172, 148);
}

void TimeEditorWndDraw(void)
{
	uint8_t buf;
	
	WatchDogReset();
	
	// draw main picture
	DrawPicture((unsigned int) THRESHOLD_KEYBOARD_ADDR,
		THRESHOLD_KEYBOARD_X_SIZE, THRESHOLD_KEYBOARD_Y_SIZE, 170, 203);
	
	// draw selecter
	DrawPicture((unsigned int) KEYBOARD_SELECTER_ADDR, 35, 35,
		206 + 35 * current_selected_keyboard_key_x, 242);
	
	// draw keys's names
	DrawWidthText(0x00000000, (uint8_t *) time_editor_wnd_text, 11, 207, 241);
	
	// draw editor segments
	buf = 0x4F;
	DrawWidthText(0x00000000, (uint8_t *) &buf, 1, 238, 203);
	buf = 0x4F;
	DrawWidthText(0x00000000, (uint8_t *) &buf, 1, 308, 203);
	buf = 0x50;
	DrawWidthText(0x00000000, (uint8_t *) &buf, 1, 448, 203);
	buf = 0x53;
	DrawWidthText(0x00000000, (uint8_t *) &buf, 1, 538, 203);
	
	// draw entered string
	if (entered_kyes_cnt <= 8)
	{
		DrawWidthText(0x00000000, keyboard_entered_keys_buffer, entered_kyes_cnt, 168, 203);
	}
	else
	{
		DrawWidthText(0x00000000, keyboard_entered_keys_buffer, 8, 168, 203);
		DrawWidthText(0x00000000, &(keyboard_entered_keys_buffer[8]), entered_kyes_cnt - 8, 483, 203);
	}
}

void GraphicWndDraw(void)
{
	int i;
	uint32_t point_cnt;
	struct Date date;
	struct Temperature temp;
	uint8_t iter;
	struct Sensor *sensor_ptr;
	int32_t temp_int;
	FRESULT f_result;
	char file_name[19];
	uint32_t x_begin;
	uint32_t y_begin;
	uint32_t x_end;
	uint32_t y_end;
	
	WatchDogReset();
	
	// draw main screen
	DrawPicture((unsigned int) GRAPHICS_ADDR, 800, 480, 0, 0);
	
	// draw main text
	DrawText(0xFFFFFFFF, (uint8_t *) main_wnd_title, 11, 7, 7);
	// draw data
	GetCurrentDate(&date);
	DateDraw(0xFFFFFFFF, date, 435, 7);
	// draw graphic
	for (iter = 0, sensor_ptr = list_begin_;
		iter < current_selected_sensor_num + current_sensor_page_num * max_sensor_draw;
		++iter)
	{
		sensor_ptr = sensor_ptr->next_;
	}
	
	point_cnt = YFileFifoElementsAmount(sensor_ptr);
	CreateFileName(sensor_ptr->id_, file_name, FileTypeData);
	f_result = f_open(&(sensor_ptr->file_), file_name, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
	if (f_result != FR_OK)
	{
		return;
	}
	
	YFileFifoGetElement(&(sensor_ptr->file_), &date, &temp, 0);
	temp_int = (uint32_t) (ConvertTemperatureToFloat(&temp));
	if (temp_int > 125)
	{
		temp_int = 125;
	}
	else if (temp_int < -55)
	{
		temp_int = -55;
	}
	x_begin = 0;
	y_begin = temp_int;
	
	for (i = 1; i < point_cnt; ++i)
	{
		WatchDogReset();
		
		YFileFifoGetElement(&(sensor_ptr->file_), &date, &temp, i);
		temp_int = (uint32_t) (ConvertTemperatureToFloat(&temp));
		if (temp_int > 125)
		{
			temp_int = 125;
		}
		else if (temp_int < -55)
		{
			temp_int = -55;
		}
		x_end = i;
		y_end = temp_int;
		
		DrawLine(0x000000FF, 52 + x_begin, 352 - y_begin, 52 + x_end, 352 - y_end);
		
		x_begin = x_end;
		y_begin = y_end;
	}
	f_close(&(sensor_ptr->file_));
}

void PassWndDraw(void)
{
	uint8_t i, hyphen = 0x50, ok_button[] = {0x3B, 0x37};
	
	WatchDogReset();
	
	// draw main picture
	DrawPicture((unsigned int) THRESHOLD_KEYBOARD_ADDR,
		THRESHOLD_KEYBOARD_X_SIZE, THRESHOLD_KEYBOARD_Y_SIZE, 170, 203);
	
	// draw selecter
	DrawPicture((unsigned int) KEYBOARD_SELECTER_ADDR, 35, 35,
		206 + 35 * current_selected_keyboard_key_x, 242);
	
	// draw keys's names
	DrawWidthText(0x00000000, (uint8_t *) time_editor_wnd_text, 10, 207, 241);
	// draw "Ok" button name
	DrawWidthText(0x00000000, (uint8_t *) &(ok_button[0]), 1, 548, 241);
	DrawWidthText(0x00000000, (uint8_t *) &(ok_button[1]), 1, 567, 241);

	// draw entered text
	for (i = 0; i < entered_kyes_cnt; ++i)
	{
		DrawText(0x00000000, (uint8_t *) &hyphen, 1, 168 + 35 * i, 203);
	}
	
	// draw help text
	DrawText(BackGroundMainColor, (uint8_t *) wrong_password_text, 15, 450, 440);
	DrawText(0x00FF0000, (uint8_t *) enter_pass_text, 14, 450, 440);
}

void ChangePassWndDraw(void)
{
	uint8_t ok_button[] = {0x3B, 0x37};
	
	WatchDogReset();
	
	// draw main picture
	DrawPicture((unsigned int) THRESHOLD_KEYBOARD_ADDR,
		THRESHOLD_KEYBOARD_X_SIZE, THRESHOLD_KEYBOARD_Y_SIZE, 170, 203);
	
	// draw selecter
	DrawPicture((unsigned int) KEYBOARD_SELECTER_ADDR, 35, 35,
		206 + 35 * current_selected_keyboard_key_x, 242);
	
	// draw keys's names
	DrawWidthText(0x00000000, (uint8_t *) time_editor_wnd_text, 10, 207, 241);
	// draw "Ok" button name
	DrawWidthText(0x00000000, (uint8_t *) &(ok_button[0]), 1, 548, 241);
	DrawWidthText(0x00000000, (uint8_t *) &(ok_button[1]), 1, 567, 241);

	// draw entered text
	DrawText(0x00000000, (uint8_t *) &(keyboard_entered_keys_buffer[0]), entered_kyes_cnt, 168, 203);
}

void LogWndDraw(void)
{
	uint8_t digits_buffer[5];
	uint8_t simbol;
	uint32_t tmp, index, log_num, text_size;
	struct Date date;
	uint8_t log_type;
	
	WatchDogReset();
	
	// draw main screen
	DrawPicture((unsigned int) SENSOR_CFG_ADDR, 800, 480, 0, 0);
	// draw main screen text
	DrawText(0xFFFFFFFF, (uint8_t *) log_wnd_main_text, 14, 7, 7);
	
	tmp = LogAmount();
	log_page_cnt = tmp / log_page_size;
	if (tmp - log_page_cnt != 0)
	{
		log_page_cnt++;
	}
	
	for (log_num = 0, index = log_page_size * current_log_page_num;
		index < tmp && log_num < log_page_size; ++index, ++log_num)
	{
		WatchDogReset();
		
		// draw log number
		ConvertNumToText(index + 1, digits_buffer, &text_size);
		DrawText(0x00000000, (uint8_t *) digits_buffer, text_size, 0,
			74 + log_num * MAIN_PICTURE_LINE_Y_STEP);
		simbol = 0x4F;
		DrawText(0x00000000, (uint8_t *) &simbol, 1, text_size * 25,
			74 + log_num * MAIN_PICTURE_LINE_Y_STEP);
		
		LogRead(index, &log_type, &date);
		
		// draw log data
		DateDraw(0x00000000, date, (text_size + 1) * 25 - 10, 74 + log_num * MAIN_PICTURE_LINE_Y_STEP);
		
		// draw log type
		if (log_type == LOG_CHANGE_THRESHOLD)
		{
			DrawText(0x00000000, (uint8_t *) &log_change_threshold_text, 13, text_size * 25 + 375,
				74 + log_num * MAIN_PICTURE_LINE_Y_STEP);
		}
		else if (log_type == LOG_CHANGE_PLACE)
		{
			DrawText(0x00000000, (uint8_t *) &log_change_place_text, 12, text_size * 25 + 375,
				74 + log_num * MAIN_PICTURE_LINE_Y_STEP);
		}
		else if (log_type == LOG_THRESHOLD_ABOVE)
		{
			DrawText(0x00000000, (uint8_t *) &log_threshold_above_text, 13, text_size * 25 + 375,
				74 + log_num * MAIN_PICTURE_LINE_Y_STEP);
		}
		else if (log_type == LOG_SENSOR_LOST)
		{
			DrawText(0x00000000, (uint8_t *) &log_sensor_lost_text, 14, text_size * 25 + 375,
				74 + log_num * MAIN_PICTURE_LINE_Y_STEP);
		}
	}
}

void TestTask1(void *pvParameters)
{
	uint32_t iter1;
	YBOOL valid_temperature;
	struct Temperature temperature;
	uint32_t devices_amount = 0;
	struct Sensor* sensor_iter;
	
	#ifdef YDEBUG
	uint32_t iter2;
	#endif
	#if 0
	YBOOL elem_date_valid = YFALSE;
	struct Data elem_data;
	struct Temperature elem_temp;
	uint32_t i;
	uint32_t file_size;
	float file_temp;
	#endif
	
	for (;;)
	{
		if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
		{
			WatchDogReset();
			xSemaphoreGive(xSemaphore);
		}
		
		YDebugSendMessage("Sensor task\r\n", 13);
		
		YOneWireFindDevices();
		
		YDebugSendMessage("After search\r\n", 14);
		
		if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
		{
			devices_amount = SensorsAmount();
			xSemaphoreGive(xSemaphore);
		}
		if (first_page_init == YTRUE)
		{
			if (devices_amount > max_sensor_draw)
			{
				current_sensor_page_size = max_sensor_draw;
			}
			else
			{
				current_sensor_page_size = SensorsAmount();
			}
			first_page_init = YFALSE;
		}
		page_cnt = devices_amount / max_sensor_draw;
		if ((devices_amount % max_sensor_draw) > 0)
		{
			page_cnt++;
		}
		
		if (devices_amount != 0)
		{
			have_devices = YTRUE;
			
			YDebugSendByte((uint8_t) devices_amount);
			YDebugSendMessage(" devices found\r\n", 16);

			for (iter1 = 0, sensor_iter = list_begin_; sensor_iter != NULL; sensor_iter = sensor_iter->next_, ++iter1)
			{
				#ifdef YDEBUG
				YDebugSendByte((uint8_t) iter1);
				YDebugSendMessage(": ", 2);
				for (iter2 = 0; iter2 < 8; ++iter2)
				{
					YDebugSendByte(sensor_iter->id_[iter2]);
					YDebugSendMessage(" ", 1);
				}
				YDebugSendMessage(" | ", 3);
				YDebugSendMessage(sensor_iter->place_, sensor_iter->place_size_);
				YDebugSendMessage(" | ", 3);
				YDebugSendMessage("threshold: ", 11);
				if (sensor_iter->valid_threshold_ == YTRUE)
				{
					YDebugSendMessage("valid ", 6);
				}
				else
				{
					YDebugSendMessage("invalid ", 8);
				}
				YDebugSendFloat((float) sensor_iter->threshold_);
				YDebugSendMessage(" ;temperature: ", 15);
				#endif // YDEBUG
			
				valid_temperature = YOneWireReadDeviceTemperature(sensor_iter->id_, &temperature);
				sensor_iter->cuttenr_temperature_ = ConvertTemperatureToFloat(&temperature);
				if (valid_temperature == YFALSE)
				{
					YDebugSendMessage("Invalid temperature\r\n", 21);
					
					sensor_iter->status_ = SENSOR_LOST;
					if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
					{
						LogSave(LOG_SENSOR_LOST);
						xSemaphoreGive(xSemaphore);
					}
				}
				else if (sensor_iter->cuttenr_temperature_ >= sensor_iter->threshold_
					&& sensor_iter->valid_threshold_ == YTRUE && current_screen == MAIN_SCREEN)
				{
					sensor_iter->status_ = SENSOR_TEMPERATURE_ABOVE;
					if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
					{
						LogSave(LOG_THRESHOLD_ABOVE);
						xSemaphoreGive(xSemaphore);
					}
				}
				
				// save to file
				if (valid_temperature == YTRUE)
				{
					if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
					{
						YFileFifoPush(sensor_iter, &temperature);
						xSemaphoreGive(xSemaphore);
					}
				}
			}
			
			if (current_screen == MAIN_SCREEN)
			{
				if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
				{
					MainWndDraw();
					xSemaphoreGive(xSemaphore);
				}
			}
		}
		
		vTaskDelay(5000);
	}
}

void TestTask2(void *pvParameters)
{
	uint8_t key_code;
	struct Sensor* sensor_ptr;
	uint32_t iter;
	float tmp_f;
	uint8_t current_key;
	struct Date date;
	uint8_t buf;
	FIL file;
	uint32_t tmp;
	YBOOL pass_valid = YFALSE;
	FRESULT f_result;
	
	#ifdef YDEBUG
	int32_t d_iter;
	#endif
	
	for (;;)
	{
		if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
		{
			WatchDogReset();
			xSemaphoreGive(xSemaphore);
		}
		
		key_code = (IOPIN0 & (1 << 21)) ? 1 : 0;
		key_code |= (IOPIN0 & (1 << 22)) ? 2 : 0;
		key_code |= (IOPIN0 & (1 << 23)) ? 4 : 0;
		key_code |= (IOPIN0 & (1 << 24)) ? 8 : 0;
		key_code |= (IOPIN0 & (1 << 27)) ? 16 : 0;
		
		if (have_devices == YTRUE)
		{
			switch(key_code)
			{
				case 1 : // 1
					current_key = 0x02;
					goto gidit_key_end;
				case 2 : // 2
					current_key = 0x03;
					goto gidit_key_end;
				case 3 : // 3
					current_key = 0x04;
					goto gidit_key_end;
				case 4 : // 4
					current_key = 0x05;
					goto gidit_key_end;
				case 5 : // 5
					current_key = 0x06;
					goto gidit_key_end;
				case 9 : // 6
					current_key = 0x07;
					goto gidit_key_end;
				case 10 : // 7
					current_key = 0x08;
					goto gidit_key_end;
				case 11 : // 8
					current_key = 0x09;
					goto gidit_key_end;
				case 12 : // 9
					current_key = 0x0A;
					goto gidit_key_end;
				case 13 : // 0
					current_key = 0x01;
					
					gidit_key_end:
					if (current_screen == THRESHOLD_KEYBOARD)
					{
						if (entered_kyes_cnt < 9)
						{
							keyboard_entered_keys_buffer[entered_kyes_cnt] = current_key;
							entered_kyes_cnt++;
							if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
							{
								ThresholdKeyboardWndDraw();
								xSemaphoreGive(xSemaphore);
							}
						}
					}
					else if (current_screen == ALPHABET_KEYBOARD)
					{
						if (current_string_lenght > 420)
						{
							break;
						}
						
						if (!(current_key >= 0x0B && current_key <= 0x2B))
						{
							current_string_lenght += 22; // 35 - 13;
						}
						else if (current_key == 0x12 || current_key == 0x24 || current_key == 0x25
							|| current_key == 0x27 || current_key == 0x2A)
						{
							current_string_lenght += 35; // 35 - 0
						}
						else
						{
							current_string_lenght += 25; // 35 - 10
						}
						
						keyboard_entered_keys_buffer[entered_kyes_cnt] = current_key;
						entered_kyes_cnt++;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							AlphabetKeyboardWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == TIME_EDITOR)
					{
						if (entered_kyes_cnt < 12)
						{
							keyboard_entered_keys_buffer[entered_kyes_cnt] = current_key;
							entered_kyes_cnt++;
						}
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							TimeEditorWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == PASSWORD)
					{
						if (entered_kyes_cnt < 12)
						{
							keyboard_entered_keys_buffer[entered_kyes_cnt] = current_key;
							entered_kyes_cnt++;
							if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
							{
								PassWndDraw();
								xSemaphoreGive(xSemaphore);
							}
						}
					}
					else if (current_screen == CHANGE_PASSWORD)
					{
						if (entered_kyes_cnt < 12)
						{
							keyboard_entered_keys_buffer[entered_kyes_cnt] = current_key;
							entered_kyes_cnt++;
							if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
							{
								ChangePassWndDraw();
								xSemaphoreGive(xSemaphore);
							}
						}
					}
					break;
				case 25 : // ESC
				{
					YDebugSendMessage("Pressed: ESC\r\n", 14);
					
					if (current_screen == SENSOR_CFG_SCREEN)
					{
						current_screen = MAIN_SCREEN;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							MainWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == THRESHOLD_KEYBOARD)
					{
						if (entered_kyes_cnt > 0)
						{
							ConvertTextToFloat(keyboard_entered_keys_buffer, entered_kyes_cnt,
								&tmp_f, is_below_zero);
							
							if (tmp_f >= -55.0f && tmp_f <= 125)
							{
								// save threshold
								for (iter = 0, sensor_ptr = list_begin_;
									iter < current_selected_sensor_num + current_sensor_page_num * max_sensor_draw;
									++iter)
								{
									sensor_ptr = sensor_ptr->next_;
								}
								ConvertTextToFloat(keyboard_entered_keys_buffer, entered_kyes_cnt,
									&tmp_f, is_below_zero);
								sensor_ptr->threshold_ = tmp_f;
								sensor_ptr->valid_threshold_ = YTRUE;
								if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
								{
									SensorSaveThresholdToFile(sensor_ptr);
									xSemaphoreGive(xSemaphore);
								}
								
								if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
								{
									LogSave(LOG_CHANGE_THRESHOLD);
									xSemaphoreGive(xSemaphore);
								}
							}
							else
							{
								// draw message
								is_below_zero = YFALSE;
								point_kye_is_pressed = YFALSE;
								entered_kyes_cnt = 0;
								
								if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
								{
									ThresholdKeyboardWndDraw();
									xSemaphoreGive(xSemaphore);
									// draw error text
									buf = 0x0E;
									DrawText(BackGroundMainColor, (uint8_t *) &(buf), 1, 7, 440);
									buf = 0x50;
									DrawText(BackGroundMainColor, (uint8_t *) &(buf), 1, 7, 435);
									DrawText(BackGroundMainColor, (uint8_t *) change_password_text, 29, 32, 440);
									DrawText(0x000000FF, (uint8_t *) error_mes_wrong_tempareture, 25, 7, 445);
								}
								
								break;
							}
						}
						current_screen = SENSOR_CFG_SCREEN;
						is_below_zero = YFALSE;
						point_kye_is_pressed = YFALSE;
						entered_kyes_cnt = 0;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							CfgWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == ALPHABET_KEYBOARD)
					{
						if (entered_kyes_cnt > 0)
						{
							// save place
							for (iter = 0, sensor_ptr = list_begin_;
								iter < current_selected_sensor_num + current_sensor_page_num * max_sensor_draw;
								++iter)
							{
								sensor_ptr = sensor_ptr->next_;
							}
							free(sensor_ptr->place_);
							sensor_ptr->place_ = (char *) malloc(entered_kyes_cnt * sizeof(uint8_t));
							sensor_ptr->place_size_ = entered_kyes_cnt;
							memcpy((void *) sensor_ptr->place_, (void *) keyboard_entered_keys_buffer,
								entered_kyes_cnt * sizeof(uint8_t));
							if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
							{
								SensorSavePlaceToFile(sensor_ptr);
								xSemaphoreGive(xSemaphore);
							}
							
							if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
							{
								LogSave(LOG_CHANGE_PLACE);
								xSemaphoreGive(xSemaphore);
							}
						}
						
						current_screen = SENSOR_CFG_SCREEN;
						current_string_lenght = 0;
						entered_kyes_cnt = 0;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							CfgWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == TIME_EDITOR)
					{
						if (entered_kyes_cnt == 0)
						{
							entered_kyes_cnt = 0;
							current_screen = MAIN_SCREEN;
							if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
							{
								MainWndDraw();
								xSemaphoreGive(xSemaphore);
							}
						}
						else if (entered_kyes_cnt == 12)
						{
							// day of month
							ConvertTextToNum(&(date.day_of_month), &(keyboard_entered_keys_buffer[0]), 2);
							// month
							ConvertTextToNum(&(date.month), &(keyboard_entered_keys_buffer[2]), 2);
							//year
							ConvertTextToNum(&(date.year), &(keyboard_entered_keys_buffer[4]), 4);
							// hour
							ConvertTextToNum(&(date.hour), &(keyboard_entered_keys_buffer[8]), 2);
							// min
							ConvertTextToNum(&(date.min), &(keyboard_entered_keys_buffer[10]), 2);
							
							// check entered data
							if (date.day_of_month < 32 && date.month < 13
								&& date.hour < 24 && date.min < 60)
							{
								// save data
								SetCurrentDate(date);
								entered_kyes_cnt = 0;
								current_screen = MAIN_SCREEN;
								if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
								{
									MainWndDraw();
									xSemaphoreGive(xSemaphore);
								}
							}
							else
							{
								// draw editor segments
								buf = 0x4F;
								DrawWidthText(0x000000FF, (uint8_t *) &buf, 1, 238, 203);
								buf = 0x4F;
								DrawWidthText(0x000000FF, (uint8_t *) &buf, 1, 308, 203);
								buf = 0x50;
								DrawWidthText(0x000000FF, (uint8_t *) &buf, 1, 448, 203);
								buf = 0x53;
								DrawWidthText(0x000000FF, (uint8_t *) &buf, 1, 538, 203);
								
								// draw entered string
								if (entered_kyes_cnt <= 8)
								{
									DrawWidthText(0x000000FF, keyboard_entered_keys_buffer, entered_kyes_cnt, 168, 203);
								}
								else
								{
									DrawWidthText(0x000000FF, keyboard_entered_keys_buffer, 8, 168, 203);
									DrawWidthText(0x000000FF, &(keyboard_entered_keys_buffer[8]), entered_kyes_cnt - 8, 483, 203);
								}
							}
						}
					}
					else if (current_screen == GRAPHIC)
					{
						current_screen = MAIN_SCREEN;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							MainWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == PASSWORD)
					{
						entered_kyes_cnt = 0;
						current_screen = MAIN_SCREEN;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							MainWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == CHANGE_PASSWORD)
					{
						entered_kyes_cnt = 0;
						current_screen = SENSOR_CFG_SCREEN;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							CfgWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == LOG_SCREEN)
					{
						current_screen = SENSOR_CFG_SCREEN;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							CfgWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					break;
				}
				case 26 : // F
				{
					if (current_screen == THRESHOLD_KEYBOARD)
					{
						if (entered_kyes_cnt == 0)
						{
							// this condition must be above
							is_below_zero = YFALSE;
						}
						if (keyboard_entered_keys_buffer[entered_kyes_cnt - 1] == 0x4F)
						{
							point_kye_is_pressed = YFALSE;
						}
						entered_kyes_cnt = entered_kyes_cnt != 0 ? entered_kyes_cnt - 1 : 0;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							ThresholdKeyboardWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == ALPHABET_KEYBOARD)
					{
						if (!(keyboard_entered_keys_buffer[entered_kyes_cnt] >= 0x0B
							&& keyboard_entered_keys_buffer[entered_kyes_cnt] <= 0x2B))
						{
							current_string_lenght -= 22; // 35 - 13;
						}
						else if (keyboard_entered_keys_buffer[entered_kyes_cnt] == 0x12
							|| keyboard_entered_keys_buffer[entered_kyes_cnt] == 0x24
							|| keyboard_entered_keys_buffer[entered_kyes_cnt] == 0x25
							|| keyboard_entered_keys_buffer[entered_kyes_cnt] == 0x27
							|| keyboard_entered_keys_buffer[entered_kyes_cnt] == 0x2A)
						{
							current_string_lenght -= 35; // 35 - 0
						}
						else
						{
							current_string_lenght -= 25; // 35 - 10
						}
						
						if (current_string_lenght < 0)
						{
							current_string_lenght = 0;
						}
						
						entered_kyes_cnt = entered_kyes_cnt != 0 ? entered_kyes_cnt - 1 : 0;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							AlphabetKeyboardWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == MAIN_SCREEN)
					{
						entered_kyes_cnt = 0;
						current_selected_keyboard_key_x = 10;
						current_screen = PASSWORD;
						next_screen = TIME_EDITOR;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							PassWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == TIME_EDITOR)
					{
						entered_kyes_cnt = entered_kyes_cnt != 0 ? entered_kyes_cnt - 1 : 0;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							TimeEditorWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == PASSWORD)
					{
						entered_kyes_cnt = entered_kyes_cnt != 0 ? entered_kyes_cnt - 1 : 0;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							PassWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == CHANGE_PASSWORD)
					{
						entered_kyes_cnt = entered_kyes_cnt != 0 ? entered_kyes_cnt - 1 : 0;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							ChangePassWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == SENSOR_CFG_SCREEN)
					{
						entered_kyes_cnt = 0;
						current_selected_keyboard_key_x = 10;
						current_screen = CHANGE_PASSWORD;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							ChangePassWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == LOG_SCREEN)
					{
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							EraseLog();
							LogWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					break;
				}
				case 27 : // UP
				{
					YDebugSendMessage("Pressed: UP\r\n", 13);
					
					if (current_screen == MAIN_SCREEN)
					{
						current_selected_sensor_num--;
						if (current_selected_sensor_num < 0)
						{
							current_sensor_page_num--;
							if (current_sensor_page_num < 0)
							{
								current_sensor_page_num = page_cnt - 1;
								current_sensor_page_size = SensorsAmount() - max_sensor_draw * (page_cnt - 1);
							}
							else
							{
								current_sensor_page_size = max_sensor_draw;
							}
							current_selected_sensor_num = current_sensor_page_size - 1;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							MainWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == SENSOR_CFG_SCREEN)
					{
						current_selected_cfg_part--;
						if (current_selected_cfg_part < 0)
						{
							current_selected_cfg_part = 1;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							CfgWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == ALPHABET_KEYBOARD)
					{
						current_selected_keyboard_key_y--;
						if (current_selected_keyboard_key_y < 0)
						{
							current_selected_keyboard_key_y = 3;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							AlphabetKeyboardWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if(current_screen == LOG_SCREEN)
					{
						current_log_page_num--;
						if (current_log_page_num < 0)
						{
							current_log_page_num = log_page_cnt - 1;
						}
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							LogWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					
					break;
				}
				case 28 : // POINT
				{
					current_key = 0x4F;
					if (current_screen == THRESHOLD_KEYBOARD)
					{
						if (entered_kyes_cnt < 9)
						{
							if (point_kye_is_pressed == YFALSE && entered_kyes_cnt != 0)
							{
								keyboard_entered_keys_buffer[entered_kyes_cnt] = current_key;
								entered_kyes_cnt++;
								point_kye_is_pressed = YTRUE;
								if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
								{
									ThresholdKeyboardWndDraw();
									xSemaphoreGive(xSemaphore);
								}
							}
						}
					}
					else if (current_screen == ALPHABET_KEYBOARD)
					{
						if (current_string_lenght > 420)
						{
							break;
						}
						
						if (!(current_key >= 0x0B && current_key <= 0x2B))
						{
							current_string_lenght += 22; // 35 - 13;
						}
						else if (current_key == 0x12 || current_key == 0x24 || current_key == 0x25
							|| current_key == 0x27 || current_key == 0x2A)
						{
							current_string_lenght += 35; // 35 - 0
						}
						else
						{
							current_string_lenght += 25; // 35 - 10
						}
						
						keyboard_entered_keys_buffer[entered_kyes_cnt] = current_key;
						entered_kyes_cnt++;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							AlphabetKeyboardWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					break;
				}
				case 29 : // ENT
				{
					YDebugSendMessage("Pressed: ENT\r\n", 14);
					
					if (current_screen == MAIN_SCREEN)
					{
						entered_kyes_cnt = 0;
						current_selected_keyboard_key_x = 10;
						current_screen = PASSWORD;
						next_screen = SENSOR_CFG_SCREEN;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							PassWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == SENSOR_CFG_SCREEN)
					{
						if (current_selected_cfg_part == 0)
						{
							// threshold keyboard
							entered_kyes_cnt = 0;
							current_screen = THRESHOLD_KEYBOARD;
							point_kye_is_pressed = YFALSE;
							if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
							{
								ThresholdKeyboardWndDraw();
								xSemaphoreGive(xSemaphore);
							}
						}
						else
						{
							// place keyboard
							entered_kyes_cnt = 0;
							current_screen = ALPHABET_KEYBOARD;
							if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
							{
								AlphabetKeyboardWndDraw();
								xSemaphoreGive(xSemaphore);
							}
						}
					}
					else if (current_screen == THRESHOLD_KEYBOARD)
					{
						if (current_selected_keyboard_key_x == 12) // '<-' key
						{
							if (entered_kyes_cnt == 0)
							{
								// this condition must be above
								is_below_zero = YFALSE;
							}
							if (keyboard_entered_keys_buffer[entered_kyes_cnt - 1] == 0x4F)
							{
								point_kye_is_pressed = YFALSE;
							}
							entered_kyes_cnt = entered_kyes_cnt != 0 ? entered_kyes_cnt - 1 : 0;
						}
						else if (current_selected_keyboard_key_x == 11) // '.' key
						{
							if (point_kye_is_pressed == YFALSE && entered_kyes_cnt != 0)
							{
								keyboard_entered_keys_buffer[entered_kyes_cnt] = 0x4F;
								entered_kyes_cnt++;
								point_kye_is_pressed = YTRUE;
							}
						}
						else if (current_selected_keyboard_key_x == 10) // '+/-' key
						{
							is_below_zero = is_below_zero == YFALSE ? YTRUE : YFALSE;
						}
						else if (current_selected_keyboard_key_x == 0 && entered_kyes_cnt == 0)
						{
							break;
						}
						else
						{
							if (entered_kyes_cnt < 9)
							{
								keyboard_entered_keys_buffer[entered_kyes_cnt] =
									current_selected_keyboard_key_x + 1;
								entered_kyes_cnt++;
							}
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							ThresholdKeyboardWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == ALPHABET_KEYBOARD)
					{
						current_key = *(current_alphabet_keyboard
							+ current_selected_keyboard_key_y * 13
							+ current_selected_keyboard_key_x);
						
						if (current_key == 85) // '<-' key
						{
							if (!(keyboard_entered_keys_buffer[entered_kyes_cnt] >= 0x0B
								&& keyboard_entered_keys_buffer[entered_kyes_cnt] <= 0x2B))
							{
								current_string_lenght -= 22; // 35 - 13;
							}
							else if (keyboard_entered_keys_buffer[entered_kyes_cnt] == 0x12
								|| keyboard_entered_keys_buffer[entered_kyes_cnt] == 0x24
								|| keyboard_entered_keys_buffer[entered_kyes_cnt] == 0x25
								|| keyboard_entered_keys_buffer[entered_kyes_cnt] == 0x27
								|| keyboard_entered_keys_buffer[entered_kyes_cnt] == 0x2A)
							{
								current_string_lenght -= 35; // 35 - 0
							}
							else
							{
								current_string_lenght -= 25; // 35 - 10
							}
							
							if (current_string_lenght < 0)
							{
								current_string_lenght = 0;
							}
							
							entered_kyes_cnt = entered_kyes_cnt != 0 ? entered_kyes_cnt - 1 : 0;
						}
						else if (current_key == 87) // 'space' key
						{
							keyboard_entered_keys_buffer[entered_kyes_cnt] = 0x00;
							entered_kyes_cnt++;
						}
						else if (current_key== 88) // 'up' key
						{
							current_alphabet_keyboard = (uint8_t *) alphabet_keyboard_wnd_keys_high_names;
						}
						else if (current_key == 84) // 'down' key
						{
							current_alphabet_keyboard = (uint8_t *) alphabet_keyboard_wnd_keys_low_names;
						}
						else
						{
							if (current_string_lenght > 420)
							{
								break;
							}
							
							if (!(current_key >= 0x0B && current_key <= 0x2B))
							{
								current_string_lenght += 22; // 35 - 13;
							}
							else if (current_key == 0x12 || current_key == 0x24 || current_key == 0x25
								|| current_key == 0x27 || current_key == 0x2A)
							{
								current_string_lenght += 35; // 35 - 0
							}
							else
							{
								current_string_lenght += 25; // 35 - 10
							}
							
							keyboard_entered_keys_buffer[entered_kyes_cnt] = current_key;
							entered_kyes_cnt++;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							AlphabetKeyboardWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == TIME_EDITOR)
					{
						if (current_selected_keyboard_key_x == 10)
						{
							entered_kyes_cnt--;
						}
						else if (entered_kyes_cnt < 12)
						{
							keyboard_entered_keys_buffer[entered_kyes_cnt] = current_selected_keyboard_key_x + 1;
							entered_kyes_cnt++;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							TimeEditorWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == PASSWORD)
					{
						if (current_selected_keyboard_key_x == 10)
						{
							// check entered password
							#ifdef YDEBUG
							YDebugSendMessage("pass: \r\n", 8);
							for (d_iter = 0; d_iter < password.pass_size; ++d_iter)
							{
								YDebugSendByte(password.pass[d_iter]);
							}
							YDebugSendEndl();
							YDebugSendMessage("entered pass: \r\n", 16);
							for (d_iter = 0; d_iter < entered_kyes_cnt; ++d_iter)
							{
								YDebugSendByte(keyboard_entered_keys_buffer[d_iter]);
							}
							YDebugSendEndl();
							#endif
							
							if (password.pass_size == entered_kyes_cnt)
							{
								pass_valid = YTRUE;
								for (iter = 0; iter < entered_kyes_cnt; ++iter)
								{
									if (password.pass[iter] != keyboard_entered_keys_buffer[iter])
									{
										pass_valid = YFALSE;
										break;
									}
								}
								
								if (pass_valid == YTRUE)
								{
									if (next_screen == SENSOR_CFG_SCREEN)
									{
										for (iter = 0, sensor_ptr = list_begin_;
											iter < current_selected_sensor_num + current_sensor_page_num * max_sensor_draw;
											++iter)
										{
											sensor_ptr = sensor_ptr->next_;
										}
										
										if (sensor_ptr->status_ == SENSOR_LOST)
										{
											SensorRemove(sensor_ptr->id_);
											current_screen = MAIN_SCREEN;
											if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
											{
												MainWndDraw();
												xSemaphoreGive(xSemaphore);
											}
										}
										else
										{
											sensor_ptr->status_ = SENSOR_OK;
											
											current_screen = SENSOR_CFG_SCREEN;
											if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
											{
												CfgWndDraw();
												xSemaphoreGive(xSemaphore);
											}
										}
									}
									else if (next_screen == TIME_EDITOR)
									{
										entered_kyes_cnt = 0;
										current_screen = TIME_EDITOR;
										if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
										{
											TimeEditorWndDraw();
											xSemaphoreGive(xSemaphore);
										}
										DrawText(BackGroundMainColor, (uint8_t *) enter_pass_text, 14, 450, 440);
									}
									break;
								}
							}
							entered_kyes_cnt = 0;
							if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
							{
								PassWndDraw();
								xSemaphoreGive(xSemaphore);
							}
							DrawText(BackGroundMainColor, (uint8_t *) enter_pass_text, 14, 450, 440);
							DrawText(0x000000FF, (uint8_t *) wrong_password_text, 15, 450, 440);
						}
						else if (entered_kyes_cnt < 12)
						{
							keyboard_entered_keys_buffer[entered_kyes_cnt] = current_selected_keyboard_key_x + 1;
							entered_kyes_cnt++;
							if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
							{
								PassWndDraw();
								xSemaphoreGive(xSemaphore);
							}
						}
					}
					else if (current_screen == CHANGE_PASSWORD)
					{
						if (current_selected_keyboard_key_x == 10)
						{
							if (entered_kyes_cnt < 5)
							{
								// draw error message
								buf = 0x0E;
								DrawText(0x000000FF, (uint8_t *) &buf, 1, 7, 440);
								buf = 0x50;
								DrawText(0x000000FF, (uint8_t *) &buf, 1, 7, 435);
								DrawText(0x000000FF, (uint8_t *) change_password_text, 29, 32, 440);
								break;
							}
							else
							{
								// save new password
								password.pass_size = entered_kyes_cnt;
								free(password.pass);
								password.pass = (uint8_t *) malloc(sizeof(uint8_t) * entered_kyes_cnt);
								memcpy(password.pass, keyboard_entered_keys_buffer, entered_kyes_cnt);
								
								if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
								{
									f_result = f_open(&file, "system/pass.cfg", FA_OPEN_EXISTING | FA_WRITE);
									if (f_result != FR_OK)
									{
										f_result = f_open(&file, "system/pass.cfg", FA_CREATE_NEW | FA_WRITE);
									}
									f_write(&file, (void *) &password.pass_size, 1, &tmp);
									f_write(&file, (void *) password.pass, password.pass_size, &tmp);
									f_close(&file);
									xSemaphoreGive(xSemaphore);
								}
								
								current_screen = SENSOR_CFG_SCREEN;
								if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
								{
									CfgWndDraw();
									xSemaphoreGive(xSemaphore);
								}
							}
						}
						else if (entered_kyes_cnt < 12)
						{
							keyboard_entered_keys_buffer[entered_kyes_cnt] = current_selected_keyboard_key_x + 1;
							entered_kyes_cnt++;
							if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
							{
								ChangePassWndDraw();
								xSemaphoreGive(xSemaphore);
							}
						}
					}
					
					break;
				}
				case 18 : // LEFT
				{
					YDebugSendMessage("Pressed: LEFT\r\n", 15);
					
					if (current_screen == THRESHOLD_KEYBOARD)
					{
						current_selected_keyboard_key_x--;
						if (current_selected_keyboard_key_x < 0)
						{
							current_selected_keyboard_key_x = 12;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							ThresholdKeyboardWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if(current_screen == ALPHABET_KEYBOARD)
					{
						current_selected_keyboard_key_x--;
						if (current_selected_keyboard_key_x < 0)
						{
							current_selected_keyboard_key_x = 12;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							AlphabetKeyboardWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if(current_screen == TIME_EDITOR)
					{
						current_selected_keyboard_key_x--;
						if (current_selected_keyboard_key_x < 0)
						{
							current_selected_keyboard_key_x = 10;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							TimeEditorWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == GRAPHIC)
					{
						current_screen = MAIN_SCREEN;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							MainWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if(current_screen == PASSWORD)
					{
						current_selected_keyboard_key_x--;
						if (current_selected_keyboard_key_x < 0)
						{
							current_selected_keyboard_key_x = 10;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							PassWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if(current_screen == CHANGE_PASSWORD)
					{
						current_selected_keyboard_key_x--;
						if (current_selected_keyboard_key_x < 0)
						{
							current_selected_keyboard_key_x = 10;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							ChangePassWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == LOG_SCREEN)
					{
						current_screen = SENSOR_CFG_SCREEN;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							CfgWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					
					break;
				}
				case 19 : // DOWN
				{
					YDebugSendMessage("Pressed: DOWN\r\n", 15);
					
					if (current_screen == MAIN_SCREEN)
					{
						current_selected_sensor_num++;
						if (current_selected_sensor_num >= current_sensor_page_size)
						{
							current_selected_sensor_num = 0;
							
							current_sensor_page_num++;
							if (current_sensor_page_num >= page_cnt)
							{
								current_sensor_page_num = 0;
							}
							
							if ((SensorsAmount() - current_sensor_page_num * max_sensor_draw) > max_sensor_draw)
							{
								current_sensor_page_size = max_sensor_draw;
							}
							else
							{
								current_sensor_page_size = SensorsAmount() - current_sensor_page_num * max_sensor_draw;
							}
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							MainWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == SENSOR_CFG_SCREEN)
					{
						current_selected_cfg_part++; 
						if (current_selected_cfg_part > 1)
						{
							current_selected_cfg_part = 0;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							CfgWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == ALPHABET_KEYBOARD)
					{
						current_selected_keyboard_key_y++;
						if (current_selected_keyboard_key_y > 3)
						{
							current_selected_keyboard_key_y = 0;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							AlphabetKeyboardWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if(current_screen == LOG_SCREEN)
					{
						current_log_page_num++;
						if (current_log_page_num >= log_page_cnt)
						{
							current_log_page_num = 0;
						}
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							LogWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					
					break;
				}
				case 20 : // RIGHT
				{
					YDebugSendMessage("Pressed: RIGHT\r\n", 16);
					
					if (current_screen == THRESHOLD_KEYBOARD)
					{
						current_selected_keyboard_key_x++;
						if (current_selected_keyboard_key_x > 12)
						{
							current_selected_keyboard_key_x = 0;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							ThresholdKeyboardWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == ALPHABET_KEYBOARD)
					{
						current_selected_keyboard_key_x++;
						if (current_selected_keyboard_key_x > 12)
						{
							current_selected_keyboard_key_x = 0;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							AlphabetKeyboardWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == TIME_EDITOR)
					{
						current_selected_keyboard_key_x++;
						if (current_selected_keyboard_key_x > 10)
						{
							current_selected_keyboard_key_x = 0;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							TimeEditorWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == MAIN_SCREEN)
					{
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							current_screen = GRAPHIC;
							GraphicWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == PASSWORD)
					{
						current_selected_keyboard_key_x++;
						if (current_selected_keyboard_key_x > 10)
						{
							current_selected_keyboard_key_x = 0;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							PassWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == CHANGE_PASSWORD)
					{
						current_selected_keyboard_key_x++;
						if (current_selected_keyboard_key_x > 10)
						{
							current_selected_keyboard_key_x = 0;
						}
						
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							ChangePassWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					else if (current_screen == SENSOR_CFG_SCREEN)
					{
						current_screen = LOG_SCREEN;
						current_log_page_num = 0;
						if(xSemaphoreTake(xSemaphore, semaphore_timeout) == pdTRUE)
						{
							LogWndDraw();
							xSemaphoreGive(xSemaphore);
						}
					}
					
					break;
				}
				default:
				{
				}
			}
		}
		
		KyesSetAccessPinLow();
		YDelay(50);
		KyesSetAccessPinHigh();

		vTaskDelay(100);
	}
}

void OneWireUartInit(void)
{
	unsigned long Fdiv = ( Fpclk / 16 ) / 9600;
	
	PINSEL1 |= (unsigned long) ((3 << 18) | (3 << 20));
	
	PCONP |= (1 << 25);
	
	U3LCR |= (1 << 7) | (1 << 1) | 1;
	U3DLM = Fdiv / 256;
	U3DLL = Fdiv % 256;
	U3LCR &= (1 << 1) | 1;
	U3FCR |= (1 << 2) | (1 << 1) | 1;
}

void OneWireUsartTransmit(uint8_t byte)
{
	U3THR = byte;
}

YBOOL OneWireUsartIsTransmissionComplete(void)
{
	if (U3LSR & (1 << 5))
	{
		return YTRUE;
	}
	return YFALSE;
}

uint8_t OneWireUsartReceive(void)
{
	return (uint8_t) U3RBR;
}

YBOOL OneWireUsartReceivingComplete(void)
{
	if (U3LSR & 1)
	{
		return YTRUE;
	}
	return YFALSE;
}

#ifdef YDEBUG
void DebugUartInit(void)
{
	unsigned long Fdiv = ( Fpclk / 16 ) / 56000;
	
	PINSEL0 |= (unsigned long) ((1 << 4) | (1 << 6));
	
	PCONP |= (1 << 3);
	
	U0LCR |= (1 << 7) | (1 << 1) | 1;
	U0DLM = Fdiv / 256;
	U0DLL = Fdiv % 256;
	U0LCR &= (1 << 1) | 1;
	U0FCR |= (1 << 2) | (1 << 1) | 1;
}

void DebugUartTransmit(uint8_t byte)
{
	U0THR = byte;
}

YBOOL DebugUartIsTransmissionComplete(void)
{
	if (U0LSR & (1 << 5))
	{
		return YTRUE;
	}
	return YFALSE;
}
#endif // YDEBUG

int main( void )
{
	__disable_irq();
	
	#ifdef YDEBUG
	// UART initialization
	DebugUartInit();
	// Debug library initialization
	YDebugInit(&DebugUartTransmit, &DebugUartIsTransmissionComplete);
	#endif // YDEBUG
	
	// watchdog init
	//WatchDogInit();
	
	RTC_CCR |= 1 << 4;
	
	AHBCFG1 &= ~1 ;
	AHBCFG1 |= (3<<12);
	AHBCFG1 |= (4<<16);
	AHBCFG1 |= (2<<20);
	AHBCFG1 |= (1<<24);
	AHBCFG1 |= (5<<28);
	
	YDebugSendMessage("Start up\r\n", 10);
	
	// 1-wire library and chip initialization
	OneWireUartInit();
	YOneWireInit(&OneWireUsartTransmit, &OneWireUsartIsTransmissionComplete,
		&OneWireUsartReceive, &OneWireUsartReceivingComplete);
	
	YDebugSendMessage("1-wire inited\r\n", 15);
	
	d_status = 1;
	while(d_status != 0)
	{
		d_status = disk_initialize(0);
		if (d_status == 0)
		{
			f_result = f_mount(0, &fat_fs);
			if (f_result != FR_OK)
			{
				YDebugSendMessage("Wrong SD cart file system, need FAT32\t\n", 39);
			}
			else
			{
				sd_card_inited = YTRUE;
			}
		}
		else
		{
			YDebugSendMessage("Can't find SD card\t\n", 20);
		}
		YDelay(400);
	}
	
	WatchDogReset();
	
	__enable_irq();
	
	SensorUseSDCard(sd_card_inited);
	
	// kyeboard initialization
	KeysInit();
	
	// LCD initialization
	LCDinit();
	DrawPicture((unsigned int) MAIN_PICTURE_ADDR, 800, 480, 0, 0);
	DrawText(0xFFFFFFFF, (uint8_t *) main_wnd_sensor_finding_title, 14, 7, 7);
	
	LoadPassword();
	
	xSemaphore = xSemaphoreCreateMutex();
	xTaskCreate(&TestTask1, (const signed char *) "t1", 1024, NULL, 2, (xTaskHandle *) NULL);
	xTaskCreate(&TestTask2, (const signed char *) "t2", 1024, NULL, 2, (xTaskHandle *) NULL);
	
	vTaskStartScheduler();
	
	YDebugSendMessage("All started\r\n", 13);
	
	for( ;; );
}
