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

enum class GameState {
    HOME,
    GAME,
    CLEAR
};

enum class MenuState {
    NONE,
    PAUSE,
    SETTINGS
};

struct Button {
    SDL_Rect rect;
    std::string text;
};

struct Frontier {
    int wallX;
    int wallY;
    int nextX;
    int nextY;
};

std::vector<std::vector<int>> maze(
    MAZE_HEIGHT,
    std::vector<int>(MAZE_WIDTH, 1)
);

std::vector<std::vector<bool>> visited(
    MAZE_HEIGHT,
    std::vector<bool>(MAZE_WIDTH, false)
);

std::mt19937 randomEngine(
    std::random_device{}()
);

double playerX = 1.5;
double playerY = 1.5;

double playerAngle = 0.0;
double cameraPitch = 0.0;

double mouseSensitivity = 0.0025;
int sensitivityLevel = 1;

int goalX = MAZE_WIDTH - 2;
int goalY = MAZE_HEIGHT - 2;

int score = 0;

bool mapOpen = false;

MenuState menuState = MenuState::NONE;

Uint32 saveMessageUntil = 0;

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

    SDL_Texture* texture =
        SDL_CreateTextureFromSurface(
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

    SDL_RenderCopy(
        renderer,
        texture,
        nullptr,
        &destination
    );

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

bool isButtonClicked(
    const Button& button,
    int mouseX,
    int mouseY
) {
    return
        mouseX >= button.rect.x &&
        mouseX < button.rect.x + button.rect.w &&
        mouseY >= button.rect.y &&
        mouseY < button.rect.y + button.rect.h;
}

void drawButton(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const Button& button
) {
    int mouseX;
    int mouseY;

    SDL_GetMouseState(
        &mouseX,
        &mouseY
    );

    bool hovered = isButtonClicked(
        button,
        mouseX,
        mouseY
    );

    if (hovered) {
        SDL_SetRenderDrawColor(
            renderer,
            85,
            115,
            175,
            255
        );
    } else {
        SDL_SetRenderDrawColor(
            renderer,
            48,
            60,
            88,
            255
        );
    }

    SDL_RenderFillRect(
        renderer,
        &button.rect
    );

    SDL_SetRenderDrawColor(
        renderer,
        205,
        215,
        235,
        255
    );

    SDL_RenderDrawRect(
        renderer,
        &button.rect
    );

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

void addFrontiers(
    int x,
    int y,
    std::vector<Frontier>& frontiers
) {
    const int directions[4][2] = {
        {0, -2},
        {0, 2},
        {-2, 0},
        {2, 0}
    };

    for (const auto& direction : directions) {
        int nextX = x + direction[0];
        int nextY = y + direction[1];

        if (!isInsideMaze(nextX, nextY)) {
            continue;
        }

        if (maze[nextY][nextX] == 0) {
            continue;
        }

        Frontier frontier;

        frontier.wallX =
            x + direction[0] / 2;

        frontier.wallY =
            y + direction[1] / 2;

        frontier.nextX = nextX;
        frontier.nextY = nextY;

        frontiers.push_back(frontier);
    }
}

/*
 * ランダム化プリム法。
 *
 * 深さ優先探索よりも、
 * 一本道だけになりにくく、
 * 分かれ道が多めの形になりやすい。
 */
void generateMaze() {
    for (int y = 0; y < MAZE_HEIGHT; y++) {
        for (int x = 0; x < MAZE_WIDTH; x++) {
            maze[y][x] = 1;
            visited[y][x] = false;
        }
    }

    std::vector<Frontier> frontiers;

    maze[1][1] = 0;

    addFrontiers(
        1,
        1,
        frontiers
    );

    while (!frontiers.empty()) {
        std::uniform_int_distribution<int> distribution(
            0,
            static_cast<int>(frontiers.size()) - 1
        );

        int index = distribution(randomEngine);

        Frontier selected =
            frontiers[index];

        frontiers[index] =
            frontiers.back();

        frontiers.pop_back();

        if (
            maze[selected.nextY]
                [selected.nextX] == 0
        ) {
            continue;
        }

        maze[selected.wallY]
            [selected.wallX] = 0;

        maze[selected.nextY]
            [selected.nextX] = 0;

        addFrontiers(
            selected.nextX,
            selected.nextY,
            frontiers
        );
    }

    playerX = 1.5;
    playerY = 1.5;

    playerAngle = 0.0;
    cameraPitch = 0.0;

    goalX = MAZE_WIDTH - 2;
    goalY = MAZE_HEIGHT - 2;

    maze[goalY][goalX] = 0;

    score = 0;

    visited[1][1] = true;

    mapOpen = false;
    menuState = MenuState::NONE;
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

    drawButton(
        renderer,
        buttonFont,
        startButton
    );

    drawButton(
        renderer,
        buttonFont,
        settingsButton
    );

    drawButton(
        renderer,
        buttonFont,
        rulesButton
    );
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
        int mapX =
            static_cast<int>(checkX[i]);

        int mapY =
            static_cast<int>(checkY[i]);

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
    int cellX =
        static_cast<int>(playerX);

    int cellY =
        static_cast<int>(playerY);

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

void tryMove(
    double moveX,
    double moveY
) {
    double nextX =
        playerX + moveX;

    double nextY =
        playerY + moveY;

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
        menuState != MenuState::NONE
    ) {
        return;
    }

    const Uint8* keys =
        SDL_GetKeyboardState(nullptr);

    bool wPressed =
        keys[SDL_SCANCODE_W];

    bool upPressed =
        keys[SDL_SCANCODE_UP];

    /*
     * Wと上矢印の同時押しだけ走る。
     * それ以外の方向は通常速度。
     */
    double forwardSpeed = 3.0;

    if (wPressed && upPressed) {
        forwardSpeed = 5.5;
    }

    double sideSpeed = 3.0;
    double backwardSpeed = 3.0;

    double forwardX =
        std::cos(playerAngle);

    double forwardY =
        std::sin(playerAngle);

    double rightX =
        -std::sin(playerAngle);

    double rightY =
        std::cos(playerAngle);

    if (wPressed || upPressed) {
        double movement =
            forwardSpeed * deltaTime;

        tryMove(
            forwardX * movement,
            forwardY * movement
        );
    }

    // Sキーはメニュー用なので、
    // 後退は下矢印のみ
    if (keys[SDL_SCANCODE_DOWN]) {
        double movement =
            backwardSpeed * deltaTime;

        tryMove(
            -forwardX * movement,
            -forwardY * movement
        );
    }

    if (
        keys[SDL_SCANCODE_A] ||
        keys[SDL_SCANCODE_LEFT]
    ) {
        double movement =
            sideSpeed * deltaTime;

        tryMove(
            -rightX * movement,
            -rightY * movement
        );
    }

    if (
        keys[SDL_SCANCODE_D] ||
        keys[SDL_SCANCODE_RIGHT]
    ) {
        double movement =
            sideSpeed * deltaTime;

        tryMove(
            rightX * movement,
            rightY * movement
        );
    }
}

void updateMouseLook(
    int movementX,
    int movementY
) {
    if (
        mapOpen ||
        menuState != MenuState::NONE
    ) {
        return;
    }

    playerAngle +=
        movementX *
        mouseSensitivity;

    /*
     * 上へ動かすと上を見る。
     * 下へ動かすと下を見る。
     */
    cameraPitch -=
        movementY * 0.55;

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

void draw3DView(
    SDL_Renderer* renderer
) {
    int horizonY = getHorizonY();

    SDL_SetRenderDrawColor(
        renderer,
        5,
        7,
        11,
        255
    );

    SDL_Rect ceiling = {
        0,
        0,
        SCREEN_WIDTH,
        horizonY
    };

    SDL_RenderFillRect(
        renderer,
        &ceiling
    );

    SDL_SetRenderDrawColor(
        renderer,
        13,
        12,
        11,
        255
    );

    SDL_Rect floor = {
        0,
        horizonY,
        SCREEN_WIDTH,
        SCREEN_HEIGHT - horizonY
    };

    SDL_RenderFillRect(
        renderer,
        &floor
    );

    const double fieldOfView =
        PI / 3.0;

    double directionX =
        std::cos(playerAngle);

    double directionY =
        std::sin(playerAngle);

    double planeLength =
        std::tan(fieldOfView / 2.0);

    double planeX =
        -directionY * planeLength;

    double planeY =
        directionX * planeLength;

    for (
        int screenX = 0;
        screenX < SCREEN_WIDTH;
        screenX++
    ) {
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

        int mapX =
            static_cast<int>(playerX);

        int mapY =
            static_cast<int>(playerY);

        double deltaDistanceX =
            rayDirectionX == 0.0
            ? 1e30
            : std::abs(
                1.0 / rayDirectionX
            );

        double deltaDistanceY =
            rayDirectionY == 0.0
            ? 1e30
            : std::abs(
                1.0 / rayDirectionY
            );

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
            if (
                sideDistanceX <
                sideDistanceY
            ) {
                sideDistanceX +=
                    deltaDistanceX;

                mapX += stepX;
                wallSide = 0;
            } else {
                sideDistanceY +=
                    deltaDistanceY;

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

        double wallDistance;

        if (wallSide == 0) {
            wallDistance =
                sideDistanceX -
                deltaDistanceX;
        } else {
            wallDistance =
                sideDistanceY -
                deltaDistanceY;
        }

        wallDistance =
            std::max(
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

        wallTop =
            std::max(
                0,
                wallTop
            );

        wallBottom =
            std::min(
                SCREEN_HEIGHT - 1,
                wallBottom
            );

        /*
         * 壁には懐中電灯を当てない。
         * 距離による最低限の明るさだけ。
         */
        double distanceBrightness =
            1.0 /
            (1.0 + wallDistance * 0.22);

        int brightness =
            static_cast<int>(
                18 +
                distanceBrightness * 78
            );

        if (wallSide == 1) {
            brightness =
                static_cast<int>(
                    brightness * 0.70
                );
        }

        brightness = std::clamp(
            brightness,
            10,
            95
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

/*
 * 地面だけを照らす光。
 *
 * 手前：
 *  狭く、強い
 *
 * 奥：
 *  広く、弱い
 */
void drawGroundLight(
    SDL_Renderer* renderer
) {
    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_ADD
    );

    int horizonY = getHorizonY();

    int farY =
        horizonY + 12;

    int nearY =
        SCREEN_HEIGHT - 48;

    if (farY >= nearY) {
        SDL_SetRenderDrawBlendMode(
            renderer,
            SDL_BLENDMODE_BLEND
        );

        return;
    }

    int centerX =
        SCREEN_WIDTH / 2;

    int farHalfWidth = 340;
    int nearHalfWidth = 75;

    for (
        int y = farY;
        y <= nearY;
        y++
    ) {
        double progress =
            static_cast<double>(
                y - farY
            ) /
            static_cast<double>(
                nearY - farY
            );

        /*
         * 奥は広く、
         * 手前は狭くする。
         */
        int halfWidth =
            static_cast<int>(
                farHalfWidth +
                (
                    nearHalfWidth -
                    farHalfWidth
                ) *
                progress
            );

        /*
         * 奥は弱く、
         * 手前ほど強くする。
         */
        double strength =
            progress * progress;

        int alpha =
            static_cast<int>(
                2 +
                strength * 60
            );

        SDL_SetRenderDrawColor(
            renderer,
            245,
            230,
            175,
            alpha
        );

        SDL_RenderDrawLine(
            renderer,
            centerX - halfWidth,
            y,
            centerX + halfWidth,
            y
        );
    }

    /*
     * 手前の強い光を追加。
     */
    int strongStart =
        nearY - 105;

    for (
        int y = strongStart;
        y <= nearY;
        y++
    ) {
        double progress =
            static_cast<double>(
                y - strongStart
            ) /
            static_cast<double>(
                nearY - strongStart
            );

        int halfWidth =
            static_cast<int>(
                105 -
                progress * 30
            );

        int alpha =
            static_cast<int>(
                8 +
                progress * 42
            );

        SDL_SetRenderDrawColor(
            renderer,
            255,
            240,
            190,
            alpha
        );

        SDL_RenderDrawLine(
            renderer,
            centerX - halfWidth,
            y,
            centerX + halfWidth,
            y
        );
    }

    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_BLEND
    );
}

void drawCrosshair(
    SDL_Renderer* renderer
) {
    int centerX =
        SCREEN_WIDTH / 2;

    int centerY =
        SCREEN_HEIGHT / 2;

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

    SDL_RenderFillRect(
        renderer,
        &scoreBox
    );

    SDL_SetRenderDrawColor(
        renderer,
        210,
        220,
        235,
        255
    );

    SDL_RenderDrawRect(
        renderer,
        &scoreBox
    );

    drawText(
        renderer,
        font,
        "スコア：" +
            std::to_string(score),
        scoreBox.x +
            scoreBox.w / 2,
        scoreBox.y +
            scoreBox.h / 2
    );
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
        (
            SCREEN_WIDTH -
            mapPixelSize
        ) /
        2;

    int offsetY = 110;

    for (
        int relativeY =
            -MAP_VIEW_RADIUS;
        relativeY <=
            MAP_VIEW_RADIUS;
        relativeY++
    ) {
        for (
            int relativeX =
                -MAP_VIEW_RADIUS;
            relativeX <=
                MAP_VIEW_RADIUS;
            relativeX++
        ) {
            int mazeX =
                playerCellX +
                relativeX;

            int mazeY =
                playerCellY +
                relativeY;

            int screenX =
                relativeX +
                MAP_VIEW_RADIUS;

            int screenY =
                relativeY +
                MAP_VIEW_RADIUS;

            SDL_Rect tile = {
                offsetX +
                    screenX *
                    tileSize,
                offsetY +
                    screenY *
                    tileSize,
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
        "周囲5マスを表示　T：閉じる",
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

void drawGame(
    SDL_Renderer* renderer,
    TTF_Font* smallFont
) {
    draw3DView(renderer);
    drawGroundLight(renderer);
    drawCrosshair(renderer);
    drawScore(renderer, smallFont);

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
        "W・↑：前進　W＋↑：走る　↓：後退　A/D・←/→：左右　T：地図　S：メニュー",
        SCREEN_WIDTH / 2,
        SCREEN_HEIGHT - 23
    );
}

void saveGame() {
    std::ofstream file(
        "maze_save.txt"
    );

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
            file <<
                (
                    visited[y][x]
                    ? 1
                    : 0
                );
        }

        file << "\n";
    }

    saveMessageUntil =
        SDL_GetTicks() + 1800;
}

std::string getSensitivityText() {
    if (sensitivityLevel == 0) {
        return "マウス感度：低";
    }

    if (sensitivityLevel == 1) {
        return "マウス感度：普通";
    }

    return "マウス感度：高";
}

void changeSensitivity() {
    sensitivityLevel++;

    if (sensitivityLevel > 2) {
        sensitivityLevel = 0;
    }

    if (sensitivityLevel == 0) {
        mouseSensitivity = 0.0015;
    } else if (sensitivityLevel == 1) {
        mouseSensitivity = 0.0025;
    } else {
        mouseSensitivity = 0.0040;
    }
}

void drawPauseMenu(
    SDL_Renderer* renderer,
    TTF_Font* titleFont,
    TTF_Font* buttonFont,
    TTF_Font* smallFont,
    const Button& pauseSettingsButton,
    const Button& pauseSaveButton,
    const Button& pauseHomeButton
) {
    drawGame(
        renderer,
        smallFont
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
        185
    );

    SDL_Rect shade = {
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    };

    SDL_RenderFillRect(
        renderer,
        &shade
    );

    drawText(
        renderer,
        titleFont,
        "一時メニュー",
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
        "Sキーでもゲームに戻れます",
        SCREEN_WIDTH / 2,
        585
    );

    if (
        SDL_GetTicks() <
        saveMessageUntil
    ) {
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

void drawSettingsMenu(
    SDL_Renderer* renderer,
    TTF_Font* titleFont,
    TTF_Font* buttonFont,
    TTF_Font* smallFont,
    const Button& sensitivityButton,
    const Button& settingsBackButton
) {
    drawGame(
        renderer,
        smallFont
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
        205
    );

    SDL_Rect shade = {
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    };

    SDL_RenderFillRect(
        renderer,
        &shade
    );

    drawText(
        renderer,
        titleFont,
        "設定",
        SCREEN_WIDTH / 2,
        165
    );

    Button currentSensitivity =
        sensitivityButton;

    currentSensitivity.text =
        getSensitivityText();

    drawButton(
        renderer,
        buttonFont,
        currentSensitivity
    );

    drawButton(
        renderer,
        buttonFont,
        settingsBackButton
    );

    drawText(
        renderer,
        smallFont,
        "感度ボタンを押すと、低・普通・高が切り替わります",
        SCREEN_WIDTH / 2,
        500
    );
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
    int currentX =
        static_cast<int>(playerX);

    int currentY =
        static_cast<int>(playerY);

    return
        currentX == goalX &&
        currentY == goalY;
}

void startGame() {
    generateMaze();

    SDL_SetRelativeMouseMode(
        SDL_TRUE
    );
}

void returnHome() {
    mapOpen = false;
    menuState = MenuState::NONE;

    SDL_SetRelativeMouseMode(
        SDL_FALSE
    );
}

void openPauseMenu() {
    mapOpen = false;
    menuState = MenuState::PAUSE;

    SDL_SetRelativeMouseMode(
        SDL_FALSE
    );
}

void closePauseMenu() {
    menuState = MenuState::NONE;

    SDL_SetRelativeMouseMode(
        SDL_TRUE
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

    SDL_Window* window =
        SDL_CreateWindow(
            "3D Maze",
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

    SDL_Renderer* renderer =
        SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_ACCELERATED |
                SDL_RENDERER_PRESENTVSYNC
        );

    if (renderer == nullptr) {
        renderer =
            SDL_CreateRenderer(
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

    TTF_Font* titleFont =
        openFont(50);

    TTF_Font* buttonFont =
        openFont(28);

    TTF_Font* smallFont =
        openFont(17);

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
        {325, 270, 350, 75},
        "マウス感度"
    };

    Button settingsBackButton = {
        {350, 390, 300, 70},
        "戻る"
    };

    GameState gameState =
        GameState::HOME;

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

        previousCounter =
            currentCounter;

        if (deltaTime > 0.05) {
            deltaTime = 0.05;
        }

        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (
                event.type ==
                    SDL_MOUSEMOTION &&
                gameState ==
                    GameState::GAME &&
                menuState ==
                    MenuState::NONE &&
                !mapOpen
            ) {
                updateMouseLook(
                    event.motion.xrel,
                    event.motion.yrel
                );
            }

            if (
                event.type ==
                    SDL_MOUSEBUTTONDOWN &&
                event.button.button ==
                    SDL_BUTTON_LEFT
            ) {
                int mouseX =
                    event.button.x;

                int mouseY =
                    event.button.y;

                if (
                    gameState ==
                    GameState::HOME
                ) {
                    if (
                        isButtonClicked(
                            startButton,
                            mouseX,
                            mouseY
                        )
                    ) {
                        startGame();

                        gameState =
                            GameState::GAME;
                    }
                } else if (
                    gameState ==
                        GameState::GAME &&
                    menuState ==
                        MenuState::PAUSE
                ) {
                    if (
                        isButtonClicked(
                            pauseSettingsButton,
                            mouseX,
                            mouseY
                        )
                    ) {
                        menuState =
                            MenuState::SETTINGS;
                    } else if (
                        isButtonClicked(
                            pauseSaveButton,
                            mouseX,
                            mouseY
                        )
                    ) {
                        saveGame();
                    } else if (
                        isButtonClicked(
                            pauseHomeButton,
                            mouseX,
                            mouseY
                        )
                    ) {
                        returnHome();

                        gameState =
                            GameState::HOME;
                    }
                } else if (
                    gameState ==
                        GameState::GAME &&
                    menuState ==
                        MenuState::SETTINGS
                ) {
                    if (
                        isButtonClicked(
                            sensitivityButton,
                            mouseX,
                            mouseY
                        )
                    ) {
                        changeSensitivity();
                    } else if (
                        isButtonClicked(
                            settingsBackButton,
                            mouseX,
                            mouseY
                        )
                    ) {
                        menuState =
                            MenuState::PAUSE;
                    }
                }
            }

            if (
                event.type ==
                    SDL_KEYDOWN &&
                event.key.repeat == 0
            ) {
                SDL_Keycode key =
                    event.key.keysym.sym;

                if (
                    gameState ==
                        GameState::GAME &&
                    key == SDLK_s
                ) {
                    if (
                        menuState ==
                        MenuState::NONE
                    ) {
                        openPauseMenu();
                    } else if (
                        menuState ==
                        MenuState::PAUSE
                    ) {
                        closePauseMenu();
                    }
                }

                if (
                    gameState ==
                        GameState::GAME &&
                    key == SDLK_t &&
                    menuState ==
                        MenuState::NONE
                ) {
                    mapOpen = !mapOpen;

                    if (mapOpen) {
                        SDL_SetRelativeMouseMode(
                            SDL_FALSE
                        );
                    } else {
                        SDL_SetRelativeMouseMode(
                            SDL_TRUE
                        );
                    }
                }

                if (
                    gameState ==
                        GameState::CLEAR &&
                    key == SDLK_RETURN
                ) {
                    startGame();

                    gameState =
                        GameState::GAME;
                }
            }
        }

        if (
            gameState ==
                GameState::GAME &&
            menuState ==
                MenuState::NONE &&
            !mapOpen
        ) {
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
            gameState ==
            GameState::HOME
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
            gameState ==
                GameState::GAME &&
            menuState ==
                MenuState::PAUSE
        ) {
            drawPauseMenu(
                renderer,
                titleFont,
                buttonFont,
                smallFont,
                pauseSettingsButton,
                pauseSaveButton,
                pauseHomeButton
            );
        } else if (
            gameState ==
                GameState::GAME &&
            menuState ==
                MenuState::SETTINGS
        ) {
            drawSettingsMenu(
                renderer,
                titleFont,
                buttonFont,
                smallFont,
                sensitivityButton,
                settingsBackButton
            );
        } else if (
            gameState ==
                GameState::GAME &&
            mapOpen
        ) {
            drawLocalMap(
                renderer,
                titleFont,
                smallFont
            );
        } else if (
            gameState ==
            GameState::GAME
        ) {
            drawGame(
                renderer,
                smallFont
            );
        } else if (
            gameState ==
            GameState::CLEAR
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

    SDL_SetRelativeMouseMode(
        SDL_FALSE
    );

    TTF_CloseFont(smallFont);
    TTF_CloseFont(buttonFont);
    TTF_CloseFont(titleFont);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    return 0;
}
