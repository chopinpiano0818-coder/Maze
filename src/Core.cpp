#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

const int SCREEN_WIDTH = 1000;
const int SCREEN_HEIGHT = 700;

int MAZE_CELLS = 8;
int MAZE_WIDTH = MAZE_CELLS * 2 + 1;
int MAZE_HEIGHT = MAZE_CELLS * 2 + 1;
const int MAP_VIEW_RADIUS = 5;

const double PI = 3.14159265358979323846;
const char* BUILD_VERSION = "MAZE-MODULAR-LIGHT-V7-2026-07-29";
const int RAY_STEP = 2;
const int SPRITE_STEP = 2;

enum class GameState {
    HOME,
    GAME,
    CLEAR,
    GAME_OVER
};

enum class OverlayState {
    NONE,
    PAUSE,
    SETTINGS,
    DIFFICULTY,
    RULES
};

enum class SettingsReturnTarget {
    HOME,
    PAUSE
};

enum class ControlMode {
    PC,
    MOBILE
};

enum class Difficulty {
    EASY,
    SLIGHTLY_EASY,
    NORMAL,
    SLIGHTLY_HARD,
    HARD
};

struct DifficultyConfig {
    int cells;
    int chaserCount;
    int verticalCount;
    int horizontalCount;
    int trapCount;
    bool itemsEnabled;
    double badItemChance;
    int requiredKeys;
    const char* name;
};

Difficulty selectedDifficulty = Difficulty::NORMAL;
bool difficultyLocked = false;

DifficultyConfig getDifficultyConfig(Difficulty d) {
    switch (d) {
        case Difficulty::EASY:          return {6, 1, 0, 0, 0, true, 0.28, 1, "簡単"};
        case Difficulty::SLIGHTLY_EASY: return {8, 1, 1, 0, 0, true, 0.30, 1, "やや簡単"};
        case Difficulty::NORMAL:        return {11,1, 1, 1, 4, true, 0.38, 1, "普通"};
        case Difficulty::SLIGHTLY_HARD: return {17,1, 2, 2, 7, true, 0.48, 2, "やや難関"};
        case Difficulty::HARD:          return {25,2, 3, 3, 13,true, 0.68, 3, "難関"};
    }
    return {11,1,1,1,4,true,0.38,1,"普通"};
}


struct Button {
    SDL_Rect rect;
    std::string text;
};

std::vector<std::vector<int>> maze;

std::vector<std::vector<bool>> visited;

std::mt19937 randomEngine(std::random_device{}());

SDL_Texture* wallTexture = nullptr;
SDL_Texture* enemyTexture = nullptr;
int wallTextureWidth = 0;
int wallTextureHeight = 0;
int enemyTextureWidth = 0;
int enemyTextureHeight = 0;
std::vector<double> depthBuffer(SCREEN_WIDTH, 1e30);

double enemyX = 0.0;
double enemyY = 0.0;
double enemySpeed = 0.62;
double enemyAngle = 0.0;
int enemyWanderTargetX = -1;
int enemyWanderTargetY = -1;
Uint32 enemyWanderRetargetAt = 0;
int enemyHealth = 2;
bool enemyAlive = true;
struct ExtraChaser { double x=0.0, y=0.0, angle=0.0; int health=2; bool alive=true; };
std::vector<ExtraChaser> extraChasers;

struct PatrolEnemy {
    double x = 0.0;
    double y = 0.0;
    double startX = 0.0;
    double startY = 0.0;
    double endX = 0.0;
    double endY = 0.0;
    double speed = 1.55;
    int direction = 1;
    bool vertical = false;
};

struct FloorTrap {
    double x = 0.0;
    double y = 0.0;
    bool usedByPlayer = false;
    bool usedByEnemy = false;
};

enum class ItemEffect { SPEED_UP, SPEED_DOWN, DAMAGE, HEAL };
struct RandomItem {
    double x = 0.0;
    double y = 0.0;
    ItemEffect effect = ItemEffect::SPEED_UP;
    bool available = true;
};

