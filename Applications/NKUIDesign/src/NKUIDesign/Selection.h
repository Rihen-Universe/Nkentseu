// =============================================================================
//  NkUIDesign — LA SELECTION : UNE, partagee, et qui vit dans la VUE
// =============================================================================
//
//  ⚠️ OU VIT-ELLE ? DANS LA VUE, ET C'EST TRANCHE PAR LA MESURE ET PAR LE
//     DOCUMENT 3. Mesure : `selected` a toujours vecu dans `DesignState`, jamais
//     dans `NkUIDocument`, et ni `Save` ni `Load` ne l'ont jamais ecrit.
//
//     Si la selection etait dans le DOCUMENT, elle s'enregistrerait : deux
//     personnes ouvrant le meme fichier heriteraient de la selection de l'autre,
//     et un simple clic salirait le fichier. Dans la vue, elle disparait a la
//     fermeture — c'est ce qu'on veut d'un curseur.
//
//     **Le controle qui le prouve mecaniquement : le md5 du document ne bouge
//     pas quand on selectionne.**
//
//  ⚠️ UNE SEULE SELECTION POUR TOUS LES PANNEAUX. Document 3 §11.5, mot pour
//     mot : *« La selection est la MEME que celle du canvas et que celle de
//     l'Inspecteur [...] deux notions de "ce qui est selectionne" finiraient par
//     diverger, et personne ne saurait laquelle fait foi. »* Elle vit donc dans
//     `DesignState`, pas dans un panneau.
//
//  ⚠️ ET DANS QUEL ESPACE TESTE-T-ON L'APPARTENANCE ? EN DOCUMENT.
//     Le rectangle de selection se TRACE a l'ecran mais les formes vivent en
//     document. On traduit donc le rectangle UNE fois (deux coins), puis on
//     teste contre la disposition document. L'inverse — traduire N formes vers
//     l'ecran pour tester — donnerait le meme resultat et ferait N conversions
//     au lieu de deux : autant d'occasions de se tromper d'espace, et ce
//     chantier en a deja paye trois en une seule etape.
// =============================================================================

#ifndef __NKENTSEU_NKUIDESIGN_SELECTION_H__
#define __NKENTSEU_NKUIDESIGN_SELECTION_H__

#include "Document.h"
#include "Layout.h"

namespace nkuidesign {

	/// L'ensemble des noeuds selectionnes. Un ensemble, pas une liste : poser
	/// deux fois le meme noeud ne le compte pas deux fois.
	struct NkSelection {
			/// ⚠️ LE PREMIER EST LE PRINCIPAL, et l'ordre d'ajout est conserve.
			///    L'Inspecteur affiche le nom du principal et `Multiple` sur ce qui
			///    differe (doc 3 §12.2) ; il lui faut donc savoir lequel a ete
			///    designe en premier.
			NkVector<nkentseu::int32> items;

			bool Contains(nkentseu::int32 i) const {
				for (nkentseu::uint32 k = 0; k < (nkentseu::uint32)items.Size(); ++k)
					if (items[k] == i)
						return true;
				return false;
			}
			nkentseu::uint32 Count() const {
				return (nkentseu::uint32)items.Size();
			}
			bool Empty() const {
				return items.Empty();
			}
			/// Le noeud principal, ou -1 si rien n'est selectionne.
			nkentseu::int32 Primary() const {
				return items.Empty() ? -1 : items[0];
			}
			void Clear() {
				items.Clear();
			}
			/// Remplace tout par un seul noeud. Le clic simple.
			void Set(nkentseu::int32 i) {
				items.Clear();
				if (i >= 0)
					items.PushBack(i);
			}
			/// Ajoute sans doublon. Le rectangle de selection.
			void Add(nkentseu::int32 i) {
				if (i >= 0 && !Contains(i))
					items.PushBack(i);
			}
			/// Ajoute ou retire. `Ctrl`+clic (doc 3 §11.5).
			void Toggle(nkentseu::int32 i) {
				if (i < 0)
					return;
				for (nkentseu::uint32 k = 0; k < (nkentseu::uint32)items.Size(); ++k) {
					if (items[k] == i) {
						for (nkentseu::uint32 j = k + 1; j < (nkentseu::uint32)items.Size(); ++j)
							items[j - 1] = items[j];
						items.PopBack();
						return;
					}
				}
				items.PushBack(i);
			}
			/// Retire les index qui n'existent plus. `RemoveSubtree` renumerote les
			/// noeuds : une selection gardee telle quelle designerait autre chose.
			void DropInvalid(const NkUIDocument &doc) {
				NkVector<nkentseu::int32> keep;
				for (nkentseu::uint32 k = 0; k < (nkentseu::uint32)items.Size(); ++k)
					if (doc.IsValidIndex(items[k]))
						keep.PushBack(items[k]);
				items = keep;
			}
	};

