    return static_cast<int>(
        fingerX * SCREEN_WIDTH
    );
}

int fingerToScreenY(float fingerY) {
    return static_cast<int>(
        fingerY * SCREEN_HEIGHT
    );
}

int runGame() {
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

    if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG)) == 0) {
        std::cerr << "SDL_imageの初期化に失敗: " << IMG_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "3D Maze - TRAPS V5",
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

    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_BLEND
    );

    wallTexture = IMG_LoadTexture(renderer, "wall.jpg");
    enemyTexture = IMG_LoadTexture(renderer, "enemy.png");
    if (wallTexture != nullptr) SDL_QueryTexture(wallTexture, nullptr, nullptr, &wallTextureWidth, &wallTextureHeight);
    if (enemyTexture != nullptr) {
        SDL_QueryTexture(enemyTexture, nullptr, nullptr, &enemyTextureWidth, &enemyTextureHeight);
        SDL_SetTextureBlendMode(enemyTexture, SDL_BLENDMODE_BLEND);
    }
    if (wallTexture == nullptr) std::cerr << "wall.jpgを読み込めません: " << IMG_GetError() << std::endl;
    if (enemyTexture == nullptr) std::cerr << "enemy.pngを読み込めません: " << IMG_GetError() << std::endl;

    TTF_Font* titleFont = openFont(50);
    TTF_Font* buttonFont = openFont(28);
    TTF_Font* smallFont = openFont(17);

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
        {325, 225, 350, 75},
        "マウス感度"
    };

    Button brightnessButton = {
        {325, 330, 350, 70},
        "明るさ"
    };

    Button controlModeButton = {
        {325, 425, 350, 70},
        "操作方法"
    };

    Button settingsBackButton = {
        {350, 535, 300, 60},
        "戻る"
    };


    std::vector<Button> difficultyButtons = {
        {{110, 135, 780, 70}, "簡単　6×6／罠なし／種類1×1／鍵1"},
        {{110, 220, 780, 70}, "やや簡単　8×8／罠なし／種類1×1・種類2×1"},
        {{110, 305, 780, 70}, "普通　11×11／罠あり／3種類を各1"},
        {{110, 390, 780, 70}, "やや難関　17×17／黄・青の鍵／敵増加"},
        {{110, 475, 780, 70}, "難関　25×25／3色の鍵／敵・罠多め"}
    };
    Button menuBackButton = {{350, 575, 300, 58}, "戻る"};

    Button mobileUpButton = {
        {70, 475, 72, 72},
        "↑"
    };

    Button mobileDownButton = {
        {70, 615, 72, 72},
        "↓"
    };

    Button mobileLeftButton = {
        {0, 545, 72, 72},
        "←"
    };

    Button mobileRightButton = {
        {140, 545, 72, 72},
        "→"
    };

    Button mobileMapButton = {
        {70, 545, 72, 72},
        "地図"
    };

    Button mobilePauseButton = {
        {460, 18, 80, 58},
        "⏸"
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
                currentCounter -
                previousCounter
            ) /
            static_cast<double>(
                SDL_GetPerformanceFrequency()
            );

        previousCounter = currentCounter;

        if (deltaTime > 0.05) {
            deltaTime = 0.05;
        }

        processForwardPressUpdate();

        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (
                event.type == SDL_MOUSEMOTION &&
                event.motion.which != SDL_TOUCH_MOUSEID &&
                gameState == GameState::GAME &&
                controlMode == ControlMode::PC
            ) {
                addLookInput(
                    event.motion.xrel,
                    event.motion.yrel
                );
            }

            if (event.type == SDL_MOUSEMOTION && sensitivityDragging &&
                overlayState == OverlayState::SETTINGS) {
                SDL_Rect sliderTrack = {350, 255, 300, 12};
                setSensitivityFromPointer(event.motion.x, sliderTrack);
            }

            if (event.type == SDL_MOUSEBUTTONDOWN &&
                event.button.which != SDL_TOUCH_MOUSEID &&
                gameState == GameState::GAME &&
                overlayState == OverlayState::NONE && !mapOpen) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    tryPickUpKey();
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    if (tryUseKeyAtGoal()) {
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                        gameState = GameState::CLEAR;
                    }
                }
            }

            int pointerX = -1;
            int pointerY = -1;
            bool pointerDown = false;
            bool pointerUp = false;

            if (
                event.type == SDL_MOUSEBUTTONDOWN &&
                event.button.which != SDL_TOUCH_MOUSEID &&
                event.button.button == SDL_BUTTON_LEFT
            ) {
                pointerX = event.button.x;
                pointerY = event.button.y;
                pointerDown = true;
            }

            if (
                event.type == SDL_MOUSEBUTTONUP &&
                event.button.which != SDL_TOUCH_MOUSEID &&
                event.button.button == SDL_BUTTON_LEFT
            ) {
                pointerX = event.button.x;
                pointerY = event.button.y;
                pointerUp = true;
            }

            if (event.type == SDL_FINGERDOWN) {
                pointerX =
                    fingerToScreenX(event.tfinger.x);

                pointerY =
                    fingerToScreenY(event.tfinger.y);

                pointerDown = true;

                // モバイルでは画面右側をスワイプして視点操作
                if (
                    controlMode == ControlMode::MOBILE &&
                    gameState == GameState::GAME &&
                    overlayState == OverlayState::NONE &&
                    !mapOpen &&
                    event.tfinger.x >= 0.50f &&
                    !pointInside(
                        mobilePauseButton.rect,
                        pointerX,
                        pointerY
                    )
                ) {
                    mobileLookActive = true;
                    mobileLookFingerId = event.tfinger.fingerId;
                    mobileLookLastX = event.tfinger.x;
                    mobileLookLastY = event.tfinger.y;
                    pointerDown = false;
                }
            }

            if (
                event.type == SDL_FINGERMOTION &&
                mobileLookActive &&
                event.tfinger.fingerId == mobileLookFingerId &&
                controlMode == ControlMode::MOBILE &&
                gameState == GameState::GAME &&
                overlayState == OverlayState::NONE &&
                !mapOpen
            ) {
                float deltaX = event.tfinger.x - mobileLookLastX;
                float deltaY = event.tfinger.y - mobileLookLastY;

                mobileLookLastX = event.tfinger.x;
                mobileLookLastY = event.tfinger.y;

                addLookInput(
                    deltaX * SCREEN_WIDTH,
                    deltaY * SCREEN_HEIGHT,
                    1.15
                );
            }

            if (event.type == SDL_FINGERUP) {
                pointerX =
                    fingerToScreenX(event.tfinger.x);

                pointerY =
                    fingerToScreenY(event.tfinger.y);

                pointerUp = true;

                if (
                    mobileLookActive &&
                    event.tfinger.fingerId == mobileLookFingerId
                ) {
                    mobileLookActive = false;
                    pointerUp = false;
                }
            }

            if (pointerDown) {
                // 設定画面が開いている間は、ホーム画面の裏側にある
                // スタートボタンを絶対に判定しない。
                if (overlayState == OverlayState::DIFFICULTY) {
                    bool chosen = false;
                    for (int i=0; i<static_cast<int>(difficultyButtons.size()); ++i) {
                        if (pointInside(difficultyButtons[i].rect, pointerX, pointerY)) {
                            selectedDifficulty = static_cast<Difficulty>(i);
                            startGame(); gameState = GameState::GAME; chosen = true; break;
                        }
                    }
                    if (!chosen && pointInside(menuBackButton.rect, pointerX, pointerY)) overlayState = OverlayState::NONE;
                } else if (overlayState == OverlayState::RULES) {
                    if (pointInside(menuBackButton.rect, pointerX, pointerY)) overlayState = OverlayState::NONE;
                } else if (overlayState == OverlayState::SETTINGS) {
                    if (
                        pointInside(
                            sensitivityButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        SDL_Rect sliderTrack = {350, 255, 300, 12};
                        setSensitivityFromPointer(pointerX, sliderTrack);
                        sensitivityDragging = true;
                    } else if (
                        pointInside(
                            brightnessButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        changeBrightness();
                    } else if (
                        pointInside(
                            controlModeButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        toggleControlMode();
                    } else if (
                        pointInside(
                            settingsBackButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        if (
                            settingsReturnTarget ==
                            SettingsReturnTarget::HOME
                        ) {
                            overlayState = OverlayState::NONE;
                        } else {
                            overlayState = OverlayState::PAUSE;
                        }

                        setMouseCaptureForGameplay();
                    }
                } else if (gameState == GameState::HOME) {
                    if (pointInside(startButton.rect, pointerX, pointerY)) {
                        overlayState = OverlayState::DIFFICULTY;
                    } else if (
                        pointInside(
                            homeSettingsButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        settingsReturnTarget = SettingsReturnTarget::HOME;
                        overlayState = OverlayState::SETTINGS;
                    } else if (pointInside(rulesButton.rect, pointerX, pointerY)) {
                        overlayState = OverlayState::RULES;
                    }
                } else if (
                    gameState == GameState::GAME &&
                    overlayState == OverlayState::PAUSE
                ) {
                    if (
                        pointInside(
                            pauseSettingsButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        settingsReturnTarget = SettingsReturnTarget::PAUSE;
                        overlayState = OverlayState::SETTINGS;
                    } else if (
                        pointInside(
                            pauseSaveButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        saveGame();
                    } else if (
                        pointInside(
                            pauseHomeButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        returnHome();
                        gameState = GameState::HOME;
                    } else if (
                        controlMode == ControlMode::MOBILE &&
                        pointInside(
                            mobilePauseButton.rect,
                            pointerX,
                            pointerY
                        )
                    ) {
                        closePauseMenu();
                    }
                } else if (
                    gameState == GameState::GAME &&
                    overlayState == OverlayState::NONE
                ) {
                    if (controlMode == ControlMode::MOBILE) {
                        if (
                            pointInside(
                                mobilePauseButton.rect,
                                pointerX,
                                pointerY
                            )
                        ) {
                            openPauseMenu();
                        } else if (
                            pointInside(
                                mobileMapButton.rect,
                                pointerX,
                                pointerY
                            )
                        ) {
                            mapOpen = !mapOpen;
                            setMouseCaptureForGameplay();
                        } else if (
                            pointInside(
                                mobileUpButton.rect,
                                pointerX,
                                pointerY
                            )
                        ) {
                            processForwardPressStart();
                        } else if (
                            pointInside(
                                mobileDownButton.rect,
                                pointerX,
                                pointerY
                            )
                        ) {
                            mobileDownHeld = true;
                        } else if (
                            pointInside(
                                mobileLeftButton.rect,
                                pointerX,
                                pointerY
                            )
                        ) {
                            mobileLeftHeld = true;
                        } else if (
                            pointInside(
                                mobileRightButton.rect,
                                pointerX,
                                pointerY
                            )
                        ) {
                            mobileRightHeld = true;
                        }
                    }
                }
            }

            if (pointerUp) {
                sensitivityDragging = false;
            }

            if (
                pointerUp &&
                controlMode == ControlMode::MOBILE
            ) {
                if (mobileUpHeld) {
                    processForwardPressEnd();
                }

                mobileDownHeld = false;
                mobileLeftHeld = false;
                mobileRightHeld = false;
            }

            if (
                event.type == SDL_KEYDOWN &&
                event.key.repeat == 0
            ) {
                SDL_Keycode key =
                    event.key.keysym.sym;

                if (gameState == GameState::GAME && key == SDLK_SPACE) {
                    startPlayerJump();
                }

                if (
                    gameState == GameState::GAME &&
                    key == SDLK_i
                ) {
                    if (
                        overlayState ==
                        OverlayState::NONE
                    ) {
                        openPauseMenu();
                    } else if (
                        overlayState ==
                        OverlayState::PAUSE
                    ) {
                        closePauseMenu();
                    }
                }

                if (
                    gameState == GameState::GAME &&
                    key == SDLK_t &&
                    overlayState ==
                        OverlayState::NONE
                ) {
                    mapOpen = !mapOpen;
                    setMouseCaptureForGameplay();
                }

                if (
                    (gameState == GameState::CLEAR || gameState == GameState::GAME_OVER) &&
                    key == SDLK_RETURN
                ) {
                    startGame();
                    gameState = GameState::GAME;
                }
            }
        }

        if (
            gameState == GameState::GAME &&
            overlayState ==
                OverlayState::NONE &&
            !mapOpen
        ) {
            updateSmoothCamera(deltaTime);
            updatePlayer(deltaTime);
            // 経路探索は毎フレーム行わず約10回/秒に制限する。
            static double enemyUpdateAccumulator = 0.0;
            enemyUpdateAccumulator += deltaTime;
            if (enemyUpdateAccumulator >= 0.10) {
                updateEnemy(enemyUpdateAccumulator);
                enemyUpdateAccumulator = 0.0;
            }

            updateExtraChasers(deltaTime);
            updatePatrolEnemies(deltaTime);
            updateTrapsAndItems();
            if (playerHearts <= 0) {
                SDL_SetRelativeMouseMode(SDL_FALSE);
                gameState = GameState::GAME_OVER;
            }

            if (gameState == GameState::GAME && (enemyTouchedPlayer() || extraChaserTouchedPlayer() || patrolEnemyTouchedPlayer()) && SDL_GetTicks() >= playerDamageCooldownUntil) {
                playerHearts--;
                playerDamageCooldownUntil = SDL_GetTicks() + 2000;
                showActionMessage("ロボットにぶつかった！ ハートが1つ減った");
                // 連続接触を避けるため、少し後ろへ押し戻す。
                tryMove(-std::cos(playerAngle) * 0.35, -std::sin(playerAngle) * 0.35);
                if (playerHearts <= 0) {
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    gameState = GameState::GAME_OVER;
                }
            }
        }

        if (gameState == GameState::HOME && overlayState == OverlayState::DIFFICULTY) {
            drawDifficultyScreen(renderer, titleFont, buttonFont, difficultyButtons, menuBackButton);
        } else if (gameState == GameState::HOME && overlayState == OverlayState::RULES) {
            drawRulesScreen(renderer, titleFont, buttonFont, smallFont, menuBackButton);
        } else if (
            gameState == GameState::HOME &&
            overlayState ==
                OverlayState::SETTINGS
        ) {
            drawSettings(
                renderer,
                titleFont,
                buttonFont,
                smallFont,
                sensitivityButton,
                brightnessButton,
                controlModeButton,
                settingsBackButton
            );
        } else if (
            gameState == GameState::HOME
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
            gameState == GameState::GAME &&
            overlayState ==
                OverlayState::SETTINGS
        ) {
            drawSettings(
                renderer,
                titleFont,
                buttonFont,
                smallFont,
                sensitivityButton,
                brightnessButton,
                controlModeButton,
                settingsBackButton
            );
        } else if (
            gameState == GameState::GAME &&
            overlayState ==
                OverlayState::PAUSE
        ) {
            drawPauseMenu(
                renderer,
                titleFont,
                buttonFont,
                smallFont,
                pauseSettingsButton,
                pauseSaveButton,
                pauseHomeButton,
                mobileUpButton,
                mobileDownButton,
                mobileLeftButton,
                mobileRightButton,
                mobileMapButton,
                mobilePauseButton
            );
        } else if (
            gameState == GameState::GAME &&
            mapOpen
        ) {
            drawLocalMap(
                renderer,
                titleFont,
                smallFont
            );
        } else if (
            gameState == GameState::GAME
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
        } else if (
            gameState == GameState::CLEAR
        ) {
            drawClear(
                renderer,
                titleFont,
                buttonFont,
                smallFont
            );
        } else if (gameState == GameState::GAME_OVER) {
            drawGameOver(renderer, titleFont, smallFont);
        }

        SDL_RenderPresent(renderer);
    }

    SDL_SetRelativeMouseMode(SDL_FALSE);

    TTF_CloseFont(smallFont);
    TTF_CloseFont(buttonFont);
    TTF_CloseFont(titleFont);

    if (enemyTexture != nullptr) SDL_DestroyTexture(enemyTexture);
    if (wallTexture != nullptr) SDL_DestroyTexture(wallTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
