#pragma once
// -----------------------------------------------------------------------------
// @File    MenuContexte.h
// @Brief   LE MENU DU CLIC DROIT (Lunacy), en MÉCANISME : ce qu'il propose, ce
//          qui agit, et la RAISON de ce qui n'agit pas.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ ÉCRIT AVANT LES DEUX CHEMINS QU'IL SERT, PAS APRÈS LE PREMIER QUI MORD
// =============================================================================
//  Porte du dépôt du 28/08 : *« demander quels chemins FRÈRES partagent le même
//  danger, et l'écrire une fois pour tous. »* Ici les frères sont **la TOILE et
//  la HIÉRARCHIE** — troisième fois que ce couple revient sur ce chantier après
//  les tables de clic et l'englobant, donc on ne recommence pas à écrire deux
//  fois.
//
//  ⚠️ ET LA DIFFÉRENCE ENTRE LES DEUX SURFACES EST **L'APPLICABILITÉ, JAMAIS LE
//     JEU D'ENTRÉES**. Lunacy montre le même menu depuis sa toile et depuis son
//     panneau Layers ; ce qui change, c'est ce qui est grisé. Deux jeux
//     d'entrées auraient dérivé au premier ajout — et l'utilisateur aurait
//     cherché dans un menu une commande qu'il venait de voir dans l'autre.
//
//  ⚠️ **UNE ENTRÉE QUI N'AGIT PAS PORTE SA RAISON, ET C'EST MESURÉ.** C'est la
//     règle de la maison appliquée au menu : un contrôle qui ne fait rien sans
//     rien dire est le défaut qu'on chasse. Ici l'oubli est *mécaniquement*
//     détectable — la recette parcourt les entrées et exige : qui n'agit pas a
//     une raison non vide ; qui agit n'en a **pas** (sinon la raison serait un
//     ornement qu'on cesse de lire).
//
//  Ce fichier ne connaît ni la souris, ni l'écran, ni NKGui : il se mesure.
// -----------------------------------------------------------------------------
#include "Document.h"

namespace nkuidesign {

	using nkentseu::int32;
	using nkentseu::uint32;

	/// CE QU'UNE ENTRÉE DÉCLENCHE. L'index d'affichage ne sert **jamais** de
	/// clé : il change dès qu'on insère une entrée, et l'appelant exécuterait
	/// alors le geste voisin. C'est l'action qui voyage, pas la position.
	enum class NkActionCtx {
		EditerTexte,
		Vectoriser,
		IA,
		CollerParDessus,
		CopierComme,
		Dupliquer,
		// ── L'ORDRE DE PROFONDEUR — QUATRE GESTES, PAS UN ────────────────────
		// ⚠️ L'ENTRÉE UNIQUE « Envoyer derrière » ÉTAIT UN QUART DU SUJET. Lunacy
		//    en fait un groupe de quatre (`Ctrl+]`, `Ctrl+Maj+]`, `Ctrl+[`,
		//    `Ctrl+Maj+[`), et « d'un cran » n'est pas « tout au fond » : sur une
		//    pile de six calques, les confondre fait sauter quatre rangs d'un
		//    coup. On pose les quatre, distingués comme la source les distingue.
		Avancer,	 ///< d'un rang vers l'avant (`Ctrl+]`)
		Reculer,	 ///< d'un rang vers l'arrière (`Ctrl+[`)
		PremierPlan, ///< tout devant (`Ctrl+Maj+]`)
		ArrierePlan, ///< tout derrière (`Ctrl+Maj+[`)
		// ── LES COMPOSANTS DE DOCUMENT (02/09) ──────────────────────────────
		// 🔴 LES DEUX ARRIVENT ENSEMBLE, et c'est la contrainte du modèle §15.4,
		//    pas un choix de commodité : *« créer un composant » sans « détacher »
		//    enferme l'utilisateur dans une décision qu'il ne peut pas défaire.*
		// ⚠️ POSÉS APRÈS le groupe de la profondeur, et pas au milieu : ma
		//    première version les avait glissés entre le commentaire des quatre
		//    gestes de profondeur et les quatre valeurs qu'il décrit — le
		//    commentaire se retrouvait orphelin, à expliquer des valeurs qui ne le
		//    suivaient plus. *Un commentaire séparé de ce qu'il explique se lit
		//    comme une erreur, puis se supprime.*
		ExtraireComposant, ///< la sélection devient une déclaration + une instance (`Ctrl+Alt+K`)
		DetacherComposant, ///< l'instance redevient un sous-arbre ordinaire (`Ctrl+Alt+D`)
		/// Les proprietes de l'instance deviennent celles de sa DECLARATION,
		/// puis la propagation part vers les autres instances (`Ctrl+Alt+M`).
		/// C'est la branche « appliquer a l'original » du dialogue a trois
		/// branches (Q51 R2') ; les deux autres attendent le dialogue.
		AppliquerAuComposant,
		MiroirH, ///< retourne la selection sur l'axe horizontal (`Maj+H`)
		MiroirV, ///< retourne la selection sur l'axe vertical (`Maj+V`)
		Grouper,
		Degrouper,
		CadrerSelection,
		AjouterAgencement,
		Renommer,
		Copier,
		Couper,
		Coller,
		Supprimer,
		NB ///< le compte, lu par la recette — jamais une liste écrite à la main
	};

