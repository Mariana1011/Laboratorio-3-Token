#ifndef LCD_I2C_H
#define LCD_I2C_H

#include "main.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// BORRA estas (mapeo A):
#define LCD_RS 0x01
#define LCD_RW 0x02
#define LCD_EN 0x04
#define LCD_BL 0x08

// Y PON estas (mapeo B):
//#define LCD_RS 0x80  // P7
//#define LCD_RW 0x40  // P6
//#define LCD_EN 0x20  // P5
//#define LCD_BL 0x10  // P4


#ifdef __cplusplus
extern "C" {
#endif

/* LCD_Begin:
   - i2c_addr = 0           → AUTODETECT (prueba 0x27 y 0x3F)
   - i2c_addr = (0x27<<1)   → fuerza 0x27
   - i2c_addr = (0x3F<<1)   → fuerza 0x3F    */
HAL_StatusTypeDef LCD_Begin(I2C_HandleTypeDef *hi2c, uint16_t i2c_addr);

void LCD_Init(void);                 // init 4-bit, display ON
void LCD_Clear(void);
void LCD_Home(void);
void LCD_Goto(uint8_t row, uint8_t col);   // row: 0..1, col: 0..15
void LCD_Print(const char *s);
void LCD_PrintAt(uint8_t row, uint8_t col, const char *s);
void LCD_Printf(const char *fmt, ...);

void LCD_Backlight(uint8_t on);      // 1=ON, 0=OFF
void LCD_DisplayOn(uint8_t on);      // 1=ON, 0=OFF

#ifdef __cplusplus
}
#endif

#endif /* LCD_I2C_H */
