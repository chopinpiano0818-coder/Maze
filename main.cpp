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

const int MAZE_CELLS = 6;
const int MAZE_WIDTH = MAZE_CELLS * 2 + 1;
const int MAZE_HEIGHT = MAZE_CELLS * 2 + 1;
const int MAP_VIEW_RADIUS = 5;

const double PI = 3.14159265358979323846;
const char* BUILD_VERSION = "FINAL-V3-2026-07-29";

enum class GameState {
    HOME,
    GAME,
    CLEAR,
    GAME_OVER
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
double keyX = 0.0;
double keyY = 0.0;
bool keyAvailable = true;
bool playerHasKey = false;
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
    playerCrouching = false;
    playerHearts = 3;
    playerDamageCooldownUntil = 0;
    enemyAngle = 0.0;
    enemyWanderTargetX = -1;
    enemyWanderTargetY = -1;
    enemyWanderRetargetAt = 0;

    goalX = MAZE_WIDTH - 2;
    goalY = MAZE_HEIGHT - 2;

    maze[1][1] = 0;
    maze[goalY][goalX] = 0;

    // 敵はスタートからなるべく遠い通路に配置
    enemyX = goalX + 0.5;
    enemyY = goalY + 0.5;

    // 鍵ゾーンはゴールとは別の奥まった通路に配置
    int keyCellX = 1;
    int keyCellY = goalY;
    if (keyCellX == goalX && keyCellY == goalY) keyCellY = 1;
    maze[keyCellY][keyCellX] = 0;
    keyX = keyCellX + 0.5;
    keyY = keyCellY + 0.5;
    keyAvailable = true;
    playerHasKey = false;
    actionMessage.clear();
    actionMessageUntil = 0;

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
    if (mapOpen || overlayState != OverlayState::NONE) return;

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    playerCrouching = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];

    bool forwardPressed = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP] || mobileUpHeld;
    bool backwardPressed = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN] || mobileDownHeld;
    bool leftPressed = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT] || mobileLeftHeld;
    bool rightPressed = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT] || mobileRightHeld;

    bool ctrlHeld = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];
    bool runningForward = !playerCrouching &&
        (((keys[SDL_SCANCODE_UP] && ctrlHeld) ||
          (keys[SDL_SCANCODE_W] && keys[SDL_SCANCODE_Q])) || mobileRunning);

    double localForward = (forwardPressed ? 1.0 : 0.0) - (backwardPressed ? 1.0 : 0.0);
    double localRight = (rightPressed ? 1.0 : 0.0) - (leftPressed ? 1.0 : 0.0);
    double length = std::hypot(localForward, localRight);
    if (length < 0.001) return;
    localForward /= length;
    localRight /= length;

    double speed = playerCrouching ? 1.08 : (runningForward && localForward > 0.0 ? 3.45 : 1.85);
    double forwardX = std::cos(playerAngle);
    double forwardY = std::sin(playerAngle);
    double rightX = -std::sin(playerAngle);
    double rightY = std::cos(playerAngle);
    double movement = speed * deltaTime;

    tryMove(
        (forwardX * localForward + rightX * localRight) * movement,
        (forwardY * localForward + rightY * localRight) * movement
    );
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
        static_cast<int>(cameraPitch) +
        (playerCrouching ? 42 : 0);

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

        depthBuffer[screenX] = wallDistance;
        
        if (wallTexture != nullptr && wallTextureWidth > 0) {
            double wallHitX;
            if (wallSide == 0) {
                wallHitX = playerY + wallDistance * rayDirectionY;
            } else {
                wallHitX = playerX + wallDistance * rayDirectionX;
            }
            wallHitX -= std::floor(wallHitX);

            int textureX = static_cast<int>(wallHitX * wallTextureWidth);
            if ((wallSide == 0 && rayDirectionX > 0.0) ||
                (wallSide == 1 && rayDirectionY < 0.0)) {
                textureX = wallTextureWidth - textureX - 1;
            }
            textureX = std::clamp(textureX, 0, wallTextureWidth - 1);

            SDL_Rect source = {textureX, 0, 1, wallTextureHeight};
            SDL_Rect destination = {screenX, wallTop, 1, wallBottom - wallTop + 1};
            SDL_SetTextureColorMod(
                wallTexture,
                static_cast<Uint8>(std::clamp(brightness * 2, 20, 255)),
                static_cast<Uint8>(std::clamp(brightness * 2, 20, 255)),
                static_cast<Uint8>(std::clamp(brightness * 2 + 5, 20, 255))
            );
            SDL_RenderCopy(renderer, wallTexture, &source, &destination);
        } else {
            SDL_SetRenderDrawColor(
                renderer,
                brightness,
                brightness,
                std::min(255, brightness + 9),
                255
            );
            SDL_Rect wallColumn = {screenX, wallTop, 1, wallBottom - wallTop + 1};
            SDL_RenderFillRect(renderer, &wallColumn);
        }
    }
}


