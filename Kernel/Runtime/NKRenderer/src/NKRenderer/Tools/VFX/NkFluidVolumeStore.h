#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkFluidVolumeStore.h — BRANCHER la grille (2026-09-06).
//
// LE MANQUE QUE CE FICHIER COMBLE, dit tel quel le 05/09 : « la grille n'est
// branchée sur RIEN : ni ECS, ni NkVFXSystem, ni le vent. C'est un solveur et son
// banc, pas une fonctionnalité du moteur. »
//
// CE FICHIER EST LE REGISTRE, et rien d'autre : il tient N volumes de fluide,
// leur donne une identité stable (poignée), les suit quand leur entité bouge,
// leur passe le VENT, et les fait avancer. Il est **CPU PUR** — aucun device,
// aucune fenêtre, aucun include de NKRHI. C'est délibéré, et c'est ce qui rend
// le branchement ÉPROUVABLE sans GPU : le banc appelle `StepAll`, exactement la
// fonction que `NkVFXSystem::Update` appelle.
//
// LES TROIS CONSOMMATEURS, et qui fait quoi :
//   - `NkVFXSystem` en POSSÈDE un et appelle `StepAll` dans son Update ;
//   - `nkentseu::NkFluidVolumeSystem` (Noge/ECS) DÉCLARE et POSITIONNE les
//     volumes depuis les entités, et ne les fait PAS avancer — même partage que
//     `NkParticleSystem`, qui laisse l'Update au renderer. Un seul appelant de
//     `StepAll`, sinon le monde avancerait deux fois par image ;
//   - le banc `NkFluidGridProbe`, qui appelle `StepAll` sans device.
//
// ⚠️ CE QUE « SUIVRE L'ENTITÉ » VEUT DIRE, ET IL FAUT LE DIRE : déplacer le
// centre TRANSLATE la boîte, et le contenu suit — les champs sont stockés par
// INDICE de cellule, il n'y a aucun ré-échantillonnage. Le volume est donc
// SOLIDAIRE de son entité (une torche qu'on déplace emporte sa flamme), ce qui
// est le comportement voulu pour un émetteur attaché à un transform. Ce n'est
// PAS un volume fixe dans le monde à travers lequel une entité passerait : ça,
// ce serait un autre objet, et il n'est pas écrit.
//
// ZÉRO STL : NkVector, allocateurs NKMemory. Aucun std::.
// =============================================================================
#include "NkFluidGrid.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace renderer {

		struct NkFluidVolumeId {
				uint64 id = 0;

				bool IsValid() const {
					return id != 0;
				}
		};

		// =====================================================================
		// La SOURCE attachée au volume — ce qu'une entité DÉCLARE, en débit par
		// seconde. Le registre multiplie par dt : l'appelant décrit une intention
		// (« ce feu produit tant de fumée par seconde »), jamais un pas de temps.
		// C'est la même règle que le véhicule : le jeu déclare une intention.
		// =====================================================================
		struct NkFluidSourceDesc {
				math::NkVec3f offset = {0.f, 0.f, 0.f}; // par rapport au CENTRE du volume (m)
				float32 radius = 0.05f;					// m
				float32 densityRate = 4.f;				// densité par seconde
				float32 temperatureRate = 1400.f;		// K par seconde ajoutés
				float32 fuelRate = 0.f;					// carburant par seconde
				bool enabled = true;
		};

		// =====================================================================
		// Le VOLUME tel qu'une entité le décrit. `grid.boundsMin/Max` sont
		// RELATIFS au centre : le registre les recale en monde à chaque
		// `SetCenter`. Une entité n'a pas à connaître ses coordonnées absolues.
		// =====================================================================
		struct NkFluidVolumeDesc {
				NkFluidGridParams grid; // bornes RELATIVES au centre
				NkFluidSourceDesc source;
				bool enabled = true;
		};

		// =====================================================================
		// Le REGISTRE
		// =====================================================================
		class NkFluidVolumeStore final {
			public:
				NkFluidVolumeStore() = default;
				~NkFluidVolumeStore();

				NkFluidVolumeStore(const NkFluidVolumeStore &) = delete;
				NkFluidVolumeStore &operator=(const NkFluidVolumeStore &) = delete;

				// Rend une poignée invalide si `Init` de la grille échoue — un refus se
				// DIT, il ne se déguise pas en poignée qui ne marchera pas.
				NkFluidVolumeId Create(const NkFluidVolumeDesc &desc, const math::NkVec3f &worldCenter);
				void Destroy(NkFluidVolumeId &id);
				void Clear();

				void SetCenter(NkFluidVolumeId id, const math::NkVec3f &worldCenter);
				void SetEnabled(NkFluidVolumeId id, bool on);
				// Le VENT. Contrat `math::NkIForceField` : la force est en NEWTONS, et le
				// consommateur divise par la masse de SA particule — ici la masse déclarée
				// de la cellule. Le champ est EMPRUNTÉ : il doit survivre au registre.
				void SetField(NkFluidVolumeId id, const math::NkIForceField *field, float32 particleMass);

				NkFluidGrid *Grid(NkFluidVolumeId id);
				const NkFluidGrid *Grid(NkFluidVolumeId id) const;
				math::NkVec3f Center(NkFluidVolumeId id) const;
				NkFluidVolumeDesc *Desc(NkFluidVolumeId id);

				// Émet les sources puis fait avancer TOUS les volumes actifs.
				// UN SEUL appelant par image (voir l'en-tête).
				void StepAll(float32 dt);

				// ── Témoins ────────────────────────────────────────────────────
				uint32 Count() const {
					return (uint32)mVolumes.Size();
				}
				// Parcourir le registre par INDICE (le rendu et le ramassage des
				// orphelins en ont besoin). L'indice n'est PAS stable : il change des
				// qu'un volume est detruit. Seule la poignee identifie un volume.
				NkFluidVolumeId IdAt(uint32 i) const {
					return (i < (uint32)mVolumes.Size() && mVolumes[i] != nullptr) ? mVolumes[i]->id
																				  : NkFluidVolumeId{};
				}
				// Combien de volumes ont RÉELLEMENT avancé au dernier StepAll. C'est le
				// chiffre qui dit « la grille est branchée sur quelque chose » : il reste
				// à zéro si personne n'appelle StepAll, si tout est éteint, ou si une
				// poignée est morte. Un compteur d'intentions ne vaudrait rien ici — il
				// est incrémenté APRÈS le pas, jamais avant.
				uint32 SteppedLastFrame() const {
					return mSteppedLastFrame;
				}
				uint64 StepsTotal() const {
					return mStepsTotal;
				}
				float32 LastStepMs() const {
					return mLastStepMs;
				}

			private:
				struct Volume {
						NkFluidVolumeId id;
						NkFluidVolumeDesc desc;
						math::NkVec3f center = {0.f, 0.f, 0.f};
						NkFluidGrid *grid = nullptr;
				};

				Volume *Find(NkFluidVolumeId id);
				const Volume *Find(NkFluidVolumeId id) const;
				void ApplyCenter(Volume &v);

				NkVector<Volume *> mVolumes;
				uint64 mNextId = 1;
				uint32 mSteppedLastFrame = 0;
				uint64 mStepsTotal = 0;
				float32 mLastStepMs = 0.f;
		};

	} // namespace renderer
} // namespace nkentseu
