#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <math.h>

namespace {

constexpr int kWidth = 135;
constexpr int kHeight = 240;
constexpr int kHudHeight = 20;
constexpr int kFieldHeight = kHeight - kHudHeight;
constexpr int kBacklightPin = 4;
constexpr int kBatteryEnablePin = 14;
constexpr int kBatteryAdcPin = 34;
constexpr uint32_t kBatteryRefreshMs = 1500;
constexpr int kButtonPins[] = {0, 35};
constexpr uint32_t kDebounceMs = 25;
constexpr uint32_t kStepMs = 20;
constexpr uint32_t kFrameMs = 33;
constexpr float kDt = kStepMs / 1000.0f;
constexpr float kSpeed = 46.0f;         // Pixels per second.
constexpr float kAngularSpeed = 2.0f;   // Radians per second; radius 23 px.
constexpr float kPi = 3.14159265358979323846f;
constexpr int kBodyRadius = 2;
constexpr int kFoodRadius = 3;
constexpr int kMaxPoints = 700;
constexpr int kInitialLength = 38;
constexpr int kGrowth = 14;
// Nearby samples form the neck, so they must not count as self collisions.
constexpr int kNeckPoints = 10;

struct Point {
    float x;
    float y;
};

struct Button {
    bool raw = false;
    bool stable = false;
    uint32_t changedAt = 0;

