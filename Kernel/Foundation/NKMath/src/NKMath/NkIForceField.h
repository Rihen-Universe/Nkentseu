#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkIForceField.h — un CHAMP DE FORCE externe, lu par tout ce qui a des
// particules (tissu, cheveux, particules ordinaires, SPH, herbe) (2026-09-05).
//
// Pourquoi ICI, dans NKMath : le tissu vit dans NKPhysics (Runtime), les
// particules et le SPH dans NKRenderer (Runtime), et aucun des deux ne voit
// l'autre. La seule couche que les deux consomment est la Foundation ; un
// champ de force est une fonction R^3 x R -> R^3, c'est bien une notion
// mathématique. Les CHAMPS eux-mêmes (uniforme / vortex / turbulence de curl,
// `NkForceField`, ROADMAP_PRODUITS.md §6.2 « vent ») sont écrits par un autre
// chantier ; ce fichier ne porte que le CONTRAT et un champ uniforme de test,
// pour que le tissu le consomme sans attendre et que les deux se rejoignent
// à la fusion.
//
// CONTRAT : Force(position, time) rend une force en NEWTONS exercée sur une
// particule PONCTUELLE placée en `position` (m) à l'instant `time` (s). C'est
// au consommateur de la convertir en accélération (F / m), de la projeter sur
// une normale (tissu) ou de la pondérer par une aire. Ce choix rend le témoin
// du vent lisible sans connaître le consommateur : force uniforme F ->
// accélération F/m sur une particule libre (§6.4), angle atan(F / m g) sur un
// pendule de particules (témoin (e) du tissu).
// =============================================================================
#include "NKMath/NkVec.h"

namespace nkentseu {
	namespace math {

		class NkIForceField {
			public:
				virtual ~NkIForceField() = default;
				// Force (N) sur une particule ponctuelle en `position` (m) à `time` (s).
				virtual NkVec3f Force(const NkVec3f &position, float32 time) const = 0;
		};

		// Champ UNIFORME : la même force partout, tout le temps. C'est le champ de
		// test du tissu (témoin (e)) ; le vent réel (rafales, vortex, curl) vient
		// de `NkForceField`, pas d'ici.
		class NkUniformForceField final : public NkIForceField {
			public:
				NkVec3f force = {0.f, 0.f, 0.f};

				NkUniformForceField() = default;
				explicit NkUniformForceField(const NkVec3f &f) noexcept : force(f) {
				}

				NkVec3f Force(const NkVec3f &position, float32 time) const override {
					(void)position;
					(void)time;
					return force;
				}
		};

	} // namespace math
} // namespace nkentseu
