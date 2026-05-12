#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// Robot face layout (for 240x135 landscape)
const int SCREEN_W = 240;
const int SCREEN_H = 135;

// Battery ADC pins for TTGO T-Display v1.1
const int BAT_ADC_EN_PIN = 14;
const int BAT_ADC_PIN = 34;

// Pixel size multiplier (2 = 2x2 pixel blocks)
const int PIXEL_SIZE = 2;

// Eye geometry
const int EYE_W = 60;
const int EYE_H = 30;
const int LEFT_EYE_X = 70;
const int RIGHT_EYE_X = 170;
const int EYE_Y = 45;

// Pupil (now square)
const int PUPIL_SIZE = 16;

// Mouth
const int MOUTH_W = 120;
const int MOUTH_H = 36;
const int MOUTH_X = (SCREEN_W - MOUTH_W) / 2;
const int MOUTH_Y = 86;

// Blink state
bool eyesOpen = true;
bool blinking = false;
unsigned long blinkStart = 0;
unsigned long blinkDuration = 120;

// Colors
uint16_t col_head = 0;
uint16_t col_glow = 0;
uint16_t col_pupil = 0;
uint16_t col_crease = 0;

unsigned long lastBatteryDrawMs = 0;
const unsigned long BATTERY_REFRESH_MS = 1500;

// Forward declarations
void drawFace();
void drawEyes(bool open);
void drawMouth();
void drawPixelRect(int x, int y, int w, int h, uint16_t color);
void drawPixelLine(int x, int y, int len, uint16_t color, bool horizontal);
float readBatteryVoltage();
int readBatteryPercent();
void drawBatteryStatus();

void setup() {
    Serial.begin(115200);
    delay(200);

    // Turn on backlight
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);

    // Enable battery voltage divider output
    pinMode(BAT_ADC_EN_PIN, OUTPUT);
    digitalWrite(BAT_ADC_EN_PIN, HIGH);

    analogReadResolution(12);
    analogSetPinAttenuation(BAT_ADC_PIN, ADC_11db);

    // Initialize display
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // Prepare colors
    col_head   = tft.color565(40, 40, 40);
    col_glow   = tft.color565(0, 160, 200);
    col_pupil  = tft.color565(0, 200, 230);
    col_crease = tft.color565(0, 120, 200);

    // Seed PRNG
    randomSeed((unsigned long)micros());

    // Draw static parts
    drawFace();
    drawBatteryStatus();
}

void loop() {
    if (!blinking) {
        if (random(0, 1000) < 8) {
            blinking = true;
            blinkStart = millis();
            blinkDuration = random(80, 220);
            drawEyes(false);
        }
    } else {
        if (millis() - blinkStart >= blinkDuration) {
            blinking = false;
            drawEyes(true);
        }
    }

    if (millis() - lastBatteryDrawMs >= BATTERY_REFRESH_MS) {
        drawBatteryStatus();
        lastBatteryDrawMs = millis();
    }

    delay(40);
}

void drawFace() {
    // Head background (pixelated border)
    int pad = 8;
    drawPixelRect(pad, pad, SCREEN_W - pad * 2, SCREEN_H - pad * 2, col_head);
    
    // Inner screen
    drawPixelRect(pad + 4, pad + 4, SCREEN_W - (pad + 4) * 2, SCREEN_H - (pad + 4) * 2, TFT_BLACK);

    drawEyes(true);
    drawMouth();
}

