#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

namespace {

constexpr int kScreenW = 135;
constexpr int kScreenH = 240;

constexpr int kBacklightPin = 4;
constexpr int kBtnLeftPin = 0;    // Physical left in portrait
constexpr int kBtnRightPin = 35;  // Physical right in portrait

constexpr int kRoadX = 14;
constexpr int kRoadW = 107;
constexpr int kRoadTop = 20;
constexpr int kRoadBottom = kScreenH;
constexpr int kRoadH = kRoadBottom - kRoadTop;

constexpr int kLaneCount = 3;
constexpr int kLaneGap = kRoadW / kLaneCount;

constexpr int kPlayerW = 20;
constexpr int kPlayerH = 30;
constexpr int kEnemyW = 20;
constexpr int kEnemyH = 30;
constexpr int kNitroSize = 12;

constexpr uint32_t kFrameMs = 33;
constexpr float kBaseSpeed = 2.7f;
constexpr float kNitroExtraSpeed = 2.2f;
constexpr float kNitroDrainPerSecond = 28.0f;

constexpr int kMaxObstacles = 6;

struct Obstacle {
    int lane = 0;
    float y = 0;
    bool active = false;
};

struct NitroPickup {
    int lane = 0;
    float y = 0;
    bool active = false;
};

enum class GameState {
    Running,
    Crashed,
};

TFT_eSPI tft;
TFT_eSprite frame = TFT_eSprite(&tft);
Obstacle obstacles[kMaxObstacles];
NitroPickup nitro;
Preferences prefs;

GameState gameState = GameState::Running;

int playerLane = 1;
float lineScroll = 0.0f;
float nitroGauge = 25.0f;  // 0..100
uint32_t score = 0;
uint32_t bestScore = 0;

bool leftPressed = false;
bool rightPressed = false;
bool leftPressedPrev = false;
bool rightPressedPrev = false;

uint32_t lastFrameAt = 0;
uint32_t nextObstacleAt = 0;
uint32_t nextNitroAt = 0;

int laneCenterX(int lane) {
    return kRoadX + lane * kLaneGap + (kLaneGap / 2);
}

int playerX() {
    return laneCenterX(playerLane) - (kPlayerW / 2);
}

void setBacklight(bool enabled) {
    pinMode(kBacklightPin, OUTPUT);
    digitalWrite(kBacklightPin, enabled ? HIGH : LOW);
}

bool readButtonPressed(int pin) {
    return digitalRead(pin) == LOW;
}

void saveBestScore() {
    if (score > bestScore) {
        bestScore = score;
        prefs.putUInt("bestScore", bestScore);
    }
}

void loadBestScore() {
    bestScore = prefs.getUInt("bestScore", 0);
}

void resetGame() {
    playerLane = 1;
    lineScroll = 0.0f;
    nitroGauge = 25.0f;
    score = 0;
    nitro.active = false;

    for (int i = 0; i < kMaxObstacles; ++i) {
        obstacles[i].active = false;
    }

    const uint32_t now = millis();
    nextObstacleAt = now + 900;
    nextNitroAt = now + 3000;
    gameState = GameState::Running;
}

bool intersects(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

void spawnObstacle() {
    for (int i = 0; i < kMaxObstacles; ++i) {
        if (!obstacles[i].active) {
            obstacles[i].active = true;
            obstacles[i].lane = random(0, kLaneCount);
            obstacles[i].y = static_cast<float>(kRoadTop - kEnemyH - 2);
            return;
        }
    }
}

void maybeSpawnNitro() {
    if (nitro.active) {
        return;
    }

    nitro.active = true;
    nitro.lane = random(0, kLaneCount);
    nitro.y = static_cast<float>(kRoadTop - kNitroSize - 2);
}

void handleInput() {
    leftPressed = readButtonPressed(kBtnLeftPin);
    rightPressed = readButtonPressed(kBtnRightPin);

    if (gameState == GameState::Crashed) {
        if (leftPressed && rightPressed) {
            saveBestScore();
            resetGame();
        }
        leftPressedPrev = leftPressed;
        rightPressedPrev = rightPressed;
        return;
    }

    const bool bothPressed = leftPressed && rightPressed;

    if (!bothPressed) {
        if (leftPressed && !leftPressedPrev && playerLane > 0) {
            --playerLane;
        }
        if (rightPressed && !rightPressedPrev && playerLane < (kLaneCount - 1)) {
            ++playerLane;
        }
    }

    leftPressedPrev = leftPressed;
    rightPressedPrev = rightPressed;
}

void updateGame(float dtSeconds) {
    if (gameState != GameState::Running) {
        return;
    }

    const bool nitroActive = leftPressed && rightPressed && nitroGauge > 0.8f;
    float speed = kBaseSpeed;

    if (nitroActive) {
        speed += kNitroExtraSpeed;
        nitroGauge -= (kNitroDrainPerSecond * dtSeconds);
        if (nitroGauge < 0.0f) {
            nitroGauge = 0.0f;
        }
    }

    lineScroll += speed;
    if (lineScroll >= 24.0f) {
        lineScroll -= 24.0f;
    }

    score += static_cast<uint32_t>(speed * 8.0f);

    const uint32_t now = millis();
    if (now >= nextObstacleAt) {
        spawnObstacle();
        nextObstacleAt = now + random(1200, 1800);
    }

    if (now >= nextNitroAt) {
        maybeSpawnNitro();
        nextNitroAt = now + random(3200, 5400);
    }

    const int px = playerX();
    const int py = kRoadBottom - kPlayerH - 10;

    for (int i = 0; i < kMaxObstacles; ++i) {
        if (!obstacles[i].active) {
            continue;
        }

        obstacles[i].y += speed;
        if (obstacles[i].y > kRoadBottom + 2) {
            obstacles[i].active = false;
            continue;
        }

        const int ox = laneCenterX(obstacles[i].lane) - (kEnemyW / 2);
        const int oy = static_cast<int>(obstacles[i].y);

        if (intersects(px, py, kPlayerW, kPlayerH, ox, oy, kEnemyW, kEnemyH)) {
            gameState = GameState::Crashed;
        }
    }

    if (nitro.active) {
        nitro.y += speed;
        if (nitro.y > kRoadBottom + 2) {
            nitro.active = false;
        } else {
            const int nx = laneCenterX(nitro.lane) - (kNitroSize / 2);
            const int ny = static_cast<int>(nitro.y);
            if (intersects(px, py, kPlayerW, kPlayerH, nx, ny, kNitroSize, kNitroSize)) {
                nitroGauge += 30.0f;
                if (nitroGauge > 100.0f) {
                    nitroGauge = 100.0f;
                }
                nitro.active = false;
            }
        }
    }
}

void drawRoad() {
    frame.fillRect(0, 0, kScreenW, kScreenH, TFT_DARKGREY);
    frame.fillRect(kRoadX, kRoadTop, kRoadW, kRoadH, TFT_BLACK);

    frame.drawFastVLine(kRoadX, kRoadTop, kRoadH, TFT_WHITE);
    frame.drawFastVLine(kRoadX + kRoadW - 1, kRoadTop, kRoadH, TFT_WHITE);

    const int dashW = 4;
    const int dashH = 14;
    const int center1X = kRoadX + kLaneGap - (dashW / 2);
    const int center2X = kRoadX + 2 * kLaneGap - (dashW / 2);

    for (int y = kRoadTop + static_cast<int>(lineScroll); y < kRoadBottom; y += 24) {
        frame.fillRect(center1X, y, dashW, dashH, TFT_WHITE);
        frame.fillRect(center2X, y, dashW, dashH, TFT_WHITE);
    }
}

void drawCar(int x, int y, uint16_t bodyColor, uint16_t windowColor) {
    frame.fillRoundRect(x, y, kPlayerW, kPlayerH, 3, bodyColor);
    frame.fillRoundRect(x + 4, y + 4, kPlayerW - 8, 9, 2, windowColor);
    frame.fillRect(x + 4, y + kPlayerH - 4, kPlayerW - 8, 2, TFT_BLACK);
}

void drawHud() {
    frame.fillRect(0, 0, kScreenW, 18, TFT_NAVY);

    char scoreText[24];
    snprintf(scoreText, sizeof(scoreText), "DIST %lu", static_cast<unsigned long>(score));
    frame.setTextColor(TFT_WHITE, TFT_NAVY);
    frame.setTextDatum(TL_DATUM);
    frame.drawString(scoreText, 4, 4, 2);

    const int barX = 72;
    const int barY = 4;
    const int barW = 56;
    const int barH = 10;
    const int fillW = static_cast<int>((nitroGauge / 100.0f) * barW);

    frame.drawRect(barX, barY, barW, barH, TFT_CYAN);
    frame.fillRect(barX + 1, barY + 1, barW - 2, barH - 2, TFT_BLACK);
    if (fillW > 1) {
        frame.fillRect(barX + 1, barY + 1, fillW - 1, barH - 2, TFT_CYAN);
    }
}

void drawGame() {
    drawRoad();

    if (nitro.active) {
        const int nx = laneCenterX(nitro.lane) - (kNitroSize / 2);
        const int ny = static_cast<int>(nitro.y);
        frame.fillRoundRect(nx, ny, kNitroSize, kNitroSize, 2, TFT_CYAN);
        frame.setTextColor(TFT_BLACK, TFT_CYAN);
        frame.drawCentreString("N", nx + (kNitroSize / 2), ny + 2, 2);
    }

    for (int i = 0; i < kMaxObstacles; ++i) {
        if (!obstacles[i].active) {
            continue;
        }

        const int ox = laneCenterX(obstacles[i].lane) - (kEnemyW / 2);
        const int oy = static_cast<int>(obstacles[i].y);
        drawCar(ox, oy, TFT_RED, TFT_SILVER);
    }

    const int px = playerX();
    const int py = kRoadBottom - kPlayerH - 10;
    drawCar(px, py, TFT_GREEN, TFT_SKYBLUE);

    drawHud();

    if (gameState == GameState::Crashed) {
        frame.fillRoundRect(8, 80, kScreenW - 16, 80, 6, TFT_MAROON);
        frame.setTextColor(TFT_WHITE, TFT_MAROON);
        frame.drawCentreString("CRASH!", kScreenW / 2, 90, 4);

        char bestText[24];
        snprintf(bestText, sizeof(bestText), "BEST %lu", static_cast<unsigned long>(bestScore));
        frame.drawCentreString(bestText, kScreenW / 2, 112, 2);

        frame.drawCentreString("Both buttons", kScreenW / 2, 135, 2);
        frame.drawCentreString("to restart", kScreenW / 2, 151, 2);
    }

    frame.pushSprite(0, 0);
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(150);

    setBacklight(true);

    pinMode(kBtnLeftPin, INPUT_PULLUP);
    pinMode(kBtnRightPin, INPUT);

    tft.init();
    tft.setRotation(0);  // portrait mode

    frame.setColorDepth(16);
    frame.createSprite(kScreenW, kScreenH);
    frame.fillSprite(TFT_BLACK);
    frame.pushSprite(0, 0);

    prefs.begin("racing", false);
    loadBestScore();

    randomSeed(static_cast<uint32_t>(micros()));
    resetGame();
    lastFrameAt = millis();
}

void loop() {
    handleInput();

    const uint32_t now = millis();
    if (now - lastFrameAt < kFrameMs) {
        delay(1);
        return;
    }

    const float dtSeconds = static_cast<float>(now - lastFrameAt) / 1000.0f;
    lastFrameAt = now;

    updateGame(dtSeconds);
    drawGame();
}