bool hasClearLine(double fromX, double fromY, double toX, double toY);

bool enemyCanMoveTo(double x, double y) {
    const double radius = 0.16;
    const double checks[4][2] = {
        {x - radius, y - radius}, {x + radius, y - radius},
        {x - radius, y + radius}, {x + radius, y + radius}
    };
    for (const auto& point : checks) {
        int cellX = static_cast<int>(point[0]);
        int cellY = static_cast<int>(point[1]);
        if (cellX < 0 || cellX >= MAZE_WIDTH || cellY < 0 || cellY >= MAZE_HEIGHT ||
            maze[cellY][cellX] == 1) {
            return false;
        }
    }
    return true;
}

bool enemyHasLineOfSightToPlayer() {
    double dx = playerX - enemyX;
    double dy = playerY - enemyY;
    double distance = std::hypot(dx, dy);
    if (distance < 0.001) return true;

    // しゃがみ中は、ロボットから1マス以内なら見つからない。
    if (playerCrouching && distance <= 1.0) return false;

    double targetAngle = std::atan2(dy, dx);
    double difference = targetAngle - enemyAngle;
    while (difference > PI) difference -= 2.0 * PI;
    while (difference < -PI) difference += 2.0 * PI;

    // 約110度の視野。正面に入っていて、壁に遮られていない時だけ追跡。
    if (std::abs(difference) > 55.0 * PI / 180.0) return false;
    return hasClearLine(enemyX, enemyY, playerX, playerY);
}

std::pair<int,int> chooseWanderTarget() {
    int cx = static_cast<int>(enemyX);
    int cy = static_cast<int>(enemyY);
    std::vector<std::pair<int,int>> choices;
    const int dx[4] = {1,-1,0,0};
    const int dy[4] = {0,0,1,-1};
    for (int i=0;i<4;i++) {
        int nx=cx+dx[i], ny=cy+dy[i];
        if (nx>=0 && nx<MAZE_WIDTH && ny>=0 && ny<MAZE_HEIGHT && maze[ny][nx]==0)
            choices.push_back({nx,ny});
    }
    if (choices.empty()) return {cx,cy};
    std::uniform_int_distribution<int> pick(0, static_cast<int>(choices.size())-1);
    return choices[pick(randomEngine)];
}

