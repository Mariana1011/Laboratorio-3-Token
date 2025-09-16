#include "main.h"
#include "lcd_i2c.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

// ======= Secret & token (igual que en PC) =======
static const char *SECRET = "USAB_2025_LAB3";
enum { TOKEN_WINDOW_S = 30 };

static uint32_t fnv1a32(const uint8_t *data, size_t len){
    uint32_t h = 2166136261u;
    for (size_t i=0;i<len;i++){ h ^= data[i]; h *= 16777619u; }
    return h;
}
static uint32_t token_from_step(uint32_t step){
    uint8_t buf[64]; size_t n = 0;
    for (const char *p=SECRET; *p && n<sizeof(buf); ++p) buf[n++] = (uint8_t)*p;
    if (n+4 <= sizeof(buf)){
        buf[n++] = (uint8_t)(step & 0xFF);
        buf[n++] = (uint8_t)((step>>8)&0xFF);
        buf[n++] = (uint8_t)((step>>16)&0xFF);
        buf[n++] = (uint8_t)((step>>24)&0xFF);
    }
    return fnv1a32(buf, n) % 1000000u;
}

// ======= HAL handles =======
I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim2;

// ======= Estado =======
static uint8_t  synced = 0;
static uint64_t t0_ms  = 0;     // ms en el momento del pulso blanco
static uint8_t  last_light = 0; // último estado leído del pin (0/1)

// ======= Utils: ms desde TIM2 (PSC=99 -> 160 kHz => 160 ticks = 1ms) =======
static inline uint32_t tim2_ticks(void){
    return __HAL_TIM_GET_COUNTER(&htim2);
}
static inline uint64_t now_ms(void){
    return (uint64_t)(tim2_ticks() / 160u);
}

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);

// ======= UI helpers =======
static char line0[17] = "Esperando sync ";
static char line1[17] = "t=---- s TOKEN ";

static void lcd_refresh(void){
    LCD_PrintAt(0,0, line0);
    LCD_PrintAt(1,0, line1);
}

// ======= main =======
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_TIM2_Init();

    // LCD
    if (LCD_Begin(&hi2c1, 0) != HAL_OK) { while(1){} }
    LCD_Init();
    LCD_Clear();
    lcd_refresh();

    // TIM2
    HAL_TIM_Base_Start(&htim2);

    // Estado inicial de la fotocelda
    last_light = (uint8_t)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

    // Lazo principal SIN HAL_Delay (solo temporización con TIM2)
    uint64_t last_ui_ms = 0;
    uint32_t last_step  = 0xFFFFFFFFu;

    while (1)
    {
        // 1) Detección de flanco para sincronizar (negro->blanco)
        uint8_t light = (uint8_t)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
        if (!last_light && light){         // flanco ascendente => apareció el recuadro blanco
            t0_ms = now_ms();
            synced = 1;
        }
        last_light = light;

        // 2) Si sincronizado, calcular TOKEN cada 30 s (solo si cambia de ventana)
        if (synced){
            uint64_t elapsed_s = (now_ms() - t0_ms)/1000u;
            uint32_t step = (uint32_t)(elapsed_s / TOKEN_WINDOW_S);
            if (step != last_step){
                last_step = step;
                uint32_t code = token_from_step(step);
                // Actualiza líneas
                snprintf(line0, sizeof line0, "SYNC ok  t=%4lus", (unsigned long)elapsed_s);
                snprintf(line1, sizeof line1, "TOKEN: %06lu   ", (unsigned long)code);
                lcd_refresh();
            } else {
                // refresco UI cada 200 ms
                if (ms - last_ui_ms >= 200){
                    last_ui_ms = ms;
                    uint64_t elapsed_s2 = (ms - t0_ms)/1000u;
                    snprintf(line0, sizeof line0, "SYNC ok  t=%4lus", (unsigned long)elapsed_s2);
                    lcd_refresh();
                }
            }
        } else {
            // No sincronizado aún: mostrar estado y parpadear backlight
            uint64_t ms = now_ms();
            if (ms - last_ui_ms >= 300){
                last_ui_ms = ms;
                // alterna un puntito
                static uint8_t dot=0; dot^=1;
                snprintf(line0, sizeof line0, "Esperando sync %c", dot?'.':' ');
                lcd_refresh();
            }
        }


    }
}



void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK){ Error_Handler(); }
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK){ Error_Handler(); }
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK){ Error_Handler(); }
}

static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 100-1;                // 16 MHz / 100 = 160 kHz
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 0xFFFFFFFF;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK){ Error_Handler(); }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK){ Error_Handler(); }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK){ Error_Handler(); }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();


  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}
