#pragma once
// -----------------------------------------------------------------------------
// @File    Sommets.h
// @Brief   LES SOMMETS D'UNE FORME — la table unique du PEINTRE et du MODE POINTS.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ CE FICHIER EXISTE AVANT LE DÉFAUT QU'IL EMPÊCHE
// =============================================================================
//  Le mode points doit poser une poignée SUR CHAQUE SOMMET DESSINÉ. Or les
//  sommets étaient calculés *dans le peintre* (`Renderers.h`, tables `kTri`,
//  `kPenta`, `kEtoile`, locales à la fonction de dessin). Les relire là où on
//  en a besoin aurait voulu dire les RECOPIER — et deux tables de sommets, ce
//  sont des poignées qui dérivent du dessin au premier ajustement d'une
//  étoile : l'utilisateur tirerait un coin qui n'est pas là.
//
//  C'est le motif de la porte du 28/08 (« quels chemins frères partagent le
//  même danger »), et c'est la **quatrième fois de la semaine** qu'il se
//  présente sur ce chantier. On l'écrit donc une fois, ici, AVANT d'écrire le
//  second consommateur — pas après l'avoir vu mordre.
//
// =============================================================================
//  CE QU'UN SOMMET VEUT DIRE N'EST PAS LE MÊME SELON LA FORME
// =============================================================================
//  ⚠️ ET C'EST LA VRAIE DIFFICULTÉ, PAS LE CALCUL. Trois natures :
//
//    • `Bouts`    — la LIGNE : ses deux extrémités. Les déplacer change la
//                   BOÎTE (une ligne est la diagonale de son rectangle) : c'est
//                   exprimable avec le modèle d'aujourd'hui, sans rien ajouter.
//    • `Polygone` — triangle, pentagone, étoile : des sommets RÉELS. Les
//                   déplacer demande de les STOCKER (une étoile dont on bouge
//                   une pointe n'est plus l'étoile régulière de la table).
//    • `Coins`    — rect, ellipse, image, avatar, cadre : leurs « sommets » ne
//                   sont pas des sommets, ce sont les COINS DE LA BOÎTE. Les
//                   tirer REDIMENSIONNE. Lunacy le fait aussi, et l'outil doit
//                   le DIRE au lieu de laisser croire à une édition vectorielle.
//
//  Confondre les trois donnerait une interface qui a l'air de tout savoir faire
//  et qui ment sur deux cas sur trois.
// -----------------------------------------------------------------------------
#include "Document.h"

namespace nkuidesign {

	using nkentseu::float32;
	using nkentseu::int32;
	using nkentseu::uint32;

	/// LA LONGUEUR D'UN VECTEUR, par Newton — quatre itérations depuis une
	/// estimation grossière.
	/// ⚠️ ÉCRITE ICI PLUTÔT QU'EMPRUNTÉE À `<math.h>` parce que le dépôt est
	///    zero-STL et que ce fichier n'a aucune autre dépendance : lui en donner
	///    une pour une racine carrée aurait été le prix le plus mal placé du
	///    chantier. Elle ne sert qu'à BORNER un rayon d'arrondi — une erreur de
	///    l'ordre du millième y est invisible.
	inline float32 NkLongueur2D(float32 dx, float32 dy) {
		const float32 c = dx * dx + dy * dy;
		if (c <= 0.f)
			return 0.f;
		float32 r = c > 1.f ? c * 0.5f : 1.f;
		for (uint32 i = 0; i < 12; ++i)
			r = 0.5f * (r + c / r);
		return r;
	}

	/// Ce que les sommets d'une forme VEULENT DIRE.
	enum class NkNatureSommets {
		Aucun,			///< rien à éditer (un texte n'a pas de sommets)
		Bouts,			///< les deux extrémités d'une ligne
		Polygone,		///< des sommets réels, déplaçables un à un
		CoinsEditables, ///< rect / ellipse : les coins SONT des sommets réels
		Coins			///< les coins de la boîte : les tirer REDIMENSIONNE
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  ⚠️ POURQUOI `CoinsEditables` EXISTE — LA MESURE QUI L'A IMPOSÉ (01/09)
	// ═══════════════════════════════════════════════════════════════════════════
	//  Retour de Rodolf : *« double-cliquer sur une shape ne permet pas encore de
	//  faire l'édition de la shape. »* Mesure de la table, forme par forme :
	//  `rect` et `ellipse` rendaient `Coins`, donc `CoinsSeuls`, donc la phrase
	//  *« cette forme n'a pas de sommets »* — et le mode ne s'ouvrait pas. Or ce
	//  sont **les deux formes qu'on pose le plus**, et ce sont exactement celles
	//  de sa référence : `lunacy_shape_edit_1_105026.png` montre un **Rectangle**
	//  en `EDIT SHAPE`, avec ses **quatre ancres** ; la capture 2 en déforme une
	//  en courbe, la capture 3 **ajoute une cinquième ancre** au milieu d'un côté.
	//
	//  ⚠️ ET CE N'ÉTAIT PAS UN OUBLI : la note d'origine disait *« ouvrir un mode
	//     points qui ne ferait que redimensionner laisserait croire à une édition
	//     vectorielle qui n'existe pas. »* **Le raisonnement était juste, sa
	//     prémisse était fausse** — la sortie n'est pas « des coins qui
	//     redimensionnent », c'est **un tracé de quatre sommets réels**. Une fois
	//     matérialisé, le rectangle EST un polygone : le peintre le dessine par la
	//     même liste de points que le triangle, et le déplacer déforme vraiment.
	//
	//  ⚠️ CE QUI RESTE `Coins`, ET LA RAISON SE DIT : `image`, `avatar`, `frame`.
	//     Un artboard qu'on déformerait en quadrilatère ne veut rien dire (il
	//     porte une CIBLE et une zone sûre) ; un cadre d'image non plus tant que
	//     la source n'existe pas. *Ce qui n'a pas de sens chez nous se note, il ne
	//     s'ébauche pas.*
	inline NkNatureSommets NkNatureDe(const char *shape) {
		if (!shape || !*shape)
			return NkNatureSommets::Coins;
		if (NkComponentDecl::StrEq(shape, "line") || NkComponentDecl::StrEq(shape, "line_up"))
			return NkNatureSommets::Bouts;
		if (NkComponentDecl::StrEq(shape, "triangle") || NkComponentDecl::StrEq(shape, "pentagone")
			|| NkComponentDecl::StrEq(shape, "etoile"))
			return NkNatureSommets::Polygone;
		if (NkComponentDecl::StrEq(shape, "rect") || NkComponentDecl::StrEq(shape, "ellipse"))
			return NkNatureSommets::CoinsEditables;
		if (NkComponentDecl::StrEq(shape, "text"))
			return NkNatureSommets::Aucun;
		return NkNatureSommets::Coins;
	}