void moveEnemyTowardCell(int targetX, int targetY, double deltaTime) {
    int startX = static_cast<int>(enemyX);
    int startY = static_cast<int>(enemyY);
    targetX = std::clamp(targetX, 0, MAZE_WIDTH-1);
    targetY = std::clamp(targetY, 0, MAZE_HEIGHT-1);

    std::vector<std::vector<int>> previous(MAZE_HEIGHT, std::vector<int>(MAZE_WIDTH, -1));
    std::vector<std::pair<int,int>> queue{{startX,startY}};
    previous[startY][startX] = startY * MAZE_WIDTH + startX;
    const int dx[4] = {1,-1,0,0};
    const int dy[4] = {0,0,1,-1};
    for (size_t index=0; index<queue.size(); ++index) {
        auto [x,y]=queue[index];
        if (x==targetX && y==targetY) break;
        for (int i=0;i<4;i++) {
            int nx=x+dx[i], ny=y+dy[i];
            if (nx<0||nx>=MAZE_WIDTH||ny<0||ny>=MAZE_HEIGHT) continue;
            if (maze[ny][nx]==1 || previous[ny][nx]!=-1) continue;
            previous[ny][nx]=y*MAZE_WIDTH+x;
            queue.push_back({nx,ny});
        }
    }
    if (previous[targetY][targetX] == -1) return;

    int pathX=targetX, pathY=targetY;
    while (true) {
        int value=previous[pathY][pathX];
        int parentX=value%MAZE_WIDTH, parentY=value/MAZE_WIDTH;
        if (parentX==startX && parentY==startY) break;
        if (parentX==pathX && parentY==pathY) break;
        pathX=parentX; pathY=parentY;
    }

    double destinationX=pathX+0.5, destinationY=pathY+0.5;
    double directionX=destinationX-enemyX, directionY=destinationY-enemyY;
    double distance=std::hypot(directionX,directionY);
    if (distance <= 0.001) return;
    directionX/=distance; directionY/=distance;
    enemyAngle=std::atan2(directionY,directionX);
    double movement=enemySpeed*deltaTime;
    double nextX=enemyX+directionX*movement;
    double nextY=enemyY+directionY*movement;
    if (enemyCanMoveTo(nextX,enemyY)) enemyX=nextX;
    if (enemyCanMoveTo(enemyX,nextY)) enemyY=nextY;
}

void updateEnemy(double deltaTime) {
    if (enemyHasLineOfSightToPlayer()) {
        moveEnemyTowardCell(static_cast<int>(playerX), static_cast<int>(playerY), deltaTime);
        return;
    }

    Uint32 now=SDL_GetTicks();
    double targetDistance = enemyWanderTargetX >= 0
        ? std::hypot((enemyWanderTargetX+0.5)-enemyX, (enemyWanderTargetY+0.5)-enemyY)
        : 0.0;
    if (enemyWanderTargetX < 0 || targetDistance < 0.20 || now >= enemyWanderRetargetAt) {
        auto target=chooseWanderTarget();
        enemyWanderTargetX=target.first;
        enemyWanderTargetY=target.second;
        enemyWanderRetargetAt=now+1800;
    }
    moveEnemyTowardCell(enemyWanderTargetX, enemyWanderTargetY, deltaTime);
}

bool enemyTouchedPlayer() {
    return std::hypot(enemyX - playerX, enemyY - playerY) < 0.48;
}

