#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtEase.h — LES LOIS DE TEMPS d'un mouvement fabrique (13/10/2026).
//
// D'OU VIENT CE FICHIER. Le patient virtuel de PV3DE fabrique ses gestes : un bras
// qui se leve et se repose, une toux, un saut. QUAND un geste commence, combien il
// dure, jusqu'ou il va : c'est PV3DE qui le decide. COMMENT une grandeur passe
// d'une valeur a une autre sans a-coup, comment un corps monte et retombe : ce sont
// des lois de mouvement, et elles etaient ecrites a la main dans PV3DE
// (Body/NkCorpsExecuteur.cpp). Elles sont ici.
//
// CE QUE CE FICHIER OFFRE, en fonctions pures (aucun etat) :
//   - NkRtMinJerk         : le passage de 0 a 1 a SECOUSSE MINIMALE (10u^3 - 15u^4 +
//                           6u^5) : vitesse ET acceleration nulles aux deux bouts --
//                           la loi d'un geste humain d'un point a un autre ;
//   - NkRtEnvelopeWeight  : attaque, tenue, relache (un geste qu'on fait puis lache) ;
//   - NkRtWindow          : une fenetre entre deux instants, a bords doux ;
//   - NkRtHermite         : d'une valeur et d'une pente a une autre valeur et une
//                           autre pente (raccorder une flexion a un vol sans saut de
//                           vitesse) ;
//   - NkRtBallisticHeight, NkRtBallisticSpeed, NkRtBallisticTime : le vol d'un corps
//                           lance vers le haut sous la pesanteur ;
//   - NkRtPulse           : une secousse (montee franche, retombee en carre).
// Les fondus de la machine a etats (NkFadeCurve, Clip/NkAnimation.h) ne sont pas
// touches : ils melangent deux clips ; ici on fabrique une grandeur.
// =============================================================================

#include "NKAnima/Realtime/NkRtMath.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			/// LA SECOUSSE MINIMALE : 0 en 0, 1 en 1, vitesse et acceleration nulles aux deux
			/// bouts ; `u` est borne a [0 ; 1].
			[[nodiscard]] float32 NkRtMinJerk(float32 u) noexcept;

			/// ATTAQUE, TENUE, RELACHE (secondes). `release` <= 0 : le geste n'est pas relache
			/// par l'enveloppe (elle rend 0 apres la tenue).
			struct NkRtEnvelope {
					float32 attack = 0.f, hold = 0.f, release = 0.f;
			};
			/// Le poids de l'enveloppe a l'instant `t` (depuis son debut) : il monte a
			/// secousse minimale pendant l'attaque, vaut 1 pendant la tenue, redescend a
			/// secousse minimale pendant la relache ; 0 avant le debut et apres la fin.
			[[nodiscard]] float32 NkRtEnvelopeWeight(const NkRtEnvelope &e, float32 t) noexcept;

			/// UNE FENETRE : 0 hors de ]start ; end[, 1 au milieu ; elle s'ouvre en `rise`
			/// secondes et se referme en `fall`, a secousse minimale.
			[[nodiscard]] float32 NkRtWindow(float32 t, float32 start, float32 end, float32 rise, float32 fall) noexcept;

			/// HERMITE, sur u de 0 a 1 : la valeur `p0` et la pente `m0` au depart, `p1` et
			/// `m1` a l'arrivee (les pentes par unite de u : une vitesse fois la duree).
			[[nodiscard]] float32 NkRtHermite(float32 u, float32 p0, float32 m0, float32 p1, float32 m1) noexcept;

			/// LE VOL. La hauteur, a l'instant `t`, d'un corps lance vers le haut a la vitesse
			/// `v0` sous la pesanteur `g` : v0 t - g t^2 / 2.
			[[nodiscard]] float32 NkRtBallisticHeight(float32 v0, float32 g, float32 t) noexcept;
			/// La vitesse de decollage qui monte a la hauteur `height` : racine de 2 g h.
			[[nodiscard]] float32 NkRtBallisticSpeed(float32 height, float32 g) noexcept;
			/// La duree du vol (retour a la hauteur de depart) : 2 v0 / g.
			[[nodiscard]] float32 NkRtBallisticTime(float32 v0, float32 g) noexcept;

			/// UNE SECOUSSE : elle monte en ligne droite de 0 a 1 en `rise` secondes, puis
			/// retombe en carre a 0 en `fall` ; `u` est le temps depuis son debut. 0 avant.
			[[nodiscard]] float32 NkRtPulse(float32 u, float32 rise, float32 fall) noexcept;

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