	/// Une forme dont les sommets se STOCKENT (par opposition à la ligne, qui
	/// s'exprime par sa boîte, et aux boîtes qui ne s'éditent pas).
	/// ⚠️ CE PRÉDICAT EXISTE POUR QUE LA QUESTION NE SE REPOSE PAS À CHAQUE SITE.
	///    Depuis que `CoinsEditables` a rejoint `Polygone`, **six** endroits
	///    demandent « est-ce que j'écris dans `n.sommets` ? » — écrit six fois,
	///    le septième aurait oublié le rect, et l'édition aurait marché partout
	///    sauf sur la forme la plus courante.
	inline bool NkSommetsStockes(NkNatureSommets nat) {
		return nat == NkNatureSommets::Polygone || nat == NkNatureSommets::CoinsEditables;
	}

	/// Les sommets UNITAIRES d'un polygone régulier (cercle inscrit, pointe en
	/// haut), en coordonnées -1..1. Rend le nombre de sommets, 0 si la forme
	/// n'est pas un polygone.
	/// ⚠️ PRÉCALCULÉS : pas de trigonométrie à l'exécution (la raison d'origine
	///    du peintre, conservée telle quelle en déménageant).
	inline uint32 NkSommetsUnitaires(const char *shape, const float32 *&out) {
		static const float32 kTri[6] = {0.f, -1.f, 1.f, 1.f, -1.f, 1.f};
		static const float32 kPenta[10] = {0.f,	   -1.f,   .9511f, -.3090f, .5878f,
										   .8090f, -.5878f, .8090f, -.9511f, -.3090f};
		static const float32 kEtoile[20] = {
			0.f,	 -1.f,	  .2246f,  -.3090f, .9511f,	 -.3090f, .3633f,  .1180f,
			.5878f,	 .8090f,  0.f,	   .3820f,	-.5878f, .8090f,  -.3633f, .1180f,
			-.9511f, -.3090f, -.2246f, -.3090f};
		if (NkComponentDecl::StrEq(shape, "triangle")) {
			out = kTri;
			return 3;
		}
		if (NkComponentDecl::StrEq(shape, "pentagone")) {
			out = kPenta;
			return 5;
		}
		if (NkComponentDecl::StrEq(shape, "etoile")) {
			out = kEtoile;
			return 10;
		}
		// LES QUATRE COINS D'UNE BOÎTE, dans le MÊME repère unitaire et dans le
		// MÊME sens horaire que les polygones ci-dessus.
		// ⚠️ ILS PASSENT PAR CETTE FONCTION PLUTÔT QUE PAR UN COIN DE CODE À PART
		//    pour que `NkMaterialiserSommets` n'ait rien à savoir de la forme : une
		//    seule table, un seul matérialisateur, un seul dessinateur. Écrits
		//    ailleurs, ils auraient donné un rect dont les poignées ne tombent pas
		//    sur les coins peints — le défaut exact que ce fichier existe pour
		//    empêcher.
		static const float32 kBoite[8] = {-1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f};
		if (NkComponentDecl::StrEq(shape, "rect")) {
			out = kBoite;
			return 4;
		}
		// ── L'ELLIPSE : DOUZE POINTS **SUR LA COURBE**, pas quatre aux coins ──
		// ⚠️ RETOUR DE RODOLF, 01/09 : *« en plus collé sur la forme, donc épouser
		//    la forme »*. L'ellipse partageait la table du rectangle : ses
		//    « sommets » étaient les quatre COINS DE SA BOÎTE — quatre points qui
		//    ne touchent l'ellipse **nulle part**, posés dans le vide de ses
		//    angles. On tirait un point qui n'est pas sur la forme.
		//
		// ⚠️ ET DOUZE PLUTÔT QUE QUATRE, POUR UNE RAISON QUI SE VOIT : quatre
		//    points cardinaux sont bien *sur* la courbe, mais dès qu'on en déplace
		//    un, le tracé peint devient un LOSANGE — l'ellipse disparaîtrait au
		//    premier geste d'édition. Lunacy garde quatre ancres parce qu'il a des
		//    poignées de Bézier pour tenir la rondeur entre elles ; **nous ne les
		//    avons pas encore** (nommées, non ébauchées). Douze segments
		//    approchent l'ellipse à ~1 % de son rayon : elle reste ronde, et
		//    déplacer un point la déforme *localement*, ce qui est le geste
		//    demandé. *On diverge de Lunacy sur le nombre parce qu'on n'a pas sa
		//    courbe — et on l'écrit, plutôt que de rendre un losange en silence.*
		static const float32 kEllipse[24] = {
			0.f,	 -1.f,	 .5f,	  -.8660f, .8660f,	-.5f,	 1.f,	  0.f,
			.8660f,	 .5f,	 .5f,	  .8660f,  0.f,		1.f,	 -.5f,	  .8660f,
			-.8660f, .5f,	 -1.f,	  0.f,	   -.8660f, -.5f,	 -.5f,	  -.8660f};
		if (NkComponentDecl::StrEq(shape, "ellipse")) {
			out = kEllipse;
			return 12;
		}
		out = nullptr;
		return 0;
	}

