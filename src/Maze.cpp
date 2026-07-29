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
    DifficultyConfig config = getDifficultyConfig(selectedDifficulty);
    MAZE_CELLS = config.cells;
    MAZE_WIDTH = MAZE_CELLS * 2 + 1;
    MAZE_HEIGHT = MAZE_CELLS * 2 + 1;
    maze.assign(MAZE_HEIGHT, std::vector<int>(MAZE_WIDTH, 1));
    visited.assign(MAZE_HEIGHT, std::vector<bool>(MAZE_WIDTH, false));
    depthBuffer.assign(SCREEN_WIDTH, 1e30);
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
    enemyHealth = 2;
    enemyAlive = true;
    patrolEnemies.clear();
    floorTraps.clear();
    randomItems.clear();
    jumpHeight = 0.0;
    jumpVelocity = 0.0;
    playerOnGround = true;
    speedEffectMultiplier = 1.0;
    speedEffectUntil = 0;

    goalX = MAZE_WIDTH - 2;
    goalY = MAZE_HEIGHT - 2;

    maze[1][1] = 0;
    maze[goalY][goalX] = 0;

    // 敵はスタートからなるべく遠い通路に配置
    enemyX = goalX + 0.5;
    enemyY = goalY + 0.5;
    extraChasers.clear();
    for (int i = 1; i < config.chaserCount; ++i) {
        ExtraChaser extra;
        extra.x = std::max(1.5, goalX + 0.5 - i * 2.0);
        extra.y = goalY + 0.5;
        extraChasers.push_back(extra);
    }

    // 難易度に応じて1～3色の鍵を離れた場所へ配置。
    keysInMaze.clear();
    std::vector<SDL_Color> keyColors = {
        {255, 215, 40, 255},   // 黄
        {50, 145, 255, 255},   // 青
        {245, 60, 65, 255}     // 赤
    };
    std::vector<std::string> keyNames = {"黄", "青", "赤"};
    std::vector<std::pair<int,int>> keyCandidates;
    for (int y = 1; y < MAZE_HEIGHT - 1; ++y) {
        for (int x = 1; x < MAZE_WIDTH - 1; ++x) {
            if (maze[y][x] != 0) continue;
            if (std::hypot(x - 1.0, y - 1.0) < MAZE_CELLS * 0.45) continue;
            if (std::hypot(x - goalX, y - goalY) < 2.0) continue;
            keyCandidates.push_back({x,y});
        }
    }
    std::shuffle(keyCandidates.begin(), keyCandidates.end(), randomEngine);
    for (int i = 0; i < config.requiredKeys && i < static_cast<int>(keyCandidates.size()); ++i) {
        auto [kx, ky] = keyCandidates[i];
        keysInMaze.push_back({kx + 0.5, ky + 0.5, keyColors[i], keyNames[i], true, false});
    }
    actionMessage.clear();
    actionMessageUntil = 0;

    // 直線往復型ロボットを2体配置する。
    auto findPatrol = [&](bool vertical) -> PatrolEnemy {
        PatrolEnemy result;
        result.vertical = vertical;
        int bestLength = 0;
        for (int y = 1; y < MAZE_HEIGHT - 1; ++y) {
            for (int x = 1; x < MAZE_WIDTH - 1; ++x) {
                if (maze[y][x] != 0) continue;
                int ex = x, ey = y;
                if (vertical) {
                    while (ey + 1 < MAZE_HEIGHT - 1 && maze[ey + 1][x] == 0) ++ey;
                    int len = ey - y;
                    if (len > bestLength) {
                        bestLength = len;
                        result.startX = result.endX = x + 0.5;
                        result.startY = y + 0.5;
                        result.endY = ey + 0.5;
                    }
                } else {
                    while (ex + 1 < MAZE_WIDTH - 1 && maze[y][ex + 1] == 0) ++ex;
                    int len = ex - x;
                    if (len > bestLength) {
                        bestLength = len;
                        result.startY = result.endY = y + 0.5;
                        result.startX = x + 0.5;
                        result.endX = ex + 0.5;
                    }
                }
            }
        }
        result.x = result.startX;
        result.y = result.startY;
        return result;
    };
    for (int i = 0; i < config.verticalCount; ++i) {
        PatrolEnemy p = findPatrol(true);
        p.x = p.startX;
        p.y = std::min(p.endY, p.startY + i * 0.55);
        patrolEnemies.push_back(p);
    }
    for (int i = 0; i < config.horizontalCount; ++i) {
        PatrolEnemy p = findPatrol(false);
        p.x = std::min(p.endX, p.startX + i * 0.55);
        p.y = p.startY;
        patrolEnemies.push_back(p);
    }

    // 通路に床トラップを生成（スタート・ゴール・鍵の近くは避ける）。
    std::vector<std::pair<int,int>> openCells;
    for (int y = 1; y < MAZE_HEIGHT - 1; ++y) {
        for (int x = 1; x < MAZE_WIDTH - 1; ++x) {
            if (maze[y][x] != 0) continue;
            if (std::hypot(x - 1.0, y - 1.0) < 2.5) continue;
            if (std::hypot(x - goalX, y - goalY) < 2.0) continue;
            bool nearKey = false;
            for (const auto& key : keysInMaze) if (std::hypot(x + 0.5 - key.x, y + 0.5 - key.y) < 1.5) nearKey = true;
            if (nearKey) continue;
            openCells.push_back({x,y});
        }
    }
    std::shuffle(openCells.begin(), openCells.end(), randomEngine);
    int trapCount = std::min(config.trapCount, static_cast<int>(openCells.size()));
    for (int i = 0; i < trapCount; ++i) {
        floorTraps.push_back({openCells[i].first + 0.5, openCells[i].second + 0.5, false, false});
    }

    // 低確率のランダムアイテム。今回は迷路ごとに0～2個。
    std::uniform_int_distribution<int> itemCountRoll(0, 9);
    int itemCount = config.itemsEnabled ? (MAZE_CELLS >= 17 ? 2 : 1) : 0;
    std::uniform_real_distribution<double> effectChance(0.0, 1.0);
    for (int i = 0; i < itemCount && trapCount + i < static_cast<int>(openCells.size()); ++i) {
        auto [ix, iy] = openCells[trapCount + i];
        double roll = effectChance(randomEngine);
        ItemEffect effect;
        if (roll < config.badItemChance * 0.55) effect = ItemEffect::DAMAGE;
        else if (roll < config.badItemChance) effect = ItemEffect::SPEED_DOWN;
        else if (roll < config.badItemChance + (1.0-config.badItemChance)*0.55) effect = ItemEffect::SPEED_UP;
        else effect = ItemEffect::HEAL;
        randomItems.push_back({ix + 0.5, iy + 0.5, effect, true});
    }

    score = 0;
    visited[1][1] = true;

    mapOpen = false;
    overlayState = OverlayState::NONE;
}