    bool pressed(int pin, uint32_t now) {
        const bool down = digitalRead(pin) == LOW;
        if (down != raw) {
            raw = down;
            changedAt = now;
        }
        if (raw != stable && now - changedAt >= kDebounceMs) {
            stable = raw;
            return stable;
        }
        return false;
    }
};

enum class State { Running, Crashed, Won };

TFT_eSPI tft;
TFT_eSprite frame(&tft);
Preferences prefs;
bool prefsReady = false;
Button buttons[2];
Point body[kMaxPoints];  // Ring buffer: each point is one fixed physics step.
Point food;
int headIndex = 0;
int bodyLength = kInitialLength;
int targetLength = kInitialLength;
int turn = 1;
float heading = 0.0f;
unsigned int score = 0;
unsigned int bestScore = 0;
State state = State::Running;
uint32_t finishedAt = 0;
uint32_t lastStepAt = 0;
uint32_t lastFrameAt = 0;
uint32_t lastBatteryAt = 0;
float batteryVoltage = -1.0f;
int batteryPercent = 0;

void updateBattery(uint32_t now) {
    if (batteryVoltage >= 0.0f && now - lastBatteryAt < kBatteryRefreshMs) return;
    uint32_t millivolts = 0;
    constexpr int kSamples = 12;
    for (int i = 0; i < kSamples; ++i) {
        millivolts += analogReadMilliVolts(kBatteryAdcPin);
    }
    // The board's 1:1 resistor divider halves battery voltage at GPIO34.
    const float measured = millivolts * 2.0f / (kSamples * 1000.0f);
    batteryVoltage = batteryVoltage < 0.0f ? measured
                                         : batteryVoltage * 0.75f + measured * 0.25f;
    // Approximate LiPo charge under load, consistent with face_sample.
    batteryPercent = constrain(static_cast<int>((batteryVoltage - 3.2f) * 100.0f + 0.5f),
                               0, 100);
    lastBatteryAt = now;
}

void drawBattery() {
    const uint16_t color = batteryPercent <= 20 ? TFT_RED
                          : batteryPercent <= 50 ? TFT_YELLOW : TFT_GREEN;
    frame.setTextColor(color, TFT_NAVY);
    frame.setTextDatum(TR_DATUM);
    char text[8];
    snprintf(text, sizeof(text), "%d%%", batteryPercent);
    frame.drawString(text, 108, 6, 1);
    frame.drawRect(111, 5, 19, 10, color);
    frame.fillRect(130, 8, 2, 4, color);
    const int fillWidth = (batteryPercent * 15 + 99) / 100;
    if (fillWidth > 0) frame.fillRect(113, 7, fillWidth, 6, color);
    frame.setTextDatum(TL_DATUM);
}

float wrap(float value, float size) {
    value = fmodf(value, size);
    return value < 0.0f ? value + size : value;
}

Point wrapped(Point p) {
    return {wrap(p.x, kWidth), wrap(p.y, kFieldHeight)};
}

const Point& bodyAt(int age) {
    return body[(headIndex + age) % kMaxPoints];
}

float distanceSquared(Point a, Point b) {
    // Shortest distance on a torus, including across opposite edges.
    float dx = fabsf(a.x - b.x);
    float dy = fabsf(a.y - b.y);
    if (dx > kWidth / 2.0f) dx = kWidth - dx;
    if (dy > kFieldHeight / 2.0f) dy = kFieldHeight - dy;
    return dx * dx + dy * dy;
}

bool foodFits(Point candidate) {
    for (int i = 0; i < bodyLength; ++i) {
        if (distanceSquared(candidate, bodyAt(i)) < 64.0f) return false;
    }
    return true;
}

bool placeFood() {
    for (int attempt = 0; attempt < 256; ++attempt) {
        Point candidate = {static_cast<float>(random(kWidth)),
                           static_cast<float>(random(kFieldHeight))};
        if (foodFits(candidate)) {
            food = candidate;
            return true;
        }
    }
    // Bounded fallback for a crowded field, with no infinite random retry.
    for (int y = 0; y < kFieldHeight; y += 4) {
        for (int x = 0; x < kWidth; x += 4) {
            Point candidate = {static_cast<float>(x), static_cast<float>(y)};
            if (foodFits(candidate)) {
                food = candidate;
                return true;
            }
        }
    }
    return false;
}

void finish(State result) {
    state = result;
    finishedAt = millis();
    if (score > bestScore) {
        bestScore = score;
        if (prefsReady && prefs.putUInt("bestScore", bestScore) != sizeof(uint32_t)) {
            Serial.println("Failed to save snake best score");
        }
    }
}

void resetGame(uint32_t now) {
    headIndex = 0;
    bodyLength = targetLength = kInitialLength;
    heading = 0.0f;
    turn = 1;
    score = 0;
    state = State::Running;
    // Seed the tail along the same circle as the initial forward trajectory.
    const float radius = kSpeed / kAngularSpeed;
    const Point start = {kWidth / 2.0f, kFieldHeight / 2.0f};
    for (int i = 0; i < bodyLength; ++i) {
        const float angle = -i * kAngularSpeed * kDt;
        body[i] = wrapped({start.x + radius * sinf(angle),
                           start.y + radius * (1.0f - cosf(angle))});
    }
    placeFood();
    lastStepAt = now;
}

void handleInput(uint32_t now) {
    bool pressed = false;
    for (int i = 0; i < 2; ++i) {
        // Poll both even when the first button produces an event.
        if (buttons[i].pressed(kButtonPins[i], now)) pressed = true;
    }
    if (!pressed) return;
    if (state == State::Running) {
        turn = -turn;
    } else if (now - finishedAt >= 350) {
        resetGame(now);
    }
}

void updateGame() {
    const float omega = turn * kAngularSpeed;
    const float nextHeading = heading + omega * kDt;
    const Point head = bodyAt(0);
    // Exact circular integration preserves the radius in either direction.
    const Point next = wrapped({head.x + kSpeed / omega *
                                            (sinf(nextHeading) - sinf(heading)),
                                head.y + kSpeed / omega *
                                            (cosf(heading) - cosf(nextHeading))});
    heading = nextHeading;
    if (heading > kPi) heading -= 2.0f * kPi;
    if (heading < -kPi) heading += 2.0f * kPi;

    const bool eating = distanceSquared(next, food) < 36.0f;
    const int newTarget = eating ? min(kMaxPoints, targetLength + kGrowth)
                                 : targetLength;
    const bool growing = bodyLength < newTarget;
    // The last point vacates this step unless the snake is growing.
    const int retainedPoints = growing ? bodyLength : bodyLength - 1;
    for (int i = kNeckPoints; i < retainedPoints; ++i) {
        if (distanceSquared(next, bodyAt(i)) < 16.0f) {
            finish(State::Crashed);
            return;
        }
    }

    headIndex = (headIndex + kMaxPoints - 1) % kMaxPoints;
    body[headIndex] = next;
    targetLength = newTarget;
    if (growing) ++bodyLength;

    if (eating) {
        ++score;
        if (targetLength == kMaxPoints || !placeFood()) finish(State::Won);
    }
}

void drawWrappedCircle(Point p, int radius, uint16_t color) {
    const int x = static_cast<int>(p.x);
    const int y = static_cast<int>(p.y);
    // Duplicate only circles that overlap a seam. Clip at the playfield,
    // so circles never paint over the HUD at the upper edge.
    for (int oy = -kFieldHeight; oy <= kFieldHeight; oy += kFieldHeight) {
        if (y + oy + radius < 0 || y + oy - radius >= kFieldHeight) continue;
        for (int ox = -kWidth; ox <= kWidth; ox += kWidth) {
            if (x + ox + radius < 0 || x + ox - radius >= kWidth) continue;
            frame.fillCircle(x + ox, y + oy, radius, color);
        }
    }
}

void drawGame() {
    // Render in field coordinates; the viewport translates below the HUD.
    frame.fillSprite(TFT_BLACK);
    frame.setViewport(0, kHudHeight, kWidth, kFieldHeight);
    drawWrappedCircle(food, kFoodRadius + 1, TFT_MAROON);
    drawWrappedCircle(food, kFoodRadius, TFT_ORANGE);
    for (int i = bodyLength - 1; i >= 0; --i) {
        drawWrappedCircle(bodyAt(i), kBodyRadius,
                          i < 8 ? TFT_GREENYELLOW : TFT_GREEN);
    }
    const Point head = bodyAt(0);
    drawWrappedCircle(head, kBodyRadius + 1, TFT_GREENYELLOW);
    const Point eye = wrapped({head.x + 2.0f * cosf(heading),
                               head.y + 2.0f * sinf(heading)});
    drawWrappedCircle(eye, 1, TFT_BLACK);
    frame.resetViewport();

    frame.fillRect(0, 0, kWidth, kHudHeight, TFT_NAVY);
    frame.setTextColor(TFT_WHITE, TFT_NAVY);
    frame.setTextDatum(TL_DATUM);
    char text[32];
    snprintf(text, sizeof(text), "SCORE %u", score);
    frame.drawString(text, 3, 1, 1);
    snprintf(text, sizeof(text), "BEST %u", bestScore);
    frame.drawString(text, 3, 11, 1);
    frame.drawString(turn > 0 ? "CW" : "CCW", 61, 6, 1);
    drawBattery();

    if (state != State::Running) {
        frame.fillRoundRect(5, 79, kWidth - 10, 87, 5, TFT_NAVY);
        frame.setTextColor(TFT_WHITE, TFT_NAVY);
        frame.drawCentreString(state == State::Won ? "YOU WIN!" : "GAME OVER",
                               kWidth / 2, 87, 2);
        snprintf(text, sizeof(text), "SCORE %u", score);
        frame.drawCentreString(text, kWidth / 2, 109, 2);
        frame.drawCentreString("Press any button", kWidth / 2, 135, 1);
        frame.drawCentreString("to restart", kWidth / 2, 148, 1);
    }
    frame.pushSprite(0, 0);
}

}  // namespace

