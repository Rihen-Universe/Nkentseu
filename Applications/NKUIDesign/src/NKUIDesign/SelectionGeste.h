#pragma once
// -----------------------------------------------------------------------------
// @File    SelectionGeste.h
// @Brief   LE CONTRAT DE SÉLECTION (Lunacy), en MÉCANISME : ce qu'un clic veut
//          dire, ce que la sélection occupe, et ce qu'elle a en commun.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ POURQUOI CE FICHIER EXISTE — ET IL EST ÉCRIT AVANT LE CHEMIN QU'IL PROTÈGE
// =============================================================================
//  Porte du dépôt du 28/08 : *« une leçon écrite à côté d'un chemin ne couvre
//  pas le chemin voisin — demander quels chemins FRÈRES partagent le même
//  danger, et l'écrire une fois pour tous. »*
//
//  Ici, les chemins frères sont **la TOILE et la HIÉRARCHIE**. Tous deux
//  traduisent `ctrl`/`maj` en un geste de sélection, et **ils avaient déjà
//  divergé** : la toile prenait `Ctrl` pour la multi-sélection (doc 3 §11.5),
//  Lunacy le prend pour la sélection profonde, et la Hiérarchie, elle,
//  **ignorait purement et simplement les modificateurs** — un `SelectSingle`
//  inconditionnel. Trois surfaces, trois avis, aucun écrit au même endroit.
//
//  ⚠️ ET LA RÉPONSE N'EST PAS « UNE SEULE TABLE ». Ce serait le raccourci
//     tentant, et il serait faux : une TOILE est spatiale (le curseur désigne
//     une profondeur, donc `Ctrl` a un sens « descends »), une LISTE ne l'est
//     pas (rien à traverser, `Ctrl` y est l'ajout, comme dans tous les
//     explorateurs — et **c'est ce que fait Lunacy dans son panneau Layers**).
//     Les deux tables sont donc **différentes par nature**, et c'est
//     précisément pour ça qu'elles doivent être **côte à côte** : le danger
//     n'est pas qu'elles diffèrent, c'est qu'elles dérivent sans qu'on le voie.
//
//  Ce fichier ne connaît ni la souris, ni l'écran, ni NKGui : il se mesure.
// -----------------------------------------------------------------------------
#include "Layout.h"
#include "Selection.h"

namespace nkuidesign {

	using nkentseu::float32;
	using nkentseu::int32;
	using nkentseu::uint32;

	/// Ce qu'un clic VEUT DIRE, une fois les modificateurs lus.
	enum class NkGesteSel {
		Remplacer, ///< la sélection devient ce seul nœud (le clic nu)
		Basculer,  ///< ajoute / retire (la multi-sélection)
		Profond	   ///< désigne le nœud le plus PROFOND sous le curseur
	};

	/// LA TABLE DE LA TOILE (Lunacy / Figma / Sketch).
	/// ⚠️ NE PAS L'INVERSER SANS RAISON ÉCRITE : elle a déjà été inversée une
	///    fois (Ctrl faisait la multi-sélection), et ça s'est vu à l'usage.
	inline NkGesteSel NkGesteToile(bool ctrl, bool maj) {
		if (maj)
			return NkGesteSel::Basculer;
		if (ctrl)
			return NkGesteSel::Profond;
		return NkGesteSel::Remplacer;
	}

	/// LA TABLE DE LA LISTE (Hiérarchie) — volontairement différente.
	/// `Profond` n'y existe pas : une rangée d'arbre DÉSIGNE DÉJÀ son nœud,
	/// il n'y a aucune profondeur à traverser. `Ctrl` y prend donc le rôle
	/// d'ajout, comme dans tout explorateur — et comme dans Lunacy.
	inline NkGesteSel NkGesteListe(bool ctrl, bool maj) {
		if (ctrl || maj)
			return NkGesteSel::Basculer;
		return NkGesteSel::Remplacer;
	}

