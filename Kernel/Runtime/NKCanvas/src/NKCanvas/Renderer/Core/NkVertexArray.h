#pragma once
// =============================================================================
// NkVertexArray.h — Conteneur de NkVertex + NkPrimitiveType (style SFML)
//
// Equivalent de sf::VertexArray : un tableau dynamique de NkVertex (POD
// position+texCoords+color) accompagne d'un NkPrimitiveType qui dicte la
// facon dont les vertices sont assembles par le backend (POINTS, LINES,
// LINE_STRIP, TRIANGLES, TRIANGLE_STRIP, TRIANGLE_FAN).
//
// USAGE
//   NkVertexArray va(NkPrimitiveType::NK_TRIANGLES, 6);
//   va[0] = NkVertex{ 0.f, 0.f,  0.f, 0.f,  255, 0, 0, 255};
//   va[1] = NkVertex{50.f, 0.f,  1.f, 0.f,  0, 255, 0, 255};
//   ... // 4 vertices restants
//   target.Draw(va, states);    // (drawable une fois A.3 livre)
//
// ARCHITECTURE
//   Composition (NOT private inheritance) sur NkVector<NkVertex> : on garde
//   le control sur l'API exposee (Append, GetVertexCount, …) sans heriter
//   d'une API conteneur generique parasite.
//
//   Pas encore NkDrawable : le rename de NkIDrawable2D et la nouvelle
//   signature Draw(NkRenderTarget&, NkRenderStates) sont en etape A.3.
//   A ce moment-la, NkVertexArray heritera de NkDrawable et fournira un
//   Draw() qui delegue au backend via target.
// =============================================================================

#include "NkRenderer2DTypes.h"        // NkVertex, NkPrimitiveType, NkRect2f
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace renderer {

        class NkVertexArray {
            public:
                // ── Construction ────────────────────────────────────────────────
                /// Tableau vide, primitive = TRIANGLES par defaut.
                NkVertexArray() noexcept = default;

                /// Tableau avec primitive specifiee et capacite/taille initiale.
                explicit NkVertexArray(NkPrimitiveType type, uint32 count = 0) noexcept
                    : mPrimitive(type) {
                    if (count > 0) mVertices.Resize(count);
                }

                // ── Type de primitive ───────────────────────────────────────────
                void              SetPrimitiveType(NkPrimitiveType type) noexcept { mPrimitive = type; }
                NkPrimitiveType   GetPrimitiveType() const noexcept              { return mPrimitive; }

                // ── Taille / capacite ───────────────────────────────────────────
                uint32 GetVertexCount() const noexcept { return static_cast<uint32>(mVertices.Size()); }
                bool   IsEmpty()        const noexcept { return mVertices.Size() == 0; }

                /// Vide tous les vertices (conserve la capacite et le primitive type).
                void Clear() noexcept { mVertices.Clear(); }

                /// Redimensionne le tableau a `count` vertices. Les nouveaux
                /// elements sont laisses non initialises (NkVertex est un POD).
                void Resize(uint32 count) { mVertices.Resize(count); }

                /// Reserve de la capacite sans changer la taille (pour eviter les
                /// re-allocations sur un Append() en boucle).
                void Reserve(uint32 capacity) { mVertices.Reserve(capacity); }

                /// Ajoute un vertex en fin (O(amorti) constant).
                void Append(const NkVertex& v) { mVertices.PushBack(v); }

                // ── Acces ───────────────────────────────────────────────────────
                NkVertex&       operator[](uint32 index)       noexcept { return mVertices[index]; }
                const NkVertex& operator[](uint32 index) const noexcept { return mVertices[index]; }

                /// Pointeur sur le buffer contigu (pour upload backend).
                NkVertex*       Data()       noexcept { return mVertices.Data(); }
                const NkVertex* Data() const noexcept { return mVertices.Data(); }

                // ── Bounding box ────────────────────────────────────────────────

                /// Calcule l'AABB englobante des positions de tous les vertices.
                /// Retourne un rectangle vide (0,0,0,0) si le tableau est vide.
                NkRect2f GetBounds() const noexcept {
                    const uint32 n = GetVertexCount();
                    if (n == 0) return NkRect2f(0.f, 0.f, 0.f, 0.f);
                    float32 minX = mVertices[0].x, maxX = mVertices[0].x;
                    float32 minY = mVertices[0].y, maxY = mVertices[0].y;
                    for (uint32 i = 1; i < n; ++i) {
                        const float32 x = mVertices[i].x;
                        const float32 y = mVertices[i].y;
                        if      (x < minX) minX = x;
                        else if (x > maxX) maxX = x;
                        if      (y < minY) minY = y;
                        else if (y > maxY) maxY = y;
                    }
                    return NkRect2f(minX, minY, maxX - minX, maxY - minY);
                }

            private:
                NkVector<NkVertex> mVertices;
                NkPrimitiveType    mPrimitive{NkPrimitiveType::NK_TRIANGLES};
        };

    } // namespace renderer
} // namespace nkentseu