	/// Une entrée construite : ce qui s'affiche, ce qui agit, et pourquoi pas.
	struct NkEntreeCtx {
			char libelle[80] = {}; ///< le texte final, RAISON INCLUSE quand il y en a une
			const char *raccourci = nullptr;
			const char *raison = nullptr; ///< non vide **si et seulement si** `!agit`
			bool agit = false;
			bool sousMenu = false;
			bool sepApres = false; ///< un trait sous cette entrée (les groupes de Lunacy)
			NkActionCtx action = NkActionCtx::EditerTexte;
	};

	/// L'état qui décide de l'applicabilité. Rien d'autre n'entre ici : ni
	/// pointeur d'interface, ni position de souris.
	struct NkContexteCtx {
			bool aTexte = false;			 ///< le nœud porte une clé `texte`
			/// `forme = text` — un nœud dont la NATURE est le texte. Distinct de
			/// `aTexte` : un rectangle peut porter une clé `texte` sans être un
			/// texte, et c'est lui qu'on ne vectorise pas.
			bool estTexte = false;
			bool estCadre = false;			 ///< `forme = frame` (une page)
			bool aEnfants = false;
			bool pasRacine = false;
			bool pressePapiersPlein = false;
			/// ⚠️ LA SURFACE NE CHANGE PAS LE JEU D'ENTRÉES, seulement deux
			///    applicabilités : dans une LISTE, renommer agit toujours (c'est
			///    le geste natif d'un arbre) et l'édition de texte n'a pas de
			///    point d'insertion. Dans la TOILE, c'est l'inverse.
			bool surfaceListe = false;
			/// Ce nœud est-il DÉJÀ une instance de composant de document ?
			/// ⚠️ IL DÉCIDE LAQUELLE DES DEUX ENTRÉES AGIT, et elles s'excluent :
			///    on n'extrait pas une instance (on forkerait sans le dire), et on
			///    ne détache pas ce qui n'est pas attaché. *Deux entrées toujours
			///    actives auraient laissé l'une des deux ne rien faire en
			///    silence.*
			bool estInstance = false;
	};


