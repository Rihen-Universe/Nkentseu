// =============================================================================
// NkOverlayRenderer.cpp — Overlay de debug, stats HUD, gizmos
// =============================================================================
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"
#include "NKRenderer/Core/NkResourceManager.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
    namespace renderer {

        bool NkOverlayRenderer::Init(NkIDevice* device, NkResourceManager* resources,
                                       NkRender2D* r2d, uint32 width, uint32 height)
        {
            if (!device || !resources || !r2d) return false;
            mDevice    = device;
            mResources = resources;
            mR2D       = r2d;
            mWidth     = width;
            mHeight    = height;

            // Créer un ring-buffer de textes pour les stats
            mLines.Reserve(128);
            mReady = true;
            return true;
        }

        void NkOverlayRenderer::Shutdown() {
            mLines.Clear();
            mReady  = false;
            mDevice = nullptr;
        }

        void NkOverlayRenderer::Resize(uint32 w, uint32 h) {
            mWidth = w; mHeight = h;
        }

        // =============================================================================
        void NkOverlayRenderer::BeginOverlay(NkICommandBuffer* cmd) {
            if (!mReady || !cmd) return;
            mCmd = cmd;
            mLines.Clear();
            mGizmos.Clear();
        }

        void NkOverlayRenderer::EndOverlay() {
            if (!mCmd) return;

            // Rendu avec R2D
            if (mR2D && mR2D->IsReady()) {
                mR2D->Begin(mCmd, mWidth, mHeight, nullptr);

                // ── Stats HUD ─────────────────────────────────────────────────────────
                if (mShowStats) {
                    float32 x = 8.f, y = 8.f;
                    const NkColorF bg  = {0,0,0,0.55f};
                    const NkColorF txt = {0.1f,1.f,0.3f,1.f};
                    float32 lineH = mFontSize + 2.f;
                    float32 panelW = 220.f;
                    float32 panelH = lineH * (float32)(mLines.Size()) + 8.f;
                    mR2D->FillRoundRect({x-4,y-4,panelW+8,panelH}, bg, 4.f);
                    for (uint32 i = 0; i < (uint32)mLines.Size(); ++i) {
                        if (mFont.IsValid())
                            mR2D->DrawText({x, y + lineH*(float32)i}, mLines[i].CStr(),
                                            mFont, mFontSize, txt);
                        else
                            mR2D->DrawText({x, y + lineH*(float32)i}, mLines[i].CStr(),
                                            {}, mFontSize, txt);
                    }
                }

                // ── Gizmos 3D projetés en 2D ──────────────────────────────────────────
                for (uint32 i = 0; i < (uint32)mGizmos.Size(); ++i) {
                    const GizmoEntry& g = mGizmos[i];
                    switch (g.type) {
                        case GizmoType::CROSS: {
                            float s = g.size;
                            mR2D->DrawLine({g.screenPos.x-s,g.screenPos.y},
                                            {g.screenPos.x+s,g.screenPos.y}, g.color, 2.f);
                            mR2D->DrawLine({g.screenPos.x,g.screenPos.y-s},
                                            {g.screenPos.x,g.screenPos.y+s}, g.color, 2.f);
                            break;
                        }
                        case GizmoType::CIRCLE:
                            mR2D->DrawCircle(g.screenPos, g.size, g.color, 2.f, 32);
                            break;
                        case GizmoType::DIAMOND: {
                            float s = g.size;
                            NkVec2f pts[4] = {
                                {g.screenPos.x,g.screenPos.y-s},
                                {g.screenPos.x+s,g.screenPos.y},
                                {g.screenPos.x,g.screenPos.y+s},
                                {g.screenPos.x-s,g.screenPos.y}
                            };
                            mR2D->DrawPolyline(pts, 4, g.color, 2.f, true);
                            break;
                        }
                        default: break;
                    }
                    // Label du gizmo
                    if (!g.label.Empty()) {
                        mR2D->DrawText({g.screenPos.x+g.size+2.f, g.screenPos.y-6.f},
                                        g.label.CStr(), {}, mFontSize, g.color);
                    }
                }

                mR2D->End();
            }

            mCmd = nullptr;
        }

        // =============================================================================
        // Stats
        // =============================================================================
        void NkOverlayRenderer::AddStatLine(const char* label, const char* value) {
            NkString line = label;
            line += ": ";
            line += value;
            mLines.PushBack(line);
        }

        void NkOverlayRenderer::AddStatLine(const char* label, float32 value, int decimals) {
            NkString line = label;
            line += ": ";
            line += NkString::FromFloat(value, decimals);
            mLines.PushBack(line);
        }

        void NkOverlayRenderer::AddStatLine(const char* label, uint32 value) {
            NkString line = label;
            line += ": ";
            line += NkString::FromUInt(value);
            mLines.PushBack(line);
        }

        void NkOverlayRenderer::PushRendererStats(const NkRendererStats& stats) {
            if (!mShowStats) return;
            mLines.Clear();
            AddStatLine("FPS",       stats.fps);
            AddStatLine("CPU ms",    stats.cpuMs, 2);
            AddStatLine("GPU ms",    stats.gpuMs, 2);
            AddStatLine("DrawCalls", stats.drawCalls);
            AddStatLine("Triangles", stats.triangles);
            AddStatLine("Culled",    stats.culledObjects);
            AddStatLine("VRAM MB",   stats.vramUsedMB);
        }

        // =============================================================================
        // Gizmos
        // =============================================================================
        void NkOverlayRenderer::AddGizmo(NkVec2f screenPos, GizmoType type,
                                           const NkColorF& color, float32 size,
                                           const char* label)
        {
            GizmoEntry g{};
            g.screenPos = screenPos;
            g.type      = type;
            g.color     = color;
            g.size      = size;
            g.label     = label ? label : "";
            mGizmos.PushBack(g);
        }

        void NkOverlayRenderer::AddWorldGizmo(NkVec3f worldPos,
                                                const NkMat4f& viewProj,
                                                GizmoType type,
                                                const NkColorF& color, float32 size,
                                                const char* label)
        {
            // Projeter en screen space
            NkVec4f clip = viewProj * NkVec4f{worldPos.x,worldPos.y,worldPos.z,1.f};
            if (clip.w <= 0.f) return;
            clip.x /= clip.w; clip.y /= clip.w;
            NkVec2f screen = {
                (clip.x * 0.5f + 0.5f) * (float32)mWidth,
                (0.5f - clip.y * 0.5f) * (float32)mHeight
            };
            AddGizmo(screen, type, color, size, label);
        }

        // =============================================================================
        void NkOverlayRenderer::DrawTextWorld(NkVec3f worldPos, const char* text,
                                               const NkMat4f& viewProj,
                                               const NkColorF& color)
        {
            NkVec4f clip = viewProj * NkVec4f{worldPos.x,worldPos.y,worldPos.z,1.f};
            if (clip.w <= 0.f) return;
            NkVec2f screen = {
                (clip.x/clip.w * 0.5f+0.5f) * (float32)mWidth,
                (0.5f - clip.y/clip.w * 0.5f) * (float32)mHeight
            };
            // Enregistrer comme label de texte à dessiner en EndOverlay
            GizmoEntry g{};
            g.screenPos = screen;
            g.type  = GizmoType::NONE;
            g.color = color;
            g.label = text;
            mGizmos.PushBack(g);
        }

    } // namespace renderer
} // namespace nkentseu
