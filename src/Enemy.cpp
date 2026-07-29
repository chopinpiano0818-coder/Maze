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
    if (!enemyAlive) return;
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
    return enemyAlive && std::hypot(enemyX - playerX, enemyY - playerY) < 0.48;
}

void drawSpriteAt(SDL_Renderer* renderer, double worldX, double worldY, double scale, Uint8 red, Uint8 green, Uint8 blue) {
    if (enemyTexture == nullptr) return;
    const double fieldOfView = PI / 3.0;
    double directionX = std::cos(playerAngle), directionY = std::sin(playerAngle);
    double planeLength = std::tan(fieldOfView / 2.0);
    double planeX = -directionY * planeLength, planeY = directionX * planeLength;
    double spriteX = worldX - playerX, spriteY = worldY - playerY;
    double determinant = planeX * directionY - directionX * planeY;
    if (std::abs(determinant) < 0.00001) return;
    double inverseDeterminant = 1.0 / determinant;
    double transformX = inverseDeterminant * (directionY * spriteX - directionX * spriteY);
    double transformY = inverseDeterminant * (-planeY * spriteX + planeX * spriteY);
    if (transformY <= 0.05) return;
    int horizonY = getHorizonY();
    int spriteScreenX = static_cast<int>((SCREEN_WIDTH / 2.0) * (1.0 + transformX / transformY));
    int spriteHeight = std::abs(static_cast<int>(SCREEN_HEIGHT / transformY * scale));
    int spriteWidth = static_cast<int>(spriteHeight * (enemyTextureWidth / static_cast<double>(enemyTextureHeight)));
    int startY = horizonY - spriteHeight / 2;
    int startX = spriteScreenX - spriteWidth / 2;
    SDL_SetTextureColorMod(enemyTexture, red, green, blue);
    for (int stripe = std::max(0, startX); stripe < std::min(SCREEN_WIDTH, startX + spriteWidth); stripe += SPRITE_STEP) {
        if (transformY >= depthBuffer[stripe]) continue;
        int textureX = (stripe - startX) * enemyTextureWidth / std::max(1, spriteWidth);
        SDL_Rect source = {std::clamp(textureX, 0, enemyTextureWidth - 1), 0, 1, enemyTextureHeight};
        SDL_Rect destination = {stripe, startY, std::min(SPRITE_STEP, SCREEN_WIDTH - stripe), spriteHeight};
        SDL_RenderCopy(renderer, enemyTexture, &source, &destination);
    }
    SDL_SetTextureColorMod(enemyTexture, 255, 255, 255);
}

void drawEnemy(SDL_Renderer* renderer) {
    if (!enemyAlive || enemyTexture == nullptr) return;

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

    int horizonY = getHorizonY();
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
    for (auto& key : keysInMaze) {
        if (!key.available) continue;
        if (lookingAt(key.x, key.y, 1.65, 0.22)) {
            key.available = false;
            key.collected = true;
            showActionMessage(key.name + "の鍵を手に入れた！");
            return true;
        }
    }
    showActionMessage("鍵に近づいて、中央に合わせて左クリック");
    return false;
}

int collectedKeyCount() {
    int count = 0;
    for (const auto& key : keysInMaze) if (key.collected) ++count;
    return count;
}

bool tryUseKeyAtGoal() {
    double gx = goalX + 0.5;
    double gy = goalY + 0.5;
    if (!lookingAt(gx, gy, 1.8, 0.35)) {
        showActionMessage("ゴールに近づいて右クリック");
        return false;
    }
    int need = getDifficultyConfig(selectedDifficulty).requiredKeys;
    int have = collectedKeyCount();
    if (have < need) {
        showActionMessage("鍵が足りない！ あと" + std::to_string(need-have) + "個必要");
        return false;
    }
    return true;
}