	/// LES SOMMETS D'UN NŒUD, dans le rectangle `r` (écran ou document, selon ce
	/// qu'on lui donne — la fonction ne s'en mêle pas).
	/// ⚠️ LA LISTE DU DOCUMENT PRIME SUR LA TABLE. `n.sommets` vide = le polygone
	///    RÉGULIER de la table ; non vide = les sommets que la main a déplacés.
	///    C'est exactement la discipline des remplissages : la forme simple est
	///    « la liste par défaut », et le fichier ne gagne une clé que le jour où
	///    quelqu'un s'en sert.
	/// @param xy tableau de sortie, 2 floats par sommet.
	/// @return le nombre de sommets écrits.
	inline uint32 NkSommetsDe(const NkUINode &n, const NkPaintRect &r, float32 *xy, uint32 cap) {
		const char *shape = n.shape.Data();
		const NkNatureSommets nat = NkNatureDe(shape);
		const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
		const float32 hx = r.w * 0.5f, hy = r.h * 0.5f;
		if (nat == NkNatureSommets::Bouts) {
			if (cap < 2)
				return 0;
			// `line` descend, `line_up` monte : la diagonale de la boîte.
			const bool monte = NkComponentDecl::StrEq(shape, "line_up");
			xy[0] = r.x;
			xy[1] = monte ? r.y + r.h : r.y;
			xy[2] = r.x + r.w;
			xy[3] = monte ? r.y : r.y + r.h;
			return 2;
		}
		if (NkSommetsStockes(nat)) {
			if (!n.sommets.Empty()) {
				// LES SOMMETS DU DOCUMENT (déplacés à la main), en unitaire.
				uint32 k = 0;
				for (uint32 i = 0; i < (uint32)n.sommets.Size() && k + 1 < cap; ++i) {
					xy[k * 2] = cx + n.sommets[i].x * hx;
					xy[k * 2 + 1] = cy + n.sommets[i].y * hy;
					++k;
				}
				return k;
			}
			const float32 *unit = nullptr;
			const uint32 nb = NkSommetsUnitaires(shape, unit);
			if (!unit || nb > cap)
				return 0;
			for (uint32 i = 0; i < nb; ++i) {
				xy[i * 2] = cx + unit[i * 2] * hx;
				xy[i * 2 + 1] = cy + unit[i * 2 + 1] * hy;
			}
			return nb;
		}
		if (nat == NkNatureSommets::Coins) {
			if (cap < 4)
				return 0;
			xy[0] = r.x;
			xy[1] = r.y;
			xy[2] = r.x + r.w;
			xy[3] = r.y;
			xy[4] = r.x + r.w;
			xy[5] = r.y + r.h;
			xy[6] = r.x;
			xy[7] = r.y + r.h;
			return 4;
		}
		return 0;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  LIRE UN SOMMET SANS RIEN ÉCRIRE — ET C'EST UNE RÈGLE, PAS UN CONFORT
	// ═══════════════════════════════════════════════════════════════════════════
	/// ⚠️ CES DEUX FONCTIONS EXISTENT PARCE QUE LA SECTION « ÉDITION DE FORME »
	///    DE L'INSPECTEUR A FAILLI ÉCRIRE DANS LE DOCUMENT RIEN QU'EN S'AFFICHANT.
	///    Sa première version appelait `NkMaterialiserSommets` pour lire — ce qui
	///    ajoute la liste `sommets` au nœud, change les octets du fichier et
	///    marque le document modifié **alors que l'utilisateur n'a fait
	///    qu'ouvrir un panneau**. Et le volet CONSERVATION du mode points
	///    (cas 22) ne l'aurait pas vu : il n'exécute pas l'Inspecteur.
	///
	///    *Regarder n'écrit pas.* La matérialisation reste au moment de
	///    l'ÉCRITURE — le glisser d'un sommet, l'ajout, l'arrondi — exactement
	///    comme la toile le fait déjà.
	/// @return le nombre de sommets, liste stockée ou table régulière.
	inline uint32 NkNbSommetsDe(const NkUINode &n) {
		if (!n.sommets.Empty())
			return (uint32)n.sommets.Size();
		if (!NkSommetsStockes(NkNatureDe(n.shape.Data())))
			return 0;
		const float32 *unit = nullptr;
		return NkSommetsUnitaires(n.shape.Data(), unit);
	}

	/// Le sommet `i` en coordonnées UNITAIRES (-1..1) et son rayon, sans écrire.
	inline bool NkLireSommet(const NkUINode &n, uint32 i, float32 &x, float32 &y,
							 float32 &rayon) {
		if (!n.sommets.Empty()) {
			if (i >= (uint32)n.sommets.Size())
				return false;
			x = n.sommets[i].x;
			y = n.sommets[i].y;
			rayon = n.sommets[i].rayon;
			return true;
		}
		const float32 *unit = nullptr;
		const uint32 nb = NkSommetsUnitaires(n.shape.Data(), unit);
		if (!unit || i >= nb)
			return false;
		x = unit[i * 2];
		y = unit[i * 2 + 1];
		rayon = 0.f; // la table régulière est VIVE par définition
		return true;
	}

	/// MATÉRIALISER les sommets d'un polygone : la table régulière devient une
	/// liste que la main peut modifier. Idempotent, et sans effet sur une forme
	/// qui n'est pas un polygone.
	/// ⚠️ MÊME RÈGLE QUE `MaterialiserFills` : on ne matérialise que s'il y a
	///    quelque chose à préserver — ici, la table du polygone régulier.
	inline void NkMaterialiserSommets(NkUINode &n) {
		if (!n.sommets.Empty())
			return;
		if (!NkSommetsStockes(NkNatureDe(n.shape.Data())))
			return;
		const float32 *unit = nullptr;
		const uint32 nb = NkSommetsUnitaires(n.shape.Data(), unit);
		for (uint32 i = 0; i < nb; ++i)
			n.sommets.PushBack(NkPoint2{unit[i * 2], unit[i * 2 + 1], 0.f});
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  DÉPLACER LES SOMMETS MARQUÉS — UN ÉCART, JAMAIS UNE POSITION
	// ═══════════════════════════════════════════════════════════════════════════
	/// Retour de Rodolf, 01/09 (soir) : *« on doit pouvoir sélectionner plusieurs
	/// vertices pour déplacement ou transformation simultanés. »*
	///
	/// ⚠️ ÉCRIT ICI, PAS DANS LA BOUCLE DE RENDU, et c'est la quatrième fois que
	///    ce chantier paie la même facture (le zoom en Q41, l'aimantation en Q42,
	///    le tracé et la liste des sections aujourd'hui). Un calcul écrit dans
	///    `OnUI` vit là où aucun banc ne va, et « les sommets bougent ensemble »
	///    redevient une opinion.
	///
	/// ⚠️ ET LA VRAIE DIFFICULTÉ N'EST PAS LA BOUCLE, C'EST L'ÉCART. Poser chaque
	///    sommet marqué SOUS la souris les ferait tous **se superposer au premier
	///    pixel du geste** — la forme s'effondrerait en un point. On applique donc
	///    le déplacement du sommet TIRÉ à tous les autres. *Une grandeur juste,
	///    réutilisée là où sa définition ne vaut plus* — exactement la faute que
	///    la rotation avait déjà payée avec son angle cumulé (Q44, C).
	///
	/// @param tire    le sommet sous la main (il reçoit `nx, ny` exactement)
	/// @param marques masque des sommets sélectionnés (bit i = sommet i)
	/// @param nx,ny   la position UNITAIRE demandée pour le sommet tiré
	/// @return le nombre de sommets déplacés.
	inline uint32 NkDeplacerSommetsMarques(NkUINode &n, int32 tire, nkentseu::uint64 marques,
										   float32 nx, float32 ny) {
		if (tire < 0 || tire >= (int32)n.sommets.Size())
			return 0;
		const float32 dx = nx - n.sommets[(uint32)tire].x;
		const float32 dy = ny - n.sommets[(uint32)tire].y;
		const uint32 nb = (uint32)n.sommets.Size();
		uint32 bouges = 0;
		for (uint32 k = 0; k < nb; ++k) {
			const bool marque = k < 64u && (marques & (1ull << k)) != 0ull;
			if ((int32)k != tire && !marque)
				continue;
			n.sommets[k].x += dx;
			n.sommets[k].y += dy;
			++bouges;
		}
		return bouges;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  RECADRER LA BOÎTE SUR LE TRACÉ — ET RENORMALISER, SINON LA FORME SAUTE
	// ═══════════════════════════════════════════════════════════════════════════
	/// Retour de Rodolf, 01/09 (soir) : *« lorsqu'on modifie un objet par ses
	/// vertices, on doit redéfinir sa bounding box pour la sélection. »*
	///
	/// ⚠️ MESURE AVANT D'ÉCRIRE. Déplacer un sommet n'écrivait QUE
	///    `sommets[i].x/y` : `posX`, `posY`, `width` et `height` ne bougeaient
	///    pas d'un pixel. Conséquence, et elle est plus large qu'un liseré mal
	///    placé : **tout ce qui lit la boîte lisait une boîte périmée** — les
	///    poignées de sélection, la puce de dimensions, les champs L/H de
	///    l'Inspecteur, l'aimantation, l'englobant d'une multi-sélection, et
	///    surtout le POINTAGE. On pouvait cliquer sur la forme sans la
	///    sélectionner, et la sélectionner en cliquant dans le vide. *Un objet
	///    qu'on voit à un endroit et qu'on attrape à un autre fait douter de tout
	///    le reste de l'outil* — c'est l'arbitrage déjà écrit pour la rotation
	///    des groupes, appliqué ici.
	///
	/// ⚠️ ET LE PIÈGE EST LE RÉFÉRENTIEL, PAS LE CALCUL DE L'ENGLOBANT. Nos
	///    sommets sont **UNITAIRES** : `-1..1` en fraction de la boîte. Recadrer
	///    la boîte **change donc leur unité** — les mêmes nombres désignent
	///    d'autres points. Recalculer la boîte sans renormaliser ferait
	///    **sauter la forme à l'écran au moment même où on relâche le sommet**,
	///    et de plus en plus fort à mesure qu'on s'éloigne. Les deux opérations
	///    ne sont pas « le calcul et une finition » : c'est **une seule
	///    transformation**, et elle est écrite ici pour qu'aucun site d'appel
	///    n'en fasse la moitié.
	///
	/// ⚠️ UN CÔTÉ PLAT NE DIVISE PAS. Trois sommets alignés verticalement donnent
	///    une largeur nulle : la renormalisation diviserait par zéro et rendrait
	///    des NaN — qui se propagent en silence jusqu'au dessin, où ils
	///    n'affichent rien du tout. L'axe dégénéré garde donc un plancher, et ses
	///    sommets y sont ramenés à 0 (le centre), ce qui est exactement leur
	///    place.
	///
	/// @param px,py position du nœud (in/out) — l'appelant décide s'il peut
	///        l'écrire (elle n'a de sens que sous un parent `Free`/`Anchor`).
	/// @param w,h  taille du nœud (in/out).
	/// @return vrai si la boîte a changé.
	inline bool NkRecadrerSurLeTrace(NkUINode &n, float32 &px, float32 &py, float32 &w,
									 float32 &h) {
		if (n.sommets.Empty() || !NkSommetsStockes(NkNatureDe(n.shape.Data())))
			return false;
		if (w <= 0.f || h <= 0.f)
			return false;
		const uint32 nb = (uint32)n.sommets.Size();
		// ── L'ENGLOBANT DU TRACÉ, COURBES COMPRISES ──────────────────────────
		// ⚠️ DEUX FAÇONS DE LE CALCULER, ET LE CHOIX SE DIT PLUTÔT QU'IL NE SE
		//    SUBIT. (a) l'enveloppe des POINTS DE CONTRÔLE : une cubique reste
		//    toujours DANS l'enveloppe convexe de ses quatre points, donc la
		//    boîte contient à coup sûr la courbe — simple, exact au sens « rien
		//    ne dépasse », mais légèrement LARGE. (b) la courbe ÉCHANTILLONNÉE :
		//    serrée au pixel, et il faut la rééchantillonner à chaque
		//    recadrage.
		//
		//    **On prend (a), et voici pourquoi ce n'est pas la paresse.** Un
		//    englobant *un peu large* ne casse rien : on attrape la forme un
		//    peu avant de la toucher, ce que tout éditeur vectoriel fait. Un
		//    englobant *calculé sur un échantillonnage* change avec le nombre
		//    d'échantillons — or ce nombre suit désormais le ZOOM
		//    (`NkEchantillonsCourbe`). La boîte du document dépendrait donc du
		//    facteur d'agrandissement au moment du geste : **deux recadrages au
		//    même endroit, à deux zooms différents, donneraient deux tailles
		//    différentes.** Une géométrie de document qui dépend de l'état de la
		//    vue est exactement ce que ce chantier refuse depuis la règle
		//    « les décors ne sont pas la mesure ».
		//
		//    ⚠️ ET SANS LES POINTS DE CONTRÔLE, LA BOÎTE SERAIT TROP PETITE :
		//    l'englobant des seuls sommets coupe le ventre de chaque courbe. Le
		//    cas de recette le tient dans ce sens-là.
		float32 x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
		bool premier = true;
		auto avaler = [&](float32 X, float32 Y) {
			if (premier) {
				x0 = x1 = X;
				y0 = y1 = Y;
				premier = false;
				return;
			}
			if (X < x0)
				x0 = X;
			if (X > x1)
				x1 = X;
			if (Y < y0)
				y0 = Y;
			if (Y > y1)
				y1 = Y;
		};
		for (uint32 i = 0; i < nb; ++i) {
			const NkPoint2 &sp = n.sommets[i];
			const float32 X = px + (sp.x + 1.f) * 0.5f * w;
			const float32 Y = py + (sp.y + 1.f) * 0.5f * h;
			avaler(X, Y);
			if (sp.liaison == NkPoint2::LiaisonDroit)
				continue;
			// les deux points de contrôle, dans la même unité que le sommet
			avaler(X + sp.ex * w * 0.5f, Y + sp.ey * h * 0.5f);
			avaler(X + sp.sx * w * 0.5f, Y + sp.sy * h * 0.5f);
		}
		float32 nw = x1 - x0, nh = y1 - y0;
		const bool platX = nw < 1.f;
		const bool platY = nh < 1.f;
		if (platX)
			nw = 1.f;
		if (platY)
			nh = 1.f;
		// ⚠️ RIEN À FAIRE SI RIEN N'A BOUGÉ, ET CE N'EST PAS UNE OPTIMISATION :
		//    c'est ce qui rend l'opération IDEMPOTENTE. Sans ce test, dix appels
		//    successifs sur une forme immobile enchaîneraient dix
		//    renormalisations, et l'erreur d'arrondi du flottant ferait DÉRIVER
		//    la forme sans qu'aucun geste ne l'ait touchée.
		const float32 eps = 0.01f;
		const float32 dx0 = x0 - px < 0.f ? px - x0 : x0 - px;
		const float32 dy0 = y0 - py < 0.f ? py - y0 : y0 - py;
		const float32 dw = nw - w < 0.f ? w - nw : nw - w;
		const float32 dh = nh - h < 0.f ? h - nh : nh - h;
		if (dx0 < eps && dy0 < eps && dw < eps && dh < eps)
			return false;
		// RENORMALISER : les mêmes points, dans la nouvelle unité.
		// ⚠️ LES TANGENTES AUSSI, ET C'EST LE PIÈGE DANS LE PIÈGE. Elles sont
		//    unitaires comme les sommets, mais ce ne sont pas des POSITIONS : ce
		//    sont des VECTEURS. Une position se renormalise en la ramenant dans
		//    la nouvelle boîte ; un vecteur se renormalise en le mettant à
		//    l'échelle du RAPPORT des deux largeurs. Leur appliquer la formule
		//    des positions (`(X - x0) / nw * 2 - 1`) aurait ajouté un décalage à
		//    un vecteur qui n'en a pas — les poignées seraient parties dans le
		//    coin haut-gauche au premier recadrage. *Deux grandeurs qui portent
		//    la même unité ne se transforment pas de la même façon.*
		const float32 kx = platX ? 1.f : w / nw;
		const float32 ky = platY ? 1.f : h / nh;
		for (uint32 i = 0; i < nb; ++i) {
			NkPoint2 &sp = n.sommets[i];
			const float32 X = px + (sp.x + 1.f) * 0.5f * w;
			const float32 Y = py + (sp.y + 1.f) * 0.5f * h;
			sp.x = platX ? 0.f : ((X - x0) / nw) * 2.f - 1.f;
			sp.y = platY ? 0.f : ((Y - y0) / nh) * 2.f - 1.f;
			sp.ex *= kx;
			sp.ey *= ky;
			sp.sx *= kx;
			sp.sy *= ky;
		}
		px = x0;
		py = y0;
		w = nw;
		h = nh;
		return true;
	}

	/// LE MÊME GESTE, APPLIQUÉ AU NŒUD — position comprise quand elle a un sens.
	///
	/// ⚠️ LA POSITION NE S'ÉCRIT PAS TOUJOURS, ET LE TAIRE SERAIT UN DÉFAUT. Sous
	///    un parent en agencement calculé (colonne, rangée), `posX`/`posY` sont
	///    IGNORÉS : la disposition place l'enfant. Y écrire un décalage donnerait
	///    une valeur qui n'a aucun effet — et le tracé, lui, aurait bougé. On ne
	///    recadre donc QUE quand le parent est libre ; sinon on rend faux, et
	///    l'appelant a une raison à dire.
	/// @param parentLibre le parent place-t-il ses enfants librement ?
	inline bool NkRecadrerNoeud(NkUINode &n, bool parentLibre) {
		if (!parentLibre)
			return false;
		float32 px = n.posX, py = n.posY;
		float32 w = n.width.value, h = n.height.value;
		if (!NkRecadrerSurLeTrace(n, px, py, w, h))
			return false;
		n.posX = px;
		n.posY = py;
		n.width.value = w;
		n.height.value = h;
		// ⚠️ ET LES MODES PASSENT À `Fixed`, comme le fait déjà le glisser d'un
		//    bout de ligne : écrire une valeur dans un axe `expand` la laisserait
		//    sans effet, et la boîte resterait fausse en silence.
		n.width.mode = NkSizeMode::Fixed;
		n.height.mode = NkSizeMode::Fixed;
		return true;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  AJOUTER UN SOMMET — ET LA POSITION EST UN RÉSULTAT, PAS UNE SAISIE
	// ═══════════════════════════════════════════════════════════════════════════
	/// Insère un sommet SUR le segment `seg` (entre le sommet `seg` et le
	/// suivant, le tracé étant fermé), à la fraction `t` de ce segment.
	///
	/// ⚠️ POURQUOI `t` ET PAS UN POINT LIBRE. Rodolf : *« on peut ajouter des
	///    informations, entre autres des vertices avec le pinceau »* — et sur sa
	///    capture 3, le sommet neuf est **sur le côté**, pas à côté. Prendre le
	///    point de la souris tel quel aurait posé un sommet légèrement HORS du
	///    tracé : la forme se serait déformée **au moment même de l'ajout**, avant
	///    qu'on tire quoi que ce soit. Un ajout qui déforme n'est pas un ajout.
	///    L'appelant projette donc la souris sur le segment et donne `t` ; la
	///    forme reste identique au pixel tant que le sommet neuf n'a pas bougé.
	///
	/// ⚠️ ET IL MATÉRIALISE D'ABORD. Sans ça, ajouter un sommet à une étoile
	///    régulière aurait donné une liste d'UN sommet et effacé les dix autres.
	/// @return l'indice du sommet créé, ou -1 si la forme n'en stocke pas.
	inline int32 NkInsererSommet(NkUINode &n, uint32 seg, float32 t) {
		if (!NkSommetsStockes(NkNatureDe(n.shape.Data())))
			return -1;
		NkMaterialiserSommets(n);
		const uint32 nb = (uint32)n.sommets.Size();
		if (nb < 2 || seg >= nb)
			return -1;
		if (t < 0.f)
			t = 0.f;
		if (t > 1.f)
			t = 1.f;
		const NkPoint2 a = n.sommets[seg];
		const NkPoint2 b = n.sommets[(seg + 1) % nb];
		NkPoint2 p;
		p.x = a.x + (b.x - a.x) * t;
		p.y = a.y + (b.y - a.y) * t;
		p.rayon = 0.f; // un sommet neuf est VIF : l'arrondi se demande, il ne s'hérite pas
		n.sommets.PushBack(p);
		// remonter le point neuf juste après `seg` (le vecteur du dépôt n'a pas
		// d'insertion au milieu ; le décalage est explicite plutôt qu'emprunté).
		for (uint32 i = (uint32)n.sommets.Size() - 1; i > seg + 1; --i) {
			const NkPoint2 tmp = n.sommets[i];
			n.sommets[i] = n.sommets[i - 1];
			n.sommets[i - 1] = tmp;
		}
		return (int32)(seg + 1);
	}

	/// SUPPRIMER un sommet. Rend faux quand le tracé tomberait sous trois sommets
	/// — en dessous, il n'y a plus de forme à peindre, seulement un segment, et
	/// l'utilisateur aurait fait disparaître son objet par un geste d'édition.
	inline bool NkSupprimerSommet(NkUINode &n, uint32 i) {
		if (!NkSommetsStockes(NkNatureDe(n.shape.Data())))
			return false;
		NkMaterialiserSommets(n);
		const uint32 nb = (uint32)n.sommets.Size();
		if (nb <= 3 || i >= nb)
			return false;
		for (uint32 k = i; k + 1 < nb; ++k)
			n.sommets[k] = n.sommets[k + 1];
		n.sommets.PopBack();
		return true;
	}

	/// LES RAYONS D'ARRONDI QU'UN DOUBLE-CLIC FAIT DÉFILER, en pixels.
	/// ⚠️ UN CYCLE PLUTÔT QU'UN INTERRUPTEUR : *« si on double-clique sur une
	///    poignée sombre on peut l'arrondir »* ne dit pas « de combien ». Un
	///    interrupteur vif/rond aurait figé une seule valeur ; le champ numérique
	///    de l'Inspecteur reste la voie exacte, ce cycle est la voie rapide.
	inline uint32 NkNbRayonsSommet() {
		return 4u;
	}
	inline float32 NkRayonSommetCycle(uint32 k) {
		static const float32 kR[4] = {0.f, 8.f, 16.f, 32.f};
		return kR[k % 4u];
	}
	/// Le rayon SUIVANT dans le cycle, à partir de celui qu'on a.
	inline float32 NkRayonSommetSuivant(float32 actuel) {
		for (uint32 k = 0; k < NkNbRayonsSommet(); ++k)
			if (NkRayonSommetCycle(k) == actuel)
				return NkRayonSommetCycle(k + 1u);
		// une valeur saisie à la main hors cycle : le double-clic la remet à vif,
		// il ne la « corrige » pas vers la plus proche (ce serait deviner).
		return 0.f;
	}

	/// ARRONDIR (ou ré-affûter) UN SOMMET. Rend le rayon obtenu, -1 si refusé.
	inline float32 NkArrondirSommet(NkUINode &n, uint32 i) {
		if (!NkSommetsStockes(NkNatureDe(n.shape.Data())))
			return -1.f;
		NkMaterialiserSommets(n);
		if (i >= (uint32)n.sommets.Size())
			return -1.f;
		const float32 r = NkRayonSommetSuivant(n.sommets[i].rayon);
		n.sommets[i].rayon = r;
		return r;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  QUEL SEGMENT LA SOURIS DÉSIGNE — LE POINTAGE EST UN MÉCANISME
	// ═══════════════════════════════════════════════════════════════════════════
	/// Le segment du tracé le plus proche du point `(px, py)`, avec la fraction
	/// `t` du projeté et la distance obtenue.
	///
	/// ⚠️ ÉCRIT ICI, PAS DANS `OnUI` — c'est la facture exacte que l'aimantation
	///    a payée en Q42 : *« le calcul est un mécanisme, pas un dessin »*. Un
	///    pointage écrit dans la boucle de rendu vit là où aucun banc ne va, et
	///    « le sommet s'ajoute au bon endroit » redevient une opinion.
	///
	/// @param xy   les ANCRES à l'écran (sortie de `NkSommetsDe`) ; le tracé est
	///             considéré FERMÉ — le dernier segment relie le dernier au premier.
	/// @return l'indice du segment, ou -1 s'il y a moins de deux ancres.
	inline int32 NkSegmentLePlusProche(const float32 *xy, uint32 nb, float32 px, float32 py,
									   float32 &t, float32 &dist) {
		if (!xy || nb < 2)
			return -1;
		int32 best = -1;
		float32 bestD2 = 0.f, bestT = 0.f;
		for (uint32 i = 0; i < nb; ++i) {
			const uint32 j = (i + 1) % nb;
			const float32 ax = xy[i * 2], ay = xy[i * 2 + 1];
			const float32 bx = xy[j * 2], by = xy[j * 2 + 1];
			const float32 ex = bx - ax, ey = by - ay;
			const float32 len2 = ex * ex + ey * ey;
			float32 u = 0.f;
			if (len2 > 0.0001f)
				u = ((px - ax) * ex + (py - ay) * ey) / len2;
			if (u < 0.f)
				u = 0.f;
			if (u > 1.f)
				u = 1.f;
			const float32 qx = ax + ex * u, qy = ay + ey * u;
			const float32 d2 = (px - qx) * (px - qx) + (py - qy) * (py - qy);
			if (best < 0 || d2 < bestD2) {
				best = (int32)i;
				bestD2 = d2;
				bestT = u;
			}
		}
		t = bestT;
		// ⚠️ LA DISTANCE SE REND EN VRAIE LONGUEUR, PAS AU CARRÉ : l'appelant la
		//    compare à une tolérance en PIXELS. Rendre le carré aurait « marché »
		//    à 1 px (1² = 1) et menti partout ailleurs — une tolérance de 10 px
		//    aurait accepté tout ce qui est à moins de 10 px… au carré, soit
		//    3,2 px. Le genre de faute qui passe la relecture.
		dist = NkLongueur2D(bestD2, 0.f);
		return best;
	}

	/// L'ANCRE la plus proche du point, ou -1 au-delà de `tol` pixels.
	/// ⚠️ ELLE EST INTERROGÉE AVANT LE SEGMENT, TOUJOURS : une ancre est POSÉE
	///    SUR son segment, donc les deux répondent au même clic. Sans une
	///    priorité dite, cliquer une ancre pour la déplacer en aurait ajouté une
	///    seconde par-dessus — c'est la même classe de collision que la poignée
	///    de redimension contre la poignée de sommet, déjà tranchée par
	///    `NkAQuiLaPoignee`, et on la tranche du même côté : **le sommet existant
	///    gagne sur le sommet à naître.**
	inline int32 NkAncreLaPlusProche(const float32 *xy, uint32 nb, float32 px, float32 py,
									 float32 tol) {
		int32 best = -1;
		float32 bestD2 = 0.f;
		for (uint32 i = 0; i < nb; ++i) {
			const float32 dx = px - xy[i * 2], dy = py - xy[i * 2 + 1];
			const float32 d2 = dx * dx + dy * dy;
			if (d2 <= tol * tol && (best < 0 || d2 < bestD2)) {
				best = (int32)i;
				bestD2 = d2;
			}
		}
		return best;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  POSER UNE TANGENTE — ET C'EST LA LIAISON QUI DÉCIDE DE LA JUMELLE
	// ═══════════════════════════════════════════════════════════════════════════
	/// Retour de Rodolf, 01/09 (nuit) : *« la possibilité d'avoir le manipulateur
	/// de chaque côté indépendant ou dépendant en fonction de l'utilisateur. »*
	///
	/// ⚠️ ÉCRIT ICI, PAS DANS LE GESTE, ET C'EST LA CINQUIÈME FOIS QUE CE CHANTIER
	///    PREND CETTE DÉCISION (le zoom en Q41, l'aimantation en Q42, le tracé,
	///    la liste des sections, le déplacement groupé). Une règle « si miroir, la
	///    jumelle suit » écrite dans la boucle de rendu vivrait là où aucun banc
	///    ne va — et « asymétrique garde bien sa longueur » resterait une opinion.
	///
	/// ⚠️ ET LA DIFFICULTÉ N'EST PAS LE `switch`, C'EST LA LONGUEUR NULLE. En
	///    `Miroir` la jumelle est l'opposé exact ; en `Asym` elle doit garder SA
	///    longueur et prendre la direction opposée — or diviser par la longueur de
	///    la nouvelle tangente pour l'orienter **explose quand on ramène une
	///    poignée exactement sur son sommet**. Le cas est atteignable à la souris
	///    (il suffit de relâcher pile dessus), et sans garde il rendrait des NaN
	///    qui se propagent en silence jusqu'au dessin. Longueur nulle : la jumelle
	///    ne bouge pas — c'est la seule réponse qui ne fabrique pas une direction
	///    que personne n'a demandée.
	///
	/// @param cote 0 = on pose la tangente ENTRANTE, 1 = la SORTANTE.
	inline void NkPoserTangente(NkPoint2 &p, uint32 cote, float32 tx, float32 ty) {
		if (cote == 0) {
			p.ex = tx;
			p.ey = ty;
		} else {
			p.sx = tx;
			p.sy = ty;
		}
		if (p.liaison == NkPoint2::LiaisonDroit || p.liaison == NkPoint2::LiaisonDeconnecte)
			return; // rien à propager : pas de tangente, ou deux poignées libres
		const float32 lon = NkLongueur2D(tx, ty);
		if (lon < 0.0001f)
			return; // voir ci-dessus : pas de direction, donc pas de propagation
		if (p.liaison == NkPoint2::LiaisonMiroir) {
			// direction ET longueur : l'opposé exact
			if (cote == 0) {
				p.sx = -tx;
				p.sy = -ty;
			} else {
				p.ex = -tx;
				p.ey = -ty;
			}
			return;
		}
		// LiaisonAsymetrique : direction opposée, LONGUEUR DE LA JUMELLE CONSERVÉE.
		const float32 jx = (cote == 0) ? p.sx : p.ex;
		const float32 jy = (cote == 0) ? p.sy : p.ey;
		const float32 lj = NkLongueur2D(jx, jy);
		if (lj < 0.0001f)
			return; // la jumelle n'existe pas encore : on ne lui invente pas une longueur
		const float32 nxv = -tx / lon * lj, nyv = -ty / lon * lj;
		if (cote == 0) {
			p.sx = nxv;
			p.sy = nyv;
		} else {
			p.ex = nxv;
			p.ey = nyv;
		}
	}

	/// CHANGER LE TYPE DE LIAISON d'un sommet, et rendre le modèle COHÉRENT avec
	/// ce que le type promet.
	/// ⚠️ UN TYPE QUI NE RANGE PAS LES TANGENTES N'EST QU'UNE ÉTIQUETTE. Passer
	///    « asymétrique » à « miroir » sans réaligner laisserait deux poignées
	///    visiblement dissymétriques sous un bouton qui annonce la symétrie —
	///    exactement la « case cochée à côté d'un aimant qui ne fait rien » que ce
	///    chantier chasse depuis Q42.
	/// ⚠️ ET REVENIR À `Angle` EFFACE LES TANGENTES plutôt que de les garder « au
	///    cas où » : gardées, elles ressusciteraient à la prochaine bascule, et
	///    l'utilisateur retrouverait une courbe qu'il croyait avoir supprimée.
	///    Le pas d'annulation, lui, les rend parfaitement.
	inline void NkPoserLiaison(NkPoint2 &p, nkentseu::uint8 liaison) {
		p.liaison = liaison;
		if (liaison == NkPoint2::LiaisonDroit) {
			p.ex = p.ey = p.sx = p.sy = 0.f;
			return;
		}
		if (liaison == NkPoint2::LiaisonMiroir) {
			// on aligne sur la tangente SORTANTE quand elle existe, sinon sur
			// l'entrante : il faut une référence, et prendre toujours la même
			// évite qu'un aller-retour entre deux types fasse tourner la courbe.
			if (p.sx != 0.f || p.sy != 0.f) {
				p.ex = -p.sx;
				p.ey = -p.sy;
			} else if (p.ex != 0.f || p.ey != 0.f) {
				p.sx = -p.ex;
				p.sy = -p.ey;
			}
		}
	}

	/// Le segment `i → j` porte-t-il une courbe ? Vrai dès qu'un des deux bouts
	/// pose une tangente de ce côté-là.
	inline bool NkSegmentCourbe(const NkUINode &n, uint32 i, uint32 j) {
		const uint32 nb = (uint32)n.sommets.Size();
		if (i >= nb || j >= nb)
			return false;
		const NkPoint2 &a = n.sommets[i];
		const NkPoint2 &b = n.sommets[j];
		const bool sortA = a.liaison != NkPoint2::LiaisonDroit && (a.sx != 0.f || a.sy != 0.f);
		const bool entB = b.liaison != NkPoint2::LiaisonDroit && (b.ex != 0.f || b.ey != 0.f);
		return sortA || entB;
	}

	/// ⚠️ UN RAYON N'A PAS DE SENS SUR UN SOMMET DONT UN CÔTÉ EST COURBE, et cette
	///    règle enlève la dernière ambiguïté entre les deux mécanismes. Arrondir
	///    un coin, c'est ROGNER les deux segments qui s'y rejoignent ; si l'un des
	///    deux est déjà une cubique, on ne sait plus où la cubique commence. Le
	///    modèle dit déjà « `rayon` ne vaut que sur `LiaisonDroit` » ; ici on
	///    ajoute le voisinage, et le dessin n'a plus jamais à choisir.
	inline bool NkRayonPeint(const NkUINode &n, uint32 i, uint32 nb) {
		if (i >= (uint32)n.sommets.Size() || !n.sommets[i].RayonActif())
			return false;
		const uint32 ip = (i + nb - 1) % nb, in = (i + 1) % nb;
		return !NkSegmentCourbe(n, ip, i) && !NkSegmentCourbe(n, i, in);
	}

	/// LE NOMBRE D'ÉCHANTILLONS D'UNE COURBE, D'APRÈS SA TAILLE À L'ÉCRAN.
	/// ⚠️ UN NOMBRE FIXE AURAIT ÉTÉ FAUX DANS LES DEUX SENS, et c'est le piège
	///    classique de ce genre de code : à fort zoom, six segments font d'une
	///    courbe un polygone bien visible ; à faible zoom, quarante segments sur
	///    une courbe de dix pixels coûtent trente-quatre points pour rien. On
	///    prend donc la longueur de la corde **à l'écran** — `NkContourDe` reçoit
	///    déjà un rectangle d'écran, l'information est là — et on la borne des
	///    deux côtés.
	inline uint32 NkEchantillonsCourbe(float32 corde) {
		uint32 n = (uint32)(corde / 6.f);
		if (n < 6u)
			n = 6u;
		if (n > 40u)
			n = 40u;
		return n;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  LE CONTOUR PEINT — LÀ OÙ L'ARRONDI PAR SOMMET DEVIENT VISIBLE
	// ═══════════════════════════════════════════════════════════════════════════
	/// ⚠️ DEUX FONCTIONS, ET LA DIFFÉRENCE EST LE POINT DE CONCEPTION.
	///    `NkSommetsDe` rend les **ANCRES** : ce que la main attrape, ce que
	///    l'Inspecteur numérote, ce que le double-clic arrondit. `NkContourDe`
	///    rend le **TRACÉ PEINT** : les mêmes ancres, mais chaque coin arrondi
	///    remplacé par un arc échantillonné.
	///
	///    Les confondre aurait cassé l'un des deux, dans les deux sens : poser
	///    les poignées sur le contour aurait donné **une poignée par point d'arc**
	///    (on tirerait un échantillon, pas un sommet) ; peindre par les ancres
	///    aurait fait de l'arrondi **un champ qui n'agit pas** — la famille de
	///    défauts qu'on chasse.
	///
	///    ⚠️ ET LE CONTOUR APPELLE LES ANCRES, il ne les recalcule pas. C'est la
	///       règle de ce fichier depuis son en-tête : deux tables de sommets
	///       dérivent au premier ajustement.
	/// @return le nombre de points écrits (2 par point dans `xy`).
	inline uint32 NkContourDe(const NkUINode &n, const NkPaintRect &r, float32 *xy, uint32 cap) {
		float32 anc[64];
		const uint32 nb = NkSommetsDe(n, r, anc, 32);
		if (nb == 0)
			return 0;
		// ── RIEN À COURBER NI À ARRONDIR : le contour EST les ancres ─────────
		// ⚠️ CE CHEMIN EXISTE POUR QUE LA GRANDE MAJORITÉ DES FORMES NE PAIE RIEN.
		//    Il rend les mêmes octets qu'avant les tangentes, et c'est lui qui
		//    tient la promesse « un document d'hier se dessine exactement pareil ».
		bool aucuneCourbe = true;
		for (uint32 i = 0; i < nb && i < (uint32)n.sommets.Size(); ++i) {
			if (n.sommets[i].rayon > 0.f || n.sommets[i].liaison != NkPoint2::LiaisonDroit) {
				aucuneCourbe = false;
				break;
			}
		}
		if (aucuneCourbe || nb < 3) {
			if (nb > cap)
				return 0;
			for (uint32 i = 0; i < nb * 2; ++i)
				xy[i] = anc[i];
			return nb;
		}
		uint32 k = 0;
		auto pousser = [&](float32 px, float32 py) {
			if (k < cap) {
				xy[k * 2] = px;
				xy[k * 2 + 1] = py;
				++k;
			}
		};
		// Les tangentes sont UNITAIRES : on les amène dans l'espace du rectangle
		// reçu (écran ou document — la fonction ne s'en mêle pas, elle applique
		// simplement la demi-largeur et la demi-hauteur qu'on lui a données).
		const float32 hx = r.w * 0.5f, hy = r.h * 0.5f;
		const uint32 nbS = (uint32)n.sommets.Size();
		// 🔴 LE BUDGET D'ECHANTILLONS, ET IL VIENT D'UN PLANTAGE MESURE.
		//    `pousser` refuse silencieusement ce qui depasse `cap` : un contour
		//    trop long n'etait donc pas ecrete proprement, il etait TRONQUE --
		//    la fin du trace manquait, le polygone ne se refermait plus sur
		//    lui-meme, et il se croisait.
		//
		//    Tant que le peintre remplissait par un EVENTAIL, ca ne se voyait
		//    pas : un eventail sur un contour casse dessine un eventail casse.
		//    Le jour ou le remplissage est passe a une TRIANGULATION par
		//    ear-clipping (2026-09-01), le meme contour a fait planter
		//    l'application -- code `0xC0000374`, corruption de tas, reproductible
		//    en trois secondes avec `--courber` et absent sans.
		//
		//    L'arithmetique le disait avant le debogueur : un rect dont les
		//    quatre cotes sont courbes demande 4 ancres + 4 x 39 echantillons =
		//    **160 points**, pour un `cap` de **128**. *Le defaut etait deja la
		//    hier ; c'est le consommateur suivant qui l'a rendu mortel.*
		//
		//    On repartit donc la place DISPONIBLE entre les segments courbes, au
		//    lieu d'esperer qu'elle suffise.
		uint32 nbCourbes = 0;
		for (uint32 i = 0; i < nb; ++i)
			if (i < nbS && NkSegmentCourbe(n, i, (i + 1) % nbS))
				++nbCourbes;
		// la place restante une fois les ancres et les arcs d'arrondi poses (on
		// reserve 7 points par coin arrondi, la valeur de `kSeg + 1`).
		uint32 reserve = nb;
		for (uint32 i = 0; i < nb; ++i)
			if (NkRayonPeint(n, i, nb))
				reserve += 7;
		const uint32 budget = (cap > reserve) ? (cap - reserve) : 0u;
		const uint32 parCourbe = (nbCourbes > 0) ? (budget / nbCourbes) : 0u;
		for (uint32 i = 0; i < nb; ++i) {
			const uint32 in = (i + 1) % nb;
			const float32 cxp = anc[i * 2], cyp = anc[i * 2 + 1];
			// ── 1. LE COIN : le sommet, ou son arc de rayon ──────────────────
			if (!NkRayonPeint(n, i, nb)) {
				pousser(cxp, cyp);
			} else {
				const uint32 ip = (i + nb - 1) % nb;
				const float32 axp = anc[ip * 2], ayp = anc[ip * 2 + 1];
				const float32 bxp = anc[in * 2], byp = anc[in * 2 + 1];
				// ⚠️ LE RAYON EST BORNÉ PAR LA MOITIÉ DU PLUS COURT DES DEUX CÔTÉS.
				//    Sans cette borne, un rayon de 32 px sur un rect de 20 px de
				//    large aurait fait **se croiser** les deux arcs voisins : la
				//    forme se serait retournée sur elle-même au lieu de s'arrondir.
				//    Le modèle garde la valeur saisie, c'est le DESSIN qui la
				//    borne — sinon agrandir la forme ne rendrait pas l'arrondi
				//    demandé.
				float32 d1x = axp - cxp, d1y = ayp - cyp;
				float32 d2x = bxp - cxp, d2y = byp - cyp;
				const float32 l1 = NkLongueur2D(d1x, d1y);
				const float32 l2 = NkLongueur2D(d2x, d2y);
				if (l1 < 0.001f || l2 < 0.001f) {
					pousser(cxp, cyp);
				} else {
					d1x /= l1;
					d1y /= l1;
					d2x /= l2;
					d2y /= l2;
					float32 rl = n.sommets[i].rayon;
					if (rl > l1 * 0.5f)
						rl = l1 * 0.5f;
					if (rl > l2 * 0.5f)
						rl = l2 * 0.5f;
					const float32 p0x = cxp + d1x * rl, p0y = cyp + d1y * rl;
					const float32 p2x = cxp + d2x * rl, p2y = cyp + d2y * rl;
					// Un arc approché par interpolation quadratique entre les deux
					// points de tangence, le sommet servant de point de contrôle :
					// la même courbe qu'un quart de rond, sans trigonométrie.
					const uint32 kSeg = 6;
					for (uint32 sgm = 0; sgm <= kSeg; ++sgm) {
						const float32 t = (float32)sgm / (float32)kSeg;
						const float32 u = 1.f - t;
						pousser(u * u * p0x + 2.f * u * t * cxp + t * t * p2x,
								u * u * p0y + 2.f * u * t * cyp + t * t * p2y);
					}
				}
			}
			// ── 2. LE SEGMENT i → i+1 : droit, ou cubique ────────────────────
			// ⚠️ ON N'ÉMET QUE L'INTÉRIEUR DE LA CUBIQUE (t strictement entre 0 et
			//    1) : ses deux extrémités SONT les ancres, déjà poussées — celle
			//    de `i` juste au-dessus, celle de `i+1` au tour suivant. Les
			//    pousser aussi doublerait chaque sommet, ce qui ne se voit pas à
			//    l'œil sur un trait plein mais fabrique des points de largeur
			//    nulle dans la triangulation du remplissage.
			if (i < nbS && !NkSegmentCourbe(n, i, in % nbS))
				continue;
			if (in >= nbS)
				continue;
			const NkPoint2 &a = n.sommets[i];
			const NkPoint2 &b = n.sommets[in];
			const float32 x0 = cxp, y0 = cyp;
			const float32 x3 = anc[in * 2], y3 = anc[in * 2 + 1];
			const float32 x1 = x0 + a.sx * hx, y1 = y0 + a.sy * hy;
			const float32 x2 = x3 + b.ex * hx, y2 = y3 + b.ey * hy;
			uint32 ns = NkEchantillonsCourbe(NkLongueur2D(x3 - x0, y3 - y0));
			// ⚠️ LA FINESSE CEDE DEVANT LA PLACE. Une courbe un peu anguleuse
			//    reste une forme ; une courbe tronquee n'en est plus une.
			if (ns > parCourbe + 1u)
				ns = parCourbe + 1u;
			if (ns < 2u)
				ns = 2u;
			for (uint32 sgm = 1; sgm < ns; ++sgm) {
				const float32 t = (float32)sgm / (float32)ns;
				const float32 u = 1.f - t;
				const float32 uu = u * u, tt = t * t;
				pousser(uu * u * x0 + 3.f * uu * t * x1 + 3.f * u * tt * x2 + tt * t * x3,
						uu * u * y0 + 3.f * uu * t * y1 + 3.f * u * tt * y2 + tt * t * y3);
			}
		}
		return k;
	}

} // namespace nkuidesign
