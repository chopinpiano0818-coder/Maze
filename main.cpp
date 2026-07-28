#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

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

const int MAZE_WIDTH = 17;
const int MAZE_HEIGHT = 17;
const int MAP_VIEW_RADIUS = 5;

const double PI = 3.14159265358979323846;
const char* BUILD_VERSION = "FINAL-V3-2026-07-29";

enum class GameState {
    HOME,
    GAME,
    CLEAR
};

enum class OverlayState {
    NONE,
    PAUSE,
    SETTINGS
};

enum class SettingsReturnTarget {
    HOME,
    PAUSE
};

enum class ControlMode {
    PC,
    MOBILE
};

struct Button {
    SDL_Rect rect;
    std::string text;
};

std::vector<std::vector<int>> maze(
    MAZE_HEIGHT,
    std::vector<int>(MAZE_WIDTH, 1)
);

std::vector<std::vector<bool>> visited(
    MAZE_HEIGHT,
    std::vector<bool>(MAZE_WIDTH, false)
);

std::mt19937 randomEngine(std::random_device{}());

double playerX = 1.5;
double playerY = 1.5;
double playerAngle = 0.0;
double cameraPitch = 0.0;

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

double mouseSensitivity = 0.0025;
int sensitivityLevel = 1;

int brightnessLevel = 1;
double brightnessMultiplier = 1.0;

ControlMode controlMode = ControlMode::PC;

bool mapOpen = false;
OverlayState overlayState = OverlayState::NONE;
SettingsReturnTarget settingsReturnTarget = SettingsReturnTarget::HOME;

Uint32 saveMessageUntil = 0;

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

bool isInsideMaze(int x, int y) {
    return
        x > 0 &&
        x < MAZE_WIDTH - 1 &&
        y > 0 &&
        y < MAZE_HEIGHT - 1;
}

void generateMazeFrom(int x, int y) {
    maze[y][x] = 0;

    std::vector<std::pair<int, int>> directions = {
        {0, -2},
        {0, 2},
        {-2, 0},
        {2, 0}
    };

    std::shuffle(
        directions.begin(),
        directions.end(),
        randomEngine
    );

    for (const auto& direction : directions) {
        int nextX = x + direction.first;
        int nextY = y + direction.second;

        if (!isInsideMaze(nextX, nextY)) {
            continue;
        }

        if (maze[nextY][nextX] == 0) {
            continue;
        }

        int wallX = x + direction.first / 2;
        int wallY = y + direction.second / 2;

        maze[wallY][wallX] = 0;
        generateMazeFrom(nextX, nextY);
    }
}

void addModerateExtraBranches() {
    std::vector<std::pair<int, int>> removableWalls;

    for (int y = 1; y < MAZE_HEIGHT - 1; y++) {
        for (int x = 1; x < MAZE_WIDTH - 1; x++) {
            if (maze[y][x] != 1) {
                continue;
            }

            bool horizontalPassage =
                maze[y][x - 1] == 0 &&
                maze[y][x + 1] == 0;

            bool verticalPassage =
                maze[y - 1][x] == 0 &&
                maze[y + 1][x] == 0;

            if (horizontalPassage || verticalPassage) {
                removableWalls.push_back({x, y});
            }
        }
    }

    std::shuffle(
        removableWalls.begin(),
        removableWalls.end(),
        randomEngine
    );

    int extraOpenings = std::max(
        2,
        static_cast<int>(removableWalls.size() * 0.08)
    );

    extraOpenings = std::min(
        extraOpenings,
        static_cast<int>(removableWalls.size())
    );

    for (int i = 0; i < extraOpenings; i++) {
        maze[removableWalls[i].second][removableWalls[i].first] = 0;
    }
}

void generateMaze() {
    for (int y = 0; y < MAZE_HEIGHT; y++) {
        for (int x = 0; x < MAZE_WIDTH; x++) {
            maze[y][x] = 1;
            visited[y][x] = false;
        }
    }

    generateMazeFrom(1, 1);
    addModerateExtraBranches();

    playerX = 1.5;
    playerY = 1.5;
    playerAngle = 0.0;
    cameraPitch = 0.0;
    cameraYawVelocity = 0.0;
    cameraPitchVelocity = 0.0;

    goalX = MAZE_WIDTH - 2;
    goalY = MAZE_HEIGHT - 2;

    maze[1][1] = 0;
    maze[goalY][goalX] = 0;

    score = 0;
    visited[1][1] = true;

    mapOpen = false;
    overlayState = OverlayState::NONE;
}

bool canMoveTo(double x, double y) {
    const double PLAYER_RADIUS = 0.18;

    double checkX[] = {
        x - PLAYER_RADIUS,
        x + PLAYER_RADIUS,
        x - PLAYER_RADIUS,
        x + PLAYER_RADIUS
    };

    double checkY[] = {
        y - PLAYER_RADIUS,
        y - PLAYER_RADIUS,
        y + PLAYER_RADIUS,
        y + PLAYER_RADIUS
    };

    for (int i = 0; i < 4; i++) {
        int mapX = static_cast<int>(checkX[i]);
        int mapY = static_cast<int>(checkY[i]);

        if (
            mapX < 0 ||
            mapX >= MAZE_WIDTH ||
            mapY < 0 ||
            mapY >= MAZE_HEIGHT
        ) {
            return false;
        }

        if (maze[mapY][mapX] == 1) {
            return false;
        }
    }

    return true;
}

void updateVisitedScore() {
    int cellX = static_cast<int>(playerX);
    int cellY = static_cast<int>(playerY);

    if (
        cellX < 0 ||
        cellX >= MAZE_WIDTH ||
        cellY < 0 ||
        cellY >= MAZE_HEIGHT
    ) {
        return;
    }

    if (
        maze[cellY][cellX] == 0 &&
        !visited[cellY][cellX]
    ) {
        visited[cellY][cellX] = true;
        score++;
    }
}

