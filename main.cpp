#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>

const int SCREEN_WIDTH = 1000;
const int SCREEN_HEIGHT = 700;

const int MAZE_WIDTH = 31;
const int MAZE_HEIGHT = 21;

enum class GameState {
    HOME,
    MAZE
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

    maze[1][1] = 0;
    maze[MAZE_HEIGHT - 2][MAZE_WIDTH - 2] = 0;
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
    int centerY
) {
    SDL_Color color = {255, 255, 255, 255};

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
        std::cerr
            << "文字テクスチャの作成に失敗: "
            << SDL_GetError()
            << std::endl;

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

void drawButton(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const Button& button
) {
    int mouseX;
    int mouseY;

    SDL_GetMouseState(&mouseX, &mouseY);

    bool hovered =
        mouseX >= button.rect.x &&
        mouseX <= button.rect.x + button.rect.w &&
        mouseY >= button.rect.y &&
        mouseY <= button.rect.y + button.rect.h;

    if (hovered) {
        SDL_SetRenderDrawColor(
            renderer,
            80,
            100,
            145,
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

    SDL_RenderFillRect(renderer, &button.rect);

    SDL_SetRenderDrawColor(
        renderer,
        200,
        210,
        230,
        255
    );

    SDL_RenderDrawRect(renderer, &button.rect);

    drawText(
        renderer,
        font,
        button.text,
        button.rect.x + button.rect.w / 2,
        button.rect.y + button.rect.h / 2
    );
}

bool isButtonClicked(
    const Button& button,
    int mouseX,
    int mouseY
) {
    return
        mouseX >= button.rect.x &&
        mouseX <= button.rect.x + button.rect.w &&
        mouseY >= button.rect.y &&
        mouseY <= button.rect.y + button.rect.h;
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

void drawMaze(
    SDL_Renderer* renderer,
    TTF_Font* smallFont
) {
    SDL_SetRenderDrawColor(
        renderer,
        20,
        20,
        25,
        255
    );

    SDL_RenderClear(renderer);

    int cellWidth = SCREEN_WIDTH / MAZE_WIDTH;
    int cellHeight = 600 / MAZE_HEIGHT;

    int cellSize = std::min(cellWidth, cellHeight);

    int mazePixelWidth = MAZE_WIDTH * cellSize;
    int mazePixelHeight = MAZE_HEIGHT * cellSize;

    int offsetX =
        (SCREEN_WIDTH - mazePixelWidth) / 2;

    int offsetY =
        (SCREEN_HEIGHT - mazePixelHeight) / 2 + 25;

    for (int y = 0; y < MAZE_HEIGHT; y++) {
        for (int x = 0; x < MAZE_WIDTH; x++) {
            SDL_Rect cell = {
                offsetX + x * cellSize,
                offsetY + y * cellSize,
                cellSize,
                cellSize
            };

            if (maze[y][x] == 1) {
                SDL_SetRenderDrawColor(
                    renderer,
                    70,
                    80,
                    100,
                    255
                );
            } else {
                SDL_SetRenderDrawColor(
                    renderer,
                    220,
                    220,
                    220,
                    255
                );
            }

            SDL_RenderFillRect(renderer, &cell);
        }
    }

    SDL_Rect startCell = {
        offsetX + cellSize,
        offsetY + cellSize,
        cellSize,
        cellSize
    };

    SDL_SetRenderDrawColor(
        renderer,
        50,
        130,
        255,
        255
    );

    SDL_RenderFillRect(renderer, &startCell);

    SDL_Rect goalCell = {
        offsetX + (MAZE_WIDTH - 2) * cellSize,
        offsetY + (MAZE_HEIGHT - 2) * cellSize,
        cellSize,
        cellSize
    };

    SDL_SetRenderDrawColor(
        renderer,
        60,
        210,
        100,
        255
    );

    SDL_RenderFillRect(renderer, &goalCell);

    drawText(
        renderer,
        smallFont,
        "青：スタート　緑：ゴール　Esc：ホームへ戻る",
        SCREEN_WIDTH / 2,
        35
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
                event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_ESCAPE
            ) {
                if (gameState == GameState::MAZE) {
                    gameState = GameState::HOME;
                } else {
                    running = false;
                }
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
        }

        if (gameState == GameState::MAZE) {
            drawMaze(
                renderer,
                smallFont
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