void setup() {
    Serial.begin(115200);
    pinMode(kBacklightPin, OUTPUT);
    digitalWrite(kBacklightPin, HIGH);
    pinMode(kButtonPins[0], INPUT_PULLUP);
    pinMode(kButtonPins[1], INPUT);  // GPIO35 uses the board's external pull-up.
    pinMode(kBatteryEnablePin, OUTPUT);
    digitalWrite(kBatteryEnablePin, HIGH);
    analogReadResolution(12);
    analogSetPinAttenuation(kBatteryAdcPin, ADC_11db);
    delay(5);  // Let the enabled voltage divider settle before the first reading.
    tft.init();
    tft.setRotation(0);
    frame.setColorDepth(16);
    if (frame.createSprite(kWidth, kHeight) == nullptr) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("No frame memory", 3, 100, 2);
        while (true) delay(1000);
    }
    randomSeed(static_cast<uint32_t>(micros()));
    prefsReady = prefs.begin("snake", false);
    if (prefsReady) {
        bestScore = prefs.getUInt("bestScore", 0);
    } else {
        Serial.println("Failed to open snake score storage");
    }
    const uint32_t now = millis();
    for (int i = 0; i < 2; ++i) {
        buttons[i].raw = buttons[i].stable = digitalRead(kButtonPins[i]) == LOW;
        buttons[i].changedAt = now;
    }
    resetGame(now);
    updateBattery(now);
    lastFrameAt = now;
    drawGame();
}

void loop() {
    const uint32_t now = millis();
    handleInput(now);
    updateBattery(now);
    // Limit catch-up after a stall while keeping physics steps fixed.
    if (now - lastStepAt > 5 * kStepMs) lastStepAt = now - 5 * kStepMs;
    while (now - lastStepAt >= kStepMs) {
        lastStepAt += kStepMs;
        if (state == State::Running) updateGame();
    }
    if (now - lastFrameAt >= kFrameMs) {
        lastFrameAt = now;
        drawGame();
    }
    delay(1);
}
