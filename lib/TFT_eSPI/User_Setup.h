// Setup for LILYGO T-Display v1.1 (ESP32 with ST7789 135x240)

#define USER_SETUP_LOADED

// Driver
#define ST7789_DRIVER

// Display size
#define TFT_WIDTH  135
#define TFT_HEIGHT 240

// Pin definitions for LILYGO T-Display v1.1
#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  23
#define TFT_BL   4  // Display backlight control pin

// Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

// SPI frequency
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