void drawKeySprite(SDL_Renderer* renderer) {
    for (const auto& key : keysInMaze) {
        if (!key.available) continue;
        double dx = key.x - playerX;
        double dy = key.y - playerY;
        double distance = std::hypot(dx, dy);
        if (distance < 0.05 || !hasClearLine(playerX, playerY, key.x, key.y)) continue;
        double relativeAngle = std::atan2(dy, dx) - playerAngle;
        while (relativeAngle > PI) relativeAngle -= 2.0 * PI;
        while (relativeAngle < -PI) relativeAngle += 2.0 * PI;
        const double fov = PI / 3.0;
        if (std::abs(relativeAngle) > fov * 0.65) continue;
        int screenX = SCREEN_WIDTH / 2 + static_cast<int>(std::tan(relativeAngle) / std::tan(fov / 2.0) * SCREEN_WIDTH / 2.0);
        int horizonY = getHorizonY();
        int size = std::clamp(static_cast<int>(150.0 / distance), 18, 110);
        int centerY = horizonY + static_cast<int>(SCREEN_HEIGHT / distance * 0.20);
        if (screenX < 0 || screenX >= SCREEN_WIDTH || distance >= depthBuffer[std::clamp(screenX,0,SCREEN_WIDTH-1)]) continue;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, key.color.r, key.color.g, key.color.b, 235);
        SDL_Rect ring = {screenX - size/2, centerY - size/3, size/2, size/2};
        SDL_RenderDrawRect(renderer, &ring); SDL_RenderDrawRect(renderer, &ring);
        SDL_Rect shaft = {screenX - 2, centerY, size/2 + 6, std::max(5, size/8)};
        SDL_RenderFillRect(renderer, &shaft);
        SDL_Rect tooth1 = {screenX + size/3, centerY, std::max(4,size/10), size/4};
        SDL_Rect tooth2 = {screenX + size/5, centerY, std::max(4,size/10), size/5};
        SDL_RenderFillRect(renderer, &tooth1); SDL_RenderFillRect(renderer, &tooth2);
    }
}

void drawGameOver(SDL_Renderer* renderer, TTF_Font* titleFont, TTF_Font* smallFont) {
    SDL_SetRenderDrawColor(renderer, 20, 24, 30, 255);
    SDL_RenderClear(renderer);
    drawText(renderer, titleFont, "ゲームオーバー", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 45, {255, 90, 90, 255});
    drawText(renderer, smallFont, "ロボットに捕まった！ Enterでやり直し", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 35);
}

void updateExtraChasers(double deltaTime) {
    for (auto& e : extraChasers) {
        if (!e.alive) continue;
        double dx = playerX - e.x, dy = playerY - e.y;
        double dist = std::hypot(dx, dy);
        if (dist < 0.001 || !hasClearLine(e.x, e.y, playerX, playerY)) continue;
        dx /= dist; dy /= dist; e.angle = std::atan2(dy, dx);
        double step = enemySpeed * deltaTime;
        double nx=e.x+dx*step, ny=e.y+dy*step;
        if (enemyCanMoveTo(nx,e.y)) e.x=nx;
        if (enemyCanMoveTo(e.x,ny)) e.y=ny;
    }
}

bool extraChaserTouchedPlayer() {
    for (const auto& e : extraChasers) if (e.alive && std::hypot(e.x-playerX,e.y-playerY)<0.48) return true;
    return false;
}

void updatePatrolEnemies(double deltaTime) {
    for (auto& robot : patrolEnemies) {
        double targetX = robot.direction > 0 ? robot.endX : robot.startX;
        double targetY = robot.direction > 0 ? robot.endY : robot.startY;
        double dx = targetX - robot.x, dy = targetY - robot.y;
        double distance = std::hypot(dx, dy);
        if (distance < 0.08) { robot.direction *= -1; continue; }
        robot.x += dx / distance * robot.speed * deltaTime;
        robot.y += dy / distance * robot.speed * deltaTime;
    }
}

