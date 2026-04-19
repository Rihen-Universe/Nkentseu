#include "NkGizmoSystem.h"
#include "NKECS/Components/Core/NkCoreComponents.h"

using namespace nkentseu::math;

namespace nkentseu {
    namespace Unkeny {

        static const NkVec4f kColorX     = {1.f, 0.2f, 0.2f, 1.f};
        static const NkVec4f kColorY     = {0.2f, 1.f, 0.2f, 1.f};
        static const NkVec4f kColorZ     = {0.2f, 0.5f, 1.f, 1.f};
        static const NkVec4f kColorW     = {1.f, 1.f, 0.2f, 1.f}; // XYZ/plane
        static const NkVec4f kColorHover = {1.f, 0.85f, 0.1f, 1.f};
        static constexpr float32 kHitRadius = 0.08f; // en unités screen-space

        void NkGizmoSystem::Update(ecs::NkWorld& world,
                                   const NkSelectionManager& sel,
                                   const NkEditorCamera& cam,
                                   float32 vpMouseX, float32 vpMouseY,
                                   float32 vpW, float32 vpH,
                                   bool leftDown,
                                   CommandHistory* history) noexcept {
            drawLines.Clear();
            drawArrows.Clear();

            if (!sel.HasSelection()) { mActiveAxis = NkGizmoAxis::None; return; }

            ecs::NkEntityId id = sel.Primary();
            auto* t = world.Get<ecs::NkTransformComponent>(id);
            if (!t) return;

            mEntityPos = t->WorldPosition();

            // Screen-space scale pour que le gizmo reste constant en pixels
            NkVec4f clipPos = cam.viewProjMatrix * NkVec4f(mEntityPos.x, mEntityPos.y, mEntityPos.z, 1.f);
            float32 screenScale = (clipPos.w > 0.f)
                ? size * clipPos.w / (vpH * 0.5f)
                : 1.f;

            // Build geometry
            switch (mode) {
                case NkGizmoMode::Translate:
                    BuildTranslateGizmo(mEntityPos, cam.viewProjMatrix, screenScale);
                    break;
                case NkGizmoMode::Rotate:
                    BuildRotateGizmo(mEntityPos, cam.viewProjMatrix, screenScale);
                    break;
                case NkGizmoMode::Scale:
                    BuildScaleGizmo(mEntityPos, cam.viewProjMatrix, screenScale);
                    break;
                default: break;
            }

            // Hit testing
            auto ray = cam.ScreenRay(vpMouseX, vpMouseY, vpW, vpH);
            mHoveredAxis = HitTest(ray, mEntityPos, screenScale);

            // Drag
            if (leftDown && mHoveredAxis != NkGizmoAxis::None && mActiveAxis == NkGizmoAxis::None) {
                mActiveAxis = mHoveredAxis;
                mDragStart  = mEntityPos;
                mDragEntityOrigin = mEntityPos;
            }
            if (!leftDown && mActiveAxis != NkGizmoAxis::None) {
                // Commit via CommandHistory si disponible
                mActiveAxis = NkGizmoAxis::None;
            }

            // Appliquer delta de drag (translation simple sur axe)
            if (mActiveAxis != NkGizmoAxis::None && leftDown) {
                // Projection du rayon sur l'axe de déplacement
                NkVec3f axisDir = {0,0,0};
                switch (mActiveAxis) {
                    case NkGizmoAxis::X:   axisDir = {1,0,0}; break;
                    case NkGizmoAxis::Y:   axisDir = {0,1,0}; break;
                    case NkGizmoAxis::Z:   axisDir = {0,0,1}; break;
                    default: break;
                }
                // Distance le long de l'axe (projection du rayon)
                float32 denom = NkDot(ray.dir, axisDir);
                if (NkAbs(denom) > 1e-5f) {
                    float32 tHit = NkDot(mEntityPos - ray.origin, axisDir) / denom;
                    NkVec3f hit  = ray.origin + ray.dir * tHit;
                    NkVec3f delta = hit - mDragStart;
                    float32 proj  = NkDot(delta, axisDir);
                    if (mode == NkGizmoMode::Translate) {
                        t->SetPosition(mDragEntityOrigin + axisDir * proj);
                    }
                }
            }
        }

