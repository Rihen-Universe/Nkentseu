// =============================================================================
// NkShape.cpp — Implementation du Draw partage : fan-triangulation + outline.
//
// Pattern : on compose states.transform avec notre GetTransform() (transformable
// herite), on triangule la liste de points en fan (convex only), puis on
// submit en NK_TRIANGLES via target.Draw(raw vertices). Outline en NK_LINES.
// =============================================================================

#include "NKCanvas/Renderer/Shapes/NkShape.h"
#include "NKCanvas/Renderer/Targets/NkRenderTarget.h"
#include "NKMemory/NkAllocator.h"

namespace nkentseu {
    namespace renderer {

        namespace {
            // Buffer temporaire de NkVertex : stack pour <= 256, heap sinon.
            struct VtxBuf {
                NkVertex  stack[256];
                NkVertex* heap{nullptr};
                NkVertex* Acquire(uint32 n) {
                    if (n <= 256) return stack;
                    heap = static_cast<NkVertex*>(nkentseu::memory::NkAlloc(sizeof(NkVertex) * n));
                    return heap;
                }
                ~VtxBuf() { if (heap) nkentseu::memory::NkFree(heap); }
            };

            // Construit un NkVertex (POD) depuis position + couleur + UV.
            inline NkVertex MakeVertex(NkVec2f pos, NkColor2D c, float32 u = 0.f, float32 v = 0.f) noexcept {
                NkVertex out;
                out.x = pos.x; out.y = pos.y;
                out.u = u;     out.v = v;
                out.r = c.r;   out.g = c.g; out.b = c.b; out.a = c.a;
                return out;
            }
        } // anon

        void NkShape::Draw(NkRenderTarget& target,
                           const NkRenderStates& parentStates) const {
            const uint32 n = GetPointCount();
            if (n < 2) return;

            // Composition : etat du parent * transform local.
            NkRenderStates s = parentStates;
            s.transform *= GetTransform();
            if (mTexture) s.texture = mTexture;

            // ── Fill (n >= 3 : aire pleine, NK_TRIANGLES list directe) ────────
            // On triangule en fan COTE NkShape (1 triangle = (p0, p_i, p_{i+1}))
            // et on emet directement en NK_TRIANGLES. Pourquoi pas NK_TRIANGLE_FAN
            // (n vertices, plus economique) ? Car NkRenderWindow::Draw raw fait
            // une expansion TRIANGLE_FAN -> TRIANGLES dans un buffer scratch
            // local au switch ; si le backend batch lazyement ce buffer (et ne
            // recopie pas immediatement), le scratch est detruit avant flush ->
            // donnees garbage / artefacts (visible : « rectangles creux »).
            // En emettant TRIANGLES direct, le buffer `v` de NkShape reste vivant
            // pendant tout l'appel target.Draw, et NkRenderWindow se contente
            // d'un identity-indices submit deterministe.
            if (n >= 3) {
                const NkRect2f bounds = GetLocalBounds();
                const uint32 triCount = n - 2;
                const uint32 vertCount = triCount * 3;

                VtxBuf vbuf;
                NkVertex* v = vbuf.Acquire(vertCount);

                // Pre-calcul du sommet central (p0) et son UV (reutilise pour tous les triangles).
                const NkVec2f p0  = GetPoint(0);
                const NkVec2f uv0 = GetPointUV(0, bounds);
                const NkVertex v0 = MakeVertex(p0, mFillColor, uv0.x, uv0.y);

                uint32 w = 0;
                for (uint32 i = 1; i + 1 < n; ++i) {
                    const NkVec2f p1  = GetPoint(i);
                    const NkVec2f p2  = GetPoint(i + 1);
                    const NkVec2f uv1 = GetPointUV(i,     bounds);
                    const NkVec2f uv2 = GetPointUV(i + 1, bounds);
                    v[w++] = v0;
                    v[w++] = MakeVertex(p1, mFillColor, uv1.x, uv1.y);
                    v[w++] = MakeVertex(p2, mFillColor, uv2.x, uv2.y);
                }
                target.Draw(v, vertCount, NkPrimitiveType::NK_TRIANGLES, s);
            }

            // ── Outline (NK_LINE_STRIP, n+1 vertices, ferme la boucle) ─────────
            if (mOutlineThickness > 0.f && n >= 2) {
                // LINE_STRIP : n+1 vertices (le dernier = le premier pour
                // refermer le contour). Economie : n+1 vs 2n vertices pour LINES.
                // NOTE : mOutlineThickness > 1 n'est pas honore pour l'instant
                // (backend NkRenderWindow::Draw raw appelle DrawLine avec
                // thickness=1). Sera ameliore quand le mode d'outline epais sera
                // requis (expansion en quads avec joints miter/bevel).
                const uint32 vertCount = n + 1;
                VtxBuf vbuf;
                NkVertex* v = vbuf.Acquire(vertCount);
                for (uint32 i = 0; i < n; ++i) v[i] = MakeVertex(GetPoint(i), mOutlineColor);
                v[n] = v[0]; // ferme la boucle

                NkRenderStates outlineStates = s;
                outlineStates.texture = nullptr; // l'outline ne texture pas
                target.Draw(v, vertCount, NkPrimitiveType::NK_LINE_STRIP, outlineStates);
            }
        }

    } // namespace renderer
} // namespace nkentseu