void tryMove(double moveX, double moveY) {
    double nextX = playerX + moveX;
    double nextY = playerY + moveY;

    if (canMoveTo(nextX, playerY)) {
        playerX = nextX;
    }

    if (canMoveTo(playerX, nextY)) {
        playerY = nextY;
    }

    updateVisitedScore();
}

void updatePlayer(double deltaTime) {
    if (
        mapOpen ||
        overlayState != OverlayState::NONE
    ) {
        return;
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);

    bool keyboardForward =
        keys[SDL_SCANCODE_W] ||
        keys[SDL_SCANCODE_UP];

    bool keyboardRun =
        keys[SDL_SCANCODE_W] &&
        keys[SDL_SCANCODE_Q];

    bool forwardPressed =
        keyboardForward ||
        mobileUpHeld;

    bool runningForward =
        keyboardRun ||
        mobileRunning;

    double forwardX = std::cos(playerAngle);
    double forwardY = std::sin(playerAngle);

    double rightX = -std::sin(playerAngle);
    double rightY = std::cos(playerAngle);

    double forwardSpeed = runningForward ? 5.5 : 3.0;
    double normalSpeed = 3.0;

    if (forwardPressed) {
        double movement = forwardSpeed * deltaTime;

        tryMove(
            forwardX * movement,
            forwardY * movement
        );
    }

    if (
        keys[SDL_SCANCODE_DOWN] ||
        mobileDownHeld
    ) {
        double movement = normalSpeed * deltaTime;

        tryMove(
            -forwardX * movement,
            -forwardY * movement
        );
    }

    if (
        keys[SDL_SCANCODE_A] ||
        keys[SDL_SCANCODE_LEFT] ||
        mobileLeftHeld
    ) {
        double movement = normalSpeed * deltaTime;

        tryMove(
            -rightX * movement,
            -rightY * movement
        );
    }

    if (
        keys[SDL_SCANCODE_D] ||
        keys[SDL_SCANCODE_RIGHT] ||
        mobileRightHeld
    ) {
        double movement = normalSpeed * deltaTime;

        tryMove(
            rightX * movement,
            rightY * movement
        );
    }
}

void addLookInput(double movementX, double movementY, double scale = 1.0) {
    if (
        mapOpen ||
        overlayState != OverlayState::NONE
    ) {
        return;
    }

    cameraYawVelocity +=
        movementX * mouseSensitivity * scale;

    cameraPitchVelocity -=
        movementY * 0.55 * scale;
}

void updateSmoothCamera(double deltaTime) {
    if (
        mapOpen ||
        overlayState != OverlayState::NONE
    ) {
        cameraYawVelocity = 0.0;
        cameraPitchVelocity = 0.0;
        return;
    }

    // フレームレートが変わっても同じ滑らかさになる減衰
    double damping = std::pow(0.0008, deltaTime);

    playerAngle += cameraYawVelocity;
    cameraPitch += cameraPitchVelocity;

    cameraYawVelocity *= damping;
    cameraPitchVelocity *= damping;

    if (std::abs(cameraYawVelocity) < 0.00001) {
        cameraYawVelocity = 0.0;
    }

    if (std::abs(cameraPitchVelocity) < 0.01) {
        cameraPitchVelocity = 0.0;
    }

    cameraPitch = std::clamp(
        cameraPitch,
        -190.0,
        190.0
    );

    while (playerAngle < 0.0) {
        playerAngle += PI * 2.0;
    }

    while (playerAngle >= PI * 2.0) {
        playerAngle -= PI * 2.0;
    }
}

int getHorizonY() {
    int horizonY =
        SCREEN_HEIGHT / 2 +
        static_cast<int>(cameraPitch);

    return std::clamp(
        horizonY,
        80,
        SCREEN_HEIGHT - 80
    );
}

