#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Morph/NkShapeCombos.h : LES FORMES DE COMBINAISON ET LES INTERMEDIAIRES
// (14/10/2026).
//
// D'OU VIENT CE FICHIER. Le document produit de NkAnimaEditor (design/07, § 4.3,
// manque n° 4) : « un pilote ne lit que la rotation d'UN os, en ligne droite. Il
// faut : la forme de COMBINAISON, les INTERMEDIAIRES (une forme propre au milieu de
// la course), un pilote par une autre forme, et une courbe a la place de la
// rampe ». Le visage du patient de PV3DE en a besoin aussi (un sourire bouche
// ouverte n'est pas la somme du sourire et de la bouche ouverte) : il l'ecrivait
// chez lui. C'est ici, en donnees, sans graphe : des CURSEURS donnent des POIDS.
//
// LES DEUX REGLES.
//   - INTERMEDIAIRES (« in-between » de Maya) : une forme pleine P, atteinte au
//     curseur 1, et des formes I_1..I_n atteintes EXACTEMENT aux curseurs a_1 < ...
//     < a_n (entre 0 et 1). Entre deux stations voisines (la base a 0, les I, P a
//     1), on passe en ligne droite de l'une a l'autre : au curseur a_i, le maillage
//     EST I_i (la machoire suit son arc, pas sa corde).
//   - COMBINAISON (forme corrective) : le poids d'une forme C est le PRODUIT (ou le
//     MINIMUM) des curseurs de ses formes pilotes, chacun passe par une RAMPE
//     (entree -> 0..1, droite ou douce). Une seule pilote : c'est « une forme
//     pilotee par une autre », avec sa courbe.
// Les curseurs sont l'intention de l'animateur ; les combinaisons lisent les
// CURSEURS (pas les poids : une forme pleine dont la course est prise par ses
// intermediaires a un poids nul au milieu, son curseur non).
//
// Resolve ecrit les poids qu'on donne a NkShapeKeySet::Evaluate. Une forme qui
// n'est ni pleine, ni intermediaire, ni corrective garde son curseur.
// =============================================================================

#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace anim {

		// ── Les MUTATIONS des bancs (le negatif dans le meme binaire) ─────────────
		enum NkShapeComboMutation : uint32 {
			NK_SC_MUT_NONE = 0u,
			NK_SC_MUT_NO_INBETWEEN = 1u << 0, ///< les intermediaires sont ignores (le curseur de la forme pleine est son poids)
			NK_SC_MUT_COMBO_OFF = 1u << 1,	  ///< les combinaisons sont ignorees
			NK_SC_MUT_RAMP_LINEAR = 1u << 2,  ///< la rampe douce devient droite
			NK_SC_MUT_TWO_MASTERS = 1u << 3,  ///< Validate accepte une forme menee par deux maitres
		};
		void NkShapeComboSetMutations(uint32 bits) noexcept;
		[[nodiscard]] uint32 NkShapeComboMutations() noexcept;

		/// Une rampe : 0 a `inMin`, 1 a `inMax`, bornee ; droite, ou douce (3u^2 - 2u^3 :
		/// pente nulle aux deux bouts, la forme entre et sort sans a-coup).
		struct NkShapeRamp {
				float32 inMin = 0.f;
				float32 inMax = 1.f;
				bool smooth = false;
		};
		[[nodiscard]] float32 NkShapeRampEval(const NkShapeRamp &ramp, float32 in) noexcept;

		enum class NkShapeComboMode : uint8 {
			NK_PRODUCT = 0, ///< le produit des pilotes (Maya)
			NK_MIN			///< le plus petit (la combinaison n'est pleine que si toutes le sont)
		};

		struct NkShapeCombination {
				int32 shape = -1;		  ///< la forme corrective (indice dans le NkShapeKeySet)
				NkVector<int32> drivers;  ///< les formes pilotes (leurs CURSEURS)
				NkShapeComboMode mode = NkShapeComboMode::NK_PRODUCT;
				NkShapeRamp ramp;		  ///< appliquee a chaque curseur pilote
		};

		struct NkShapeInBetween {
				int32 primary = -1;		 ///< la forme pleine (atteinte au curseur 1) ; son curseur mene le groupe
				NkVector<int32> shapes;	 ///< les intermediaires, dans l'ordre de leur station
				NkVector<float32> at;	 ///< la station de chacun (0 < at croissants < 1)
		};

		class NkShapeCombos {
			public:
				NkVector<NkShapeCombination> combinations;
				NkVector<NkShapeInBetween> inBetweens;

				/// Les POIDS (un par forme, la base comprise : `count` = NkShapeKeySet::Count())
				/// depuis les CURSEURS. Un groupe ou une combinaison incoherents (voir
				/// Validate) sont ignores : leurs formes gardent leur curseur.
				void Resolve(const float32 *sliders, float32 *weights, uint32 count) const;

				/// Vrai si tout est coherent pour `count` formes : indices dans [1, count),
				/// stations strictement croissantes dans ]0, 1[, une station par
				/// intermediaire, une forme corrective qui n'est pas sa propre pilote, et
				/// chaque forme menee par UN seul groupe ou UNE seule combinaison.
				[[nodiscard]] bool Validate(uint32 count) const;
		};

	} // namespace anim
} // namespace nkentseu
