#include <Arduino.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "local_secrets.h"

namespace {

constexpr const char* kSeatsUrl = "https://dev.gritsenko.biz/tmp/seats.json";
constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;
constexpr int kMaxVisibleLines = 6;
constexpr int kLineBufferSize = 40;
constexpr uint32_t kWifiTimeoutMs = 15000;
constexpr uint32_t kRefreshIntervalMs = 30000;

TFT_eSPI tft = TFT_eSPI();
String visibleLines[kMaxVisibleLines];
uint32_t lastRefreshAt = 0;

void setBacklight(bool enabled) {
    pinMode(4, OUTPUT);
    digitalWrite(4, enabled ? HIGH : LOW);
}

void drawHeader(const char* title, uint16_t color) {
    tft.fillRect(0, 0, kScreenWidth, 20, TFT_BLACK);
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(title, 6, 4, 2);
}

void drawStatus(const String& line1, const String& line2 = String()) {
    tft.fillRect(0, 20, kScreenWidth, kScreenHeight - 20, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(line1, 6, 30, 2);
    if (!line2.isEmpty()) {
        tft.drawString(line2, 6, 48, 2);
    }
}

String trimForDisplay(String text) {
    text.replace("\r", "");
    text.trim();
    if (text.length() <= (kLineBufferSize - 1)) {
        return text;
    }

    return text.substring(0, kLineBufferSize - 4) + "...";
}

void drawLines() {
    drawHeader("Seats JSON", TFT_CYAN);
    tft.fillRect(0, 20, kScreenWidth, kScreenHeight - 20, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);

    for (int index = 0; index < kMaxVisibleLines; ++index) {
        const int y = 24 + index * 18;
        tft.drawString(visibleLines[index], 6, y, 2);
    }
}

bool ensureWifiConnected() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    drawHeader("WiFi", TFT_YELLOW);
    drawStatus("Connecting...", kWifiSsid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(kWifiSsid, kWifiPassword);

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < kWifiTimeoutMs) {
        delay(250);
    }

    if (WiFi.status() != WL_CONNECTED) {
        drawStatus("WiFi failed", "Retrying soon");
        return false;
    }

    drawStatus("Connected", WiFi.localIP().toString());
    delay(500);
    return true;
}

bool fetchSeats() {
    if (!ensureWifiConnected()) {
        return false;
    }

    drawHeader("HTTPS", TFT_YELLOW);
    drawStatus("Downloading...", "dev.gritsenko.biz");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    if (!http.begin(client, kSeatsUrl)) {
        drawStatus("HTTP begin failed");
        return false;
    }

    const int statusCode = http.GET();
    if (statusCode <= 0) {
        drawStatus("GET failed", http.errorToString(statusCode));
        http.end();
        return false;
    }

    if (statusCode != HTTP_CODE_OK) {
        drawStatus("HTTP status", String(statusCode));
        http.end();
        return false;
    }

    const String payload = http.getString();
    http.end();

    for (int index = 0; index < kMaxVisibleLines; ++index) {
        visibleLines[index] = "";
    }

    int lineStart = 0;
    int lineIndex = 0;
    while (lineIndex < kMaxVisibleLines && lineStart <= payload.length()) {
        int lineEnd = payload.indexOf('\n', lineStart);
        if (lineEnd < 0) {
            lineEnd = payload.length();
        }

        visibleLines[lineIndex] = trimForDisplay(payload.substring(lineStart, lineEnd));
        ++lineIndex;
        lineStart = lineEnd + 1;
    }

    if (lineIndex == 0) {
        visibleLines[0] = "Empty response";
    }

    drawLines();
    return true;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(200);

    setBacklight(true);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    fetchSeats();
    lastRefreshAt = millis();
}

void loop() {
    if (millis() - lastRefreshAt >= kRefreshIntervalMs) {
        fetchSeats();
        lastRefreshAt = millis();
    }

    delay(100);
}