void drawEyes(bool open) {
    // Erase eye areas
    int eraseW = EYE_W + 8;
    int eraseH = EYE_H + 8;
    drawPixelRect(LEFT_EYE_X - eraseW/2, EYE_Y - eraseH/2, eraseW, eraseH, TFT_BLACK);
    drawPixelRect(RIGHT_EYE_X - eraseW/2, EYE_Y - eraseH/2, eraseW, eraseH, TFT_BLACK);

    if (open) {
        // Eye glow (outer pixelated border)
        drawPixelRect(LEFT_EYE_X - EYE_W/2 - 2, EYE_Y - EYE_H/2 - 2, EYE_W + 4, EYE_H + 4, col_glow);
        drawPixelRect(RIGHT_EYE_X - EYE_W/2 - 2, EYE_Y - EYE_H/2 - 2, EYE_W + 4, EYE_H + 4, col_glow);

        // Eye whites (inner)
        drawPixelRect(LEFT_EYE_X - EYE_W/2, EYE_Y - EYE_H/2, EYE_W, EYE_H, TFT_WHITE);
        drawPixelRect(RIGHT_EYE_X - EYE_W/2, EYE_Y - EYE_H/2, EYE_W, EYE_H, TFT_WHITE);

        // Pupils (square)
        int pupil_offset = (EYE_H - PUPIL_SIZE) / 2;
        drawPixelRect(LEFT_EYE_X - PUPIL_SIZE/2, EYE_Y - pupil_offset, PUPIL_SIZE, PUPIL_SIZE, col_pupil);
        drawPixelRect(RIGHT_EYE_X - PUPIL_SIZE/2, EYE_Y - pupil_offset, PUPIL_SIZE, PUPIL_SIZE, col_pupil);
    } else {
        // Closed eyelids (thin pixelated bars)
        int lidH = 8;
        drawPixelRect(LEFT_EYE_X - EYE_W/2, EYE_Y - lidH/2, EYE_W, lidH, col_pupil);
        drawPixelRect(RIGHT_EYE_X - EYE_W/2, EYE_Y - lidH/2, EYE_W, lidH, col_pupil);

        // Crease line
        drawPixelLine(LEFT_EYE_X - EYE_W/2 + 6, EYE_Y - 1, EYE_W - 12, col_crease, true);
        drawPixelLine(RIGHT_EYE_X - EYE_W/2 + 6, EYE_Y - 1, EYE_W - 12, col_crease, true);
    }
}

void drawMouth() {
    // Erase area
    drawPixelRect(MOUTH_X - 4, MOUTH_Y - 4, MOUTH_W + 8, MOUTH_H + 8, TFT_BLACK);

    // Outer mouth (pixelated frame)
    drawPixelRect(MOUTH_X, MOUTH_Y, MOUTH_W, MOUTH_H, col_pupil);

    // Inner mouth (black cavity)
    drawPixelRect(MOUTH_X + 6, MOUTH_Y + 8, MOUTH_W - 12, MOUTH_H - 16, TFT_BLACK);

    // Teeth lines (pixelated)
    int lines = 4;
    int spacing = (MOUTH_H - 16) / (lines + 1);
    for (int i = 1; i <= lines; ++i) {
        int y = MOUTH_Y + 8 + i * spacing;
        drawPixelLine(MOUTH_X + 10, y, MOUTH_W - 20, col_pupil, true);
    }
}

float readBatteryVoltage() {
    long sum = 0;
    const int samples = 12;
    for (int i = 0; i < samples; ++i) {
        sum += analogRead(BAT_ADC_PIN);
    }

    const float adc = static_cast<float>(sum) / samples;
    const float vAdc = (adc / 4095.0f) * 3.3f;
    return vAdc * 2.0f;
}

int readBatteryPercent() {
    const float vBat = readBatteryVoltage();

    // Typical LiPo range under light load
    const float vEmpty = 3.20f;
    const float vFull = 4.20f;

    float pct = ((vBat - vEmpty) / (vFull - vEmpty)) * 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return static_cast<int>(pct + 0.5f);
}

void drawBatteryStatus() {
    const int pct = readBatteryPercent();

    const int x = 12;
    const int y = 10;
    const int w = 100;
    const int h = 16;

    tft.fillRect(x, y, w, h, TFT_BLACK);
    tft.setTextColor(col_pupil, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(String("BAT ") + String(pct) + "%", x, y, 2);
}

// Helper: draw rectangle aligned to pixel grid
void drawPixelRect(int x, int y, int w, int h, uint16_t color) {
    x = (x / PIXEL_SIZE) * PIXEL_SIZE;
    y = (y / PIXEL_SIZE) * PIXEL_SIZE;
    w = (w / PIXEL_SIZE) * PIXEL_SIZE;
    h = (h / PIXEL_SIZE) * PIXEL_SIZE;
    tft.fillRect(x, y, w, h, color);
}

// Helper: draw pixelated line
void drawPixelLine(int x, int y, int len, uint16_t color, bool horizontal) {
    x = (x / PIXEL_SIZE) * PIXEL_SIZE;
    y = (y / PIXEL_SIZE) * PIXEL_SIZE;
    len = (len / PIXEL_SIZE) * PIXEL_SIZE;
    
    if (horizontal) {
        tft.fillRect(x, y, len, PIXEL_SIZE, color);
    } else {
        tft.fillRect(x, y, PIXEL_SIZE, len, color);
    }
}