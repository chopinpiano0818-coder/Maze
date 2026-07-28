#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

const int SCREEN_WIDTH = 1000;
const int SCREEN_HEIGHT = 700;

// 必ず奇数にする
const int MAZE_WIDTH = 61;
const int MAZE_HEIGHT = 61;

const int TILE_SIZE = 42;

enum class GameState {
    HOME,
    MAZE,
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

int playerX = 1;
int playerY = 1;

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

    playerX = 1;
    playerY = 1;

    goalX = MAZE_WIDTH - 2;
    goalY = MAZE_HEIGHT - 2;

    maze[playerY][playerX] = 0;
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
        std::cerr
            << "文字の作成に失敗: "
            << TTF_GetError()
            << std::endl;

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

void movePlayer(int moveX, int moveY) {
    int nextX = playerX + moveX;
    int nextY = playerY + moveY;

    if (
        nextX < 0 ||
        nextX >= MAZE_WIDTH ||
        nextY < 0 ||
        nextY >= MAZE_HEIGHT
    ) {
        return;
    }

    if (maze[nextY][nextX] == 1) {
        return;
    }

    playerX = nextX;
    playerY = nextY;
}

void drawMaze(
    SDL_Renderer* renderer,
    TTF_Font* smallFont
) {
    SDL_SetRenderDrawColor(
        renderer,
        15,
        18,
        24,
        255
    );

    SDL_RenderClear(renderer);

    // プレイヤーを常に中央へ固定
    const int playerScreenX = SCREEN_WIDTH / 2;
    const int playerScreenY = SCREEN_HEIGHT / 2;

    // 迷路側をずらす量
    int cameraOffsetX =
        playerScreenX - playerX * TILE_SIZE;

    int cameraOffsetY =
        playerScreenY - playerY * TILE_SIZE;

    for (int y = 0; y < MAZE_HEIGHT; y++) {
        for (int x = 0; x < MAZE_WIDTH; x++) {
            int screenX =
                cameraOffsetX + x * TILE_SIZE;

            int screenY =
                cameraOffsetY + y * TILE_SIZE;

            // 画面外のマスは描画しない
            if (
                screenX + TILE_SIZE < 0 ||
                screenX >= SCREEN_WIDTH ||
                screenY + TILE_SIZE < 0 ||
                screenY >= SCREEN_HEIGHT
            ) {
                continue;
            }

            SDL_Rect cell = {
                screenX,
                screenY,
                TILE_SIZE,
                TILE_SIZE
            };

            if (maze[y][x] == 1) {
                SDL_SetRenderDrawColor(
                    renderer,
                    50,
                    58,
                    72,
                    255
                );

                SDL_RenderFillRect(
                    renderer,
                    &cell
                );

                SDL_SetRenderDrawColor(
                    renderer,
                    85,
                    95,
                    115,
                    255
                );

                SDL_RenderDrawRect(
                    renderer,
                    &cell
                );
            } else {
                SDL_SetRenderDrawColor(
                    renderer,
                    200,
                    200,
                    195,
                    255
                );

                SDL_RenderFillRect(
                    renderer,
                    &cell
                );

                SDL_SetRenderDrawColor(
                    renderer,
                    175,
                    175,
                    170,
                    255
                );

                SDL_RenderDrawRect(
                    renderer,
                    &cell
                );
            }

            if (x == goalX && y == goalY) {
                SDL_Rect goalRect = {
                    screenX + 7,
                    screenY + 7,
                    TILE_SIZE - 14,
                    TILE_SIZE - 14
                };

                SDL_SetRenderDrawColor(
                    renderer,
                    60,
                    220,
                    100,
                    255
                );

                SDL_RenderFillRect(
                    renderer,
                    &goalRect
                );
            }
        }
    }

    // プレイヤーは画面中央に固定
    SDL_Rect playerRect = {
        playerScreenX - TILE_SIZE / 2 + 6,
        playerScreenY - TILE_SIZE / 2 + 6,
        TILE_SIZE - 12,
        TILE_SIZE - 12
    };

    SDL_SetRenderDrawColor(
        renderer,
        60,
        140,
        255,
        255
    );

    SDL_RenderFillRect(
        renderer,
        &playerRect
    );

    SDL_SetRenderDrawColor(
        renderer,
        230,
        240,
        255,
        255
    );

    SDL_RenderDrawRect(
        renderer,
        &playerRect
    );

    // 中央を分かりやすくする小さい印
    SDL_SetRenderDrawColor(
        renderer,
        255,
        255,
        255,
        255
    );

    SDL_RenderDrawLine(
        renderer,
        playerScreenX - 6,
        playerScreenY,
        playerScreenX + 6,
        playerScreenY
    );

    SDL_RenderDrawLine(
        renderer,
        playerScreenX,
        playerScreenY - 6,
        playerScreenX,
        playerScreenY + 6
    );

    SDL_SetRenderDrawColor(
        renderer,
        10,
        12,
        18,
        230
    );

    SDL_Rect topBar = {
        0,
        0,
        SCREEN_WIDTH,
        60
    };

    SDL_RenderFillRect(
        renderer,
        &topBar
    );

    drawText(
        renderer,
        smallFont,
        "WASD・矢印キー：移動　緑：ゴール　Esc：ホーム",
        SCREEN_WIDTH / 2,
        30
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
    TTF_Font* smallFont = openFont(20);

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

    while (running) {
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
                    gameState = GameState::MAZE;
                }
            }

            if (event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;

                if (key == SDLK_ESCAPE) {
                    if (
                        gameState == GameState::MAZE ||
                        gameState == GameState::CLEAR
                    ) {
                        gameState = GameState::HOME;
                    } else {
                        running = false;
                    }
                }

                if (gameState == GameState::MAZE) {
                    if (
                        key == SDLK_w ||
                        key == SDLK_UP
                    ) {
                        movePlayer(0, -1);
                    }

                    if (
                        key == SDLK_s ||
                        key == SDLK_DOWN
                    ) {
                        movePlayer(0, 1);
                    }

                    if (
                        key == SDLK_a ||
                        key == SDLK_LEFT
                    ) {
                        movePlayer(-1, 0);
                    }

                    if (
                        key == SDLK_d ||
                        key == SDLK_RIGHT
                    ) {
                        movePlayer(1, 0);
                    }

                    if (
                        playerX == goalX &&
                        playerY == goalY
                    ) {
                        gameState = GameState::CLEAR;
                    }
                }

                if (
                    gameState == GameState::CLEAR &&
                    key == SDLK_RETURN
                ) {
                    generateMaze();
                    gameState = GameState::MAZE;
                }
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
        } else if (gameState == GameState::MAZE) {
            drawMaze(
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