void draw3DView(SDL_Renderer* renderer) {
    int horizonY = getHorizonY();

    SDL_SetRenderDrawColor(renderer, 5, 7, 11, 255);

    SDL_Rect ceiling = {
        0,
        0,
        SCREEN_WIDTH,
        horizonY
    };

    SDL_RenderFillRect(renderer, &ceiling);

    int baseFloor =
        static_cast<int>(14 * brightnessMultiplier);

    SDL_SetRenderDrawColor(
        renderer,
        baseFloor,
        baseFloor,
        baseFloor,
        255
    );

    SDL_Rect floor = {
        0,
        horizonY,
        SCREEN_WIDTH,
        SCREEN_HEIGHT - horizonY
    };

    SDL_RenderFillRect(renderer, &floor);

    const double fieldOfView = PI / 3.0;

    double directionX = std::cos(playerAngle);
    double directionY = std::sin(playerAngle);

    double planeLength = std::tan(fieldOfView / 2.0);

    double planeX = -directionY * planeLength;
    double planeY = directionX * planeLength;

    for (int screenX = 0; screenX < SCREEN_WIDTH; screenX++) {
        double cameraX =
            2.0 *
            screenX /
            static_cast<double>(SCREEN_WIDTH) -
            1.0;

        double rayDirectionX =
            directionX +
            planeX * cameraX;

        double rayDirectionY =
            directionY +
            planeY * cameraX;

        int mapX = static_cast<int>(playerX);
        int mapY = static_cast<int>(playerY);

        double deltaDistanceX =
            rayDirectionX == 0.0
            ? 1e30
            : std::abs(1.0 / rayDirectionX);

        double deltaDistanceY =
            rayDirectionY == 0.0
            ? 1e30
            : std::abs(1.0 / rayDirectionY);

        int stepX;
        int stepY;

        double sideDistanceX;
        double sideDistanceY;

        if (rayDirectionX < 0.0) {
            stepX = -1;
            sideDistanceX =
                (playerX - mapX) *
                deltaDistanceX;
        } else {
            stepX = 1;
            sideDistanceX =
                (mapX + 1.0 - playerX) *
                deltaDistanceX;
        }

        if (rayDirectionY < 0.0) {
            stepY = -1;
            sideDistanceY =
                (playerY - mapY) *
                deltaDistanceY;
        } else {
            stepY = 1;
            sideDistanceY =
                (mapY + 1.0 - playerY) *
                deltaDistanceY;
        }

        bool hit = false;
        int wallSide = 0;

        while (!hit) {
            if (sideDistanceX < sideDistanceY) {
                sideDistanceX += deltaDistanceX;
                mapX += stepX;
                wallSide = 0;
            } else {
                sideDistanceY += deltaDistanceY;
                mapY += stepY;
                wallSide = 1;
            }

            if (
                mapX < 0 ||
                mapX >= MAZE_WIDTH ||
                mapY < 0 ||
                mapY >= MAZE_HEIGHT
            ) {
                hit = true;
                break;
            }

            if (maze[mapY][mapX] == 1) {
                hit = true;
            }
        }

        double wallDistance =
            wallSide == 0
            ? sideDistanceX - deltaDistanceX
            : sideDistanceY - deltaDistanceY;

        wallDistance = std::max(
            0.01,
            wallDistance
        );

        int wallHeight =
            static_cast<int>(
                SCREEN_HEIGHT /
                wallDistance
            );

        int wallTop =
            horizonY -
            wallHeight / 2;

        int wallBottom =
            horizonY +
            wallHeight / 2;

        wallTop = std::max(0, wallTop);
        wallBottom = std::min(
            SCREEN_HEIGHT - 1,
            wallBottom
        );

        double distanceBrightness =
            1.0 /
            (1.0 + wallDistance * 0.22);

        int brightness =
            static_cast<int>(
                (
                    18 +
                    distanceBrightness * 78
                ) *
                brightnessMultiplier
            );

        if (wallSide == 1) {
            brightness =
                static_cast<int>(
                    brightness * 0.70
                );
        }

        brightness = std::clamp(
            brightness,
            8,
            125
        );

        SDL_SetRenderDrawColor(
            renderer,
            brightness,
            brightness,
            std::min(
                255,
                brightness + 9
            ),
            255
        );

        SDL_RenderDrawLine(
            renderer,
            screenX,
            wallTop,
            screenX,
            wallBottom
        );
    }
}

void drawNaturalGroundLight(SDL_Renderer* renderer) {
    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_ADD
    );

    int horizonY = getHorizonY();

    int startY = horizonY + 8;
    int endY = SCREEN_HEIGHT - 18;

    if (startY >= endY) {
        SDL_SetRenderDrawBlendMode(
            renderer,
            SDL_BLENDMODE_BLEND
        );
        return;
    }

    int centerX = SCREEN_WIDTH / 2;

    for (int y = startY; y <= endY; y++) {
        double depth =
            static_cast<double>(
                y - startY
            ) /
            static_cast<double>(
                endY - startY
            );

        double nearAmount =
            std::pow(depth, 1.35);

        int halfWidth =
            static_cast<int>(
                470 -
                365 * nearAmount
            );

        double edgeSoftness = 0.32;
        int coreHalfWidth =
            static_cast<int>(
                halfWidth *
                (1.0 - edgeSoftness)
            );

        int maxAlpha =
            static_cast<int>(
                (
                    6 +
                    76 *
                    std::pow(depth, 1.75)
                ) *
                brightnessMultiplier
            );

        maxAlpha = std::clamp(
            maxAlpha,
            2,
            120
        );

        for (int x = centerX - halfWidth;
             x <= centerX + halfWidth;
             x++) {
            if (x < 0 || x >= SCREEN_WIDTH) {
                continue;
            }

            int distanceFromCenter =
                std::abs(x - centerX);

            double horizontalStrength = 1.0;

            if (distanceFromCenter > coreHalfWidth) {
                double fadePosition =
                    static_cast<double>(
                        distanceFromCenter -
                        coreHalfWidth
                    ) /
                    std::max(
                        1,
                        halfWidth -
                        coreHalfWidth
                    );

                horizontalStrength =
                    1.0 -
                    fadePosition;

                horizontalStrength =
                    horizontalStrength *
                    horizontalStrength;
            }

            int alpha =
                static_cast<int>(
                    maxAlpha *
                    horizontalStrength
                );

            if (alpha <= 0) {
                continue;
            }

            SDL_SetRenderDrawColor(
                renderer,
                250,
                236,
                185,
                alpha
            );

            SDL_RenderDrawPoint(
                renderer,
                x,
                y
            );
        }
    }

    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_BLEND
    );
}

void drawCrosshair(SDL_Renderer* renderer) {
    int centerX = SCREEN_WIDTH / 2;
    int centerY = SCREEN_HEIGHT / 2;

    SDL_SetRenderDrawColor(
        renderer,
        230,
        230,
        220,
        190
    );

    SDL_RenderDrawLine(
        renderer,
        centerX - 6,
        centerY,
        centerX + 6,
        centerY
    );

    SDL_RenderDrawLine(
        renderer,
        centerX,
        centerY - 6,
        centerX,
        centerY + 6
    );
}

void drawScore(
    SDL_Renderer* renderer,
    TTF_Font* font
) {
    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_BLEND
    );

    SDL_Rect scoreBox = {
        15,
        15,
        180,
        52
    };

    SDL_SetRenderDrawColor(
        renderer,
        5,
        8,
        12,
        205
    );

    SDL_RenderFillRect(renderer, &scoreBox);

    SDL_SetRenderDrawColor(
        renderer,
        210,
        220,
        235,
        255
    );

    SDL_RenderDrawRect(renderer, &scoreBox);

    drawText(
        renderer,
        font,
        "スコア：" + std::to_string(score),
        scoreBox.x + scoreBox.w / 2,
        scoreBox.y + scoreBox.h / 2
    );
}