void updateTrapsAndItems() {
    Uint32 now = SDL_GetTicks();
    for (auto& trap : floorTraps) {
        if (!trap.usedByPlayer && jumpHeight < 0.22 && std::hypot(playerX - trap.x, playerY - trap.y) < 0.34) {
            trap.usedByPlayer = true;
            if (now >= playerDamageCooldownUntil) {
                playerHearts--;
                playerDamageCooldownUntil = now + 2000;
                showActionMessage("トラップ！ ハートが1つ減った");
            }
        }
        if (enemyAlive && !trap.usedByEnemy && std::hypot(enemyX - trap.x, enemyY - trap.y) < 0.34) {
            trap.usedByEnemy = true;
            enemyHealth--;
            if (enemyHealth <= 0) {
                enemyAlive = false;
                showActionMessage("追跡ロボットがトラップで停止した！");
            }
        }
        for (auto& e : extraChasers) {
            if (!e.alive || std::hypot(e.x-trap.x, e.y-trap.y) >= 0.34) continue;
            e.health--;
            if (e.health <= 0) e.alive = false;
        }
    }
    for (auto& item : randomItems) {
        if (!item.available || std::hypot(playerX - item.x, playerY - item.y) >= 0.40) continue;
        item.available = false;
        switch (item.effect) {
            case ItemEffect::SPEED_UP:
                speedEffectMultiplier = 1.45; speedEffectUntil = now + 8000;
                showActionMessage("アイテム：8秒間スピードアップ！"); break;
            case ItemEffect::SPEED_DOWN:
                speedEffectMultiplier = 0.62; speedEffectUntil = now + 7000;
                showActionMessage("アイテム：7秒間スピードダウン…"); break;
            case ItemEffect::DAMAGE:
                if (now >= playerDamageCooldownUntil) { playerHearts--; playerDamageCooldownUntil = now + 2000; }
                showActionMessage("アイテムが爆発！ 1ダメージ"); break;
            case ItemEffect::HEAL:
                playerHearts = std::min(3, playerHearts + 1);
                showActionMessage("アイテム：ハートを1回復！"); break;
        }
    }
}

bool patrolEnemyTouchedPlayer() {
    for (const auto& robot : patrolEnemies) {
        if (std::hypot(robot.x - playerX, robot.y - playerY) < 0.46) return true;
    }
    return false;
}

void projectFloorMarker(SDL_Renderer* renderer, double worldX, double worldY, SDL_Color color, double sizeScale) {
    const double fov = PI / 3.0;
    double dx = worldX - playerX, dy = worldY - playerY;
    double forward = dx * std::cos(playerAngle) + dy * std::sin(playerAngle);
    double side = -dx * std::sin(playerAngle) + dy * std::cos(playerAngle);
    if (forward <= 0.08 || !hasClearLine(playerX, playerY, worldX, worldY)) return;
    int sx = static_cast<int>(SCREEN_WIDTH / 2.0 + (side / forward) * (SCREEN_WIDTH / (2.0 * std::tan(fov/2.0))));
    int horizon = getHorizonY();
    int sy = static_cast<int>(horizon + SCREEN_HEIGHT * 0.44 / forward);
    int size = std::clamp(static_cast<int>(65.0 * sizeScale / forward), 3, 70);
    if (sx < -size || sx >= SCREEN_WIDTH + size || sy < 0 || sy >= SCREEN_HEIGHT) return;
    int depthX = std::clamp(sx, 0, SCREEN_WIDTH - 1);
    if (!hasClearLine(playerX, playerY, worldX, worldY) || forward >= depthBuffer[depthX]) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect r = {sx-size/2, sy-size/5, size, std::max(3,size/3)};
    SDL_RenderFillRect(renderer, &r);
}

void drawWorldObjects(SDL_Renderer* renderer) {
    for (const auto& trap : floorTraps) {
        SDL_Color c = trap.usedByPlayer ? SDL_Color{85,85,85,150} : SDL_Color{230,65,55,220};
        projectFloorMarker(renderer, trap.x, trap.y, c, 1.0);
    }
    for (const auto& item : randomItems) if (item.available)
        projectFloorMarker(renderer, item.x, item.y, {100,235,255,235}, 0.8);

    // ゴールは大きな緑色のゲートとして表示。
    const double fov = PI / 3.0;
    double dx = goalX + 0.5 - playerX, dy = goalY + 0.5 - playerY;
    double forward = dx*std::cos(playerAngle)+dy*std::sin(playerAngle);
    double side = -dx*std::sin(playerAngle)+dy*std::cos(playerAngle);
    if (forward > 0.1 && hasClearLine(playerX, playerY, goalX + 0.5, goalY + 0.5)) {
        int sx = static_cast<int>(SCREEN_WIDTH/2.0 + (side/forward)*(SCREEN_WIDTH/(2.0*std::tan(fov/2.0))));
        int h = std::clamp(static_cast<int>(430.0/forward), 20, 420);
        int w = std::max(12, h/3);
        int y = getHorizonY()-h/2;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 50,255,120,110);
        SDL_Rect gate={sx-w/2,y,w,h}; SDL_RenderFillRect(renderer,&gate);
        SDL_SetRenderDrawColor(renderer, 170,255,200,240); SDL_RenderDrawRect(renderer,&gate);
    }
}

void drawNaturalGroundLight(SDL_Renderer* renderer) {
