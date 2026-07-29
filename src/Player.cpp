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
    playerCrouching = (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) && playerOnGround;

    if (!playerOnGround) {
        jumpVelocity -= 7.8 * deltaTime;
        jumpHeight += jumpVelocity * deltaTime;
        if (jumpHeight <= 0.0) {
            jumpHeight = 0.0;
            jumpVelocity = 0.0;
            playerOnGround = true;
        }
    }

    if (speedEffectUntil != 0 && SDL_GetTicks() >= speedEffectUntil) {
        speedEffectUntil = 0;
        speedEffectMultiplier = 1.0;
    }

    bool forwardPressed = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP] || mobileUpHeld;
    bool backwardPressed = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN] || mobileDownHeld;
    bool leftPressed = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT] || mobileLeftHeld;
    bool rightPressed = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT] || mobileRightHeld;

    bool ctrlHeld = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];
    bool runningForward = !playerCrouching && playerOnGround &&
        (((keys[SDL_SCANCODE_UP] && ctrlHeld) ||
          (keys[SDL_SCANCODE_W] && keys[SDL_SCANCODE_Q])) || mobileRunning);

    double localForward = (forwardPressed ? 1.0 : 0.0) - (backwardPressed ? 1.0 : 0.0);
    double localRight = (rightPressed ? 1.0 : 0.0) - (leftPressed ? 1.0 : 0.0);
    double length = std::hypot(localForward, localRight);
    if (length < 0.001) return;
    localForward /= length;
    localRight /= length;

    double speed = playerCrouching ? 0.95 : (runningForward && localForward > 0.0 ? 3.25 : 1.72);
    speed *= speedEffectMultiplier;
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

void startPlayerJump() {
    if (playerOnGround && !playerCrouching && overlayState == OverlayState::NONE && !mapOpen) {
        playerOnGround = false;
        jumpVelocity = 3.15;
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

