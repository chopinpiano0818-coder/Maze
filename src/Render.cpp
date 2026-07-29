int getHorizonY() {
    int horizonY =
        SCREEN_HEIGHT / 2 +
        static_cast<int>(cameraPitch) +
        (playerCrouching ? 54 : 0) - static_cast<int>(jumpHeight * 135.0);

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

    for (int screenX = 0; screenX < SCREEN_WIDTH; screenX += RAY_STEP) {
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

        for (int dx = 0; dx < RAY_STEP && screenX + dx < SCREEN_WIDTH; ++dx) depthBuffer[screenX + dx] = wallDistance;
        
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
            SDL_Rect destination = {screenX, wallTop, std::min(RAY_STEP, SCREEN_WIDTH - screenX), wallBottom - wallTop + 1};
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
            SDL_Rect wallColumn = {screenX, wallTop, std::min(RAY_STEP, SCREEN_WIDTH - screenX), wallBottom - wallTop + 1};
            SDL_RenderFillRect(renderer, &wallColumn);
        }
    }
}