void drawEnemy(SDL_Renderer* renderer) {
    if (enemyTexture == nullptr) return;

    const double fieldOfView = PI / 3.0;
    double directionX = std::cos(playerAngle);
    double directionY = std::sin(playerAngle);
    double planeLength = std::tan(fieldOfView / 2.0);
    double planeX = -directionY * planeLength;
    double planeY = directionX * planeLength;

    double spriteX = enemyX - playerX;
    double spriteY = enemyY - playerY;
    double inverseDeterminant = 1.0 / (planeX * directionY - directionX * planeY);
    double transformX = inverseDeterminant * (directionY * spriteX - directionX * spriteY);
    double transformY = inverseDeterminant * (-planeY * spriteX + planeX * spriteY);
    if (transformY <= 0.05) return;

    int horizonY = SCREEN_HEIGHT / 2 + static_cast<int>(cameraPitch);
    int spriteScreenX = static_cast<int>((SCREEN_WIDTH / 2.0) * (1.0 + transformX / transformY));
    int spriteHeight = std::abs(static_cast<int>(SCREEN_HEIGHT / transformY * 0.92));
    int spriteWidth = static_cast<int>(spriteHeight * (enemyTextureWidth / static_cast<double>(enemyTextureHeight)));
    int startY = horizonY - spriteHeight / 2;
    int endY = horizonY + spriteHeight / 2;
    int startX = spriteScreenX - spriteWidth / 2;
    int endX = spriteScreenX + spriteWidth / 2;

    for (int stripe = std::max(0, startX); stripe < std::min(SCREEN_WIDTH, endX); stripe += 2) {
        if (transformY >= depthBuffer[stripe]) continue;
        int textureX = static_cast<int>((stripe - startX) * enemyTextureWidth / static_cast<double>(std::max(1, spriteWidth)));
        textureX = std::clamp(textureX, 0, enemyTextureWidth - 1);
        SDL_Rect source = {textureX, 0, 1, enemyTextureHeight};
        SDL_Rect destination = {stripe, startY, std::min(2, SCREEN_WIDTH - stripe), endY - startY};
        SDL_RenderCopy(renderer, enemyTexture, &source, &destination);
    }
}


bool hasClearLine(double fromX, double fromY, double toX, double toY) {
    double dx = toX - fromX;
    double dy = toY - fromY;
    double distance = std::hypot(dx, dy);
    if (distance < 0.001) return true;
    int steps = std::max(1, static_cast<int>(distance / 0.05));
    for (int i = 1; i < steps; ++i) {
        double t = static_cast<double>(i) / steps;
        int x = static_cast<int>(fromX + dx * t);
        int y = static_cast<int>(fromY + dy * t);
        if (x < 0 || x >= MAZE_WIDTH || y < 0 || y >= MAZE_HEIGHT || maze[y][x] == 1) {
            return false;
        }
    }
    return true;
}

bool lookingAt(double targetX, double targetY, double maxDistance, double maxAngle) {
    double dx = targetX - playerX;
    double dy = targetY - playerY;
    double distance = std::hypot(dx, dy);
    if (distance > maxDistance) return false;
    double angle = std::atan2(dy, dx) - playerAngle;
    while (angle > PI) angle -= 2.0 * PI;
    while (angle < -PI) angle += 2.0 * PI;
    return std::abs(angle) <= maxAngle && hasClearLine(playerX, playerY, targetX, targetY);
}

void showActionMessage(const std::string& text) {
    actionMessage = text;
    actionMessageUntil = SDL_GetTicks() + 1800;
}

bool tryPickUpKey() {
    if (!keyAvailable) return false;
    if (!lookingAt(keyX, keyY, 1.65, 0.22)) {
        showActionMessage("鍵に近づいて、中央に合わせて左クリック");
        return false;
    }
    keyAvailable = false;
    playerHasKey = true;
    showActionMessage("鍵を手に入れた！");
    return true;
}

bool tryUseKeyAtGoal() {
    double gx = goalX + 0.5;
    double gy = goalY + 0.5;
    if (!lookingAt(gx, gy, 1.8, 0.35)) {
        showActionMessage("ゴールに近づいて右クリック");
        return false;
    }
    if (!playerHasKey) {
        showActionMessage("鍵がない！ 鍵ゾーンを探そう");
        return false;
    }
    playerHasKey = false;
    return true;
}

