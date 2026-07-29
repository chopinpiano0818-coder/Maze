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
        "スタート後に難易度を選び、その条件で迷路を生成",
        SCREEN_WIDTH / 2,
        205,
        {150, 210, 255, 255}
    );

    drawButton(renderer, buttonFont, startButton);
    drawButton(renderer, buttonFont, settingsButton);
    drawButton(renderer, buttonFont, rulesButton);
}

void drawDifficultyScreen(SDL_Renderer* renderer, TTF_Font* titleFont, TTF_Font* font, const std::vector<Button>& buttons, const Button& backButton) {
    SDL_SetRenderDrawColor(renderer, 12, 17, 28, 255); SDL_RenderClear(renderer);
    drawText(renderer, titleFont, "難易度を選択", SCREEN_WIDTH/2, 62);
    drawText(renderer, font, "生成後はホームへ戻るまで変更できません", SCREEN_WIDTH/2, 105, {255,210,110,255});
    for (const auto& b : buttons) drawButton(renderer, font, b);
    drawButton(renderer, font, backButton);
}

void drawRulesScreen(SDL_Renderer* renderer, TTF_Font* titleFont, TTF_Font* font, TTF_Font* smallFont, const Button& backButton) {
    SDL_SetRenderDrawColor(renderer, 12,17,28,255); SDL_RenderClear(renderer);
    drawText(renderer, titleFont, "ルール", SCREEN_WIDTH/2, 48);
    std::vector<std::string> lines = {
        "W/↑：前進　S/↓：後退　A/D：左右　Space：ジャンプ",
        "Shift：しゃがみ　↑＋Ctrl：走る　T：地図　I：一時停止",
        "左クリック：鍵を取る　ゴール前で右クリック：鍵を使う",
        "トラップはジャンプで回避。床・敵・鍵・アイテムは壁越しに見えません",
        "簡単：6×6／罠なし／種類1×1／鍵1",
        "やや簡単：8×8／罠なし／種類1×1・種類2×1／鍵1",
        "普通：11×11／罠あり／種類1・2・3を各1／鍵1",
        "やや難関：17×17／種類1×1・種類2/3×2／黄・青の鍵",
        "難関：25×25／罠やや多め／種類1×2・種類2/3×3／赤・青・黄の鍵",
        "アイテム効果はランダム。難しいほど外れの確率が上がります",
        "チャッピーが頑張って作りました！"
    };
    int y=105; for (const auto& line:lines) { drawText(renderer, smallFont, line, SCREEN_WIDTH/2, y); y+=43; }
    drawButton(renderer, font, backButton);
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
    difficultyLocked = true;
    generateMaze();
    setMouseCaptureForGameplay();
}

void returnHome() {
    difficultyLocked = false;
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
