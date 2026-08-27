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
			Lumiere = 5, Texte = 6, Courbe = 7, Surface = 8, Metaball = 9,
			// 10 : CERCLE, un vrai maillage (boucle d'aretes fermee). Il ne suit
			// PAS la numerotation 1..9 parce qu'il a ete ajoute apres ; c'est
			// justement ce saut qui l'a fait oublier de la regle ci-dessous.
			Cercle = 10
		};

		// Une nature porte-t-elle une geometrie editable ?
		// REGLE DE RODOLF (27/08) : « TOUT MESH DOIT POUVOIR ETRE EDITABLE ».
		// On enumere donc les natures qui NE sont PAS des maillages -- marqueurs
		// et emplacement libre -- et tout le reste est editable. Ecrite dans ce
		// sens, une nature AJOUTEE devient editable par defaut : c'est l'inverse
		// de la liste blanche precedente, ou le CERCLE avait ete oublie au seul
		// motif qu'il portait le numero 10 et non 4.
		inline bool NkVpUserKindEditable(uint8 k) {
			switch ((NkVpUserKind)k) {
				case NkVpUserKind::Libre:
				case NkVpUserKind::Empty:
				case NkVpUserKind::Lumiere:
				case NkVpUserKind::Texte:
				case NkVpUserKind::Courbe:
				case NkVpUserKind::Surface:
				case NkVpUserKind::Metaball:
					return false;
				default:
					return true;
			}
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

		// ⚠ LA PRIORITE EST ECRITE, PAS SUBIE. Les deux gizmos peuvent porter une
		// selection en meme temps ; l'objet de demonstration l'emporte. Sans cette
		// phrase, la cible dependrait de l'ordre des `if`, et le jour ou quelqu'un
		// les reordonne elle changerait sans que rien ne le dise.
		//
		// AVANT (et c'etait la brique absente) : la selection des empties n'etait
		// JAMAIS consultee. Un objet cree ou importe par l'utilisateur vit sur ce
		// gizmo-la ; TAB ne le voyait donc pas, et le journal affichait
		// « Selectionne un objet (clic) avant TAB » alors qu'un objet ETAIT
		// selectionne. Le message n'etait pas faux par accident : la regle ne
		// connaissait qu'un seul gizmo.
		inline NkVpEditTarget NkVpResolveEditTarget(const NkVpEditQuery &q) {
			NkVpEditTarget t;
			if (q.selDemo >= 0) {
				t.kind = NkVpEditKind::Demo;
				t.index = q.selDemo;
				return t;
			}
			// ⚠ QUATRE CONDITIONS, ET AUCUNE N'EST DECORATIVE.
			//   slot valide      : un empty de PARENTAGE (noeud 90..95) n'est pas un
			//                      objet, et lire nkvpUserKind hors bornes rendrait
			//                      une nature au hasard ;
			//   nature editable  : une lumiere ou un marqueur de courbe ouvrirait une
			//                      cage VIDE, ce qui ressemble a un bug de rendu ;
			//   maillage present : un slot dont la geometrie n'est pas encore generee
			//                      n'a rien a cloner ;
			//   non supprime     : un objet efface ne se rouvre pas par TAB.
			const int32 u = NkVpUserSlotOfEmpty(q.selEmpty);
			if (u >= 0 && NkVpUserKindEditable(q.userKind) && q.userMeshValid && !q.userDeleted) {
				t.kind = NkVpEditKind::Utilisateur;
				t.index = u;
			}
			return t;
		}

	} // namespace demo
} // namespace nkentseu
