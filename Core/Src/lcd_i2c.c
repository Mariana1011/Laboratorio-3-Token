#include "lcd_i2c.h"

/* ======= Estado interno ======= */
static I2C_HandleTypeDef *g_i2c = NULL;
static uint16_t g_addr = 0;         // dirección HAL (7b << 1)
static uint8_t g_bl   = LCD_BL;     // backlight ON por defecto

/* ======= Helpers I2C ======= */
static inline HAL_StatusTypeDef tx(uint8_t v) {
    return HAL_I2C_Master_Transmit(g_i2c, g_addr, &v, 1, 50);
}
static inline void pulse(uint8_t v) {
    // EN alto → bajo con pequeño retardo
    tx((v | g_bl) | LCD_EN);
    HAL_Delay(1);
    tx((v | g_bl) & ~LCD_EN);
    HAL_Delay(1);
}
// Mapeo A: D4..D7 = P4..P7  (nibble alto tal cual)
// BL=P3, EN=P2, RW=P1, RS=P0
static inline void write4(uint8_t half, uint8_t rs) {
    uint8_t b = (half & 0xF0);              // nibble alto a P4..P7
    if (rs)  b |= LCD_RS;                   // data -> RS=1
    b &= (uint8_t)~LCD_RW;                  // RW=0 SIEMPRE (escritura)
    pulse(b);                               // EN y BL los añade pulse()
}

static inline void cmd(uint8_t c)  { write4(c,0); write4(c<<4,0); }
static inline void data(uint8_t d) { write4(d,1); write4(d<<4,1); }

static HAL_StatusTypeDef probe_addr(uint16_t a) {
    uint8_t dummy = 0x00;
    return HAL_I2C_Master_Transmit(g_i2c, a, &dummy, 1, 50);
}

/* ======= API ======= */
HAL_StatusTypeDef LCD_Begin(I2C_HandleTypeDef *hi2c, uint16_t i2c_addr)
{
    g_i2c = hi2c;
    g_addr = 0;

    if (i2c_addr) {
        // Fuerza una dirección concreta
        if (probe_addr(i2c_addr) != HAL_OK) return HAL_ERROR;
        g_addr = i2c_addr;
    } else {
        // AUTODETECT: prueba 0x27 y 0x3F
        if (probe_addr(0x27 << 1) == HAL_OK) g_addr = (0x27 << 1);
        else if (probe_addr(0x3F << 1) == HAL_OK) g_addr = (0x3F << 1);
        else return HAL_ERROR;
    }

    // Señal rápida: parpadea backlight 2 veces para confirmar I2C
    LCD_Backlight(0); HAL_Delay(80);
    LCD_Backlight(1); HAL_Delay(80);
    LCD_Backlight(0); HAL_Delay(80);
    LCD_Backlight(1); HAL_Delay(80);

    return HAL_OK;
}

void LCD_Init(void)
{
    // Secuencia estándar 4-bit (idéntica a la que usabas antes)
    HAL_Delay(50);
    write4(0x30,0); HAL_Delay(5);
    write4(0x30,0); HAL_Delay(1);
    write4(0x20,0); HAL_Delay(1);   // 4-bit

    cmd(0x28);      // Function set: 4-bit, 2 líneas, 5x8
    cmd(0x0C);      // Display ON, cursor OFF, blink OFF
    cmd(0x06);      // Entry mode
    cmd(0x01); HAL_Delay(2);  // Clear
}

void LCD_Clear(void)               { cmd(0x01); HAL_Delay(2); }
void LCD_Home(void)                { cmd(0x02); HAL_Delay(2); }
void LCD_DisplayOn(uint8_t on)     { cmd(on ? 0x0C : 0x08); }

void LCD_Backlight(uint8_t on)
{
    g_bl = on ? LCD_BL : 0;
    // “poke” para reflejar el estado
    tx(g_bl);
}

void LCD_Goto(uint8_t row, uint8_t col)
{
    static const uint8_t base[2] = {0x00, 0x40};
    cmd(0x80 | (base[row & 1] + (col & 0x0F)));
}

void LCD_Print(const char *s)
{
    while (*s) data((uint8_t)*s++);
}

void LCD_PrintAt(uint8_t row, uint8_t col, const char *s)
{
    LCD_Goto(row, col);
    LCD_Print(s);
}

void LCD_Printf(const char *fmt, ...)
{
    char buf[32];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    LCD_Print(buf);
}
