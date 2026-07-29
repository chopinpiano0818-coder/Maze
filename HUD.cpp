    // 軽量版：1ピクセルずつではなく、横線を重ねて同じ形の光を描く。
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);

    const int horizonY = getHorizonY();
    const int startY = horizonY + 8;
    const int endY = SCREEN_HEIGHT - 18;
    if (startY >= endY) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        return;
    }

    const int centerX = SCREEN_WIDTH / 2;
    constexpr int LIGHT_LAYERS = 5;

    // 2行ずつ描画して負荷をさらに半分にする。
    for (int y = startY; y <= endY; y += 4) {
        const double depth = static_cast<double>(y - startY) /
                             static_cast<double>(endY - startY);
        const double nearAmount = std::pow(depth, 1.35);
        const int halfWidth = static_cast<int>(470 - 365 * nearAmount);
        const int maxAlpha = std::clamp(
            static_cast<int>((6 + 76 * std::pow(depth, 1.75)) * brightnessMultiplier),
            2, 120
        );

        for (int layer = LIGHT_LAYERS; layer >= 1; --layer) {
            const double t = layer / static_cast<double>(LIGHT_LAYERS);
            const int layerHalfWidth = std::max(1, static_cast<int>(halfWidth * t));
            const double strength = 1.0 - t;
            const int alpha = std::max(1, static_cast<int>(maxAlpha * (0.10 + strength * strength * 0.34)));

            SDL_SetRenderDrawColor(renderer, 250, 236, 185, static_cast<Uint8>(alpha));
            SDL_Rect band = {
                centerX - layerHalfWidth,
                y,
                layerHalfWidth * 2,
                std::min(4, endY - y + 1)
            };
            SDL_RenderFillRect(renderer, &band);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

void drawCrosshair(SDL_Renderer* renderer) {
    int centerX = SCREEN_WIDTH / 2;
    int centerY = SCREEN_HEIGHT / 2;

    SDL_SetRenderDrawColor(
        renderer,
        230,
        230,
        220,
        190
    );

    SDL_RenderDrawLine(
        renderer,
        centerX - 6,
        centerY,
        centerX + 6,
        centerY
    );

    SDL_RenderDrawLine(
        renderer,
        centerX,
        centerY - 6,
        centerX,
        centerY + 6
    );
}

void drawScore(
    SDL_Renderer* renderer,
    TTF_Font* font
) {
    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_BLEND
    );

    SDL_Rect scoreBox = {
        15,
        15,
        180,
        52
    };

    SDL_SetRenderDrawColor(
        renderer,
        5,
        8,
        12,
        205
    );

    SDL_RenderFillRect(renderer, &scoreBox);

    SDL_SetRenderDrawColor(
        renderer,
        210,
        220,
        235,
        255
    );

    SDL_RenderDrawRect(renderer, &scoreBox);

    drawText(
        renderer,
        font,
        "スコア：" + std::to_string(score),
        scoreBox.x + scoreBox.w / 2,
        scoreBox.y + scoreBox.h / 2
    );
}

void drawMobileControls(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const Button& mobileUpButton,
    const Button& mobileDownButton,
    const Button& mobileLeftButton,
    const Button& mobileRightButton,
    const Button& mobileMapButton,
    const Button& mobilePauseButton
) {
    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_BLEND
    );

    drawButton(
        renderer,
        font,
        mobileUpButton,
        mobileUpHeld
    );

    drawButton(
        renderer,
        font,
        mobileDownButton,
        mobileDownHeld
    );

    drawButton(
        renderer,
        font,
        mobileLeftButton,
        mobileLeftHeld
    );

    drawButton(
        renderer,
        font,
        mobileRightButton,
        mobileRightHeld
    );

    drawButton(
        renderer,
        font,
        mobileMapButton,
        false
    );

    drawButton(
        renderer,
        font,
        mobilePauseButton,
        false
    );
}