	/// LE RECTANGLE DE SELECTION. `zone` est en ESPACE DOCUMENT — le nom du
	/// parametre et ce commentaire sont tout ce qui empeche de s'y tromper.
	///
	/// ⚠️ ON EXIGE LE CHEVAUCHEMENT, PAS L'INCLUSION COMPLETE, et c'est une
	///    decision : effleurer une forme la prend. C'est ce que fait la grande
	///    majorite des outils de dessin, et c'est le comportement qu'un
	///    concepteur attend quand il balaie vite. L'inclusion stricte est l'autre
	///    convention (Illustrator la propose en option) ; si Rodolf la prefere,
	///    c'est UNE ligne, et le controle 40e la designe.
	///
	/// La RACINE n'est jamais prise : elle couvre toute la surface, donc tout
	/// rectangle la toucherait et toute selection au rectangle contiendrait tout.
	inline void NkPickInRect(const NkUIDocument &doc, const NkLayoutResult &lay,
							 const NkPaintRect &zone, NkSelection &out) {
		for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)doc.nodes.Size(); ++i) {
			if (doc.nodes[i].parent < 0)
				continue; // la racine, jamais
			if (!lay.Has((nkentseu::int32)i))
				continue;
			const NkPaintRect &r = lay.At((nkentseu::int32)i);
			const bool dehors = r.x + r.w <= zone.x || r.x >= zone.x + zone.w
								|| r.y + r.h <= zone.y || r.y >= zone.y + zone.h;
			if (!dehors)
				out.Add((nkentseu::int32)i);
		}
	}

	/// LE POINTAGE POUR SELECTIONNER. `NkPickNode` rend le noeud le plus
	/// profond sous le curseur -- et la RACINE couvre toute la surface, donc
	/// elle repond a TOUS les clics. Resultat : « cliquer dans le vide »
	/// n existait pas, et la selection ne se vidait jamais.
	///
	/// ⚠️ MESURE DU 2026-08-28, ET C EST LE CONTROLE NEGATIF QUI L A TROUVE :
	///    un clic a (900,550), loin de toute forme, rendait la racine. Le
	///    controle 40c echouait pour la bonne raison des sa premiere execution.
	///
	/// Cliquer le fond de la page DESELECTIONNE, comme dans tout outil de
	/// dessin. `NkPickNode` n est pas touche : le glissement de bord continue
	/// de pouvoir viser la racine, ce sont deux gestes differents.
	inline nkentseu::int32 NkPickSelectable(const NkUIDocument &doc,
											const NkLayoutResult &lay, nkentseu::float32 x,
											nkentseu::float32 y) {
		const nkentseu::int32 hit = NkPickNode(doc, lay, x, y);
		if (hit < 0 || !doc.IsValidIndex(hit))
			return -1;
		return doc.nodes[(nkentseu::uint32)hit].parent < 0 ? -1 : hit;
	}

	/// LE NIVEAU DE SELECTION D'UN CLIC SIMPLE (regle Lunacy/Figma — mesure du
	/// bogue « effet bizarre au deplacement », Rodolf 31/08). Le pointage
	/// profond attrape la FEUILLE : sur un bouton, son LIBELLE — et le glisser
	/// SEPARAIT le libelle de son bouton, dans les quatre directions (l'element
	/// visible restait, son texte partait ; l'ancien document du matin posait
	/// meme le texte en FRERE par-dessus le bouton — meme piege des le premier
	/// clic). Un clic simple selectionne donc l'element de PREMIER NIVEAU :
	/// l'ancetre du pointage dont le parent est un artboard (`forme = frame`)
	/// ou la racine. Le DOUBLE-CLIC descend dans le composite (le geste
	/// « entrer » de tous les outils de dessin). `NkPickSelectable` ne bouge
	/// pas : les controles 40x le mesurent tel quel, et le double-clic le
	/// consomme.
	inline nkentseu::int32 NkPickTopLevel(const NkUIDocument &doc, const NkLayoutResult &lay,
										  nkentseu::float32 x, nkentseu::float32 y) {
		nkentseu::int32 hit = NkPickSelectable(doc, lay, x, y);
		nkentseu::int32 garde = 0;
		while (hit >= 0 && doc.IsValidIndex(hit) && ++garde < 64) {
			const nkentseu::int32 pa = doc.nodes[(nkentseu::uint32)hit].parent;
			if (pa < 0)
				break; // deja au premier niveau
			const NkUINode &pn = doc.nodes[(nkentseu::uint32)pa];
			const char *ps = pn.shape.Data();
			const bool artboard = ps && ps[0] == 'f' && ps[1] == 'r' && ps[2] == 'a'
								  && ps[3] == 'm' && ps[4] == 'e' && ps[5] == 0;
			if (pn.parent < 0 || artboard)
				break; // le parent est la racine ou un artboard : c'est le niveau
			hit = pa;
		}
		return hit;
	}

	/// LE RECEPTACLE D'UNE CREATION (2026-08-30, chaine du designer). Un outil
	/// qui pose une forme doit savoir DANS QUOI il la pose : le conteneur a
	/// agencement `Free` le plus PROFOND sous le point -- l'artboard si on
	/// dessine dedans, la toile sinon. Contrairement a `NkPickSelectable`, la
	/// RACINE est un receptacle legitime : dessiner sur le fond de la toile est
	/// exactement le geste « poser un artboard ». Rend -1 si aucun conteneur
	/// `Free` ne contient le point -- l'outil ne cree alors rien, plutot que de
	/// poser une forme dans un agencement calcule qui l'ignorerait.
	inline nkentseu::int32 NkPickFreeContainer(const NkUIDocument &doc, const NkLayoutResult &lay,
											   nkentseu::float32 x, nkentseu::float32 y) {
		nkentseu::int32 best = -1;
		nkentseu::int32 bestDepth = -1;
		for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)doc.nodes.Size(); ++i) {
			if (doc.nodes[i].layout.kind != nkentseu::editorkit::NkLayoutKind::Free)
				continue;
			if (!lay.Has((nkentseu::int32)i))
				continue;
			const NkPaintRect &r = lay.At((nkentseu::int32)i);
			if (!(x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h))
				continue;
			nkentseu::int32 depth = 0;
			for (nkentseu::int32 p = doc.nodes[i].parent; p >= 0 && depth < 64;
				 p = doc.nodes[(nkentseu::uint32)p].parent)
				++depth;
			if (depth > bestDepth) {
				bestDepth = depth;
				best = (nkentseu::int32)i;
			}
		}
		return best;
	}

	/// Deux points quelconques rendent un rectangle normalise. Tracer de la
	/// droite vers la gauche doit selectionner autant que l'inverse.
	inline NkPaintRect NkRectFromPoints(nkentseu::float32 ax, nkentseu::float32 ay,
										nkentseu::float32 bx, nkentseu::float32 by) {
		NkPaintRect r;
		r.x = ax < bx ? ax : bx;
		r.y = ay < by ? ay : by;
		r.w = ax < bx ? bx - ax : ax - bx;
		r.h = ay < by ? by - ay : ay - by;
		return r;
	}

} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_SELECTION_H__
