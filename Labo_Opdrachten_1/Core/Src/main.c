/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#define CFG_TUSB_MCU           OPT_MCU_STM32H5
#define CFG_TUSB_OS            OPT_OS_NONE
#define CFG_TUSB_RHPORT0_MODE  (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

#define CFG_TUD_MIDI 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "tusb.h"
#include "stm32h5xx_nucleo.h"   // BSP LED helpers (je project compileert stm32h5xx_nucleo.c al)

//--------------------------------------------------------------------+
// HAL / CubeMX prototypes
//--------------------------------------------------------------------+
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USB_Init(void);

// USB Device Core handle
PCD_HandleTypeDef hpcd_USB_DRD_FS;

//--------------------------------------------------------------------+
// Minimal replacements for TinyUSB example "board_*" functions
//--------------------------------------------------------------------+
static inline uint32_t board_millis(void)
{
  return HAL_GetTick();
}

static inline void board_led_write(bool state)
{
  if (state) BSP_LED_On(LED2);
  else       BSP_LED_Off(LED2);
}

// On the original example these exist, keep them as no-ops for compatibility
static inline void board_init_after_tusb(void) { (void)0; }

// TinyUSB example used BOARD_TUD_RHPORT, on STM32 FS device this is 0.
#ifndef BOARD_TUD_RHPORT
  #define BOARD_TUD_RHPORT 0
#endif

/* This MIDI example send sequence of note (on/off) repeatedly. To test on PC, you need to install
 * synth software and midi connection management software. On
 * - Linux (Ubuntu): install qsynth, qjackctl. Then connect TinyUSB output port to FLUID Synth input port
 * - Windows: install MIDI-OX
 * - MacOS: SimpleSynth
 */

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED     = 1000,
  BLINK_SUSPENDED   = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void midi_task(void);

//--------------------------------------------------------------------+
// MAIN
//--------------------------------------------------------------------+
int main(void)
{
  // Standard CubeMX/HAL init
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USB_Init();

  // Nucleo LED init (LED2)
  BSP_LED_Init(LED2);
  BSP_LED_Off(LED2);

  // init device stack on configured roothub port
  tusb_rhport_init_t dev_init = {
    .role  = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  board_init_after_tusb();

  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();
    midi_task();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// MIDI Task
//--------------------------------------------------------------------+

// Variable that holds the current position in the sequence.
uint32_t note_pos = 0;

// Store example melody as an array of note values
const uint8_t note_sequence[] = {
  74,78,81,86,90,93,98,102,57,61,66,69,73,78,81,85,88,92,97,100,97,92,88,85,81,78,
  74,69,66,62,57,62,66,69,74,78,81,86,90,93,97,102,97,93,90,85,81,78,73,68,64,61,
  56,61,64,68,74,78,81,86,90,93,98,102
};

void midi_task(void)
{
  static uint32_t start_ms = 0;

  uint8_t const cable_num = 0; // MIDI jack associated with USB endpoint
  uint8_t const channel   = 0; // 0 for channel 1

  // Only send MIDI if device is mounted
  if (!tud_mounted())
  {
    return;
  }

  // Read/discard incoming traffic to avoid sender blocking
  while (tud_midi_available())
  {
    uint8_t packet[4];
    tud_midi_packet_read(packet);
  }

  // send note periodically
  if (board_millis() - start_ms < 286)
  {
    return; // not enough time
  }
  start_ms += 286;

  // Previous positions in the note sequence.
  int previous = (int)(note_pos - 1);

  // If we currently are at position 0, set previous to last note in the sequence.
  if (previous < 0)
  {
    previous = (int)(sizeof(note_sequence) - 1);
  }

  // Send Note On for current note (velocity 127)
  // Use packet format: [cable_num_cin, status, data1, data2]
  uint8_t note_on[4] = { 
    (uint8_t)((cable_num << 4) | 0x09),  // Cable 0, Code Index Number 9 (Note On)
    (uint8_t)(0x90 | channel),            // Note On, channel 1
    note_sequence[note_pos],              // Note number
    127                                    // Velocity
  };
  tud_midi_packet_write(note_on);

  // Send Note Off for previous note
  uint8_t note_off[4] = { 
    (uint8_t)((cable_num << 4) | 0x08),  // Cable 0, Code Index Number 8 (Note Off)
    (uint8_t)(0x80 | channel),            // Note Off, channel 1
    note_sequence[previous],              // Note number
    0                                      // Velocity
  };
  tud_midi_packet_write(note_off);

  // Increment position
  note_pos++;

  // Wrap around
  if (note_pos >= sizeof(note_sequence))
  {
    note_pos = 0;
  }
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if (board_millis() - start_ms < blink_interval_ms) return;
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = (bool)(!led_state);
}

//--------------------------------------------------------------------+
// Minimal GPIO init (keep it simple; CubeMX usually generates this anyway)
// If CubeMX already generated MX_GPIO_Init elsewhere, keep only one definition.
//--------------------------------------------------------------------+
static void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
}

//--------------------------------------------------------------------+
// USB Device Initialization
//--------------------------------------------------------------------+
static void MX_USB_Init(void)
{
  // Initialize USB Device peripheral
  hpcd_USB_DRD_FS.Instance = USB_DRD_FS;
  hpcd_USB_DRD_FS.Init.dev_endpoints = 8;
  hpcd_USB_DRD_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_DRD_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_DRD_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.battery_charging_enable = DISABLE;
  
  if (HAL_PCD_Init(&hpcd_USB_DRD_FS) != HAL_OK)
  {
    Error_Handler();
  }

  // Enable USB interrupt
  HAL_NVIC_SetPriority(USB_DRD_FS_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USB_DRD_FS_IRQn);
}

//--------------------------------------------------------------------+
// System Clock Configuration
//--------------------------------------------------------------------+
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
}

//--------------------------------------------------------------------+
// Error Handler
//--------------------------------------------------------------------+
void Error_Handler(void)
{
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
    // Optionally blink an LED or stay in infinite loop
  }
}

//--------------------------------------------------------------------+
// TinyUSB time API (required by TinyUSB)
//--------------------------------------------------------------------+
uint32_t tusb_time_millis_api(void)
{
  return HAL_GetTick();
}
