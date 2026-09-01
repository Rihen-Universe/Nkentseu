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
#include "Sommets.h"
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
	///
	/// ⚠️ OÙ VIT SON IMPLÉMENTATION, ET POURQUOI CE N'EST PAS ICI. Le composant
	///    d'arbre du kit fait DÉJÀ Ctrl+ajout et Maj+plage (paramètres
	///    `multi_select` / `range_select`, actifs par défaut, avec son `anchor`).
	///    L'application le SUIT — elle recopie `chosen` — au lieu de redécider :
	///    redécider par-dessus une couche qui a déjà tranché, c'est se donner une
	///    seconde chance de diverger, et ça avait déjà écrasé la plage.
	///    Cette fonction reste donc **l'énoncé du contrat** (ce que la liste doit
	///    faire, tenu par la recette) et le **repli** quand rien n'a été choisi ;
	///    elle n'est pas la seconde implémentation.
	inline NkGesteSel NkGesteListe(bool ctrl, bool maj) {
		if (ctrl || maj)
			return NkGesteSel::Basculer;
		return NkGesteSel::Remplacer;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  CE QU'UN DOUBLE-CLIC OUVRE — LA TROISIÈME TABLE, AU MÊME ENDROIT
	// ═══════════════════════════════════════════════════════════════════════════
	/// ⚠️ ENTRER EN ÉDITION DE TEXTE ET ENTRER EN MODE POINTS SONT DEUX ISSUES DU
	///    MÊME GESTE — un double-clic sur un élément. Écrire la seconde à côté de
	///    la première, dans le corps de `HandleMouse`, aurait donné deux `if`
	///    voisins qu'un troisième cas (le composant, le tableau…) viendrait
	///    départager au jugé. C'est **exactement** la divergence que les deux
	///    tables ci-dessus viennent de fermer pour le clic simple : on ne la
	///    rouvre pas pour le double-clic.
	///
	///    La question « que fait un double-clic ici ? » a donc UNE réponse, et
	///    elle se lit — et se mesure — sans souris.
	enum class NkIssueDblClic {
		Forer,		  ///< un GROUPE : descendre vers l'enfant sous le point
		EditerTexte,  ///< un nœud qui PORTE du texte
		ModePoints,   ///< une forme à sommets réels (ligne, polygone, étoile)
		CoinsSeuls,   ///< rect/ellipse/image : les coins REDIMENSIONNENT — à DIRE
		RienADire	  ///< aucune issue : le refus doit être annoncé, pas muet
	};

	/// La table. `aDesEnfants` est passé à part parce que le forage prime sur
	/// tout le reste : un groupe se traverse, quoi qu'il contienne.
	inline NkIssueDblClic NkIssueDeDblClic(const NkUINode &n) {
		if (n.children.Size() > 0)
			return NkIssueDblClic::Forer;
		// ⚠️ « ÉDITABLE » = QUI PORTE UNE CLÉ `texte`, pas « de nature text »
		//    (4e retour de Rodolf) : un rect à texte par défaut s'édite pareil.
		//    Cette règle vivait dans le corps du geste ; elle vit ici désormais.
		if (NkComponentDecl::StrEq(n.shape.Data(), "text") || !n.text.Empty())
			return NkIssueDblClic::EditerTexte;
		// ⚠️ `rect` ET `ellipse` SONT PASSÉS DE `CoinsSeuls` À `ModePoints` LE
		//    01/09, ET C'EST LA CORRECTION DU PREMIER RETOUR DE RODOLF : *« double-
		//    cliquer sur une shape ne permet pas encore de faire l'édition de la
		//    shape. »* Mesuré avant de toucher : sur les cinq formes qu'il cite,
		//    ligne / polygone / étoile ouvraient bien le mode, **rect et ellipse
		//    répondaient « cette forme n'a pas de sommets »** — c'est-à-dire les
		//    deux formes qu'on pose le plus, et exactement celles de sa référence
		//    (`lunacy_shape_edit_1`, un **Rectangle** en `EDIT SHAPE` à quatre
		//    ancres). Le fond de l'affaire est dans `Sommets.h` : leurs coins ne
		//    sont pas « la boîte », ce sont des sommets réels dès qu'on y touche.
		switch (NkNatureDe(n.shape.Data())) {
			case NkNatureSommets::Bouts:
			case NkNatureSommets::Polygone:
			case NkNatureSommets::CoinsEditables: return NkIssueDblClic::ModePoints;
			case NkNatureSommets::Coins: return NkIssueDblClic::CoinsSeuls;
			default: return NkIssueDblClic::RienADire;
		}
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  UN DOUBLE-CLIC EST UN APPUI — MÊME QUAND L'OS N'EN ENVOIE PAS
	// ═══════════════════════════════════════════════════════════════════════════
	/// ⚠️ CETTE RÈGLE EXISTE PARCE QUE LE GESTE RÉEL ET LE BANC NE PASSAIENT PAS
	///    PAR LE MÊME CHEMIN, et c'est ce qui a rendu le défaut invisible un
	///    mois durant. Mesuré à la source le 2026-09-01, en quatre maillons :
	///
	///    1. `NkWin32Window.cpp:487` — la classe de fenêtre porte `CS_DBLCLKS`,
	///       donc Windows **remplace** le second `WM_LBUTTONDOWN` par
	///       `WM_LBUTTONDBLCLK` au lieu de l'ajouter ;
	///    2. `NkWin32EventSystem.cpp` — ce message n'émettait **que**
	///       `NkMouseDoubleClickEvent`, jamais d'appui (corrigé le même jour) ;
	///    3. `NkEditorShell.cpp` — seul un `NkMouseButtonPressEvent` lève
	///       `mouseDown[0]`, donc `mouseClicked[0]` restait **faux** ;
	///    4. la toile lisait `in.mousePressed = mouseClicked[0]` et **enfermait**
	///       sa branche double-clic dedans : le geste réel n'y entrait jamais.
	///
	///    Et l'INJECTEUR, lui, posait `mouseDown` **puis** `SetDoubleClick` :
	///    toutes les mesures passaient par un chemin que la souris de Rodolf ne
	///    prenait pas. *Un banc qui emprunte une autre porte que le geste ne
	///    prouve rien du geste.*
	///
	///    LA RÈGLE, DONC, ET ELLE SE CITE : **un double-clic vaut appui.**
	///    Elle vit ici plutôt que dans la condition qui a mordu, parce que le
	///    même croisement `appui && double-clic` se retrouve à cinq endroits de
	///    `HandleMouse` (édition en place, hors-toile, flottants, outils de
	///    tracé, corps du geste) : écrite à côté d'un seul, elle n'aurait pas
	///    couvert les quatre voisins.
	inline bool NkAppuiDeToile(bool mousePressed, bool doubleClick) {
		return mousePressed || doubleClick;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  CE QUE LE DOUBLE-CLIC A FAIT — ET LA RAISON QU'IL EN DIT
	// ═══════════════════════════════════════════════════════════════════════════
	/// ⚠️ LA TABLE CI-DESSUS DIT CE QU'UN DOUBLE-CLIC **OUVRE** ; celle-ci dit ce
	///    qu'il a **fait**, et surtout ce qu'il en **dit**. Les deux ne se
	///    confondent pas : `Forer` a **deux** suites, et c'est la seconde qui a
	///    mordu.
	///
	///    MESURE DU 2026-09-01 sur le document Dashboard : un double-clic sur le
	///    corps d'une carte (`Carte_Actifs`), d'un panneau (`Panel_Nav`) ou d'un
	///    artboard (`Dashboard`) — c'est-à-dire partout où aucun ENFANT DIRECT ne
	///    se trouve sous le point — rendait l'issue `Forer`, puis
	///    `NkPickDansContexte` rendait **−2**, et la branche se contentait d'un
	///    `SelectSingle` **muet**. Le forage n'était pas armé non plus : le
	///    double-clic suivant refaisait exactement la même chose. *Rien ne se
	///    passait, rien ne le disait, et ça ne progressait jamais* — le défaut
	///    que la maison chasse depuis un mois, dans sa forme la plus pure, et sur
	///    la plus grande partie de la surface d'une carte.
	///
	///    ⚠️ POURQUOI UNE FONCTION DE PHRASE PLUTÔT QU'UN `Dire` PAR BRANCHE : une
	///       branche qui oublie de parler ne se voit pas à la relecture — elle a
	///       l'air d'une branche. Ici l'oubli est **mesurable** : la recette
	///       parcourt les six suites et exige une phrase non vide pour chacune.
	///       Ajouter une septième suite sans sa phrase fait tomber le cas.
	enum class NkSuiteDblClic {
		ForerVersEnfant,  ///< un enfant était sous le point : on y descend
		ForerSansEnfant,  ///< aucun enfant sous le point : on ENTRE quand même
		EditerTexte,	  ///< le champ de saisie s'ouvre
		ModePoints,		  ///< les sommets deviennent manipulables
		CoinsSeuls,		  ///< pas de sommets : les coins redimensionnent
		RienADire		  ///< aucune issue — et le refus s'annonce
	};

	/// La suite, à partir de l'issue et de ce que le pointage a trouvé.
	/// @param enfantSousLePoint vrai si un ENFANT DIRECT du nœud foré contient
	///        le point (le résultat de `NkPickDansContexte(..., cand)`).
	inline NkSuiteDblClic NkSuiteDeDblClic(NkIssueDblClic issue, bool enfantSousLePoint) {
		switch (issue) {
			case NkIssueDblClic::Forer:
				return enfantSousLePoint ? NkSuiteDblClic::ForerVersEnfant
										 : NkSuiteDblClic::ForerSansEnfant;
			case NkIssueDblClic::EditerTexte: return NkSuiteDblClic::EditerTexte;
			case NkIssueDblClic::ModePoints: return NkSuiteDblClic::ModePoints;
			case NkIssueDblClic::CoinsSeuls: return NkSuiteDblClic::CoinsSeuls;
			default: return NkSuiteDblClic::RienADire;
		}
	}

	/// LA RAISON DITE. **Jamais vide** — c'est tout l'objet de cette fonction.
	/// Le nom du nœud, quand il y en a un, est ajouté par l'appelant.
	inline const char *NkRaisonDeDblClic(NkSuiteDblClic suite) {
		switch (suite) {
			case NkSuiteDblClic::ForerVersEnfant:
				return " — double-clic : descendre/éditer ; Échap : remonter.";
			case NkSuiteDblClic::ForerSansEnfant:
				return " — entré dans le groupe (rien sous le point) : les clics "
					   "désignent son contenu ; Échap remonte.";
			case NkSuiteDblClic::EditerTexte:
				return "Édition du texte — Entrée valide, Échap annule.";
			case NkSuiteDblClic::ModePoints:
				// ⚠️ LA PHRASE ÉNUMÈRE LES TROIS GESTES, ET CE N'EST PAS DU
				//    BAVARDAGE : le mode en a gagné deux le 01/09 (ajouter un
				//    sommet, l'arrondir) et **rien à l'écran ne les annonce**. Un
				//    sommet qui s'ajoute au clic sur un côté ne se devine pas, et
				//    personne ne double-clique une poignée pour voir ce qui
				//    arrive. *Une capacité qu'aucune phrase ne nomme est une
				//    capacité que l'utilisateur n'a pas* — c'est la même règle
				//    que « chaque issue produit un effet visible ou une raison
				//    dite », prise par l'autre bout.
				return "Mode édition de forme — glisser un sommet le déplace, cliquer un "
					   "côté en ajoute un, double-cliquer un sommet l'arrondit ; Échap "
					   "ressort.";
			case NkSuiteDblClic::CoinsSeuls:
				// ⚠️ CETTE PHRASE NE VAUT PLUS QUE POUR image / avatar / cadre —
				//    rect et ellipse sont passés au mode d'édition. Elle DIT
				//    désormais quelles formes s'éditent, au lieu de laisser
				//    croire qu'aucune ne le fait : sans ça, un utilisateur qui
				//    double-clique un cadre d'image conclurait que l'édition de
				//    forme n'existe pas dans l'outil.
				return "Cette forme n'a pas de sommets à éditer : ses coins "
					   "redimensionnent. Un rectangle, une ellipse, une ligne ou un "
					   "polygone, eux, s'ouvrent en édition de forme.";
			default: return "Élément non éditable — rien à ouvrir ici.";
		}
	}

	/// Le nombre de suites, pour que la recette les parcoure TOUTES au lieu d'en
	/// citer une liste qui se périme à la première qu'on ajoute.
	inline uint32 NkNbSuitesDblClic() {
		return 6u;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  À QUI APPARTIENT UNE POIGNÉE — LA PRIORITÉ EST DITE, PAS SUBIE
	// ═══════════════════════════════════════════════════════════════════════════
	/// ⚠️ CETTE FONCTION EXISTE PARCE QUE DEUX POIGNÉES SE SUPERPOSENT AU PIXEL.
	///    Sur une LIGNE, les deux bouts tombent **exactement** sur deux coins de
	///    la boîte : la poignée de redimensionnement et celle de sommet occupent
	///    le même point de l'écran (mesuré sur capture, 01/09).
	///
	///    La première correction les rendait exclusives par une garde dans le
	///    code du dessin et une autre dans le code du geste. Ça marchait — et
	///    c'était **une priorité subie, pas dite** : elle vivait dans l'ordre de
	///    deux `if` situés à six cents lignes l'un de l'autre, et le premier qui
	///    déplace l'un des deux la casse sans s'en apercevoir. Une règle qu'on ne
	///    peut pas citer est une règle qu'on ne peut pas défendre.
	///
	///    LA RÈGLE, DONC : **en mode points, le sommet gagne** — c'est le mode où
	///    l'on est, et un mode qui n'a pas la priorité sur ses propres poignées
	///    n'est pas un mode. Elle se lit ici, elle se mesure, et les deux sites
	///    l'appellent au lieu de la redécider.
	enum class NkProprioPoignee {
		Sommet,		  ///< le mode points a la main sur ce nœud
		Redimension,  ///< les poignées de sélection habituelles
		Aucune		  ///< ce nœud n'est ni en mode points ni sélectionné
	};
	/// @param pointsNode le nœud en mode points (-1 = aucun)
	/// @param selection  le nœud dont on s'apprête à dessiner/armer les poignées
	inline NkProprioPoignee NkAQuiLaPoignee(int32 pointsNode, int32 selection) {
		if (selection < 0)
			return NkProprioPoignee::Aucune;
		if (pointsNode >= 0 && pointsNode == selection)
			return NkProprioPoignee::Sommet;
		return NkProprioPoignee::Redimension;
	}

	/// ⚠️ LE CAS FRÈRE — ET IL A CHANGÉ DE NATURE LE 01/09, IL FAUT LE DIRE.
	///    Jusqu'ici, le conflit « poignée de coin contre poignée de sommet » était
	///    écarté sur rect / ellipse par un fait *structurel* : le mode points ne
	///    s'y ouvrait jamais. **Ce fait n'existe plus** — rect et ellipse entrent
	///    désormais en mode points, donc leurs quatre coins portent DEUX poignées
	///    au même pixel, exactement comme les deux bouts d'une ligne.
	///
	///    ⚠️ ET LE MÉCANISME N'A PAS BOUGÉ D'UNE LIGNE, PARCE QU'IL DISAIT DÉJÀ LA
	///       RÈGLE AU LIEU DE LA SUBIR : `NkAQuiLaPoignee` répond « en mode
	///       points, le sommet gagne » sans rien savoir de la forme. C'est le
	///       bénéfice qu'on avait acheté en §14a de Q42 en remplaçant deux gardes
	///       distantes par une fonction citable — il se paie aujourd'hui, sur un
	///       cas qui n'existait pas quand on l'a écrite. *Une règle dite couvre
	///       des chemins qu'on n'avait pas prévus ; une règle subie n'en couvre
	///       jamais qu'un.*
	inline bool NkPeutEntrerEnPoints(const NkUINode &n) {
		const NkNatureSommets nat = NkNatureDe(n.shape.Data());
		return n.children.Size() == 0
			   && (nat == NkNatureSommets::Bouts || NkSommetsStockes(nat));
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
