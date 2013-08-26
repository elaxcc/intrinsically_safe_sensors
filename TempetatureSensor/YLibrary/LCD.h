#ifndef __LCD_H_
#define __LCD_H_

#include <stdint.h>
#include <SDRAM.h>

#define PICTURE_SIZE 1536000 // bytes

#define MAIN_PICTURE_ADDR PICTURE_SIZE
#define MAIN_PICTURE_UP_STEP 130
#define MAIN_PICTURE_LINE_Y_STEP 40

#define SELECT_SIZE 128000
#define SELECT_ADDR MAIN_PICTURE_ADDR + PICTURE_SIZE
#define SELECT_X_SIZE 800
#define SELECT_Y_SIZE 40

#define FONT_SIZE 436100 // bytes
#define FONT_ADDR SELECT_ADDR + SELECT_SIZE
#define FONT_X_SIZE 35
#define FONT_Y_SIZE 35
#define FONT_FILE_X_SIZE 3115

#define SENSOR_CFG_SIZE 1536000
#define SENSOR_CFG_ADDR FONT_ADDR + FONT_SIZE
#define SENSOR_CFG_X_SIZE 800
#define SENSOR_CFG_Y_SIZE 480

#define THRESHOLD_KEYBOARD_SIZE 139536
#define THRESHOLD_KEYBOARD_ADDR SENSOR_CFG_ADDR + SENSOR_CFG_SIZE
#define THRESHOLD_KEYBOARD_X_SIZE 459
#define THRESHOLD_KEYBOARD_Y_SIZE 76

#define KEYBOARD_SELECTER_SIZE 4900
#define KEYBOARD_SELECTER_ADDR THRESHOLD_KEYBOARD_ADDR + THRESHOLD_KEYBOARD_SIZE
#define KEYBOARD_SELECTER_X_SIZE 35
#define KEYBOARD_SELECTER_Y_SIZE 35

#define ALPHABET_KEYBOARD_SIZE 343332
#define ALPHABET_KEYBOARD_ADDR KEYBOARD_SELECTER_ADDR + KEYBOARD_SELECTER_SIZE
#define ALPHABET_KEYBOARD_X_SIZE 459
#define ALPHABET_KEYBOARD_Y_SIZE 187

#define GRAPHICS_SIZE 1536000
#define GRAPHICS_ADDR ALPHABET_KEYBOARD_ADDR + ALPHABET_KEYBOARD_SIZE
#define GRAPHICS_X_SIZE 800
#define GRAPHICS_Y_SIZE 480

void LCDinit(void);
void LCDOn(void);
void LCDOff(void);
void DrawPicture(unsigned int picture, unsigned int x_size, unsigned int y_size,
	unsigned int x_pos, unsigned int y_pos);
void DrawText(uint32_t color, uint8_t* text, uint32_t text_size, uint32_t x_pos, uint32_t y_pos);
void DrawWidthText(uint32_t color, uint8_t* text, uint32_t text_size, uint32_t x_pos, uint32_t y_pos);
void DrawFloat(uint32_t color, float value, uint32_t x_pos, uint32_t y_pos);
void ConvertNumToText(int32_t value, uint8_t buffer[], uint32_t* text_size);
void ConvertTextToNum(int32_t *val, uint8_t* buffer, uint32_t size);
void ConvertFloatToText(float val, uint8_t* buffer, uint32_t* size);
void ConvertTextToFloat(uint8_t* text, uint32_t size, float* val, YBOOL below_zero);
void DrawPoint(uint32_t color, uint32_t x, uint32_t y);
void DrawLine(uint32_t color, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2);

#endif /*__LCD_H_*/