	// ═══════════════════════════════════════════════════════════════════════════
	//  LE CLAVIER — LA TROISIÈME PORTE VERS LES MÊMES GESTES
	// ═══════════════════════════════════════════════════════════════════════════
	/// 🔴 CETTE TABLE EXISTE PARCE QUE NOS MENUS MENTAIENT. L'inventaire du
	///    2026-09-01 (document de référence, §10.1) a trouvé que l'application ne
	///    lisait **aucune touche de lettre** — quatre touches en tout : `Échap`,
	///    `Entrée`, `Suppr`, `Retour arrière`. Or nos menus affichent `Ctrl+G`,
	///    `Ctrl+D`, `Ctrl+Maj+G` à côté d'entrées qui, elles, fonctionnent.
	///    *Un libellé qui annonce une capacité absente est un libellé qui ment*,
	///    et il jette le doute sur tous les autres raccourcis du même menu.
	///
	/// ⚠️ AUCUN GESTE N'EST RÉÉCRIT ICI, ET C'EST TOUT L'INTÉRÊT. La table rend
	///    une `NkActionCtx` — la même que le menu contextuel — que
	///    `NkAppliquerActionCtx` exécute. Trois portes (menu de la toile, menu de
	///    la hiérarchie, clavier), **une seule écriture** : c'est la condition
	///    pour qu'une correction les atteigne toutes, et c'était déjà la règle
	///    posée au-dessus du dispatcher.
	///
	/// ⚠️ ET ELLE SE MESURE SANS FENÊTRE. Écrite dans la boucle de touches, elle
	///    aurait vécu là où aucun banc ne va — pour la sixième fois sur ce
	///    chantier. Ici, un cas peut vérifier que `Ctrl+G` groupe et que `G` seul
	///    ne fait rien.
	///
	/// @return l'action, ou `NkActionCtx::NB` quand la combinaison n'est liée à
	///         rien (la valeur sentinelle, jamais une action par défaut : rendre
	///         « Copier » pour une touche inconnue serait pire que ne rien faire).
	/// ⚠️ `alt` EST ARRIVÉ LE 02/09 AVEC LES COMPOSANTS, et ce n'est pas une
	///    commodité : **les quatre gestes de composant de Lunacy passent tous par
	///    `Ctrl+Alt`** (`K` extraire, `D` détacher, `P` états, `E` aller au
	///    principal). Sans ce troisième modificateur il aurait fallu leur inventer
	///    d'autres touches — c'est-à-dire diverger de la source sur le seul
	///    chapitre qui EST la ligne d'arrivée de l'atelier.
	inline NkActionCtx NkActionDuRaccourci(bool ctrl, bool maj, char touche, bool alt = false) {
		// ── LE MIROIR H/V : `Maj+H` / `Maj+V`, SANS Ctrl (Rodolf, 02/09) ───
		// 🔑 LA CONTRADICTION DE LA SOURCE S'EST RESOLUE PAR LA MESURE, pas
		//    par le gout : elle propose `Maj+H`/`Maj+V` a un endroit et
		//    `Ctrl`+fleches a un autre -- or **`Ctrl`+fleches est DEJA PRIS**
		//    chez nous par le redimensionnement au clavier. Une seule des deux
		//    options etait libre : il n'y avait donc rien a arbitrer au gout.
		//    *Quand deux sources se contredisent, regarder ce qui est deja
		//    occupe tranche plus surement qu'une preference.*
		//
		// ⚠️ ET C'EST LA PREMIERE COMBINAISON SANS `Ctrl` DE LA TABLE. La
		//    ligne d'en dessous disait « toutes nos combinaisons passent par
		//    Ctrl » : ce n'est plus vrai, et le taire aurait laisse un
		//    commentaire mensonger au-dessus du code qu'il decrit.
		if (!ctrl && maj && !alt) {
			switch (touche) {
				case 'H': return NkActionCtx::MiroirH;
				case 'V': return NkActionCtx::MiroirV;
				default: return NkActionCtx::NB;
			}
		}
		if (!ctrl)
			return NkActionCtx::NB; // le reste de nos combinaisons passe par Ctrl
		// ── LES GESTES DE COMPOSANT : `Ctrl+Alt` (source `/components`) ─────
		// ⚠️ TESTÉS AVANT LE `switch` GÉNÉRAL, ET L'ORDRE EST LA RÈGLE : `Ctrl+D`
		//    duplique, `Ctrl+Alt+D` détache. La même lettre, deux gestes, et le
		//    plus spécifique se lit en premier — sinon `Ctrl+Alt+D` dupliquerait
		//    en silence au lieu de détacher, et le rapport de défaut qui
		//    remonterait serait « le détachement ne marche pas ».
		if (alt) {
			switch (touche) {
				case 'K': return NkActionCtx::ExtraireComposant;
				case 'D': return NkActionCtx::DetacherComposant;
				case 'M': return NkActionCtx::AppliquerAuComposant;
				default: return NkActionCtx::NB;
			}
		}
		switch (touche) {
			case 'C': return NkActionCtx::Copier;
			case 'X': return NkActionCtx::Couper;
			case 'V': return NkActionCtx::Coller;
			case 'D': return NkActionCtx::Dupliquer;
			// ⚠️ LE MAJ DISTINGUE DEUX GESTES INVERSES sur la même lettre, et
			//    c'est la convention de Lunacy, de Figma et de Sketch. Les poser
			//    sur deux lettres différentes aurait été plus simple à écrire et
			//    plus dur à retenir.
			case 'G': return maj ? NkActionCtx::Degrouper : NkActionCtx::Grouper;
			// ── L'ORDRE DE PROFONDEUR (2026-09-02) ───────────────────────────
			// 🔴 ET IL ÉTAIT BRANCHABLE DEPUIS LE DÉBUT. J'avais annoncé à Rodolf
			//    que `NkGuiKey` n'avait « ni `[` ni `]` » et que ces quatre
			//    raccourcis attendaient le socle. **C'était faux** : `LBracket` et
			//    `RBracket` sont dans l'énumération **et** traduits par
			//    `NkEditorShell` depuis le lot NKCode. Le seul manque était ici,
			//    dans cette table. *J'avais accusé le socle sans aller regarder —
			//    la vérification a coûté un `grep`.*
			// ⚠️ MÊME CONVENTION QUE `G` : `Maj` distingue « d'un cran » de « tout
			//    au bout », sur la même touche. C'est ce que font Lunacy, Figma,
			//    Sketch et Illustrator, et ça évite quatre touches à retenir.
			case ']': return maj ? NkActionCtx::PremierPlan : NkActionCtx::Avancer;
			case '[': return maj ? NkActionCtx::ArrierePlan : NkActionCtx::Reculer;
			default: return NkActionCtx::NB;
		}
	}

