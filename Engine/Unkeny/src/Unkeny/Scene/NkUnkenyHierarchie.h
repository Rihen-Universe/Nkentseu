//
// NkUnkenyHierarchie.h
// =============================================================================
// Description :
//   L'IDENTITE STABLE d'une entite et sa place dans la HIERARCHIE (parent,
//   transformation locale). Donnees seulement, comme NkUnkenyComposants.h ; les
//   gestes (rattacher, detacher, propager) sont des methodes de NkScene,
//   ecrites dans NkUnkenyHierarchie.cpp.
//
// Caracteristiques :
//   - NkIdentite2D : un numero donne par la scene a la creation, JAMAIS reutilise
//     dans une session, garde par la photo (Jouer / Arreter), par le fichier et
//     par les prefabs. La poignee ecs::NkEntityId, elle, change a Restaurer et
//     au chargement : c'est la poignee de l'ECS, rapide et verifiee
//     (generation), et elle le reste. L'identite est ce qu'on ECRIT.
//   - Le lien de parente est ecs::NkParent (NKECS/Hierarchy), porte par
//     l'ENFANT : la seule verite. Les enfants se deduisent.
//   - NkLocal2D : la place de l'enfant DANS le repere de son parent. Il n'existe
//     que sur un enfant.
//
// ⚠️ NkTransform2D RESTE LE REPERE MONDE, et c'est la decision qui garde tout
//    le reste intact : le rendu, la physique, le choix sous le curseur, les
//    gizmos, NkVitesse2D lisent et ecrivent NkTransform2D exactement comme
//    avant. La hierarchie RECALCULE le monde d'un enfant depuis son local
//    (NkScene::PropagerHierarchie) ; quand quelqu'un a ecrit le monde d'un
//    enfant entre-temps (gizmo, teleport, vitesse, physique), c'est ce geste qui
//    gagne et le local suit. `produit` et `parentProduit` servent a le voir.
//
// Algorithmes implementes :
//   - Composition 2D T.R.S (NkComposer2D) et son inverse (NkDecomposer2D)
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYHIERARCHIE_H__
#define __NKENTSEU_UNKENY_NKUNKENYHIERARCHIE_H__

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "Unkeny/Scene/NkUnkenyComposants.h"

namespace nkentseu {
	namespace unkeny {

		/// L'identite d'une entite, stable d'une session a l'autre. 0 = aucune.
		struct NkIdentite2D {
				uint64 uid = 0;
		};

		/// La place d'un ENFANT dans le repere de son parent.
		struct NkLocal2D {
				/// La VERITE de l'enfant : position, rotation, echelle relatives.
				NkTransform2D local;
				/// Le monde que la derniere propagation a ecrit. Un monde qui en
				/// differe a ete deplace par quelqu'un d'autre : son geste gagne.
				NkTransform2D produit;
				/// Le monde du parent a cette meme propagation : c'est dans CE
				/// repere que le geste a ete fait, meme si le parent a bouge depuis.
				NkTransform2D parentProduit;
		};

		/// Le monde d'un enfant : `parent` applique a `local` (T * R * S, comme
		/// NkTransform2D::VersMonde).
		/// ⚠️ Echelle NON UNIFORME d'un parent + rotation de l'enfant : le vrai
		///    produit porterait un cisaillement, qu'un NkTransform2D ne represente
		///    pas. On garde echelle = produit des echelles (le « lossyScale » de
		///    Unity) : exact pour une echelle uniforme, approche sinon.
		inline NkTransform2D NkComposer2D(const NkTransform2D &parent, const NkTransform2D &local) noexcept {
			NkTransform2D m;
			m.position = parent.VersMonde(local.position);
			m.rotation = parent.rotation + local.rotation;
			m.echelle = NkVec2f(parent.echelle.x * local.echelle.x, parent.echelle.y * local.echelle.y);
			return m;
		}

		/// L'inverse : le local qui, compose avec `parent`, rend `monde`.
		/// Une echelle de parent nulle (entite aplatie) rend une position locale
		/// nulle sur cet axe plutot qu'une division par zero.
		inline NkTransform2D NkDecomposer2D(const NkTransform2D &parent, const NkTransform2D &monde) noexcept {
			const float32 c = math::NkCos(-parent.rotation);
			const float32 s = math::NkSin(-parent.rotation);
			const float32 dx = monde.position.x - parent.position.x;
			const float32 dy = monde.position.y - parent.position.y;
			const float32 rx = dx * c - dy * s;
			const float32 ry = dx * s + dy * c;
			const float32 ex = parent.echelle.x;
			const float32 ey = parent.echelle.y;
			NkTransform2D l;
			l.position = NkVec2f(ex != 0.f ? rx / ex : 0.f, ey != 0.f ? ry / ey : 0.f);
			l.rotation = monde.rotation - parent.rotation;
			l.echelle = NkVec2f(ex != 0.f ? monde.echelle.x / ex : monde.echelle.x,
								ey != 0.f ? monde.echelle.y / ey : monde.echelle.y);
			return l;
		}

		/// Egalite EXACTE de deux transforms (octet pour octet des valeurs) : ce
		/// que la propagation compare pour savoir si un monde a ete touche.
		inline bool NkMemeTransform2D(const NkTransform2D &a, const NkTransform2D &b) noexcept {
			return a.position.x == b.position.x && a.position.y == b.position.y && a.rotation == b.rotation &&
				   a.echelle.x == b.echelle.x && a.echelle.y == b.echelle.y;
		}

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYHIERARCHIE_H__
