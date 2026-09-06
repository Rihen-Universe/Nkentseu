#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Noge/ECS/Components/Rendering/NkFluidVolume.h — un VOLUME DE FLUIDE sur une
// entité (fumée, feu), 2026-09-06.
//
// Ce composant est au solveur eulérien (`renderer::NkFluidGrid`) ce que
// `NkParticleEmitter` est au système de particules : il DÉCRIT, il ne simule
// pas. Le `NkFluidVolumeSystem` crée le volume dans le registre
// (`renderer::NkFluidVolumeStore`) et le fait suivre le `NkTransform` ; le pas de
// simulation est déclenché par `NkVFXSystem::Update`, une seule fois par image.
//
// ⚠️ IL N'INCLUT PAS `NkVFXSystem.h`, et c'est délibéré : ce dernier tire NKRHI
// (device, tampons, pipelines). Le registre, lui, est CPU pur. Un composant qui
// tirerait le RHI rendrait tout banc d'ECS impossible sans GPU — c'est
// exactement ce qui empêche d'éprouver le pont des particules aujourd'hui.
//
// Les bornes de `desc.grid` sont RELATIVES au centre de l'entité : une entité ne
// connaît pas ses coordonnées absolues, c'est le registre qui recale.
// =============================================================================
#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h"
#include "NKRenderer/Tools/VFX/NkFluidVolumeStore.h"

namespace nkentseu {
	namespace ecs {

		struct NkFluidVolume {
				renderer::NkFluidVolumeDesc desc;
				uint64 volumeId = 0; // poignée runtime (renderer::NkFluidVolumeId.id)
				bool enabled = true;
				bool dirty = false; // desc modifiée -> le volume est recréé

				// ── Réglages prêts à l'emploi ─────────────────────────────────
				// Ils déclarent une INTENTION (« de la fumée », « du feu »), pas des
				// paramètres de solveur : c'est la règle de surface du dépôt.
				[[nodiscard]] static NkFluidVolume Smoke() noexcept {
					NkFluidVolume v;
					auto &g = v.desc.grid;
					g.boundsMin = {-0.35f, 0.f, -0.35f};
					g.boundsMax = {0.35f, 1.4f, 0.35f};
					g.cellSize = 0.035f;
					// LE CONFINEMENT DE VORTICITE : ce qui fait d'un jet un PANACHE
					// (Fedkiw 2001, § 4). Valeur CHOISIE PAR MESURE, pas a l'oeil : c'est le
					// plus petit epsilon du balayage (NK_FLUID_SWEEP=1) qui atteint le seuil
					// pre-enregistre « le panache s'elargit d'un facteur 1,15 a hauteur fixee ».
					// ⚠️ IL SE PAIE, et le prix est mesure : sur la meme scene, la derive de
					// masse passe de -26,2 % a -36,0 % (240 pas) et la divergence residuelle
					// de 0,57 % a 5,33 %. Ce ne sont pas deux defauts qu'il CREE -- ce sont les
					// deux rouges deja mesures du 05/09, qu'il AGGRAVE, et dont les correctifs
					// sont nommes : grille decalee MAC (Harlow & Welch 1965) pour la
					// divergence, advection conservative en flux (Lentine & al. 2011) pour la
					// masse. Le DEFAUT du solveur brut reste 0 : rien de ce qui existe
					// aujourd'hui ne change de comportement ; c'est ce reglage-ci, dont le
					// travail est l'APPARENCE, qui l'arme.
					g.vorticityConfinement = 8.f;
					g.densityDissipation = 0.15f;
					g.temperatureDissipation = 0.4f;
					auto &s = v.desc.source;
					s.offset = {0.f, 0.06f, 0.f};
					s.radius = 0.05f;
					s.densityRate = 4.f;
					s.temperatureRate = 900.f;
					s.fuelRate = 0.f;
					return v;
				}

				[[nodiscard]] static NkFluidVolume Fire() noexcept {
					NkFluidVolume v = Smoke();
					auto &g = v.desc.grid;
					g.boundsMax = {0.35f, 1.0f, 0.35f};
					// La COMBUSTION : le carburant se consume, dégage sa chaleur et sa suie.
					g.burnRate = 6.f;
					g.heatPerFuel = 2200.f;
					g.sootPerFuel = 1.2f;
					g.coolingRate = 2.2f;
					g.fuelDissipation = 0.2f;
					auto &s = v.desc.source;
					s.densityRate = 0.6f;
					s.temperatureRate = 400.f;
					s.fuelRate = 3.f;
					return v;
				}
		};
		NK_COMPONENT(NkFluidVolume)

	} // namespace ecs
} // namespace nkentseu