	/// Le nombre de combinaisons liées, pour que la recette les parcoure toutes
	/// au lieu d'en citer une liste qui se périme à la première qu'on ajoute.
	inline nkentseu::uint32 NkNbRaccourcisCtx() {
		return 15u;
	}

	enum { kMaxEntreesCtx = 24 };
	struct NkMenuCtx {
			NkEntreeCtx items[kMaxEntreesCtx];
			uint32 n = 0;
	};

	namespace detail {
		inline void NkCopierCtx(char *dst, uint32 cap, const char *a, const char *b) {
			uint32 k = 0;
			for (const char *p = a; p && *p && k + 1 < cap; ++p)
				dst[k++] = *p;
			for (const char *p = b; p && *p && k + 1 < cap; ++p)
				dst[k++] = *p;
			dst[k] = 0;
		}
	} // namespace detail

	/// LE CONSTRUCTEUR. Une seule écriture pour les deux surfaces.
	///
	/// L'ordre et les groupes suivent `lunacy_clicdroit_1_084617.png`, relu à la
	/// source : IA / Coller par-dessus / Copier comme / Dupliquer — trait —
	/// Envoyer derrière — trait — Grouper / Cadrer — trait — Agencement /
	/// Renommer — trait — le presse-papiers et Supprimer.
	///
	/// ⚠️ DEUX ENTRÉES DE LUNACY SONT **VOLONTAIREMENT ABSENTES**, et c'est
	///    écrit ici pour que personne ne les « oublie » par accident : **Crop**
	///    (Ctrl+Entrée) et **Rasterize Selection**. Elles supposent une image
	///    matricielle, que ce document n'a pas — un `.nkuidoc` décrit des nœuds,
	///    pas des pixels. Les poser grisées serait promettre un chantier qui n'a
	///    pas lieu d'être ; les inventer serait pire. *Ce qui n'a pas de sens
	///    chez nous se note, il ne s'ébauche pas.*
	inline void NkConstruireMenuCtx(const NkContexteCtx &c, NkMenuCtx &out) {
		out.n = 0;
		auto ajouter = [&](const char *libelle, const char *raccourci, bool agit, const char *raison,
						   bool sousMenu, bool sepApres, NkActionCtx action) {
			if (out.n >= (uint32)kMaxEntreesCtx)
				return;
			NkEntreeCtx &e = out.items[out.n++];
			// ⚠️ LA RAISON EST DANS LE LIBELLÉ, pas dans une infobulle : une
			//    infobulle demande de survoler, donc de deviner qu'il y a
			//    quelque chose à survoler. Un menu se lit d'un coup d'œil.
			detail::NkCopierCtx(e.libelle, sizeof(e.libelle), libelle, agit ? nullptr : raison);
			e.raccourci = raccourci;
			e.raison = agit ? nullptr : raison;
			e.agit = agit;
			e.sousMenu = sousMenu;
			e.sepApres = sepApres;
			e.action = action;
		};

		// ── 1. Éditer, et c'est notre entrée la plus utilisée ────────────────
		const bool editable = c.aTexte && !c.surfaceListe;
		ajouter("Éditer le texte", nullptr, editable, c.surfaceListe
													  ? " (dans la toile)"
													  : " (pas de texte ici)",
				false, true, NkActionCtx::EditerTexte);

		// ── 2. Le groupe « IA / presse-papiers riche » de Lunacy ─────────────
		ajouter("IA", nullptr, false, " (à construire — le panneau IA porte la génération)", true,
				false, NkActionCtx::IA);
		ajouter("Coller à la même place", nullptr, false, " (à construire)", false, false,
				NkActionCtx::CollerParDessus);
		ajouter("Copier comme", nullptr, false, " (SVG/PNG/JSON/CSS — à construire)", true, false,
				NkActionCtx::CopierComme);
		ajouter("Dupliquer", "Ctrl+D", c.pasRacine, " (pas la racine)", false, true,
				NkActionCtx::Dupliquer);

		// ── 2bis. VECTORISER (retour de Rodolf, 01/09 nuit) ──────────────────
		// *« pour le texte on doit pouvoir le vectoriser en fonction du choix de
		// l'utilisateur, donc clic droit → Vectoriser. »* C'est le « Convert to
		// Outlines » de Figma, le « Convert to path » de Lunacy.
		//
		// 🔴 ELLE EST GRISÉE, ET LA RAISON N'EST PAS CELLE QU'ON ATTENDAIT.
		//    Mesuré à la source avant d'écrire une ligne de conversion :
		//
		//    • les CONTOURS DE GLYPHES EXISTENT et sont atteignables —
		//      `NkFont::GetGlyphOutlinePoints` rend les vrais tracés de la police
		//      (le parseur lit `glyf`, quadratiques et cubiques comprises), et le
		//      chemin depuis l'application est complet :
		//      `costume::Fontes().px13` → `NkGuiFont::Face()` → `NkFont`. Ce
		//      n'est donc PAS « on n'a que des bitmaps » ;
		//    • ce qui manque est **le REMPLISSAGE**. Notre peintre remplit par
		//      `AddConvexPolyFilled`, dont l'en-tête dit lui-même : *« Polygone
		//      CONVEXE plein (éventail depuis le 1er sommet). Non convexe : le
		//      résultat est faux, ce n'est pas vérifié. »* Or presque **toutes**
		//      les lettres sont concaves (L, S, C, E, T…), et beaucoup ont un
		//      TROU (o, a, e, p, b, d, g, R…). Vectoriser aujourd'hui rendrait
		//      des taches à la place des lettres.
		//
		// ⚠️ *Une capacité annoncée qui rend faux est pire que son absence.* On ne
		//    livre donc pas la conversion — mais on pose l'entrée, parce qu'une
		//    entrée grisée qui NOMME le maillon manquant vaut mieux qu'un menu
		//    silencieux : Rodolf a demandé ce geste, il doit voir qu'il est vu.
		//
		// 📌 ET LE MAILLON EXISTE DÉJÀ DANS LE DÉPÔT, UN MODULE PLUS LOIN :
		//    `NKFont/NkEarcut.h` + `NkFontMesh.cpp` triangulent des contours
		//    quelconques ET classent les trous par profondeur d'imbrication —
		//    exactement ce qu'il faut. Ils servent aujourd'hui aux maillages de
		//    texte 3D. Le travail n'est donc pas « écrire un triangulateur »,
		//    c'est **le faire descendre sous le peintre** — un ajout additif au
		//    socle, à décider comme tel.
		ajouter("Vectoriser", nullptr, false,
				" (le peintre ne remplit que des polygones convexes — voir le rapport)", false,
				true, NkActionCtx::Vectoriser);

		// ── 3. L'ordre de profondeur — un groupe à lui seul chez Lunacy ──────
		// ✅ LIVRÉ (vague 2). L'entrée unique « Envoyer derrière » était grisée et
		//    disait « à construire » — elle était surtout un QUART du sujet : chez
		//    Lunacy ce sont quatre gestes, et « d'un cran » n'est pas « tout au
		//    fond ».
		// 📌 ET LE MÉCANISME ÉTAIT DÉJÀ PORTÉ, UNE COUCHE PLUS BAS :
		//    `NkUIDocument::MoveChild` écrit un RANG dans la fratrie, exactement
		//    ce qu'il faut, et la Hiérarchie s'en sert depuis le glisser dans la
		//    liste. *La porte du dépôt a répondu « ça existe » pour la deuxième
		//    fois cette semaine* — le travail n'était pas d'écrire un
		//    réordonnancement, c'était de lui donner ses quatre portes.
		// ⚠️ LE RANG LE PLUS GRAND EST DEVANT, et ce n'est pas une convention
		//    choisie : `NkDrawDocument` parcourt `children` en ordre CROISSANT,
		//    donc le dernier peint recouvre. La table ci-dessous suit le dessin ;
		//    l'inverser aurait donné quatre entrées qui font le contraire de leur
		//    nom, sans qu'aucun code ne proteste.
		ajouter("Mettre au premier plan", "Ctrl+Maj+]", c.pasRacine, " (pas la racine)", false,
				false, NkActionCtx::PremierPlan);
		ajouter("Avancer d'un rang", "Ctrl+]", c.pasRacine, " (pas la racine)", false, false,
				NkActionCtx::Avancer);
		ajouter("Reculer d'un rang", "Ctrl+[", c.pasRacine, " (pas la racine)", false, false,
				NkActionCtx::Reculer);
		ajouter("Envoyer à l'arrière-plan", "Ctrl+Maj+[", c.pasRacine, " (pas la racine)", false,
				true, NkActionCtx::ArrierePlan);

		// ── 4. Grouper / dégrouper / cadrer ──────────────────────────────────
		ajouter("Grouper la sélection", "Ctrl+G", c.pasRacine, " (pas la racine)", false, false,
				NkActionCtx::Grouper);
		ajouter("Dégrouper", "Ctrl+Maj+G", c.pasRacine && c.aEnfants && !c.estCadre,
				" (pas un groupe)", false, false, NkActionCtx::Degrouper);
		ajouter("Cadrer la sélection", "Ctrl+Alt+G", false, " (à construire)", false, true,
				NkActionCtx::CadrerSelection);

		// ── 4bis. LES COMPOSANTS DE DOCUMENT — LA LIGNE D'ARRIVÉE ────────────
		// 🔴 C'EST LE GESTE QUE TOUT LE CHANTIER SERT : *« nkuidesign qui me
		//    permettra de designer nos premiers composants »*. Le modèle et les
		//    deux opérations étaient tenus par des cas depuis ce matin — mais
		//    **un geste qui n'existe pas ne devient pas vrai parce que la
		//    fonction dessous est éprouvée.** Voici sa porte.
		// ⚠️ LES DEUX S'EXCLUENT, ET CHACUNE DIT POURQUOI QUAND ELLE NE PEUT PAS :
		//    on n'extrait pas une instance (on forkerait sans le dire), et on ne
		//    détache pas ce qui n'est pas attaché. Deux entrées toujours actives
		//    auraient laissé l'une des deux ne rien faire en silence.
		ajouter("Extraire en composant", "Ctrl+Alt+K", c.pasRacine && !c.estInstance,
				c.estInstance ? " (déjà une instance)" : " (pas la racine)", false, false,
				NkActionCtx::ExtraireComposant);
		ajouter("Détacher l'instance", "Ctrl+Alt+D", c.estInstance, " (pas une instance)", false,
				true, NkActionCtx::DetacherComposant);
		// ⚠️ LA RAISON DIT « pas une instance », PAS « impossible » : une
		//    entrée grisée sans motif se lit comme une panne.
		ajouter("Appliquer au composant", "Ctrl+Alt+M", c.estInstance, " (pas une instance)",
				false, false, NkActionCtx::AppliquerAuComposant);
		ajouter("Miroir horizontal", "Maj+H", c.pasRacine, " (pas la racine)", false, false,
				NkActionCtx::MiroirH);
		ajouter("Miroir vertical", "Maj+V", c.pasRacine, " (pas la racine)", false, true,
				NkActionCtx::MiroirV);

		// ── 5. Agencement et renommage ───────────────────────────────────────
		// ⚠️ L'AGENCEMENT EXISTE, mais dans l'Inspecteur : la raison DIT OÙ, au
		//    lieu de dire seulement « non ». Une raison qui indique la porte
		//    vaut mieux qu'une raison qui la ferme.
		ajouter("Ajouter un agencement", "Maj+A", false,
				" (par l'Inspecteur, section Disposition)", false, false,
				NkActionCtx::AjouterAgencement);
		// Renommer : natif dans une LISTE, réservé aux pages dans la toile (une
		// étiquette d'artboard est la seule chose qui porte un nom visible là).
		ajouter("Renommer", "F2", c.surfaceListe ? c.pasRacine : c.estCadre,
				c.surfaceListe ? " (pas la racine)" : " (par la Hiérarchie)", false, true,
				NkActionCtx::Renommer);

		// ── 6. Le presse-papiers, et la suppression en dernier ───────────────
		ajouter("Copier", "Ctrl+C", c.pasRacine, " (pas la racine)", false, false,
				NkActionCtx::Copier);
		ajouter("Couper", "Ctrl+X", c.pasRacine, " (pas la racine)", false, false,
				NkActionCtx::Couper);
		ajouter("Coller", "Ctrl+V", c.pressePapiersPlein, " (presse-papiers vide)", false, false,
				NkActionCtx::Coller);
		ajouter("Supprimer", "Suppr", c.pasRacine, " (pas la racine)", false, false,
				NkActionCtx::Supprimer);
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  LE MENU DU CLIC DROIT **DANS LE VIDE** — un menu de VUE, pas d'objet
	// ═══════════════════════════════════════════════════════════════════════════
	/// Retour de Rodolf, 01/09, avec sa référence `lunacy_clicdroit_vide_104843`.
	/// Signalé en Q43 comme « un petit chantier à part » : le clic droit dans le
	/// vide **ne faisait rien et ne disait rien** (mesuré : `vise < 0` → aucun
	/// menu, aucun message). Même famille que le double-clic muet du matin.
	///
	/// ⚠️ CE N'EST PAS LE MÊME MENU AVEC DES ENTRÉES GRISÉES, ET C'EST LA
	///    DÉCISION DE CONCEPTION. On aurait pu réutiliser `NkConstruireMenuCtx`
	///    avec un contexte vide : *« Copier (pas la racine) »*, *« Grouper (pas la
	///    racine) »*, quatorze entrées dont zéro n'agit. Ce serait un menu qui
	///    parle de l'objet qu'on n'a pas cliqué. **Le vide de la toile n'est pas
	///    un objet sans propriétés : c'est LA VUE**, et ses commandes sont d'une
	///    autre nature — ce qu'on affiche, ce qui aimante. La capture de Lunacy
	///    le dit sans ambiguïté : pas une seule de ses entrées ne parle d'un
	///    calque.
	///
	/// ⚠️ CE QUI RESTE COMMUN EST LA **RÈGLE**, pas le contenu : qui n'agit pas
	///    porte sa raison, qui agit n'en porte pas. Elle est tenue par le même
	///    genre de cas, sur les deux menus — c'est ça, traiter les chemins frères
	///    comme un groupe : partager l'invariant sans forcer le contenu.
	enum class NkActionVide {
		CollerIci,
		ModePresentation,
		GrillePixels,
		ReperesLayout,
		Regles,
		Tranches,
		Prototypage,
		PixelsAuZoom,
		AimanterGrille,
		AimanterCalques,
		NB ///< le compte, lu par la recette
	};

	/// Une entrée du menu de vue. `bascule` = elle porte une COCHE.
	struct NkEntreeVide {
			char libelle[112] = {};
			const char *raccourci = nullptr;
			const char *raison = nullptr; ///< non vide **si et seulement si** `!agit`
			bool agit = false;
			bool bascule = false; ///< affiche une coche
			bool coche = false;	  ///< l'état RÉEL, jamais une décoration
			bool sepApres = false;
			NkActionVide action = NkActionVide::CollerIci;
	};

	/// L'état de la VUE qui décide. Rien d'autre : ni souris, ni sélection.
	struct NkContexteVide {
			bool pressePapiersPlein = false;
			bool grilleVisible = true;	///< la grille de points de la toile
			bool aimantCalques = true;	///< `aimantActif` — bords, centres, espacements
	};

	enum { kMaxEntreesVide = 16 };
	struct NkMenuVide {
			NkEntreeVide items[kMaxEntreesVide];
			uint32 n = 0;
	};

	/// L'ordre et les groupes suivent `lunacy_clicdroit_vide_104843.png`, relu au
	/// pixel : Paste Here / Presentation Mode — trait — Pixel Grid / Layout /
	/// Rulers / Slices / Prototyping / Pixels on Zoom — trait — Snap to Pixel
	/// Grid / Snap to Layers.
	///
	/// ⚠️ **DEUX ENTRÉES SUR DIX AGISSENT, ET C'EST DIT PLUTÔT QUE MAQUILLÉ.** La
	///    grille de points existe (elle est peinte depuis août) et l'aimantation
	///    aux calques existe (livrée en Q42) : ces deux-là basculent pour de bon,
	///    et leur coche lit l'état RÉEL. Les huit autres portent leur raison.
	///    Poser dix coches décoratives aurait été plus joli et faux — c'est
	///    exactement le défaut « une case toujours cochée à côté d'un aimant qui
	///    ne fait rien » que Q42 a trouvé et corrigé sur le menu Affichage.
	inline void NkConstruireMenuVide(const NkContexteVide &c, NkMenuVide &out) {
		out.n = 0;
		auto ajouter = [&](const char *libelle, const char *raccourci, bool agit,
						   const char *raison, bool bascule, bool coche, bool sepApres,
						   NkActionVide action) {
			if (out.n >= (uint32)kMaxEntreesVide)
				return;
			NkEntreeVide &e = out.items[out.n++];
			detail::NkCopierCtx(e.libelle, sizeof(e.libelle), libelle, agit ? nullptr : raison);
			e.raccourci = raccourci;
			e.raison = agit ? nullptr : raison;
			e.agit = agit;
			e.bascule = bascule;
			// ⚠️ UNE COCHE SUR UNE ENTRÉE QUI N'AGIT PAS EST TOUJOURS FAUSSE, quel
			//    qu'ait été l'argument passé : elle affirmerait un état que rien
			//    ne porte. La règle est appliquée ICI, une fois, plutôt qu'à
			//    chaque appel — sinon le premier appelant distrait la casserait.
			e.coche = agit && coche;
			e.sepApres = sepApres;
			e.action = action;
		};

		// ── 1. Coller ici, et le mode présentation ───────────────────────────
		ajouter("Coller ici", nullptr, c.pressePapiersPlein, " (presse-papiers vide)", false,
				false, false, NkActionVide::CollerIci);
		ajouter("Mode présentation", "Ctrl+.", false, " (à construire)", false, false, true,
				NkActionVide::ModePresentation);

		// ── 2. Ce qu'on AFFICHE ──────────────────────────────────────────────
		ajouter("Grille de points", nullptr, true, nullptr, true, c.grilleVisible, false,
				NkActionVide::GrillePixels);
		ajouter("Repères de mise en page", "Maj+G", false, " (à construire)", true, false, false,
				NkActionVide::ReperesLayout);
		ajouter("Règles", "Ctrl+R", false, " (à construire)", true, false, false,
				NkActionVide::Regles);
		ajouter("Tranches d'export", nullptr, false, " (à construire — l'export n'existe pas)",
				true, false, false, NkActionVide::Tranches);
		// ⚠️ LA RAISON DIT OÙ, pas seulement « non » — notre prototypage existe,
		//    il est ailleurs. Une raison qui indique la porte vaut mieux qu'une
		//    raison qui la ferme (la même règle que « Ajouter un agencement »).
		ajouter("Prototypage", nullptr, false, " (par l'onglet Behavior)", true, false, false,
				NkActionVide::Prototypage);
		ajouter("Pixels au zoom", nullptr, false, " (à construire)", true, false, true,
				NkActionVide::PixelsAuZoom);

		// ── 3. Ce qui AIMANTE ────────────────────────────────────────────────
		ajouter("Aimanter à la grille", nullptr, false,
				" (à construire — la grille est un décor, pas un repère)", true, false, false,
				NkActionVide::AimanterGrille);
		ajouter("Aimanter aux calques", nullptr, true, nullptr, true, c.aimantCalques, false,
				NkActionVide::AimanterCalques);
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  LA RANGÉE D'ICÔNES — les gestes fréquents en accès direct (Lunacy)
	// ═══════════════════════════════════════════════════════════════════════════
	/// ⚠️ SEPT ICÔNES, RELUES SUR LA RÉFÉRENCE ZOOMÉE : coller, dupliquer,
	///    couper, cadenas, œil, poubelle, composant. **Trois n'ont aucun champ
	///    dans le modèle** (verrouiller, masquer, créer un composant) : elles
	///    sont **présentes et grisées**, et leur infobulle DIT pourquoi. Les
	///    retirer aurait été plus simple et faux — l'utilisateur qui connaît
	///    Lunacy les cherche, et une absence ne se justifie pas toute seule.
	enum class NkIconeCtx {
		Coller,
		Dupliquer,
		Couper,
		Verrouiller,
		Masquer,
		Supprimer,
		CreerComposant,
		NB
	};

	/// L'infobulle. **Jamais vide**, dans les deux états — c'est ce que la
	/// recette vérifie. Une icône muette est un bouton qu'on n'ose pas presser.
	inline const char *NkInfobulleIconeCtx(NkIconeCtx i, bool agit) {
		switch (i) {
			case NkIconeCtx::Coller:
				return agit ? "Coller (Ctrl+V)" : "Coller — le presse-papiers est vide";
			case NkIconeCtx::Dupliquer:
				return agit ? "Dupliquer (Ctrl+D)" : "Dupliquer — rien de duplicable ici";
			case NkIconeCtx::Couper:
				return agit ? "Couper (Ctrl+X)" : "Couper — rien à couper ici";
			case NkIconeCtx::Verrouiller:
				return agit ? "Verrouiller" : "Verrouiller — aucun verrou dans le modèle (à construire)";
			case NkIconeCtx::Masquer:
				return agit ? "Masquer" : "Masquer — aucune visibilité par nœud dans le modèle (à construire)";
			case NkIconeCtx::Supprimer:
				return agit ? "Supprimer (Suppr)" : "Supprimer — pas la racine";
			default:
				return agit ? "Créer un composant" : "Créer un composant — le registre ne reçoit pas encore (à construire)";
		}
	}

	/// Ce qu'une icône déclenche quand elle agit — `NkActionCtx::NB` = aucune,
	/// et c'est précisément le cas des trois qui n'existent pas encore.
	inline NkActionCtx NkActionIconeCtx(NkIconeCtx i) {
		switch (i) {
			case NkIconeCtx::Coller: return NkActionCtx::Coller;
			case NkIconeCtx::Dupliquer: return NkActionCtx::Dupliquer;
			case NkIconeCtx::Couper: return NkActionCtx::Couper;
			case NkIconeCtx::Supprimer: return NkActionCtx::Supprimer;
			default: return NkActionCtx::NB;
		}
	}

	/// L'applicabilité d'une icône, dans le même contexte que le menu.
	inline bool NkIconeCtxAgit(NkIconeCtx i, const NkContexteCtx &c) {
		switch (i) {
			case NkIconeCtx::Coller: return c.pressePapiersPlein;
			case NkIconeCtx::Dupliquer:
			case NkIconeCtx::Couper:
			case NkIconeCtx::Supprimer: return c.pasRacine;
			default: return false; // verrou, visibilité, registre : rien dans le modèle
		}
	}

} // namespace nkuidesign
