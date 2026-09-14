#pragma once
// =============================================================================
// NkSceneContext.h  — NKRenderer v5.0  (Core/)
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
//
// Contexte d'une frame de rendu 3D : camera, lumieres, IBL, fog, time.
// Fourni par l'utilisateur a NkRender3D::BeginScene(ctx).
//
// Vit dans Core car c'est l'input contract du renderer 3D, et non un detail
// d'implementation de NkRender3D. Les sous-systemes (Shadow, VFX, Animation)
// peuvent egalement le consommer pour rester synchronises.
// =============================================================================
#include "NkRendererTypes.h"
#include "NkCamera.h"

namespace nkentseu {
	namespace renderer {

		// =====================================================================
		// NkSceneContext
		// =====================================================================
		struct NkSceneContext {
				// ── Camera (par valeur — copie a chaque frame, pas de lifetime issue)
				NkCamera3D camera;

				// ── Planar reflection : viewProj de la cam miroir. Le matériau
				// ReflFloor (et d'autres effets-miroir futurs) utilise cette matrice
				// pour échantillonner le RT à la position monde projetée, à la
				// place d'un screen-space mapping (qui ne marche pas avec up flippé).
				// Identity par défaut = pas de reflet (le shader peut détecter et
				// retomber sur un sample neutre).
				NkMat4f mirrorViewProj = NkMat4f::Identity();

				// ── Lights (CPU-side, copie au moment de BeginScene)
				NkVector<NkLightDesc> lights;

				// ── Environnement / IBL
				NkTexHandle envMap; // skybox cubemap (compat NkTextureLibrary)
				NkIBLHandle ibl;	// jeu prefiltre (irradiance + GGX + BRDF LUT)
				// ⚠️ CES TROIS CHAMPS NE SONT LUS NULLE PART, ET NE L'ONT JAMAIS ETE.
				// Mesure du 2026-09-07 : `ambientIntensity` a UNE seule occurrence
				// dans tout `Kernel/` -- la ligne qui suit. `BeginScene` copie le
				// contexte (`mCtx = ctx`) et personne ne relit jamais ces valeurs.
				// Vingt-quatre sites les ECRIVENT et n'obtiennent rien : NkDemo3D
				// (0.15), NkViewport3D (0.45), NkMatPreview3D (0.22), AnimBridge
				// (0.4), NKARDemo (0.45), NKXRDemo (0.15) et treize demos.
				//
				// CE QUI AGIT REELLEMENT, et qui est deja branche partout ou une
				// interface le propose :
				//     NkRender3D::SetIBLStrength(s)   au lieu de ambientIntensity
				//     NkRender3D::SetIBLColor(c)      au lieu de ambientColor
				// Les deux sont honores EN VOL (mesure : un appel en cours de vie
				// rend une image octet pour octet identique au meme reglage pose
				// par la config). L'ambiante mesuree vaut hdr = 0.615 x iblStrength,
				// constant a +/-0.7 % sur 80x de plage.
				//
				// ⚠️ ON NE LES BRANCHE PAS, ET C'EST DELIBERE. Les honorer dans
				// `BeginScene` rendrait d'un coup effectifs vingt-quatre reglages
				// jamais eprouves (jusqu'a douze fois l'ambiante actuelle sur
				// certaines demos) et, pire, ECRASERAIT A CHAQUE IMAGE le curseur
				// « Ambiance > Intensite » du panneau -- qui, lui, FONCTIONNE
				// (NkDemo3D.cpp:6942 repose 0.15 par frame). On casserait le seul
				// reglage qui marche pour faire vivre celui qui ment.
				//
				// `[[deprecated]]` : le compilateur nomme desormais chacun des
				// vingt-quatre sites. Un mensonge silencieux devient un mensonge
				// qui se signale, en attendant le lot qui les convertira.
				[[deprecated("champ jamais lu par le moteur -- utiliser "
							 "NkRender3D::SetIBLStrength()")]]
				float32 iblIntensity = 1.f;
				[[deprecated("champ jamais lu par le moteur -- utiliser "
							 "NkRender3D::SetIBLStrength()")]]
				float32 ambientIntensity = 0.15f;
				[[deprecated("champ jamais lu par le moteur -- utiliser "
							 "NkRender3D::SetIBLColor()")]]
				NkVec3f ambientColor = {1.f, 1.f, 1.f};

				// ── Time / Frame
				float32 time = 0.f;
				float32 deltaTime = 0.f;
				uint32 frameIdx = 0;

				// ── Fog
				bool fogEnabled = false;
				NkVec3f fogColor = {0.5f, 0.6f, 0.7f};
				float32 fogDensity = 0.f;
				float32 fogStart = 100.f;
				float32 fogEnd = 1000.f;
				// ── BROUILLARD AU SOL (height fog), anime ────────────────────
				// fogThickness = 0 : le brouillard ne depend que de la distance,
				// exactement comme avant. Au-dela, la nappe est DENSE au niveau
				// de fogHeightBase et s'eclaircit en montant sur cette epaisseur.
				// fogWind la souffle comme une FUMEE : sa densite est modulee par
				// un bruit qui DERIVE, a la maniere des nuages -- le sol et le
				// ciel respirent alors ensemble. Le jour ou la physique du vent
				// existera, c'est elle qui pilotera fogWindDir/fogWindSpeed.
				float32 fogHeightBase = 0.f;
				float32 fogThickness = 0.f;
				float32 fogWind = 0.f;		 // force du souffle (0 = nappe lisse)
				float32 fogWindSpeed = 0.1f; // derive de la nappe

				// ── Debug view mode
				NkViewMode viewMode = NkViewMode::NK_SOLID;
		};

	} // namespace renderer
} // namespace nkentseu