void drawMobileControls(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const Button& mobileUpButton,
    const Button& mobileDownButton,
    const Button& mobileLeftButton,
    const Button& mobileRightButton,
    const Button& mobileMapButton,
    const Button& mobilePauseButton
) {
    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_BLEND
    );

    drawButton(
        renderer,
        font,
        mobileUpButton,
        mobileUpHeld
    );

    drawButton(
        renderer,
        font,
        mobileDownButton,
        mobileDownHeld
    );

    drawButton(
        renderer,
        font,
        mobileLeftButton,
        mobileLeftHeld
    );

    drawButton(
        renderer,
        font,
        mobileRightButton,
        mobileRightHeld
    );

    drawButton(
        renderer,
        font,
        mobileMapButton,
        false
    );

    drawButton(
        renderer,
        font,
        mobilePauseButton,
        false
    );
}

void drawGame(
    SDL_Renderer* renderer,
    TTF_Font* smallFont,
    const Button& mobileUpButton,
    const Button& mobileDownButton,
    const Button& mobileLeftButton,
    const Button& mobileRightButton,
    const Button& mobileMapButton,
    const Button& mobilePauseButton
) {
    draw3DView(renderer);
    drawNaturalGroundLight(renderer);
    drawCrosshair(renderer);
    drawScore(renderer, smallFont);

    if (controlMode == ControlMode::MOBILE) {
        drawMobileControls(
            renderer,
            smallFont,
            mobileUpButton,
            mobileDownButton,
            mobileLeftButton,
            mobileRightButton,
            mobileMapButton,
            mobilePauseButton
        );
    } else {
        SDL_SetRenderDrawBlendMode(
            renderer,
            SDL_BLENDMODE_BLEND
        );

        SDL_SetRenderDrawColor(
            renderer,
            5,
            8,
            12,
            190
        );

        SDL_Rect informationBar = {
            0,
            SCREEN_HEIGHT - 46,
            SCREEN_WIDTH,
            46
        };

        SDL_RenderFillRect(
            renderer,
            &informationBar
        );

        drawText(
            renderer,
            smallFont,
            "W・↑：前進　W＋Q：走る　↓：後退　A/D・←/→：左右　T：地図　I：一時停止",
            SCREEN_WIDTH / 2,
            SCREEN_HEIGHT - 23
        );
    }
}

void drawLocalMap(
    SDL_Renderer* renderer,
    TTF_Font* titleFont,
    TTF_Font* smallFont
) {
    SDL_SetRenderDrawColor(
        renderer,
        9,
        12,
        18,
        255
    );

    SDL_RenderClear(renderer);

    drawText(
        renderer,
        titleFont,
        "周辺地図",
        SCREEN_WIDTH / 2,
        55
    );

    int playerCellX =
        static_cast<int>(playerX);

    int playerCellY =
        static_cast<int>(playerY);

    int cellsAcross =
        MAP_VIEW_RADIUS * 2 + 1;

    int tileSize = 44;

    int mapPixelSize =
        cellsAcross * tileSize;

    int offsetX =
        (SCREEN_WIDTH - mapPixelSize) / 2;

    int offsetY = 110;

    for (int relativeY = -MAP_VIEW_RADIUS;
         relativeY <= MAP_VIEW_RADIUS;
         relativeY++) {
        for (int relativeX = -MAP_VIEW_RADIUS;
             relativeX <= MAP_VIEW_RADIUS;
             relativeX++) {
            int mazeX =
                playerCellX + relativeX;

            int mazeY =
                playerCellY + relativeY;

            int screenX =
                relativeX + MAP_VIEW_RADIUS;

            int screenY =
                relativeY + MAP_VIEW_RADIUS;

            SDL_Rect tile = {
                offsetX + screenX * tileSize,
                offsetY + screenY * tileSize,
                tileSize,
                tileSize
            };

            if (
                mazeX < 0 ||
                mazeX >= MAZE_WIDTH ||
                mazeY < 0 ||
                mazeY >= MAZE_HEIGHT
            ) {
                SDL_SetRenderDrawColor(
                    renderer,
                    3,
                    5,
                    8,
                    255
                );
            } else if (
                mazeX == goalX &&
                mazeY == goalY
            ) {
                SDL_SetRenderDrawColor(
                    renderer,
                    50,
                    220,
                    100,
                    255
                );
            } else if (
                maze[mazeY][mazeX] == 1
            ) {
                SDL_SetRenderDrawColor(
                    renderer,
                    40,
                    48,
                    62,
                    255
                );
            } else if (
                visited[mazeY][mazeX]
            ) {
                SDL_SetRenderDrawColor(
                    renderer,
                    195,
                    195,
                    190,
                    255
                );
            } else {
                SDL_SetRenderDrawColor(
                    renderer,
                    110,
                    115,
                    120,
                    255
                );
            }

            SDL_RenderFillRect(
                renderer,
                &tile
            );

            SDL_SetRenderDrawColor(
                renderer,
                18,
                22,
                30,
                255
            );

            SDL_RenderDrawRect(
                renderer,
                &tile
            );
        }
    }

    int playerCenterX =
        offsetX +
        MAP_VIEW_RADIUS *
        tileSize +
        tileSize / 2;

    int playerCenterY =
        offsetY +
        MAP_VIEW_RADIUS *
        tileSize +
        tileSize / 2;

    SDL_Rect playerRect = {
        playerCenterX - 9,
        playerCenterY - 9,
        18,
        18
    };

    SDL_SetRenderDrawColor(
        renderer,
        40,
        130,
        255,
        255
    );

    SDL_RenderFillRect(
        renderer,
        &playerRect
    );

    int directionLength = 34;

    int directionEndX =
        playerCenterX +
        static_cast<int>(
            std::cos(playerAngle) *
            directionLength
        );

    int directionEndY =
        playerCenterY +
        static_cast<int>(
            std::sin(playerAngle) *
            directionLength
        );

    SDL_SetRenderDrawColor(
        renderer,
        255,
        225,
        70,
        255
    );

    SDL_RenderDrawLine(
        renderer,
        playerCenterX,
        playerCenterY,
        directionEndX,
        directionEndY
    );

    drawText(
        renderer,
        smallFont,
        controlMode == ControlMode::PC
            ? "T：閉じる"
            : "中央の地図ボタン：閉じる",
        SCREEN_WIDTH / 2,
        SCREEN_HEIGHT - 50
    );

    drawText(
        renderer,
        smallFont,
        "青：自分　黄色：向き　緑：ゴール　スコア：" +
            std::to_string(score),
        SCREEN_WIDTH / 2,
        SCREEN_HEIGHT - 23
    );
}

