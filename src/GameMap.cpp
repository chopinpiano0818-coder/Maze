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
    drawWorldObjects(renderer);
    drawEnemy(renderer);
    for (const auto& e : extraChasers) if (e.alive) drawSpriteAt(renderer, e.x, e.y, 0.92, 110, 255, 190);
    for (const auto& patrol : patrolEnemies) {
        drawSpriteAt(renderer, patrol.x, patrol.y, 0.82, patrol.vertical ? 255 : 150, patrol.vertical ? 155 : 220, 255);
    }
    drawKeySprite(renderer);
    drawCrosshair(renderer);
    drawScore(renderer, smallFont);

    int needKeys = getDifficultyConfig(selectedDifficulty).requiredKeys;
    drawText(renderer, smallFont, "鍵：" + std::to_string(collectedKeyCount()) + "/" + std::to_string(needKeys), 105, 88,
             collectedKeyCount() >= needKeys ? SDL_Color{255,215,70,255} : SDL_Color{220,220,220,255});
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

    if (enemyAlive) drawMapMarker(enemyX, enemyY, {230, 55, 55, 255}, 16);
    for (const auto& e : extraChasers) if (e.alive) drawMapMarker(e.x, e.y, {255,80,130,255}, 16);
    for (const auto& patrol : patrolEnemies) drawMapMarker(patrol.x, patrol.y, patrol.vertical ? SDL_Color{255,130,60,255} : SDL_Color{190,80,255,255}, 14);
    for (const auto& key : keysInMaze) if (key.available) drawMapMarker(key.x, key.y, key.color, 14);
    for (const auto& trap : floorTraps) if (!trap.usedByPlayer) drawMapMarker(trap.x, trap.y, {230,70,70,255}, 8);
    for (const auto& item : randomItems) if (item.available) drawMapMarker(item.x, item.y, {80,230,255,255}, 9);

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

