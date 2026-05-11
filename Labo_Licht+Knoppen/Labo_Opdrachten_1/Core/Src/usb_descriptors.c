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
 */

#include "tusb.h"

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,

  // Device class is 0 per-interface
  .bDeviceClass       = 0x00,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,

  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

  // Use your own VID/PID if you have them
  .idVendor           = 0xCafe,
  .idProduct          = 0x4001,
  .bcdDevice          = 0x0100,

  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x03,

  .bNumConfigurations = 0x01
};

uint8_t const* tud_descriptor_device_cb(void)
{
  return (uint8_t const*) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor (MIDI)
//--------------------------------------------------------------------+
//
// IMPORTANT:
// Your TinyUSB version defines:
//   TUD_MIDI_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize)
// so we must use the 5-argument form (NOT the older 8-argument audio+streaming form).
//
// MIDI uses 2 interfaces: Audio Control (0) + MIDI Streaming (1)
//
enum
{
  ITF_NUM_MIDI = 0,
  ITF_NUM_MIDI_STREAMING = 1,
  ITF_NUM_TOTAL = 2
};

#define CONFIG_TOTAL_LEN   (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)

#define EPNUM_MIDI_OUT     0x01
#define EPNUM_MIDI_IN      0x81
#define EPSIZE_MIDI        64

uint8_t const desc_configuration[] =
{
  // Config number, interface count, string index, total length, attributes, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // itf num, string index, EP OUT, EP IN, EP size
  TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 4, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, EPSIZE_MIDI),
};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index;
  return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+
//
// Note: String index 0 is language ID
//
static const char* string_desc_arr[] =
{
  (const char[]) { 0x09, 0x04 },         // 0: English (0x0409)
  "DAE",                                 // 1: Manufacturer
  "STM32H533 TinyUSB MIDI",              // 2: Product
  "0001",                                // 3: Serial (simple fixed serial)
  "TinyUSB MIDI",                        // 4: MIDI Interface
};

static uint16_t _desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  uint8_t chr_count;

  if (index == 0)
  {
    // Language ID
    _desc_str[1] = 0x0409;
    chr_count = 1;
  }
  else
  {
    if (index >= (sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
    {
      return NULL;
    }

    const char* str = string_desc_arr[index];
    chr_count = 0;

    while (str[chr_count] && chr_count < 31)
    {
      _desc_str[1 + chr_count] = (uint16_t) str[chr_count];
      chr_count++;
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
  return _desc_str;
}