std::vector<PatrolEnemy> patrolEnemies;
std::vector<FloorTrap> floorTraps;
std::vector<RandomItem> randomItems;

double jumpHeight = 0.0;
double jumpVelocity = 0.0;
bool playerOnGround = true;
Uint32 speedEffectUntil = 0;
double speedEffectMultiplier = 1.0;

double playerX = 1.5;
double playerY = 1.5;
double playerAngle = 0.0;
double cameraPitch = 0.0;
bool playerCrouching = false;
int playerHearts = 3;
Uint32 playerDamageCooldownUntil = 0;

// 視点を滑らかにするための回転速度
double cameraYawVelocity = 0.0;
double cameraPitchVelocity = 0.0;

// モバイル右側スワイプ用
bool mobileLookActive = false;
SDL_FingerID mobileLookFingerId = 0;
float mobileLookLastX = 0.0f;
float mobileLookLastY = 0.0f;

int goalX = MAZE_WIDTH - 2;
int goalY = MAZE_HEIGHT - 2;
int score = 0;

double mouseSensitivity = 0.0015;
const double MIN_SENSITIVITY = 0.00035;
const double MAX_SENSITIVITY = 0.0032;
bool sensitivityDragging = false;

int brightnessLevel = 1;
double brightnessMultiplier = 1.0;

ControlMode controlMode = ControlMode::PC;

bool mapOpen = false;
OverlayState overlayState = OverlayState::NONE;
SettingsReturnTarget settingsReturnTarget = SettingsReturnTarget::HOME;

Uint32 saveMessageUntil = 0;

// 鍵とゴール操作
struct KeyItem {
    double x = 0.0;
    double y = 0.0;
    SDL_Color color{255, 215, 40, 255};
    std::string name;
    bool available = true;
    bool collected = false;
};
std::vector<KeyItem> keysInMaze;
Uint32 actionMessageUntil = 0;
std::string actionMessage;

bool mobileUpHeld = false;
bool mobileDownHeld = false;
bool mobileLeftHeld = false;
bool mobileRightHeld = false;
bool mobileRunning = false;

Uint32 previousForwardTapTime = 0;
Uint32 currentForwardPressStart = 0;
bool secondForwardTap = false;

TTF_Font* openFont(int size) {
    const char* fontPaths[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJKjp-Regular.otf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    };

    for (const char* path : fontPaths) {
        TTF_Font* font = TTF_OpenFont(path, size);
        if (font != nullptr) {
            return font;
        }
    }

    return nullptr;
}

void drawText(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const std::string& text,
    int centerX,
    int centerY,
    SDL_Color color = {255, 255, 255, 255}
) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(
        font,
        text.c_str(),
        color
    );

    if (surface == nullptr) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(
        renderer,
        surface
    );

    if (texture == nullptr) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect destination = {
        centerX - surface->w / 2,
        centerY - surface->h / 2,
        surface->w,
        surface->h
    };

    SDL_RenderCopy(renderer, texture, nullptr, &destination);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

bool pointInside(const SDL_Rect& rect, int x, int y) {
    return
        x >= rect.x &&
        x < rect.x + rect.w &&
        y >= rect.y &&
        y < rect.y + rect.h;
}

void drawButton(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const Button& button,
    bool selected = false
) {
    int mouseX = 0;
    int mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);

    bool hovered = pointInside(button.rect, mouseX, mouseY);

    if (selected) {
        SDL_SetRenderDrawColor(renderer, 85, 125, 190, 255);
    } else if (hovered) {
        SDL_SetRenderDrawColor(renderer, 75, 100, 150, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 48, 60, 88, 255);
    }

    SDL_RenderFillRect(renderer, &button.rect);

    SDL_SetRenderDrawColor(renderer, 205, 215, 235, 255);
    SDL_RenderDrawRect(renderer, &button.rect);

    drawText(
        renderer,
        font,
        button.text,
        button.rect.x + button.rect.w / 2,
        button.rect.y + button.rect.h / 2
    );
}