void drawKeySprite(SDL_Renderer* renderer) {
    if (!keyAvailable) return;
    double dx = keyX - playerX;
    double dy = keyY - playerY;
    double distance = std::hypot(dx, dy);
    if (distance < 0.05 || !hasClearLine(playerX, playerY, keyX, keyY)) return;
    double relativeAngle = std::atan2(dy, dx) - playerAngle;
    while (relativeAngle > PI) relativeAngle -= 2.0 * PI;
    while (relativeAngle < -PI) relativeAngle += 2.0 * PI;
    const double fov = PI / 3.0;
    if (std::abs(relativeAngle) > fov * 0.65) return;
    int screenX = SCREEN_WIDTH / 2 + static_cast<int>(std::tan(relativeAngle) / std::tan(fov / 2.0) * SCREEN_WIDTH / 2.0);
    int horizonY = getHorizonY();
    int size = std::clamp(static_cast<int>(150.0 / distance), 18, 110);
    int centerY = horizonY + static_cast<int>(SCREEN_HEIGHT / distance * 0.20);
    if (screenX < 0 || screenX >= SCREEN_WIDTH || distance >= depthBuffer[std::clamp(screenX,0,SCREEN_WIDTH-1)]) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 202, 35, 230);
    SDL_Rect ring = {screenX - size/2, centerY - size/3, size/2, size/2};
    SDL_RenderDrawRect(renderer, &ring);
    SDL_RenderDrawRect(renderer, &ring);
    SDL_Rect shaft = {screenX - 2, centerY, size/2 + 6, std::max(5, size/8)};
    SDL_RenderFillRect(renderer, &shaft);
    SDL_Rect tooth1 = {screenX + size/3, centerY, std::max(4,size/10), size/4};
    SDL_Rect tooth2 = {screenX + size/5, centerY, std::max(4,size/10), size/5};
    SDL_RenderFillRect(renderer, &tooth1);
    SDL_RenderFillRect(renderer, &tooth2);
}

void drawGameOver(SDL_Renderer* renderer, TTF_Font* titleFont, TTF_Font* smallFont) {
    SDL_SetRenderDrawColor(renderer, 20, 24, 30, 255);
    SDL_RenderClear(renderer);
    drawText(renderer, titleFont, "ゲームオーバー", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 45, {255, 90, 90, 255});
    drawText(renderer, smallFont, "ロボットに捕まった！ Enterでやり直し", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 35);
}

