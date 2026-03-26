#pragma once
/**
 * @File    NkPAMesh.h
 * @Brief   Vertex type + MeshBuilder pour l'animation procédurale NKPA.
 *          Toutes les coordonnées sont en pixels (conversion NDC dans Upload).
 */

#include "NKCore/NkTypes.h"
#include "NKMath/NkVec.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
namespace nkpa {

using namespace math;

// ─── Constantes ───────────────────────────────────────────────────────────────

static constexpr float32 PA_PI  = 3.14159265358979323846f;
static constexpr float32 PA_TAU = 6.28318530717958647692f;

// ─── Type de sommet (position 3D + couleur RGBA) ───────────────────────────────

struct PaVertex {
    float32 x, y, z;    ///< Position (x,y en pixels, z=0.5)
    float32 r, g, b, a; ///< Couleur RGBA normalisée [0,1]
};

// ─── Constructeur de maillage (triangle lists) ────────────────────────────────

class MeshBuilder {
public:
    void Clear();

    // Ajoute un triangle avec couleurs par sommet
    void AddTriangle(NkVector2f a, float32 ra, float32 ga, float32 ba, float32 aa,
                     NkVector2f b, float32 rb, float32 gb, float32 bb, float32 ab,
                     NkVector2f c, float32 rc, float32 gc, float32 bc, float32 ac);

    // Triangle couleur uniforme
    void AddTriFlat(NkVector2f a, NkVector2f b, NkVector2f c,
                    float32 r, float32 g, float32 bl, float32 al);

    // Cercle rempli
    void AddCircle(NkVector2f center, float32 radius,
                   float32 r, float32 g, float32 b, float32 a,
                   int32 segs = 20);

    // Ellipse remplie (orientée)
    void AddEllipse(NkVector2f center, float32 rx, float32 ry, float32 angle,
                    float32 r, float32 g, float32 b, float32 a,
                    int32 segs = 18);

    // Quad entre deux segments (corps lisse)
    void AddQuad(NkVector2f a, float32 ra_,
                 NkVector2f b, float32 rb_,
                 float32 r, float32 g, float32 bl, float32 al);

    // Quad avec gradient de couleur début→fin
    void AddQuadGrad(NkVector2f a, float32 ra_,
                     float32 r0, float32 g0, float32 b0, float32 a0,
                     NkVector2f b, float32 rb_,
                     float32 r1, float32 g1, float32 b1, float32 a1);

    // Triangle pointu (nageoires, griffes)
    void AddFin(NkVector2f base0, NkVector2f base1, NkVector2f tip,
                float32 r0, float32 g0, float32 b0, float32 a0,
                float32 rt, float32 gt, float32 bt, float32 at);

    // Fond dégradé (deux triangles)
    void AddBackground(float32 w, float32 h,
                       float32 tlR, float32 tlG, float32 tlB,
                       float32 brR, float32 brG, float32 brB);

    bool  IsEmpty() const;
    int32 VertexCount() const;

    // Construit le buffer de sommets en espace NDC (converti depuis pixels)
    NkVector<PaVertex> BuildNDC(float32 screenW, float32 screenH) const;

    // Accès direct (en pixels)
    const NkVector<PaVertex>& RawPixels() const { return mPixels; }

private:
    NkVector<PaVertex> mPixels; // sommets en pixels
};

} // namespace nkpa
} // namespace nkentseu
