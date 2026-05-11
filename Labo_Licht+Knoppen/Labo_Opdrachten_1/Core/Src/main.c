/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdbool.h>
#include "tusb.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

PCD_HandleTypeDef hpcd_USB_DRD_FS;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ICACHE_Init(void);
void MX_USB_PCD_Init(void);
static void MX_SPI_BitBang_Init(void);
/* USER CODE BEGIN PFP */

static void keypad_task(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// MCP23S17 SPI keypad mapping
#define MCP23S17_ADDR   0x40
#define MCP_CS_PORT     GPIOA
#define MCP_CS_PIN      GPIO_PIN_10

#define MCP_IODIRA      0x00
#define MCP_IODIRB      0x01
#define MCP_GPPUA       0x0C
#define MCP_REG_GPIOA   0x12
#define MCP_REG_GPIOB   0x13

#define SPI_SCK_PORT    GPIOA
#define SPI_SCK_PIN     GPIO_PIN_5
#define SPI_MISO_PORT   GPIOA
#define SPI_MISO_PIN    GPIO_PIN_6
#define SPI_MOSI_PORT   GPIOA
#define SPI_MOSI_PIN    GPIO_PIN_7

#define LED_DIN_PORT    GPIOA
#define LED_DIN_PIN     GPIO_PIN_8
#define LED_COUNT       16
#define LED_STARTUP_ALL_ON_TEST 0
#define LED_CHASE_TEST          1

#define DEBOUNCE_COUNT  5

typedef struct
{
  uint8_t g;
  uint8_t r;
  uint8_t b;
} sk6812_pixel_t;

static uint8_t keypad_state[4]      = {0x0F, 0x0F, 0x0F, 0x0F};
static uint8_t keypad_prev[4]       = {0x0F, 0x0F, 0x0F, 0x0F};
static uint8_t keypad_candidate[4]  = {0x0F, 0x0F, 0x0F, 0x0F};
static uint8_t keypad_stable_cnt[4] = {0, 0, 0, 0};
static uint8_t active_notes[4][4]   = {0};
static sk6812_pixel_t led_pixels[LED_COUNT];

static const uint8_t note_map[4][4] = {
  {60, 62, 64, 65},
  {67, 69, 71, 72},
  {74, 76, 77, 79},
  {81, 83, 84, 86}
};

static void mcp23s17_cs_low(void);
static void mcp23s17_cs_high(void);
static uint8_t spi_transfer_byte(uint8_t data);
static void mcp23s17_write_reg(uint8_t reg, uint8_t value);
static uint8_t mcp23s17_read_reg(uint8_t reg);
static void mcp23s17_init(void);
static void keypad_scan(void);
static void leds_init(void);
static void leds_set_button(uint8_t row, uint8_t col, bool on);
static void leds_show(void);
static void led_chase_task(void);

static void MX_SPI_BitBang_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Pin   = SPI_SCK_PIN | SPI_MOSI_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin  = SPI_MISO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SPI_MISO_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin   = MCP_CS_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(MCP_CS_PORT, &GPIO_InitStruct);

  HAL_GPIO_WritePin(SPI_SCK_PORT, SPI_SCK_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SPI_MOSI_PORT, SPI_MOSI_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MCP_CS_PORT, MCP_CS_PIN, GPIO_PIN_SET);
}