void saveGame() {
    std::ofstream file("maze_save.txt");

    if (!file) {
        return;
    }

    file
        << playerX << " "
        << playerY << " "
        << playerAngle << " "
        << cameraPitch << "\n";

    file
        << goalX << " "
        << goalY << " "
        << score << "\n";

    for (int y = 0; y < MAZE_HEIGHT; y++) {
        for (int x = 0; x < MAZE_WIDTH; x++) {
            file << maze[y][x];
        }
        file << "\n";
    }

    for (int y = 0; y < MAZE_HEIGHT; y++) {
        for (int x = 0; x < MAZE_WIDTH; x++) {
            file << (visited[y][x] ? 1 : 0);
        }
        file << "\n";
    }

    saveMessageUntil = SDL_GetTicks() + 1800;
}

std::string sensitivityText() {
    if (sensitivityLevel == 0) {
        return "マウス感度：低";
    }

    if (sensitivityLevel == 1) {
        return "マウス感度：普通";
    }

    return "マウス感度：高";
}

void changeSensitivity() {
    sensitivityLevel =
        (sensitivityLevel + 1) % 3;

    if (sensitivityLevel == 0) {
        mouseSensitivity = 0.0015;
    } else if (sensitivityLevel == 1) {
        mouseSensitivity = 0.0025;
    } else {
        mouseSensitivity = 0.0040;
    }
}

std::string brightnessText() {
    if (brightnessLevel == 0) {
        return "明るさ：暗い";
    }

    if (brightnessLevel == 1) {
        return "明るさ：普通";
    }

    return "明るさ：明るい";
}

void changeBrightness() {
    brightnessLevel =
        (brightnessLevel + 1) % 3;

    if (brightnessLevel == 0) {
        brightnessMultiplier = 0.70;
    } else if (brightnessLevel == 1) {
        brightnessMultiplier = 1.0;
    } else {
        brightnessMultiplier = 1.35;
    }
}

std::string controlModeText() {
    return
        controlMode == ControlMode::PC
        ? "操作方法：PC"
        : "操作方法：モバイル";
}

void toggleControlMode() {
    controlMode =
        controlMode == ControlMode::PC
        ? ControlMode::MOBILE
        : ControlMode::PC;

    mobileUpHeld = false;
    mobileDownHeld = false;
    mobileLeftHeld = false;
    mobileRightHeld = false;
    mobileRunning = false;
    mobileLookActive = false;
}

void drawSettings(
    SDL_Renderer* renderer,
    TTF_Font* titleFont,
    TTF_Font* buttonFont,
    TTF_Font* smallFont,
    const Button& sensitivityButton,
    const Button& brightnessButton,
    const Button& controlModeButton,
    const Button& settingsBackButton
) {
    SDL_SetRenderDrawColor(
        renderer,
        15,
        20,
        32,
        255
    );

    SDL_RenderClear(renderer);

    drawText(
        renderer,
        titleFont,
        "設定",
        SCREEN_WIDTH / 2,
        120
    );

    Button currentSensitivity = sensitivityButton;
    currentSensitivity.text = sensitivityText();

    Button currentBrightness = brightnessButton;
    currentBrightness.text = brightnessText();

    Button currentControlMode = controlModeButton;
    currentControlMode.text = controlModeText();

    drawButton(
        renderer,
        buttonFont,
        currentSensitivity
    );

    drawButton(
        renderer,
        buttonFont,
        currentBrightness
    );

    drawButton(
        renderer,
        buttonFont,
        currentControlMode
    );

    drawButton(
        renderer,
        buttonFont,
        settingsBackButton
    );

    drawText(
        renderer,
        smallFont,
        "設定内容はホームと一時停止で共通です",
        SCREEN_WIDTH / 2,
        590
    );
}

void drawPauseMenu(
    SDL_Renderer* renderer,
    TTF_Font* titleFont,
    TTF_Font* buttonFont,
    TTF_Font* smallFont,
    const Button& pauseSettingsButton,
    const Button& pauseSaveButton,
    const Button& pauseHomeButton,
    const Button& mobileUpButton,
    const Button& mobileDownButton,
    const Button& mobileLeftButton,
    const Button& mobileRightButton,
    const Button& mobileMapButton,
    const Button& mobilePauseButton
) {
    drawGame(
        renderer,
        smallFont,
        mobileUpButton,
        mobileDownButton,
        mobileLeftButton,
        mobileRightButton,
        mobileMapButton,
        mobilePauseButton
    );

    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_BLEND
    );

    SDL_SetRenderDrawColor(
        renderer,
        0,
        0,
        0,
        190
    );

    SDL_Rect shade = {
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    };

    SDL_RenderFillRect(renderer, &shade);

    drawText(
        renderer,
        titleFont,
        "一時停止",
        SCREEN_WIDTH / 2,
        145
    );

    drawButton(
        renderer,
        buttonFont,
        pauseSettingsButton
    );

    drawButton(
        renderer,
        buttonFont,
        pauseSaveButton
    );

    drawButton(
        renderer,
        buttonFont,
        pauseHomeButton
    );

    drawText(
        renderer,
        smallFont,
        controlMode == ControlMode::PC
            ? "Iキーでもゲームに戻れます"
            : "上の⏸ボタンでもゲームに戻れます",
        SCREEN_WIDTH / 2,
        585
    );

    if (SDL_GetTicks() < saveMessageUntil) {
        SDL_Color green = {
            80,
            230,
            120,
            255
        };

        drawText(
            renderer,
            smallFont,
            "maze_save.txt に保存しました",
            SCREEN_WIDTH / 2,
            625,
            green
        );
    }
}

