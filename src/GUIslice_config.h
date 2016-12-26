#ifndef _GUISLICE_CONFIG_H_
#define _GUISLICE_CONFIG_H_

// =======================================================================
// GUIslice library (user-specified configuration)
// - Calvin Hass
// - http://www.impulseadventure.com/elec/microsdl-sdl-gui.html
// =======================================================================
//
// The MIT License
//
// Copyright 2016 Calvin Hass
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// =======================================================================


// =======================================================================
// User Configuration
// - This file can be modified by the user to match the
//   intended target configuration
// =======================================================================

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// -----------------------------------------------------------------------------------------
  
// Specify the display driver library
// - Uncomment one of the following display drivers
  #define DRV_DISP_SDL1              // LINUX: SDL 1.2 library
//#define DRV_DISP_SDL2              // LINUX: SDL 2.0 library
//#define DRV_DISP_ADAGFX            // Arduino: Adafruit-GFX library

  
// Specify the touchscreen driver
// - Uncomment one of the following touchscreen drivers
//#define DRV_TOUCH_NONE          // No touchscreen support
//#define DRV_TOUCH_SDL           // LINUX: Use SDL touch driver
  #define DRV_TOUCH_TSLIB         // LINUX: Use tslib touch driver
//#define DRV_TOUCH_ADA_STMPE610  // Arduino: Use Adafruit STMPE610 touch driver   [Untested]
//#define DRV_TOUCH_ADA_FT6206    // Arduino: Use Adafruit FT6206 touch driver     [TODO]



// -----------------------------------------------------------------------------------------

// Display Driver-specific additional configuration  
#if defined(DRV_DISP_SDL1)
  // Define default device paths for framebuffer & touchscreen  
  #define GSLC_DEV_FB    "/dev/fb1"
  #define GSLC_DEV_TOUCH "/dev/input/touchscreen"
  // Enable SDL startup workaround?
  #define DRV_SDL_FIX_START

  #define GSLC_LOCAL_STR  1
    
  // Error reporting
  #define DEBUG_ERR   1       // Enable error message reporting (requires more memory)


#elif defined(DRV_DISP_SDL2)
  // Define default device paths for framebuffer & touchscreen
  // - The following assumes display driver (eg. fbtft) reads from fb1
  // - Raspberry Pi can support hardware acceleration onto fb0
  // - To use SDL2.0 with hardware acceleration with such displays,
  //   use fb0 as the target and then run fbcp to mirror fb0 to fb1
  #define GSLC_DEV_FB    "/dev/fb0"
  #define GSLC_DEV_TOUCH "/dev/input/touchscreen"  

  #define GSLC_LOCAL_STR  1

  // Error reporting
  #define DEBUG_ERR   1       // Enable error message reporting (requires more memory)


#elif defined(DRV_DISP_ADAGFX)  

  #define GSLC_DEV_TOUCH ""   // No device path used

  #define GSLC_LOCAL_STR  0

  // Error reporting
  #define DEBUG_ERR   0       // Disable error message reporting (conserve memory)
  
  // The Adafruit-GFX library supports a number of displays
  // - Select a display sub-type by uncommenting one of the
  //   following DRV_DISP_ADAGFX_* lines
  #define DRV_DISP_ADAGFX_ILI9341
  //#define DRV_DISP_ADAGFX_SSD1306   // [TODO]

  // For Adafruit-GFX drivers, define pin connections
  // Define general pins (modify these example pin assignments to match your board)
  #define ADAGFX_PIN_CS    53 // ProMini: 10, ATmega2560: 53
  #define ADAGFX_PIN_DC    47 // ProMini:  9, ATmega2560: 47
  #define ADAGFX_PIN_RST   49 // ProMini:  8, ATmega2560: 49
  #define ADAGFX_PIN_MOSI
  #define ADAGFX_PIN_MISO
  #define ADAGFX_PIN_CLK
  #define ADAGFX_PIN_SDCS  48 //              ATmega2560: 48 (SD card chip select for GSLC_IMG_MEM_FLASH)

  // Use hardware SPI?
  #define ADAGFX_SPI_HW

  // Enable support for SD card
  // - Set to 1 to enable, 0 to disable
  // - Note that the inclusion of the SD library consumes considerable
  //   RAM and flash memory which could be problematic for Arduino models
  //   with limited resources.
  #define ADAGFX_SD_EN    0
  
  // Define buffer size for loading images from SD
  // - A larger buffer will be faster but at the cost of RAM
  #define ADAGFX_SD_BUFFPIXEL   20

#endif // DRV_DISP_*


// -----------------------------------------------------------------------------------------

// Touch Driver-specific additional configuration  
#if defined(DRV_TOUCH_SDL)
  #define DRV_TOUCH_IN_DISP   // Use the display driver (SDL) for touch events
  
#elif defined(DRV_TOUCH_ADA_STMPE610)

  // Select wiring method by uncommenting one of the following
  #define ADATOUCH_I2C_HW
  //#define ADATOUCH_SPI_HW   // [TODO]
  //#define ADATOUCH_SPI_SW   // [TODO]

  #define ADATOUCH_I2C_ADDR   0x41  // I2C address of touch device

#endif // DRV_TOUCH_*

// -----------------------------------------------------------------------------------------


// When using element local string storage (GSLC_LOCAL_STR=1),
// this defines the fixed length buffer used for every element
#define GSLC_LOCAL_STR_LEN  30  // Max string length of text elements


// Debug modes
//#define DBG_LOG           // Enable debugging log output
//#define DBG_TOUCH         // Enable debugging of touch-presses
//#define DBG_FRAME_RATE    // Enable diagnostic frame rate reporting
//#define DBG_DRAW_IMM      // Enable immediate rendering of drawing primitives
  
// Enable for bitmap transparency and definition of color to use
#define GSLC_BMP_TRANS_EN     1               // 1 = enabled, 0 = disabled
#define GSLC_BMP_TRANS_RGB    0xFF,0x00,0xFF  // RGB color (default:pink)



#ifdef __cplusplus
}
#endif // __cplusplus
#endif // _GUISLICE_CONFIG_H_