	/// APPLIQUER un geste à la sélection partagée.
	/// ⚠️ LA RACINE N'EST JAMAIS UN ÉLÉMENT, et la règle vit ICI plutôt que
	///    dans chaque appelant : elle avait déjà été oubliée par le premier
	///    Maj+clic de la toile (relevé du 01/09 : sélection = [0, 15], principal
	///    = la racine). Trois surfaces l'auraient oubliée trois fois.
	inline void NkAppliquerGeste(const NkUIDocument &doc, NkSelection &sel, int32 &principal,
								 int32 cible, NkGesteSel geste) {
		if (!doc.IsValidIndex(cible) || cible == 0)
			return;
		if (geste == NkGesteSel::Basculer) {
			if (sel.Contains(0))
				sel.Clear(); // la racine sélectionnée au démarrage ne compte pas
			sel.Toggle(cible);
		} else {
			sel.Set(cible);
		}
		principal = sel.Primary();
	}

	/// LE RECTANGLE ENGLOBANT de la sélection, en espace DOCUMENT.
	/// ⚠️ UN SEUL CALCUL POUR TROIS CONSOMMATEURS : les poignées de
	///    multi-sélection, la puce de taille, et le cadre de survol groupé.
	///    Écrit trois fois, il aurait donné trois boîtes légèrement
	///    différentes — et personne n'aurait su laquelle fait foi.
	/// Rend faux quand rien de mesurable n'est sélectionné (la racine ne
	/// compte pas : elle couvre tout, son englobant ne dit rien).
	inline bool NkRectSelection(const NkUIDocument &doc, const NkLayoutResult &lay,
								const NkSelection &sel, NkPaintRect &out) {
		bool trouve = false;
		float32 x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
		for (uint32 k = 0; k < (uint32)sel.items.Size(); ++k) {
			const int32 i = sel.items[k];
			if (i <= 0 || !doc.IsValidIndex(i) || !lay.Has(i))
				continue;
			const NkPaintRect r = lay.At(i);
			if (!trouve) {
				x0 = r.x;
				y0 = r.y;
				x1 = r.x + r.w;
				y1 = r.y + r.h;
				trouve = true;
			} else {
				if (r.x < x0)
					x0 = r.x;
				if (r.y < y0)
					y0 = r.y;
				if (r.x + r.w > x1)
					x1 = r.x + r.w;
				if (r.y + r.h > y1)
					y1 = r.y + r.h;
			}
		}
		if (trouve)
			out = {x0, y0, x1 - x0, y1 - y0};
		return trouve;
	}

	/// LA VALEUR COMMUNE d'une propriété sur toute la sélection.
	/// ⚠️ CE HELPER EXISTE PARCE QUE LE DANGER EST DE GROUPE. L'Inspecteur doit
	///    marquer « — » sur CHAQUE propriété mixte : position, taille, opacité,
	///    couleur, rayon, graisse… Écrite ligne par ligne, la règle « mixte =
	///    tiret » aurait été oubliée par la première rangée ajoutée après. Elle
	///    est donc dans le mécanisme, et une rangée qui l'oublie ne compile pas
	///    (elle n'a pas de valeur à afficher).
	/// @return vrai si TOUS les sélectionnés (hors racine) donnent la même
	///         valeur ; `out` la porte. Faux = mixte, ou rien à lire.
	template <typename Lire>
	inline bool NkValeurCommune(const NkUIDocument &doc, const NkSelection &sel, Lire lire,
								float32 &out) {
		bool premier = true;
		float32 v = 0.f;
		for (uint32 k = 0; k < (uint32)sel.items.Size(); ++k) {
			const int32 i = sel.items[k];
			if (i <= 0 || !doc.IsValidIndex(i))
				continue;
			const float32 w = lire(doc.nodes[(uint32)i]);
			if (premier) {
				v = w;
				premier = false;
			} else if (w != v) {
				return false; // MIXTE
			}
		}
		if (premier)
			return false; // rien de lisible
		out = v;
		return true;
	}

	/// Le nombre d'éléments RÉELS de la sélection (la racine ne compte pas).
	inline uint32 NkCompteSelection(const NkUIDocument &doc, const NkSelection &sel) {
		uint32 n = 0;
		for (uint32 k = 0; k < (uint32)sel.items.Size(); ++k)
			if (sel.items[k] > 0 && doc.IsValidIndex(sel.items[k]))
				++n;
		return n;
	}

} // namespace nkuidesign