void drawNaturalGroundLight(SDL_Renderer* renderer) {
    // 軽量版：1ピクセルずつではなく、横線を重ねて同じ形の光を描く。
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);

    const int horizonY = getHorizonY();
    const int startY = horizonY + 8;
    const int endY = SCREEN_HEIGHT - 18;
    if (startY >= endY) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        return;
    }

    const int centerX = SCREEN_WIDTH / 2;
    constexpr int LIGHT_LAYERS = 8;

    // 2行ずつ描画して負荷をさらに半分にする。
    for (int y = startY; y <= endY; y += 2) {
        const double depth = static_cast<double>(y - startY) /
                             static_cast<double>(endY - startY);
        const double nearAmount = std::pow(depth, 1.35);
        const int halfWidth = static_cast<int>(470 - 365 * nearAmount);
        const int maxAlpha = std::clamp(
            static_cast<int>((6 + 76 * std::pow(depth, 1.75)) * brightnessMultiplier),
            2, 120
        );

        for (int layer = LIGHT_LAYERS; layer >= 1; --layer) {
            const double t = layer / static_cast<double>(LIGHT_LAYERS);
            const int layerHalfWidth = std::max(1, static_cast<int>(halfWidth * t));
            const double strength = 1.0 - t;
            const int alpha = std::max(1, static_cast<int>(maxAlpha * (0.10 + strength * strength * 0.34)));

            SDL_SetRenderDrawColor(renderer, 250, 236, 185, static_cast<Uint8>(alpha));
            SDL_Rect band = {
                centerX - layerHalfWidth,
                y,
                layerHalfWidth * 2,
                std::min(2, endY - y + 1)
            };
            SDL_RenderFillRect(renderer, &band);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
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
    drawEnemy(renderer);
    drawKeySprite(renderer);
    drawCrosshair(renderer);
    drawScore(renderer, smallFont);

    drawText(renderer, smallFont, playerHasKey ? "鍵：あり" : "鍵：なし", 105, 88,
             playerHasKey ? SDL_Color{255,215,70,255} : SDL_Color{220,220,220,255});
    std::string hearts;
    for (int i = 0; i < 3; ++i) hearts += (i < playerHearts ? "♥ " : "♡ ");
    drawText(renderer, smallFont, hearts, SCREEN_WIDTH - 100, 38,
             playerDamageCooldownUntil > SDL_GetTicks() ? SDL_Color{255,150,150,255} : SDL_Color{255,70,85,255});
    if (SDL_GetTicks() < actionMessageUntil) {
        drawText(renderer, smallFont, actionMessage, SCREEN_WIDTH / 2, SCREEN_HEIGHT - 55, {255,235,120,255});
    }

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
            "W・↑：前進　↑＋Ctrl：走る　S・↓：後退　Shift：しゃがむ　A/D：横移動　T：地図",
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

    auto drawMapMarker = [&](double worldX, double worldY, SDL_Color color, int size) {
        int relativeX = static_cast<int>(worldX) - playerCellX;
        int relativeY = static_cast<int>(worldY) - playerCellY;
        if (std::abs(relativeX) > MAP_VIEW_RADIUS || std::abs(relativeY) > MAP_VIEW_RADIUS) return;
        int centerX = offsetX + (relativeX + MAP_VIEW_RADIUS) * tileSize + tileSize / 2;
        int centerY = offsetY + (relativeY + MAP_VIEW_RADIUS) * tileSize + tileSize / 2;
        SDL_Rect marker = {centerX - size/2, centerY - size/2, size, size};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &marker);
    };

    drawMapMarker(enemyX, enemyY, {230, 55, 55, 255}, 16);
    if (keyAvailable) drawMapMarker(keyX, keyY, {255, 205, 35, 255}, 14);

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
        "青：自分　赤：敵　金：鍵　緑：ゴール　スコア：" +
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
    int percent = static_cast<int>(
        (mouseSensitivity - MIN_SENSITIVITY) /
        (MAX_SENSITIVITY - MIN_SENSITIVITY) * 100.0
    );
    percent = std::clamp(percent, 0, 100);
    return "マウス感度：" + std::to_string(percent) + "%";
}