static uint8_t spi_transfer_byte(uint8_t data)
{
  uint8_t received = 0;

  for (int i = 7; i >= 0; i--)
  {
    HAL_GPIO_WritePin(SPI_MOSI_PORT, SPI_MOSI_PIN, (data & (1u << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SPI_SCK_PORT, SPI_SCK_PIN, GPIO_PIN_SET);
    if (HAL_GPIO_ReadPin(SPI_MISO_PORT, SPI_MISO_PIN) == GPIO_PIN_SET)
    {
      received |= (1u << i);
    }
    HAL_GPIO_WritePin(SPI_SCK_PORT, SPI_SCK_PIN, GPIO_PIN_RESET);
  }

  return received;
}

static void mcp23s17_cs_low(void)
{
  HAL_GPIO_WritePin(MCP_CS_PORT, MCP_CS_PIN, GPIO_PIN_RESET);
}

static void mcp23s17_cs_high(void)
{
  HAL_GPIO_WritePin(MCP_CS_PORT, MCP_CS_PIN, GPIO_PIN_SET);
}

static void mcp23s17_write_reg(uint8_t reg, uint8_t value)
{
  uint8_t opcode = MCP23S17_ADDR | 0x00;
  mcp23s17_cs_low();
  spi_transfer_byte(opcode);
  spi_transfer_byte(reg);
  spi_transfer_byte(value);
  mcp23s17_cs_high();
}

static uint8_t mcp23s17_read_reg(uint8_t reg)
{
  uint8_t opcode = MCP23S17_ADDR | 0x01;
  mcp23s17_cs_low();
  spi_transfer_byte(opcode);
  spi_transfer_byte(reg);
  uint8_t result = spi_transfer_byte(0x00);
  mcp23s17_cs_high();
  return result;
}

static void mcp23s17_init(void)
{
  mcp23s17_write_reg(MCP_IODIRA, 0x0F);
  mcp23s17_write_reg(MCP_GPPUA,  0x0F);
  mcp23s17_write_reg(MCP_IODIRB, 0x00);
  mcp23s17_write_reg(MCP_REG_GPIOB, 0x0F);
}

static void keypad_scan(void)
{
  for (int row = 0; row < 4; row++)
  {
    uint8_t row_pattern = (uint8_t)(~(1u << row)) & 0x0F;
    mcp23s17_write_reg(MCP_REG_GPIOB, row_pattern);

    uint8_t read1 = mcp23s17_read_reg(MCP_REG_GPIOA) & 0x0F;
    uint8_t read2 = mcp23s17_read_reg(MCP_REG_GPIOA) & 0x0F;

    if (read1 != read2)
    {
      keypad_stable_cnt[row] = 0;
      continue;
    }

    if (read1 == keypad_candidate[row])
    {
      if (keypad_stable_cnt[row] < DEBOUNCE_COUNT)
      {
        keypad_stable_cnt[row]++;
      }
      if (keypad_stable_cnt[row] >= DEBOUNCE_COUNT)
      {
        keypad_state[row] = read1;
      }
    }
    else
    {
      keypad_candidate[row] = read1;
      keypad_stable_cnt[row] = 1;
    }
  }

  mcp23s17_write_reg(MCP_REG_GPIOB, 0x0F);
}

static void keypad_task(void)
{
  static uint32_t scan_ms = 0;
  bool leds_dirty = false;

  if (HAL_GetTick() - scan_ms < 20u)
  {
    return;
  }
  scan_ms = HAL_GetTick();

  keypad_scan();

  if (!tud_mounted())
  {
    for (int i = 0; i < 4; i++)
    {
      keypad_prev[i] = keypad_state[i];
    }
    return;
  }

  for (int row = 0; row < 4; row++)
  {
    for (int col = 0; col < 4; col++)
    {
      uint8_t current_bit = (keypad_state[row] >> col) & 1u;
      uint8_t prev_bit = (keypad_prev[row] >> col) & 1u;

      if (prev_bit && !current_bit && !active_notes[row][col])
      {
        uint8_t note = note_map[row][col];
        uint8_t note_on[4] = { 0x09, 0x90, note, 127 };
        tud_midi_packet_write(note_on);
        active_notes[row][col] = note;
#if !LED_CHASE_TEST
        leds_set_button((uint8_t) row, (uint8_t) col, true);
        leds_dirty = true;
#endif
      }
      else if (!prev_bit && current_bit && active_notes[row][col])
      {
        uint8_t note = active_notes[row][col];
        uint8_t note_off[4] = { 0x08, 0x80, note, 0 };
        tud_midi_packet_write(note_off);
        active_notes[row][col] = 0;
#if !LED_CHASE_TEST
        leds_set_button((uint8_t) row, (uint8_t) col, false);
        leds_dirty = true;
#endif
      }
    }
  }

  if (leds_dirty)
  {
    leds_show();
  }

  for (int i = 0; i < 4; i++)
  {
    keypad_prev[i] = keypad_state[i];
  }
}

static inline void led_din_high(void)
{
  LED_DIN_PORT->BSRR = LED_DIN_PIN;
}

static inline void led_din_low(void)
{
  LED_DIN_PORT->BSRR = ((uint32_t) LED_DIN_PIN << 16U);
}

static inline void delay_cycles(uint32_t cycles)
{
  while (cycles--)
  {
    __NOP();
  }
}

static void sk6812_send_bit(bool bit)
{
  led_din_high();
  if (bit)
  {
    delay_cycles(14);
    led_din_low();
    delay_cycles(8);
  }
  else
  {
    delay_cycles(6);
    led_din_low();
    delay_cycles(16);
  }
}

static void sk6812_send_byte(uint8_t value)
{
  for (int bit = 7; bit >= 0; bit--)
  {
    sk6812_send_bit(((value >> bit) & 0x01u) != 0u);
  }
}

static inline uint8_t button_to_led_index(uint8_t row, uint8_t col)
{
  return (uint8_t) (row * 4u + col);
}

static void leds_set_button(uint8_t row, uint8_t col, bool on)
{
  uint8_t index = button_to_led_index(row, col);
  if (index >= LED_COUNT)
  {
    return;
  }

  if (on)
  {
    led_pixels[index].r = 0x20;
    led_pixels[index].g = 0x20;
    led_pixels[index].b = 0x20;
  }
  else
  {
    led_pixels[index].r = 0x00;
    led_pixels[index].g = 0x00;
    led_pixels[index].b = 0x00;
  }
}

static void leds_show(void)
{
  __disable_irq();
  for (uint8_t i = 0; i < LED_COUNT; i++)
  {
    sk6812_send_byte(led_pixels[i].g);
    sk6812_send_byte(led_pixels[i].r);
    sk6812_send_byte(led_pixels[i].b);
  }
  __enable_irq();

  for (volatile uint32_t i = 0; i < 4000u; i++)
  {
    __NOP();
  }
}

static void leds_init(void)
{
  for (uint8_t i = 0; i < LED_COUNT; i++)
  {
#if LED_STARTUP_ALL_ON_TEST
    led_pixels[i].r = 0x20;
    led_pixels[i].g = 0x20;
    led_pixels[i].b = 0x20;
#else
    led_pixels[i].r = 0;
    led_pixels[i].g = 0;
    led_pixels[i].b = 0;
#endif
  }
  leds_show();
}

static void led_chase_task(void)
{
  static uint32_t last_ms = 0;
  static uint8_t pos = 0;

  if (HAL_GetTick() - last_ms < 120u)
  {
    return;
  }
  last_ms = HAL_GetTick();

  for (uint8_t i = 0; i < LED_COUNT; i++)
  {
    led_pixels[i].r = 0;
    led_pixels[i].g = 0;
    led_pixels[i].b = 0;
  }

  led_pixels[pos].r = 0x00;
  led_pixels[pos].g = 0x20;
  led_pixels[pos].b = 0x00;
  leds_show();

  pos = (uint8_t)((pos + 1u) % LED_COUNT);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ICACHE_Init();

  /* Start the USB peripheral before TinyUSB takes ownership of the device stack */
  MX_USB_PCD_Init();
  /* USER CODE BEGIN 2 */

  MX_SPI_BitBang_Init();
  mcp23s17_init();
  leds_init();

  tusb_rhport_init_t dev_init = {
    .role  = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

#if LED_CHASE_TEST
    led_chase_task();
#endif
    keypad_task();
    tud_task();

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV2;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the programming delay
  */
  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_0);
}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
static void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */

  /** Enable instruction cache (default 2-ways set associative cache)
  */
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/**
  * @brief USB Initialization Function
  * @param None
  * @retval None
  */
void MX_USB_PCD_Init(void)
{

  /* USER CODE BEGIN USB_Init 0 */

  /* USER CODE END USB_Init 0 */

  /* USER CODE BEGIN USB_Init 1 */

  /* USER CODE END USB_Init 1 */
  hpcd_USB_DRD_FS.Instance = USB_DRD_FS;
  hpcd_USB_DRD_FS.Init.dev_endpoints = 8;
  hpcd_USB_DRD_FS.Init.speed = USBD_FS_SPEED;
  hpcd_USB_DRD_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_DRD_FS.Init.Sof_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.battery_charging_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.vbus_sensing_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.bulk_doublebuffer_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.iso_singlebuffer_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_DRD_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_Init 2 */

  /* USER CODE END USB_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