        void NkGizmoSystem::BuildTranslateGizmo(const NkVec3f& o,
                                                 const NkMat4f& /*vp*/,
                                                 float32 s) noexcept {
            float32 L = s;
            auto addAxis = [&](NkVec3f end, NkGizmoAxis a) {
                NkVec4f col = AxisColor(a, mHoveredAxis == a || mActiveAxis == a);
                drawLines.PushBack({o, end, col, 2.f});
                // Flèche (simplifiée : cone de 3 segments)
                NkVec3f d = NkNormalize(end - o) * (s * 0.15f);
                NkVec3f p = NkNormalize(end - o);
                // On fait juste un X en bout pour symboliser la tête
                drawArrows.PushBack({end, end + d, col, 3.f});
            };
            addAxis(o + NkVec3f(L, 0, 0), NkGizmoAxis::X);
            addAxis(o + NkVec3f(0, L, 0), NkGizmoAxis::Y);
            addAxis(o + NkVec3f(0, 0, L), NkGizmoAxis::Z);
        }

        void NkGizmoSystem::BuildRotateGizmo(const NkVec3f& o,
                                              const NkMat4f& /*vp*/,
                                              float32 s) noexcept {
            // Cercles approximés par polylines (16 segments)
            constexpr int N = 16;
            auto addCircle = [&](NkGizmoAxis a, NkVec4f col) {
                for (int i = 0; i < N; ++i) {
                    float32 a0 = (float32)i       / N * NkTwoPi;
                    float32 a1 = (float32)(i + 1) / N * NkTwoPi;
                    NkVec3f p0, p1;
                    switch (a) {
                        case NkGizmoAxis::X:
                            p0 = o + NkVec3f(0, NkCos(a0)*s, NkSin(a0)*s);
                            p1 = o + NkVec3f(0, NkCos(a1)*s, NkSin(a1)*s); break;
                        case NkGizmoAxis::Y:
                            p0 = o + NkVec3f(NkCos(a0)*s, 0, NkSin(a0)*s);
                            p1 = o + NkVec3f(NkCos(a1)*s, 0, NkSin(a1)*s); break;
                        default:
                            p0 = o + NkVec3f(NkCos(a0)*s, NkSin(a0)*s, 0);
                            p1 = o + NkVec3f(NkCos(a1)*s, NkSin(a1)*s, 0); break;
                    }
                    drawLines.PushBack({p0, p1, col, 2.f});
                }
            };
            addCircle(NkGizmoAxis::X, AxisColor(NkGizmoAxis::X, mHoveredAxis==NkGizmoAxis::X));
            addCircle(NkGizmoAxis::Y, AxisColor(NkGizmoAxis::Y, mHoveredAxis==NkGizmoAxis::Y));
            addCircle(NkGizmoAxis::Z, AxisColor(NkGizmoAxis::Z, mHoveredAxis==NkGizmoAxis::Z));
        }

        void NkGizmoSystem::BuildScaleGizmo(const NkVec3f& o,
                                             const NkMat4f& vp,
                                             float32 s) noexcept {
            BuildTranslateGizmo(o, vp, s); // même structure, tête cubique
        }

        NkGizmoAxis NkGizmoSystem::HitTest(const NkEditorCamera::Ray& ray,
                                            const NkVec3f& o,
                                            float32 s) const noexcept {
            auto distToLine = [&](NkVec3f end) -> float32 {
                NkVec3f lineDir = NkNormalize(end - o);
                NkVec3f w  = o - ray.origin;
                float32 b  = NkDot(lineDir, ray.dir);
                float32 denom = 1.f - b*b;
                if (NkAbs(denom) < 1e-6f) return 1e9f;
                float32 tRay  = (NkDot(w, ray.dir) - b * NkDot(w, lineDir)) / denom;
                float32 tLine = (NkDot(w, lineDir) - b * NkDot(w, ray.dir)) / denom;
                tLine = NkClamp(tLine, 0.f, s);
                NkVec3f closest = ray.origin + ray.dir * tRay;
                NkVec3f linePt  = o + lineDir * tLine;
                return NkLength(closest - linePt);
            };

            float32 dX = distToLine(o + NkVec3f(s, 0, 0));
            float32 dY = distToLine(o + NkVec3f(0, s, 0));
            float32 dZ = distToLine(o + NkVec3f(0, 0, s));
            float32 hr = kHitRadius * s;

            if (dX < hr && dX < dY && dX < dZ) return NkGizmoAxis::X;
            if (dY < hr && dY < dX && dY < dZ) return NkGizmoAxis::Y;
            if (dZ < hr && dZ < dX && dZ < dY) return NkGizmoAxis::Z;
            return NkGizmoAxis::None;
        }

        NkVec4f NkGizmoSystem::AxisColor(NkGizmoAxis axis, bool hovered) const noexcept {
            if (hovered) return kColorHover;
            switch (axis) {
                case NkGizmoAxis::X: return kColorX;
                case NkGizmoAxis::Y: return kColorY;
                case NkGizmoAxis::Z: return kColorZ;
                default:             return kColorW;
            }
        }

    } // namespace Unkeny
} // namespace nkentseu
