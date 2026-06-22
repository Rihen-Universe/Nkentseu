// =============================================================================
// NogeIntroScene.cpp
// =============================================================================

#include "NogeIntroScene.h"
#include "MainMenuScene.h"
#include "Songoo/Render/GLRenderer2D.h"
#include "Songoo/Render/FontAtlas.h"
#include "Songoo/UI/Theme.h"
#include "Songoo/UI/SceneManager.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu { namespace songoo {

    void NogeIntroScene::OnUpdate(AppContext& ctx, float dt) {
        mTime += dt;
        if (mTime >= kDuration)
            ctx.scenes->Replace(new MainMenuScene());
    }

    void NogeIntroScene::OnRender(AppContext& ctx) {
        GLRenderer2D& r = *ctx.renderer;
        FontAtlas&    f = *ctx.font;
        const int W = ctx.viewportW, H = ctx.viewportH;
        const float cx = ctx.safe.SafeCX(), cy = ctx.safe.SafeCY();

        // Fondu noir entrant/sortant
        float alpha = 1.f;
        if (mTime < 0.4f)       alpha = mTime / 0.4f;
        else if (mTime > kDuration - 0.4f)
                                 alpha = (kDuration - mTime) / 0.4f;

        r.Clear(0.f, 0.f, 0.f, 1.f);
        r.Begin(W, H);

        uint8_t a = (uint8_t)(255 * alpha);

        f.DrawStringCenteredScaled(r, FontAtlas::DisplaySlot, 1.4f,
            cx, cy - 40.f, "RIHEN UNIVERSE",
            { 255, 215, 0, a });
        f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, 1.f,
            cx, cy + 20.f, "presente",
            { 180, 140, 60, (uint8_t)(a * 0.7f) });

        r.End();
    }

}} // namespace nkentseu::songoo
