// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkRenderStyle.h -- les STYLES de rendu nommés par usage, sur UN seul socle.
//
// Demande de Rodolf (14/10/2026) : un rendu « prêt, calé et fonctionnel », qui
// serve au réaliste, au cartoon ou stylisé, pour des jeux ou des films précis.
// Pas un second moteur : les trois styles emploient le MÊME système de
// matériaux (PBR, Skin, Cloth, Toon), les mêmes lumières et les mêmes ombres.
// Un style décide seulement :
//   - quel modèle de matière sert chaque MATIÈRE (plastique, métal, peau, tissu) ;
//   - ses réglages (rampe cel, contour, reflet, diffusion) ;
//   - quelques réglages d'image (saturation, bloom, SSAO).
//
//   NK_STYLE_REALISTE : matières physiques (étude des livres, T3/T5/T44...).
//   NK_STYLE_CARTOON  : cel à deux tons, ombre teintée, contour noir, reflet net.
//   NK_STYLE_RIHEN    : « anatomies stylisées, matières réalistes » -- les
//                       matières du réaliste, une silhouette dessinée (contour
//                       fin de la teinte de l'objet) et une image un peu plus
//                       saturée.
//
// Usage :
//     NkApplyRenderStyle(cfg.postProcess, style);                 // à l'init
//     NkStyledMaterial m = NkCreateStyledMaterial(mats, style, {NK_MATIERE_PEAU, {0.8f, 0.6f, 0.5f}});
//     m.ApplyTo(drawCall);                                        // matière + paramètres d'objet
// =============================================================================
#pragma once

#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRenderer/Core/NkRendererTypes.h"

namespace nkentseu {
	namespace renderer {

		class NkMaterialSystem;
		class NkMaterialInstance;

		enum class NkRenderStyle : uint8 {
			NK_STYLE_REALISTE = 0,
			NK_STYLE_CARTOON = 1,
			NK_STYLE_RIHEN = 2,
		};

		enum class NkMatiere : uint8 {
			NK_MATIERE_PLASTIQUE = 0,
			NK_MATIERE_METAL = 1,
			NK_MATIERE_PEAU = 2,
			NK_MATIERE_TISSU = 3,
		};

		// Ce que l'appelant décrit : la matière et sa couleur. La rugosité < 0
		// prend celle de la matière (plastique 0,45 ; métal 0,3 ; peau 0,5 ;
		// tissu 0,75).
		struct NkStyledMaterialDesc {
				NkMatiere matiere = NkMatiere::NK_MATIERE_PLASTIQUE;
				NkVec3f couleur = {0.8f, 0.8f, 0.8f};
				float32 rugosite = -1.f;
		};

		// Ce que le style rend : l'instance de matériau ET les paramètres
		// d'OBJET que le nuanceur lit dans le bloc objet (rugosité, métal,
		// diffusion : NkDrawCall3D, pas l'instance -- cf. NkRender3D, bloc objet
		// statique). ApplyTo pose les deux sur un appel de dessin.
		struct NkStyledMaterial {
				NkMaterialInstance *instance = nullptr;
				NkVec3f tint = {1.f, 1.f, 1.f};
				float32 metallic = 0.f;
				float32 roughness = 0.5f;
				float32 subsurface = 0.f;
				NkVec3f subsurfaceColor = {1.f, 1.f, 1.f};

				void ApplyTo(NkDrawCall3D &dc) const;
		};

		const char *NkRenderStyleName(NkRenderStyle s);

		// Réglages d'image du style. Les champs lus à chaque image (saturation)
		// valent aussi à chaud (NkPostProcessStack::GetConfig()) ; bloom et SSAO
		// ne valent qu'à l'initialisation.
		void NkApplyRenderStyle(NkPostConfig &pp, NkRenderStyle s);

		// Crée l'instance de matériau du style pour cette matière. nullptr si
		// le système de matériaux est absent.
		NkStyledMaterial NkCreateStyledMaterial(NkMaterialSystem *mats, NkRenderStyle s,
												const NkStyledMaterialDesc &d);

	} // namespace renderer
} // namespace nkentseu
