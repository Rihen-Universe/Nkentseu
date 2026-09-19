#pragma once
// =============================================================================
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PixolSculpt/NkPixolBrush.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkPixolBrush.h  — NKRenderer v5.0  (Tools/PixolSculpt/)
//
// ⚠️ RENOMME LE 19/09/2026, ET LA RAISON EST UNE COLLISION DANGEREUSE.
//    Ce type s'appelait `NkSculptBrush`. Un AUTRE type portait exactement le
//    meme nom dans un autre espace de noms -- `nkentseu::NkSculptBrush`, dans
//    Engine/Noge/src/Noge/Sculpt/NkSculpting.h -- et les deux n'avaient PAS LES
//    MEMES UNITES :
//
//        renderer::NkSculptBrush   rayon en PIXELS ECRAN   (sculpt 2.5D)
//        nkentseu::NkSculptBrush   rayon en UNITES MONDE   (sculpt volumique)
//
//    Meme nom, semantique opposee. Toute recherche par nom melangeait les deux,
//    et surtout : UNE CONFUSION D'UNITES N'APPARAIT DANS AUCUN COMPTEUR. Un
//    rayon de 48 reste un nombre plausible qu'il vaille 48 pixels ou 48 metres ;
//    rien ne rougit, et le seul symptome est une forme fausse.
//    Le nouveau nom DIT l'unite : un pixol est un pixel, donc `NkPixolBrush`
//    est en pixels. Son cousin volumique vit ailleurs et ne peut plus etre
//    confondu avec lui.
//
// Description d'une brosse de sculpt et d'un "dab" (un tampon unique le long
// d'un trace). NkPixolBrushGPU est le bloc compact pousse au kernel compute
// via push constants : son layout DOIT matcher le bloc std430 declare dans
// shaders/sculpt_brush.comp.glsl.
//
// ⚠️ SQUELETTE — structures de donnees uniquement, pas de logique ici.
// =============================================================================
#include "NKMath/NKMath.h"
#include "NKRenderer/Tools/PixolSculpt/NkSculptTypes.h"

namespace nkentseu {
	namespace renderer {

		using namespace math;

		// Parametres d'une brosse, cote CPU / outil.
		struct NkPixolBrush {
				NkSculptBrushMode mode = NkSculptBrushMode::NK_RAISE;
				NkSculptFalloff falloff = NkSculptFalloff::NK_SMOOTH;
				float32 radiusPx = 48.f;	  ///< Rayon en pixels ecran.
				float32 strength = 0.5f;	  ///< Intensite [0..1].
				float32 hardness = 0.5f;	  ///< Durete du profil [0..1].
				bool invert = false;		  ///< Inverse le sens (raise<->lower).
				NkVec4f color = {1, 1, 1, 1}; ///< Pour NK_PAINT.
				float32 depthBias = 0.f;	  ///< Decalage Z additionnel.
		};

		// Un tampon unique. Un trace = une suite de dabs interpolee et espacee.
		struct NkPixolDab {
				NkVec2f screenPos = {0, 0}; ///< Centre en pixels ecran.
				float32 radiusPx = 48.f;
				float32 pressure = 1.f; ///< Pression stylet [0..1] (module strength/radius).
		};

		// ─────────────────────────────────────────────────────────────────────
		// Bloc GPU (push constants). Aligne 16 octets, layout std430.
		// ⚠️ Toute modif ici doit etre repercutee dans sculpt_brush.comp.glsl.
		// ─────────────────────────────────────────────────────────────────────
		struct NkPixolBrushGPU {
				NkVec2f center;	   // offset 0   — centre en pixels
				float32 radius;	   // offset 8
				float32 strength;  // offset 12
				NkVec4f color;	   // offset 16  — albedo (paint)
				uint32 mode;	   // offset 32  — NkSculptBrushMode
				uint32 falloff;	   // offset 36  — NkSculptFalloff
				float32 hardness;  // offset 40
				float32 depthBias; // offset 44
				int32 tileOffsetX; // offset 48  — origine de la tuile dispatchee
				int32 tileOffsetY; // offset 52
				uint32 _pad0;	   // offset 56
				uint32 _pad1;	   // offset 60  (taille totale 64, align 16)
		};

		// Remplit le bloc push-constant a partir de la brosse + du dab + de
		// l'origine de la tuile dispatchee. La pression module l'intensite.
		inline NkPixolBrushGPU MakeBrushGPU(const NkPixolBrush &b, const NkPixolDab &dab, int32 tileOffX,
											 int32 tileOffY) noexcept {
			NkPixolBrushGPU g{};
			g.center = dab.screenPos;
			g.radius = dab.radiusPx;
			g.strength = b.strength * dab.pressure;
			g.color = b.color;
			g.mode = (uint32)b.mode;
			g.falloff = (uint32)b.falloff;
			g.hardness = b.hardness;
			g.depthBias = b.depthBias;
			g.tileOffsetX = tileOffX;
			g.tileOffsetY = tileOffY;
			return g;
		}

	} // namespace renderer
} // namespace nkentseu