void drawHome(
    SDL_Renderer* renderer,
    TTF_Font* titleFont,
    TTF_Font* buttonFont,
    const Button& startButton,
    const Button& settingsButton,
    const Button& rulesButton
) {
    SDL_SetRenderDrawColor(
        renderer,
        15,
        20,
        32,
        255
    );

    SDL_RenderClear(renderer);

    drawText(
        renderer,
        titleFont,
        "3D 迷路ゲーム",
        SCREEN_WIDTH / 2,
        140
    );

    drawText(
        renderer,
        buttonFont,
        "FINAL V3：W＋Qダッシュ／右半分スワイプ視点",
        SCREEN_WIDTH / 2,
        205,
        {150, 210, 255, 255}
    );

    drawButton(renderer, buttonFont, startButton);
    drawButton(renderer, buttonFont, settingsButton);
    drawButton(renderer, buttonFont, rulesButton);
}

void drawClear(
    SDL_Renderer* renderer,
    TTF_Font* titleFont,
    TTF_Font* buttonFont,
    TTF_Font* smallFont
) {
    SDL_SetRenderDrawColor(
        renderer,
        15,
        20,
        32,
        255
    );

    SDL_RenderClear(renderer);

    SDL_Color green = {
        80,
        230,
        120,
        255
    };

    drawText(
        renderer,
        titleFont,
        "迷路クリア！",
        SCREEN_WIDTH / 2,
        235,
        green
    );

    drawText(
        renderer,
        buttonFont,
        "最終スコア：" +
            std::to_string(score),
        SCREEN_WIDTH / 2,
        335
    );

    drawText(
        renderer,
        smallFont,
        "Enter：もう一度",
        SCREEN_WIDTH / 2,
        420
    );
}

bool playerReachedGoal() {
    int currentX = static_cast<int>(playerX);
    int currentY = static_cast<int>(playerY);

    return
        currentX == goalX &&
        currentY == goalY;
}

void setMouseCaptureForGameplay() {
    if (
        controlMode == ControlMode::PC &&
        overlayState == OverlayState::NONE &&
        !mapOpen
    ) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    } else {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
}

void startGame() {
    generateMaze();
    setMouseCaptureForGameplay();
}

void returnHome() {
    mapOpen = false;
    overlayState = OverlayState::NONE;

    mobileUpHeld = false;
    mobileDownHeld = false;
    mobileLeftHeld = false;
    mobileRightHeld = false;
    mobileRunning = false;
    mobileLookActive = false;
    cameraYawVelocity = 0.0;
    cameraPitchVelocity = 0.0;

    SDL_SetRelativeMouseMode(SDL_FALSE);
}

void openPauseMenu() {
    mapOpen = false;
    overlayState = OverlayState::PAUSE;

    mobileUpHeld = false;
    mobileDownHeld = false;
    mobileLeftHeld = false;
    mobileRightHeld = false;
    mobileRunning = false;
    mobileLookActive = false;
    cameraYawVelocity = 0.0;
    cameraPitchVelocity = 0.0;

    SDL_SetRelativeMouseMode(SDL_FALSE);
}

void closePauseMenu() {
    overlayState = OverlayState::NONE;
    setMouseCaptureForGameplay();
}

void processForwardPressStart() {
    Uint32 now = SDL_GetTicks();

    secondForwardTap =
        now - previousForwardTapTime <= 320;

    currentForwardPressStart = now;
    mobileUpHeld = true;
    mobileRunning = false;
}

void processForwardPressUpdate() {
    if (
        mobileUpHeld &&
        secondForwardTap &&
        SDL_GetTicks() -
            currentForwardPressStart >= 180
    ) {
        mobileRunning = true;
    }
}

void processForwardPressEnd() {
    Uint32 now = SDL_GetTicks();

    mobileUpHeld = false;
    mobileRunning = false;

    if (
        now - currentForwardPressStart <
        300
    ) {
        previousForwardTapTime = now;
    } else {
        previousForwardTapTime = 0;
    }

    secondForwardTap = false;
}

int fingerToScreenX(float fingerX) {
    return static_cast<int>(
        fingerX * SCREEN_WIDTH
    );
}