void setSensitivityFromPointer(int pointerX, const SDL_Rect& sliderTrack) {
    double amount = static_cast<double>(pointerX - sliderTrack.x) / sliderTrack.w;
    amount = std::clamp(amount, 0.0, 1.0);
    mouseSensitivity = MIN_SENSITIVITY +
        amount * (MAX_SENSITIVITY - MIN_SENSITIVITY);
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

    Button currentBrightness = brightnessButton;
    currentBrightness.text = brightnessText();

    Button currentControlMode = controlModeButton;
    currentControlMode.text = controlModeText();

    drawText(renderer, buttonFont, sensitivityText(), SCREEN_WIDTH / 2, 205);

    SDL_Rect sliderTrack = {350, 255, 300, 12};
    SDL_SetRenderDrawColor(renderer, 70, 78, 92, 255);
    SDL_RenderFillRect(renderer, &sliderTrack);
    double sliderAmount = (mouseSensitivity - MIN_SENSITIVITY) /
                          (MAX_SENSITIVITY - MIN_SENSITIVITY);
    int knobX = sliderTrack.x + static_cast<int>(sliderTrack.w * sliderAmount);
    SDL_Rect sliderFill = {sliderTrack.x, sliderTrack.y, knobX - sliderTrack.x, sliderTrack.h};
    SDL_SetRenderDrawColor(renderer, 45, 145, 245, 255);
    SDL_RenderFillRect(renderer, &sliderFill);
    SDL_Rect knob = {knobX - 10, sliderTrack.y - 9, 20, 30};
    SDL_SetRenderDrawColor(renderer, 235, 242, 255, 255);
    SDL_RenderFillRect(renderer, &knob);

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
        "6×6 ROBOT：白レンガ／追跡ロボット",
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

    if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG)) == 0) {
        std::cerr << "SDL_imageの初期化に失敗: " << IMG_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "3D Maze - 6x6 ROBOT",
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

    wallTexture = IMG_LoadTexture(renderer, "wall.jpg");
    enemyTexture = IMG_LoadTexture(renderer, "enemy.png");
    if (wallTexture != nullptr) SDL_QueryTexture(wallTexture, nullptr, nullptr, &wallTextureWidth, &wallTextureHeight);
    if (enemyTexture != nullptr) {
        SDL_QueryTexture(enemyTexture, nullptr, nullptr, &enemyTextureWidth, &enemyTextureHeight);
        SDL_SetTextureBlendMode(enemyTexture, SDL_BLENDMODE_BLEND);
    }
    if (wallTexture == nullptr) std::cerr << "wall.jpgを読み込めません: " << IMG_GetError() << std::endl;
    if (enemyTexture == nullptr) std::cerr << "enemy.pngを読み込めません: " << IMG_GetError() << std::endl;

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
        {325, 225, 350, 75},
        "マウス感度"
    };

    Button brightnessButton = {
        {325, 330, 350, 70},
        "明るさ"
    };

    Button controlModeButton = {
        {325, 425, 350, 70},
        "操作方法"
    };

    Button settingsBackButton = {
        {350, 535, 300, 60},
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

            if (event.type == SDL_MOUSEMOTION && sensitivityDragging &&
                overlayState == OverlayState::SETTINGS) {
                SDL_Rect sliderTrack = {350, 255, 300, 12};
                setSensitivityFromPointer(event.motion.x, sliderTrack);
            }

            if (event.type == SDL_MOUSEBUTTONDOWN &&
                event.button.which != SDL_TOUCH_MOUSEID &&
                gameState == GameState::GAME &&
                overlayState == OverlayState::NONE && !mapOpen) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    tryPickUpKey();
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    if (tryUseKeyAtGoal()) {
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                        gameState = GameState::CLEAR;
                    }
                }
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
                        SDL_Rect sliderTrack = {350, 255, 300, 12};
                        setSensitivityFromPointer(pointerX, sliderTrack);
                        sensitivityDragging = true;
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

            if (pointerUp) {
                sensitivityDragging = false;
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
                    (gameState == GameState::CLEAR || gameState == GameState::GAME_OVER) &&
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
            // 経路探索は毎フレーム行わず約10回/秒に制限する。
            static double enemyUpdateAccumulator = 0.0;
            enemyUpdateAccumulator += deltaTime;
            if (enemyUpdateAccumulator >= 0.10) {
                updateEnemy(enemyUpdateAccumulator);
                enemyUpdateAccumulator = 0.0;
            }

            if (enemyTouchedPlayer() && SDL_GetTicks() >= playerDamageCooldownUntil) {
                playerHearts--;
                playerDamageCooldownUntil = SDL_GetTicks() + 2000;
                showActionMessage("ロボットにぶつかった！ ハートが1つ減った");
                // 連続接触を避けるため、少し後ろへ押し戻す。
                tryMove(-std::cos(playerAngle) * 0.35, -std::sin(playerAngle) * 0.35);
                if (playerHearts <= 0) {
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    gameState = GameState::GAME_OVER;
                }
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
        } else if (gameState == GameState::GAME_OVER) {
            drawGameOver(renderer, titleFont, smallFont);
        }

        SDL_RenderPresent(renderer);
    }

    SDL_SetRelativeMouseMode(SDL_FALSE);

    TTF_CloseFont(smallFont);
    TTF_CloseFont(buttonFont);
    TTF_CloseFont(titleFont);

    if (enemyTexture != nullptr) SDL_DestroyTexture(enemyTexture);
    if (wallTexture != nullptr) SDL_DestroyTexture(wallTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
