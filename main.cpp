#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

const int SCREEN_WIDTH = 1000;
const int SCREEN_HEIGHT = 700;

// 迷路の縦横は奇数にする
const int MAZE_WIDTH = 61;
const int MAZE_HEIGHT = 61;

const double PI = 3.14159265358979323846;

enum class GameState {
    HOME,
    GAME,
    CLEAR
};

struct Button {
    SDL_Rect rect;
    std::string text;
};

std::vector<std::vector<int>> maze(
    MAZE_HEIGHT,
    std::vector<int>(MAZE_WIDTH, 1)
);

std::mt19937 randomEngine(std::random_device{}());

// プレイヤーの位置
double playerX = 1.5;
double playerY = 1.5;

// プレイヤーの向き
double playerAngle = 0.0;

// ゴール
int goalX = MAZE_WIDTH - 2;
int goalY = MAZE_HEIGHT - 2;

bool isInsideMaze(int x, int y) {
    return x > 0 &&
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

void generateMaze() {
    for (int y = 0; y < MAZE_HEIGHT; y++) {
        for (int x = 0; x < MAZE_WIDTH; x++) {
            maze[y][x] = 1;
        }
    }

    generateMazeFrom(1, 1);

    playerX = 1.5;
    playerY = 1.5;
    playerAngle = 0.0;

    goalX = MAZE_WIDTH - 2;
    goalY = MAZE_HEIGHT - 2;

    maze[1][1] = 0;
    maze[goalY][goalX] = 0;
}

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

    SDL_GetMouseState(&mouseX, &mouseY);

    bool hovered = isButtonClicked(
        button,
        mouseX,
        mouseY
    );

    if (hovered) {
        SDL_SetRenderDrawColor(
            renderer,
            80,
            105,
            155,
            255
        );
    } else {
        SDL_SetRenderDrawColor(
            renderer,
            50,
            60,
            85,
            255
        );
    }

    SDL_RenderFillRect(
        renderer,
        &button.rect
    );

    SDL_SetRenderDrawColor(
        renderer,
        200,
        210,
        230,
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

void tryMove(double moveX, double moveY) {
    double nextX = playerX + moveX;
    double nextY = playerY + moveY;

    // X方向とY方向を別々に判定する
    // これにより壁に沿って滑らかに移動できる
    if (canMoveTo(nextX, playerY)) {
        playerX = nextX;
    }

    if (canMoveTo(playerX, nextY)) {
        playerY = nextY;
    }
}

void updatePlayer(double deltaTime) {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);

    const double moveSpeed = 3.0;
    const double rotationSpeed = 2.2;

    double forwardX = std::cos(playerAngle);
    double forwardY = std::sin(playerAngle);

    double rightX = -std::sin(playerAngle);
    double rightY = std::cos(playerAngle);

    double movement = moveSpeed * deltaTime;

    // 前進
    if (
        keys[SDL_SCANCODE_W] ||
        keys[SDL_SCANCODE_UP]
    ) {
        tryMove(
            forwardX * movement,
            forwardY * movement
        );
    }

    // 後退
    if (
        keys[SDL_SCANCODE_S] ||
        keys[SDL_SCANCODE_DOWN]
    ) {
        tryMove(
            -forwardX * movement,
            -forwardY * movement
        );
    }

    // 左へ平行移動
    if (keys[SDL_SCANCODE_A]) {
        tryMove(
            -rightX * movement,
            -rightY * movement
        );
    }

    // 右へ平行移動
    if (keys[SDL_SCANCODE_D]) {
        tryMove(
            rightX * movement,
            rightY * movement
        );
    }

    // 左を向く
    if (keys[SDL_SCANCODE_LEFT]) {
        playerAngle -= rotationSpeed * deltaTime;
    }

    // 右を向く
    if (keys[SDL_SCANCODE_RIGHT]) {
        playerAngle += rotationSpeed * deltaTime;
    }

    if (playerAngle < 0.0) {
        playerAngle += PI * 2.0;
    }

    if (playerAngle >= PI * 2.0) {
        playerAngle -= PI * 2.0;
    }
}

void draw3DView(SDL_Renderer* renderer) {
    // 天井
    SDL_SetRenderDrawColor(
        renderer,
        35,
        42,
        55,
        255
    );

    SDL_Rect ceiling = {
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT / 2
    };

    SDL_RenderFillRect(
        renderer,
        &ceiling
    );

    // 床
    SDL_SetRenderDrawColor(
        renderer,
        65,
        62,
        58,
        255
    );

    SDL_Rect floor = {
        0,
        SCREEN_HEIGHT / 2,
        SCREEN_WIDTH,
        SCREEN_HEIGHT / 2
    };

    SDL_RenderFillRect(
        renderer,
        &floor
    );

    const double fieldOfView = PI / 3.0;

    // 画面の縦線1本ずつ光線を飛ばす
    for (int screenX = 0; screenX < SCREEN_WIDTH; screenX++) {
        double cameraX =
            2.0 * screenX / static_cast<double>(SCREEN_WIDTH) - 1.0;

        double directionX = std::cos(playerAngle);
        double directionY = std::sin(playerAngle);

        double planeLength = std::tan(fieldOfView / 2.0);

        double planeX = -directionY * planeLength;
        double planeY = directionX * planeLength;

        double rayDirectionX =
            directionX + planeX * cameraX;

        double rayDirectionY =
            directionY + planeY * cameraX;

        int mapX = static_cast<int>(playerX);
        int mapY = static_cast<int>(playerY);

        double deltaDistanceX;

        if (rayDirectionX == 0.0) {
            deltaDistanceX = 1e30;
        } else {
            deltaDistanceX = std::abs(1.0 / rayDirectionX);
        }

        double deltaDistanceY;

        if (rayDirectionY == 0.0) {
            deltaDistanceY = 1e30;
        } else {
            deltaDistanceY = std::abs(1.0 / rayDirectionY);
        }

        int stepX;
        int stepY;

        double sideDistanceX;
        double sideDistanceY;

        if (rayDirectionX < 0.0) {
            stepX = -1;

            sideDistanceX =
                (playerX - mapX) * deltaDistanceX;
        } else {
            stepX = 1;

            sideDistanceX =
                (mapX + 1.0 - playerX) * deltaDistanceX;
        }

        if (rayDirectionY < 0.0) {
            stepY = -1;

            sideDistanceY =
                (playerY - mapY) * deltaDistanceY;
        } else {
            stepY = 1;

            sideDistanceY =
                (mapY + 1.0 - playerY) * deltaDistanceY;
        }

        bool wallHit = false;
        int wallSide = 0;

        while (!wallHit) {
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
                wallHit = true;
                break;
            }

            if (maze[mapY][mapX] == 1) {
                wallHit = true;
            }
        }

        double wallDistance;

        if (wallSide == 0) {
            wallDistance =
                sideDistanceX - deltaDistanceX;
        } else {
            wallDistance =
                sideDistanceY - deltaDistanceY;
        }

        if (wallDistance < 0.01) {
            wallDistance = 0.01;
        }

        int wallHeight = static_cast<int>(
            SCREEN_HEIGHT / wallDistance
        );

        int wallTop =
            -wallHeight / 2 +
            SCREEN_HEIGHT / 2;

        int wallBottom =
            wallHeight / 2 +
            SCREEN_HEIGHT / 2;

        if (wallTop < 0) {
            wallTop = 0;
        }

        if (wallBottom >= SCREEN_HEIGHT) {
            wallBottom = SCREEN_HEIGHT - 1;
        }

        // 距離によって壁を暗くする
        int brightness = static_cast<int>(
            230.0 / (1.0 + wallDistance * 0.12)
        );

        brightness = std::clamp(
            brightness,
            35,
            220
        );

        // 横向きの壁は少し暗くする
        if (wallSide == 1) {
            brightness =
                static_cast<int>(brightness * 0.72);
        }

        SDL_SetRenderDrawColor(
            renderer,
            brightness,
            brightness,
            std::min(255, brightness + 18),
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

void drawMiniMap(SDL_Renderer* renderer) {
    const int mapSize = 190;
    const int mapX = SCREEN_WIDTH - mapSize - 15;
    const int mapY = 15;

    const int visibleRadius = 7;
    const int miniTile = 12;

    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_BLEND
    );

    SDL_SetRenderDrawColor(
        renderer,
        5,
        8,
        12,
        210
    );

    SDL_Rect background = {
        mapX,
        mapY,
        mapSize,
        mapSize
    };

    SDL_RenderFillRect(
        renderer,
        &background
    );

    int playerMapX = static_cast<int>(playerX);
    int playerMapY = static_cast<int>(playerY);

    for (
        int mazeY = playerMapY - visibleRadius;
        mazeY <= playerMapY + visibleRadius;
        mazeY++
    ) {
        for (
            int mazeX = playerMapX - visibleRadius;
            mazeX <= playerMapX + visibleRadius;
            mazeX++
        ) {
            int relativeX =
                mazeX - playerMapX + visibleRadius;

            int relativeY =
                mazeY - playerMapY + visibleRadius;

            SDL_Rect tile = {
                mapX + 5 + relativeX * miniTile,
                mapY + 5 + relativeY * miniTile,
                miniTile,
                miniTile
            };

            if (
                mazeX < 0 ||
                mazeX >= MAZE_WIDTH ||
                mazeY < 0 ||
                mazeY >= MAZE_HEIGHT
            ) {
                SDL_SetRenderDrawColor(
                    renderer,
                    20,
                    20,
                    25,
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
            } else if (maze[mazeY][mazeX] == 1) {
                SDL_SetRenderDrawColor(
                    renderer,
                    75,
                    85,
                    105,
                    255
                );
            } else {
                SDL_SetRenderDrawColor(
                    renderer,
                    195,
                    195,
                    190,
                    255
                );
            }

            SDL_RenderFillRect(
                renderer,
                &tile
            );
        }
    }

    int centerX =
        mapX + 5 +
        visibleRadius * miniTile +
        miniTile / 2;

    int centerY =
        mapY + 5 +
        visibleRadius * miniTile +
        miniTile / 2;

    // プレイヤー
    SDL_SetRenderDrawColor(
        renderer,
        40,
        130,
        255,
        255
    );

    SDL_Rect playerRect = {
        centerX - 4,
        centerY - 4,
        8,
        8
    };

    SDL_RenderFillRect(
        renderer,
        &playerRect
    );

    // 向いている方向
    SDL_SetRenderDrawColor(
        renderer,
        255,
        255,
        255,
        255
    );

    int directionEndX =
        centerX +
        static_cast<int>(std::cos(playerAngle) * 15.0);

    int directionEndY =
        centerY +
        static_cast<int>(std::sin(playerAngle) * 15.0);

    SDL_RenderDrawLine(
        renderer,
        centerX,
        centerY,
        directionEndX,
        directionEndY
    );

    SDL_SetRenderDrawColor(
        renderer,
        220,
        225,
        235,
        255
    );

    SDL_RenderDrawRect(
        renderer,
        &background
    );
}

void drawCrosshair(SDL_Renderer* renderer) {
    int centerX = SCREEN_WIDTH / 2;
    int centerY = SCREEN_HEIGHT / 2;

    SDL_SetRenderDrawColor(
        renderer,
        255,
        255,
        255,
        210
    );

    SDL_RenderDrawLine(
        renderer,
        centerX - 7,
        centerY,
        centerX + 7,
        centerY
    );

    SDL_RenderDrawLine(
        renderer,
        centerX,
        centerY - 7,
        centerX,
        centerY + 7
    );
}

void drawGame(
    SDL_Renderer* renderer,
    TTF_Font* smallFont
) {
    draw3DView(renderer);
    drawMiniMap(renderer);
    drawCrosshair(renderer);

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
        SCREEN_HEIGHT - 48,
        SCREEN_WIDTH,
        48
    };

    SDL_RenderFillRect(
        renderer,
        &informationBar
    );

    drawText(
        renderer,
        smallFont,
        "W・↑ 前進　S・↓ 後退　A・D 平行移動　←・→ 回転　Esc ホーム",
        SCREEN_WIDTH / 2,
        SCREEN_HEIGHT - 24
    );
}

void drawClear(
    SDL_Renderer* renderer,
    TTF_Font* titleFont,
    TTF_Font* buttonFont
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
        270,
        green
    );

    drawText(
        renderer,
        buttonFont,
        "Enter：もう一度　Esc：ホーム",
        SCREEN_WIDTH / 2,
        390
    );
}

bool playerReachedGoal() {
    int currentX = static_cast<int>(playerX);
    int currentY = static_cast<int>(playerY);

    return
        currentX == goalX &&
        currentY == goalY;
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

    TTF_Font* titleFont = openFont(54);
    TTF_Font* buttonFont = openFont(30);
    TTF_Font* smallFont = openFont(18);

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

    Button settingsButton = {
        {350, 375, 300, 75},
        "設定"
    };

    Button rulesButton = {
        {350, 480, 300, 75},
        "ルール"
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
                currentCounter - previousCounter
            ) /
            static_cast<double>(
                SDL_GetPerformanceFrequency()
            );

        previousCounter = currentCounter;

        // ウィンドウ移動後などの大きな時間差を制限する
        if (deltaTime > 0.05) {
            deltaTime = 0.05;
        }

        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (
                event.type == SDL_MOUSEBUTTONDOWN &&
                event.button.button == SDL_BUTTON_LEFT &&
                gameState == GameState::HOME
            ) {
                int mouseX = event.button.x;
                int mouseY = event.button.y;

                if (
                    isButtonClicked(
                        startButton,
                        mouseX,
                        mouseY
                    )
                ) {
                    generateMaze();
                    gameState = GameState::GAME;
                }
            }

            if (event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;

                if (key == SDLK_ESCAPE) {
                    if (
                        gameState == GameState::GAME ||
                        gameState == GameState::CLEAR
                    ) {
                        gameState = GameState::HOME;
                    } else {
                        running = false;
                    }
                }

                if (
                    gameState == GameState::CLEAR &&
                    key == SDLK_RETURN
                ) {
                    generateMaze();
                    gameState = GameState::GAME;
                }
            }
        }

        if (gameState == GameState::GAME) {
            updatePlayer(deltaTime);

            if (playerReachedGoal()) {
                gameState = GameState::CLEAR;
            }
        }

        if (gameState == GameState::HOME) {
            drawHome(
                renderer,
                titleFont,
                buttonFont,
                startButton,
                settingsButton,
                rulesButton
            );
        } else if (gameState == GameState::GAME) {
            drawGame(
                renderer,
                smallFont
            );
        } else if (gameState == GameState::CLEAR) {
            drawClear(
                renderer,
                titleFont,
                buttonFont
            );
        }

        SDL_RenderPresent(renderer);
    }

    TTF_CloseFont(smallFont);
    TTF_CloseFont(buttonFont);
    TTF_CloseFont(titleFont);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    return 0;
}
