#pragma once
// =============================================================================
// NkVpEditTarget.h — QUEL OBJET LE MODE EDITION PREND-IL ?
// =============================================================================
// POURQUOI CETTE REGLE VIT DANS UN EN-TETE ET PAS DANS NkDemo3D.cpp
// Elle y vivait, en trois lignes, au milieu du gestionnaire de TAB. Un banc ne
// pouvait pas l'atteindre : `NkDemo3D.cpp` fait 18 000 lignes et exige un
// device graphique — c'est le mur qui avait deja fait retirer NkMatInventaireTest
// du workspace le 17/08, et la raison pour laquelle NkVpMatTypeDefaults.h existe.
// Meme parade, meme forme : la REGLE sort dans un en-tete que les deux cotes
// partagent, la vue l'appelle, le banc l'exerce.
//
// ⚠ CE QU'ELLE DECRIT AUJOURD'HUI EST LE COMPORTEMENT ACTUEL, PAS LE SOUHAITE.
// Extraire une regle et la corriger dans le meme geste ferait passer le banc du
// premier coup, et un banc qui n'a jamais rougi ne mesure rien. Elle est donc
// posee VERBATIM : seuls les objets de demonstration sont editables.
// =============================================================================
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace demo {

		// ── L'ESPACE D'INDICES DES NOEUDS, EN UN SEUL ENDROIT ────────────────
		// Ces trois constantes etaient dispersees dans NkDemo3D.cpp sous forme de
		// « 90 » et « 96 » ecrits a la main dans une vingtaine d'expressions. Les
		// nommer ici ne change aucun calcul ; ca rend seulement possible d'ecrire
		// la regle une fois au lieu de la redeviner a chaque site.
		//   [0, kNumObj)                        objets de DEMONSTRATION (st->gizmo)
		//   [kNkvpEmptyBase, kNkvpFirstUser)    empties de parentage
		//   [kNkvpFirstUser, +kNkvpMaxUser)     objets de L'UTILISATEUR
		static constexpr int32 kNkvpEmptyBase = 90;
		static constexpr int32 kNkvpFirstUser = 96;
		static constexpr int32 kNkvpMaxUser = 64;

		// Natures d'un slot utilisateur (menu Ajouter) : 0 libre, 1 sphere,
		// 2 cube, 3 plan, 4 empty, 5 lumiere, 6 texte, 7 courbe, 8 surface,
		// 9 metaball. Les natures 6..9 sont des MARQUEURS sans geometrie.
		// ⚠ La nature 2 est aussi celle que prend toute GEOMETRIE IMPORTEE
		// (cf. le commentaire de HostAllocUser dans NkDemo3D.cpp) : c'est donc
		// elle, et pas une nature « importe » separee, qui porte les modeles de
		// l'utilisateur.
		enum class NkVpUserKind : uint8 {
			Libre = 0, Sphere = 1, Cube = 2, Plan = 3, Empty = 4,
			Lumiere = 5, Texte = 6, Courbe = 7, Surface = 8, Metaball = 9
		};

		// Une nature porte-t-elle une geometrie editable ?
		inline bool NkVpUserKindEditable(uint8 k) {
			return k == (uint8)NkVpUserKind::Sphere || k == (uint8)NkVpUserKind::Cube ||
				   k == (uint8)NkVpUserKind::Plan;
		}

		// Slot utilisateur vise par une selection du gizmo des empties, ou -1.
		// `selEmpty` est l'indice RENDU par emptyGizmo.ActiveIndex() ; le noeud
		// vaut selEmpty + kNkvpEmptyBase.
		inline int32 NkVpUserSlotOfEmpty(int32 selEmpty) {
			if (selEmpty < 0)
				return -1;
			const int32 node = selEmpty + kNkvpEmptyBase;
			const int32 u = node - kNkvpFirstUser;
			return (u >= 0 && u < kNkvpMaxUser) ? u : -1;
		}

		enum class NkVpEditKind : uint8 { Aucun = 0, Demo = 1, Utilisateur = 2 };

		struct NkVpEditTarget {
				NkVpEditKind kind = NkVpEditKind::Aucun;
				int32 index = -1; // Demo : objet [0,kNumObj) ; Utilisateur : slot [0,kNkvpMaxUser)
		};

		// Etat MINIMAL dont la regle a besoin. Le passer explicitement plutot que
		// de lire les tableaux statiques de la vue est ce qui rend la regle
		// exercable sans device — et ce qui permet au banc de fabriquer le cas
		// « objet de l'utilisateur » que l'application ne sait pas encore produire.
		struct NkVpEditQuery {
				int32 selDemo = -1;			// st->gizmo.ActiveIndex()
				int32 selEmpty = -1;		// st->emptyGizmo.ActiveIndex()
				uint8 userKind = 0;			// nkvpUserKind[slot], slot = NkVpUserSlotOfEmpty(selEmpty)
				bool userDeleted = false;	// nkvpDeleted[noeud]
				bool userMeshValid = false; // nkvpUserMesh[slot].IsValid()
		};

		// ⚠ ETAT ACTUEL, VERBATIM : la selection des empties n'est jamais
		// consultee. Un objet cree ou importe par l'utilisateur vit sur ce
		// gizmo-la ; TAB ne le voit donc pas, et le journal affiche
		// « Selectionne un objet (clic) avant TAB » alors qu'un objet EST
		// selectionne. Le message n'est pas faux par accident : la regle ne
		// connait qu'un seul gizmo.
		inline NkVpEditTarget NkVpResolveEditTarget(const NkVpEditQuery &q) {
			NkVpEditTarget t;
			if (q.selDemo >= 0) {
				t.kind = NkVpEditKind::Demo;
				t.index = q.selDemo;
			}
			return t;
		}

	} // namespace demo
} // namespace nkentseu
