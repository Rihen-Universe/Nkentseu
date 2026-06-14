// =============================================================================
// Demo9_Glow2D.cpp  — Phase E Materials 2D (v0 pragmatique)
//
// Demontre la nouvelle pipeline Glow2D dans NkRender2D : sprite avec halo
// radial additif parametrable. Plus tard (v1), ce sera unifie dans
// NkMaterialSystem comme un template NK_GLOW_2D.
//
// Affiche 3 sprites cote-a-cote :
//   - gauche : sprite normal (DrawSprite classique)
//   - centre : sprite avec glow rouge (intensite 1.5, power 3)
//   - droite : sprite avec glow cyan (intensite 2.5, power 4)
//
// Touches :
//   I/K : augmente/baisse intensite globale du glow
//   U/J : augmente/baisse power (concentration au bord)
//   H   : cycle couleur du glow
// =============================================================================
#include "DemoCommon.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"

namespace nkentseu { namespace demo {

// Palette pour cycler la couleur du glow (touche H)
static const NkVec3f kGlowPalette9[] = {
    {1.0f, 0.4f, 0.2f},   // orange / rouge feu
    {0.4f, 0.85f, 1.0f},  // cyan
    {0.4f, 1.0f, 0.5f},   // vert
    {1.0f, 0.5f, 0.95f},  // magenta
    {1.0f, 1.0f, 0.4f},   // jaune
};
static constexpr int kGlowPaletteSize9 = (int)(sizeof(kGlowPalette9) / sizeof(kGlowPalette9[0]));

struct Demo9State {
    NkTexHandle  tex;
    float32      intensity   = 1.5f;
    float32      power       = 3.0f;
    int          colorIdx    = 0;
    NkVec3f      glowColor   = {1.0f, 0.4f, 0.2f};
};

bool Demo9_Glow2D_Init(DemoCtx& ctx) {
    auto* st = new Demo9State();
    ctx.userData = st;

    auto* texLib = ctx.renderer->GetTextures();
    if (!texLib) {
        logger.Errorf("[Demo9] TextureLibrary manquant\n");
        delete st; ctx.userData = nullptr; return false;
    }

    // Reutilise la texture AwesomeFace (existe dans Resources/.../Textures/vracs/)
    NkLoadOptions opts;
    opts.srgb       = true;
    opts.genMipmaps = true;
    st->tex = texLib->Load("Resources/NKRenderer/Textures/vracs/awesomeface.png", opts);
    if (!st->tex.IsValid()) {
        logger.Warnf("[Demo9] AwesomeFace introuvable, fallback texture blanche\n");
        st->tex = texLib->GetWhite1x1();
    }

    // Inputs clavier : modifier les params glow live
    auto* state = st;
    NkEvents().AddEventCallback<NkKeyPressEvent>([state](NkKeyPressEvent* e) {
        if (!e) return;
        switch (e->GetKey()) {
            case NkKey::NK_I:
                state->intensity = NkMin(5.f, state->intensity + 0.25f);
                logger.Info("[Demo9] glow intensity = {0}\n", state->intensity);
                break;
            case NkKey::NK_K:
                state->intensity = NkMax(0.f, state->intensity - 0.25f);
                logger.Info("[Demo9] glow intensity = {0}\n", state->intensity);
                break;
            case NkKey::NK_U:
                state->power = NkMin(8.f, state->power + 0.5f);
                logger.Info("[Demo9] glow power = {0}\n", state->power);
                break;
            case NkKey::NK_J:
                state->power = NkMax(0.5f, state->power - 0.5f);
                logger.Info("[Demo9] glow power = {0}\n", state->power);
                break;
            case NkKey::NK_H:
                state->colorIdx = (state->colorIdx + 1) % kGlowPaletteSize9;
                state->glowColor = kGlowPalette9[state->colorIdx];
                logger.Info("[Demo9] glow color cycle -> #{0}\n", state->colorIdx);
                break;
            default: break;
        }
    });

    logger.Info("[Demo9] === Phase E Materials 2D (v0 : Glow2D) ===\n");
    logger.Info("[Demo9] I/K intensity | U/J power | H cycle color\n");
    return true;
}

void Demo9_Glow2D_Frame(DemoCtx& ctx, float32 dt) {
    auto* st = (Demo9State*)ctx.userData;

    if (!ctx.renderer->BeginFrame()) return;

    auto* r2d = ctx.renderer->GetRender2D();
    if (!r2d) {
        ctx.renderer->Present();
        ctx.renderer->EndFrame();
        return;
    }

    {
        r2d->Begin(ctx.renderer->GetCmd(), ctx.width, ctx.height);

        // Layout : 3 sprites de 256x256 centres verticalement
        const float32 spriteSize = 256.f;
        const float32 spacing    = 100.f;
        const float32 totalW     = 3.f * spriteSize + 2.f * spacing;
        const float32 startX     = ((float32)ctx.width - totalW) * 0.5f;
        const float32 cy         = ((float32)ctx.height - spriteSize) * 0.5f;

        // Gauche : sprite normal (DrawSprite classique)
        NkRectF rectN = {startX, cy, spriteSize, spriteSize};
        r2d->DrawSprite(rectN, st->tex);

        // Centre : sprite Glow rouge feu (color #0)
        r2d->SetGlowParams({1.0f, 0.4f, 0.2f}, st->intensity, st->power);
        NkRectF rectG1 = {startX + (spriteSize + spacing), cy, spriteSize, spriteSize};
        r2d->DrawSpriteGlow(rectG1, st->tex);

        // Droite : sprite Glow couleur palette courante (touche H)
        r2d->SetGlowParams(st->glowColor, st->intensity, st->power);
        NkRectF rectG2 = {startX + 2.f * (spriteSize + spacing), cy, spriteSize, spriteSize};
        r2d->DrawSpriteGlow(rectG2, st->tex);

        r2d->End();
    }

    if (auto* overlay = ctx.renderer->GetOverlay()) {
        overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
        overlay->DrawStats(ctx.renderer->GetStats());
        overlay->DrawText({20.f, 35.f},
            "Demo9 Phase E Materials 2D (v0 Glow2D)  |  API : %s",
            NkGraphicsApiName(ctx.api));
        overlay->DrawText({20.f, 55.f},
            "Gauche : sprite normal  |  Centre : glow rouge  |  Droite : glow color #%d",
            st->colorIdx);
        overlay->DrawText({20.f, 75.f},
            "intensity=%.2f  power=%.2f",
            st->intensity, st->power);
        overlay->DrawText({20.f, 95.f},
            "I/K intensity  U/J power  H cycle color");
        overlay->DrawText({20.f, 115.f},
            "FPS : %.0f", dt > 1e-5f ? 1.f / dt : 0.f);
        overlay->EndOverlay();
    }

    ctx.renderer->Present();
    ctx.renderer->EndFrame();
}

void Demo9_Glow2D_Shutdown(DemoCtx& ctx) {
    auto* st = (Demo9State*)ctx.userData;
    if (!st) return;
    delete st;
    ctx.userData = nullptr;
    logger.Info("[Demo9] Shutdown\n");
}

}} // namespace nkentseu::demo