int fingerToScreenY(float fingerY) {
    return static_cast<int>(
        fingerY * SCREEN_HEIGHT
    );
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr
            << "SDLの初期化に失敗: "
            << SDL_GetError()
            << std::endl;

        return 1;
    }

    if (TTF_Init() != 0) {
        std::cerr
            << "SDL_ttfの初期化に失敗: "
            << TTF_GetError()
            << std::endl;

        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "3D Maze - FINAL V3",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (window == nullptr) {
        std::cerr
            << "ウィンドウ作成失敗: "
            << SDL_GetError()
            << std::endl;

        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED |
        SDL_RENDERER_PRESENTVSYNC
    );

    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_SOFTWARE
        );
    }

    if (renderer == nullptr) {
        std::cerr
            << "レンダラー作成失敗: "
            << SDL_GetError()
            << std::endl;

        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_BLEND
    );

    TTF_Font* titleFont = openFont(50);
    TTF_Font* buttonFont = openFont(28);
    TTF_Font* smallFont = openFont(17);

    if (
        titleFont == nullptr ||
        buttonFont == nullptr ||
        smallFont == nullptr
    ) {
        std::cerr
            << "フォントを開けません: "
            << TTF_GetError()
            << std::endl;

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);

        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    Button startButton = {
        {350, 270, 300, 75},
        "スタート"
    };

    Button homeSettingsButton = {
        {350, 375, 300, 75},
        "設定"
    };

    Button rulesButton = {
        {350, 480, 300, 75},
        "ルール"
    };

    Button pauseSettingsButton = {
        {350, 235, 300, 70},
        "設定"
    };

    Button pauseSaveButton = {
        {350, 335, 300, 70},
        "セーブ"
    };

    Button pauseHomeButton = {
        {350, 435, 300, 70},
        "ホーム"
    };

    Button sensitivityButton = {
        {325, 210, 350, 70},
        "マウス感度"
    };

    Button brightnessButton = {
        {325, 305, 350, 70},
        "明るさ"
    };

    Button controlModeButton = {
        {325, 400, 350, 70},
        "操作方法"
    };

    Button settingsBackButton = {
        {350, 510, 300, 65},
        "戻る"
    };

    Button mobileUpButton = {
        {70, 475, 72, 72},
        "↑"
    };

    Button mobileDownButton = {
        {70, 615, 72, 72},
        "↓"
    };

    Button mobileLeftButton = {
        {0, 545, 72, 72},
        "←"
    };

    Button mobileRightButton = {
        {140, 545, 72, 72},
        "→"
    };

    Button mobileMapButton = {
        {70, 545, 72, 72},
        "地図"
    };

    Button mobilePauseButton = {
        {460, 18, 80, 58},
        "⏸"
    };

    GameState gameState = GameState::HOME;
    bool running = true;

    Uint64 previousCounter =
        SDL_GetPerformanceCounter();

    while (running) {
        Uint64 currentCounter =
            SDL_GetPerformanceCounter();

        double deltaTime =
            static_cast<double>(
                currentCounter -
                previousCounter
            ) /
            static_cast<double>(
                SDL_GetPerformanceFrequency()
            );

        previousCounter = currentCounter;

        if (deltaTime > 0.05) {
            deltaTime = 0.05;
        }

        processForwardPressUpdate();

        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (
                event.type == SDL_MOUSEMOTION &&
                event.motion.which != SDL_TOUCH_MOUSEID &&
                gameState == GameState::GAME &&
                controlMode == ControlMode::PC
            ) {
                addLookInput(
                    event.motion.xrel,
                    event.motion.yrel
                );
            }

            int pointerX = -1;
            int pointerY = -1;
            bool pointerDown = false;
            bool pointerUp = false;

            if (
                event.type == SDL_MOUSEBUTTONDOWN &&
                event.button.which != SDL_TOUCH_MOUSEID &&
                event.button.button == SDL_BUTTON_LEFT
            ) {
                pointerX = event.button.x;
                pointerY = event.button.y;
                pointerDown = true;
            }

            if (
                event.type == SDL_MOUSEBUTTONUP &&
                event.button.which != SDL_TOUCH_MOUSEID &&
                event.button.button == SDL_BUTTON_LEFT
            ) {
                pointerX = event.button.x;
                pointerY = event.button.y;
                pointerUp = true;
            }

            if (event.type == SDL_FINGERDOWN) {
                pointerX =
                    fingerToScreenX(event.tfinger.x);

                pointerY =
                    fingerToScreenY(event.tfinger.y);

                pointerDown = true;

                // モバイルでは画面右側をスワイプして視点操作
                if (
                    controlMode == ControlMode::MOBILE &&
                    gameState == GameState::GAME &&
                    overlayState == OverlayState::NONE &&
                    !mapOpen &&
                    event.tfinger.x >= 0.50f &&
                    !pointInside(
                        mobilePauseButton.rect,
                        pointerX,
                        pointerY
                    )
                ) {
                    mobileLookActive = true;
                    mobileLookFingerId = event.tfinger.fingerId;
                    mobileLookLastX = event.tfinger.x;
                    mobileLookLastY = event.tfinger.y;
                    pointerDown = false;
                }
            }

            if (
                event.type == SDL_FINGERMOTION &&
                mobileLookActive &&
                event.tfinger.fingerId == mobileLookFingerId &&
                controlMode == ControlMode::MOBILE &&
                gameState == GameState::GAME &&
                overlayState == OverlayState::NONE &&
                !mapOpen
            ) {
                float deltaX = event.tfinger.x - mobileLookLastX;
                float deltaY = event.tfinger.y - mobileLookLastY;

                mobileLookLastX = event.tfinger.x;
                mobileLookLastY = event.tfinger.y;

                addLookInput(
                    deltaX * SCREEN_WIDTH,
                    deltaY * SCREEN_HEIGHT,
                    1.15
                );
            }

            if (event.type == SDL_FINGERUP) {
                pointerX =
                    fingerToScreenX(event.tfinger.x);

                pointerY =
                    fingerToScreenY(event.tfinger.y);

                pointerUp = true;

                if (
                    mobileLookActive &&
                    event.tfinger.fingerId == mobileLookFingerId
                ) {
                    mobileLookActive = false;
                    pointerUp = false;
                }
            }

            if (pointerDown) {
                // 設定画面が開いている間は、ホーム画面の裏側にある
                // スタートボタンを絶対に判定しない。
                if (overlayState == OverlayState::SETTINGS) {
                    if (
                        pointInside(
                            sensitivityButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        changeSensitivity();
                    } else if (
                        pointInside(
                            brightnessButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        changeBrightness();
                    } else if (
                        pointInside(
                            controlModeButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        toggleControlMode();
                    } else if (
                        pointInside(
                            settingsBackButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        if (
                            settingsReturnTarget ==
                            SettingsReturnTarget::HOME
                        ) {
                            overlayState = OverlayState::NONE;
                        } else {
                            overlayState = OverlayState::PAUSE;
                        }

                        setMouseCaptureForGameplay();
                    }
                } else if (gameState == GameState::HOME) {
                    if (pointInside(startButton.rect, pointerX, pointerY)) {
                        startGame();
                        gameState = GameState::GAME;
                    } else if (
                        pointInside(
                            homeSettingsButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        settingsReturnTarget = SettingsReturnTarget::HOME;
                        overlayState = OverlayState::SETTINGS;
                    }
                } else if (
                    gameState == GameState::GAME &&
                    overlayState == OverlayState::PAUSE
                ) {
                    if (
                        pointInside(
                            pauseSettingsButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        settingsReturnTarget = SettingsReturnTarget::PAUSE;
                        overlayState = OverlayState::SETTINGS;
                    } else if (
                        pointInside(
                            pauseSaveButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        saveGame();
                    } else if (
                        pointInside(
                            pauseHomeButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        returnHome();
                        gameState = GameState::HOME;
                    } else if (
                        controlMode == ControlMode::MOBILE &&
                        pointInside(
                            mobilePauseButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        closePauseMenu();
                    }
                } else if (
                    gameState == GameState::GAME &&
                    overlayState == OverlayState::NONE
                ) {
                    if (controlMode == ControlMode::MOBILE) {
                        if (
                            pointInside(
                                mobilePauseButton.rect,
                                pointerX,
                                pointerY
                            )
                        ) {
                            openPauseMenu();
                        } else if (
                            pointInside(
                                mobileMapButton.rect,
                                pointerX,
                                pointerY
                            )
                        ) {
                            mapOpen = !mapOpen;
                            setMouseCaptureForGameplay();
                        } else if (
                            pointInside(
                                mobileUpButton.rect,
                                pointerX,
                                pointerY
                            )
                        ) {
                            processForwardPressStart();
                        } else if (
                            pointInside(
                                mobileDownButton.rect,
                                pointerX,
                                pointerY
                            )
                        ) {
                            mobileDownHeld = true;
                        } else if (
                            pointInside(
                                mobileLeftButton.rect,
                                pointerX,
                                pointerY
                            )
                        ) {
                            mobileLeftHeld = true;
                        } else if (
                            pointInside(
                                mobileRightButton.rect,
                                pointerX,
                                pointerY
                            )
                        ) {
                            mobileRightHeld = true;
                        }
                    }
                }
            }

            if (
                pointerUp &&
                controlMode == ControlMode::MOBILE
            ) {
                if (mobileUpHeld) {
                    processForwardPressEnd();
                }

                mobileDownHeld = false;
                mobileLeftHeld = false;
                mobileRightHeld = false;
            }

            if (
                event.type == SDL_KEYDOWN &&
                event.key.repeat == 0
            ) {
                SDL_Keycode key =
                    event.key.keysym.sym;

                if (
                    gameState == GameState::GAME &&
                    key == SDLK_i
                ) {
                    if (
                        overlayState ==
                        OverlayState::NONE
                    ) {
                        openPauseMenu();
                    } else if (
                        overlayState ==
                        OverlayState::PAUSE
                    ) {
                        closePauseMenu();
                    }
                }

                if (
                    gameState == GameState::GAME &&
                    key == SDLK_t &&
                    overlayState ==
                        OverlayState::NONE
                ) {
                    mapOpen = !mapOpen;
                    setMouseCaptureForGameplay();
                }

                if (
                    gameState == GameState::CLEAR &&
                    key == SDLK_RETURN
                ) {
                    startGame();
                    gameState = GameState::GAME;
                }
            }
        }

        if (
            gameState == GameState::GAME &&
            overlayState ==
                OverlayState::NONE &&
            !mapOpen
        ) {
            updateSmoothCamera(deltaTime);
            updatePlayer(deltaTime);

            if (playerReachedGoal()) {
                SDL_SetRelativeMouseMode(
                    SDL_FALSE
                );

                gameState =
                    GameState::CLEAR;
            }
        }

        if (
            gameState == GameState::HOME &&
            overlayState ==
                OverlayState::SETTINGS
        ) {
            drawSettings(
                renderer,
                titleFont,
                buttonFont,
                smallFont,
                sensitivityButton,
                brightnessButton,
                controlModeButton,
                settingsBackButton
            );
        } else if (
            gameState == GameState::HOME
        ) {
            drawHome(
                renderer,
                titleFont,
                buttonFont,
                startButton,
                homeSettingsButton,
                rulesButton
            );
        } else if (
            gameState == GameState::GAME &&
            overlayState ==
                OverlayState::SETTINGS
        ) {
            drawSettings(
                renderer,
                titleFont,
                buttonFont,
                smallFont,
                sensitivityButton,
                brightnessButton,
                controlModeButton,
                settingsBackButton
            );
        } else if (
            gameState == GameState::GAME &&
            overlayState ==
                OverlayState::PAUSE
        ) {
            drawPauseMenu(
                renderer,
                titleFont,
                buttonFont,
                smallFont,
                pauseSettingsButton,
                pauseSaveButton,
                pauseHomeButton,
                mobileUpButton,
                mobileDownButton,
                mobileLeftButton,
                mobileRightButton,
                mobileMapButton,
                mobilePauseButton
            );
        } else if (
            gameState == GameState::GAME &&
            mapOpen
        ) {
            drawLocalMap(
                renderer,
                titleFont,
                smallFont
            );
        } else if (
            gameState == GameState::GAME
        ) {
            drawGame(
                renderer,
                smallFont,
                mobileUpButton,
                mobileDownButton,
                mobileLeftButton,
                mobileRightButton,
                mobileMapButton,
                mobilePauseButton
            );
        } else if (
            gameState == GameState::CLEAR
        ) {
            drawClear(
                renderer,
                titleFont,
                buttonFont,
                smallFont
            );
        }

        SDL_RenderPresent(renderer);
    }

    SDL_SetRelativeMouseMode(SDL_FALSE);

    TTF_CloseFont(smallFont);
    TTF_CloseFont(buttonFont);
    TTF_CloseFont(titleFont);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    return 0;
}
