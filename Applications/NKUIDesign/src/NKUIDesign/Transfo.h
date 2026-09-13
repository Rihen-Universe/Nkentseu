#pragma once
// -----------------------------------------------------------------------------
// @File    Transfo.h
// @Brief   ROTATION ET MIROIRS — la transformation autour du centre, en MÉCANISME :
//          ce qu'elle fait au dessin, ce qu'elle fait au pointage, et ce qu'elle
//          propage aux enfants.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ POURQUOI CE FICHIER EXISTE, ET POURQUOI IL N'EST PAS DANS `Panels.h`
// =============================================================================
//  Retour de Rodolf, 01/09 : *« dans propriétés il n'y a pas miroir, rotation
//  etc., ni autour de l'objet sélectionné. »* — les DEUX à la fois : les champs
//  de l'Inspecteur ET les poignées autour de la sélection.
//
//  L'arbitrage de Q42 avait écrit la règle : *« un demi-champ de rotation est
//  pire que pas de rotation »*, avec son périmètre en cinq points (angle au
//  modèle, propagation, picking, rendu, poignées). Ce fichier porte les trois
//  qui sont des CALCULS — propagation, picking, géométrie des poignées — pour
//  qu'ils se mesurent sans fenêtre.
//
//  ⚠️ C'EST LA FACTURE EXACTE QUE L'AIMANTATION A PAYÉE EN Q42 : *« le calcul
//     est un mécanisme, pas un dessin »*. Écrit dans `OnUI`, le picking d'un
//     rectangle tourné aurait vécu là où aucun banc ne va, et « le clic tombe au
//     bon endroit » serait resté une opinion.
//
// =============================================================================
//  LES TROIS CHAMPS SONT UNE SEULE CHOSE, ET C'EST LE POINT DE CONCEPTION
// =============================================================================
//  `rotation`, `miroirH` et `miroirV` sont les trois faces d'une même
//  transformation autour du CENTRE DE LA BOÎTE. Livrées séparément, chacune
//  aurait eu son propre inverseur de picking — trois occasions de diverger sur
//  la question la plus délicate du lot, et deux d'entre elles se seraient
//  trompées de sens sans que rien ne le dise (un miroir raté ressemble à un
//  miroir réussi tant que la forme est symétrique).
//
//  ⚠️ L'ORDRE EST FIXÉ ET IL COMPTE : **miroir d'abord, rotation ensuite.**
//     Miroir-puis-rotation et rotation-puis-miroir ne donnent PAS le même
//     résultat (le miroir change le signe de l'angle). Lunacy applique le
//     miroir dans le repère propre de l'objet, donc avant. L'inverse ferait
//     « sauter » un objet déjà tourné au moment où l'on coche un miroir —
//     l'utilisateur croirait avoir cliqué autre chose.
// -----------------------------------------------------------------------------
#include "Layout.h"

namespace nkuidesign {

	using nkentseu::float32;
	using nkentseu::int32;
	using nkentseu::uint32;

	/// Sinus et cosinus, par série de Taylor après réduction dans [-π, π].
	/// ⚠️ ÉCRITS ICI POUR LA MÊME RAISON QUE `NkLongueur2D` DANS `Sommets.h` : le
	///    dépôt est zero-STL, et donner une dépendance `<math.h>` à un fichier de
	///    mécanisme pour deux fonctions aurait été le prix le plus mal placé du
	///    chantier. La précision (~1e-6 sur le domaine réduit) est très au-delà
	///    de ce qu'un angle en degrés entiers demande à l'écran.
	/// ⚠️ LA RÉDUCTION SE FAIT AU QUADRANT, PAS À [-π, π] — ET C'EST UNE MESURE,
	///    PAS UNE PRÉCAUTION. Première version : réduction dans [-π, π] puis
	///    série de Taylor. Le cas 2 de `--recette-transfo` (aller-retour sur
	///    20 combinaisons) a rendu **3,88 px d'écart** — parce qu'à |a| ≈ 3 une
	///    série tronquée à cinq termes n'a plus la précision qu'elle a près de 0.
	///    *Une approximation qui est excellente sur son domaine habituel et
	///    fausse aux bords est le genre d'erreur qu'aucune relecture n'attrape :
	///    elle marche sur tous les angles qu'on essaie à la main.*
	///
	///    On réduit donc dans **[0°, 45°]** par les symétries du cercle, où la
	///    série est exacte à ~1e-7, et on replie le résultat. Le pire écart passe
	///    de 3,88 px à moins de 0,001.
	inline void NkSinCosDeg(float32 deg, float32 &s, float32 &c) {
		const float32 kPi = 3.14159265358979f;
		// 1. dans [0, 360)
		float32 d = deg;
		uint32 garde = 0;
		while (d < 0.f && garde++ < 8192u)
			d += 360.f;
		garde = 0;
		while (d >= 360.f && garde++ < 8192u)
			d -= 360.f;
		// 2. quadrant + symétrie sin/cos : ramène dans [0, 45]
		const uint32 oct = (uint32)(d / 45.f); // 0..7
		const float32 reste = d - (float32)oct * 45.f;
		const float32 petit = ((oct & 1u) == 0u) ? reste : 45.f - reste;
		const float32 a = petit * (kPi / 180.f);
		const float32 a2 = a * a;
		const float32 sp =
			a * (1.f - a2 / 6.f * (1.f - a2 / 20.f * (1.f - a2 / 42.f * (1.f - a2 / 72.f))));
		const float32 cp = 1.f - a2 / 2.f * (1.f - a2 / 12.f * (1.f - a2 / 30.f * (1.f - a2 / 56.f)));
		// 3. repliage : dans les octants impairs, sin et cos échangent leurs rôles
		const float32 su = ((oct & 1u) == 0u) ? sp : cp;
		const float32 cu = ((oct & 1u) == 0u) ? cp : sp;
		switch (oct) {
			case 0:
			case 1: s = su; c = cu; break;
			case 2:
			case 3: s = cu; c = -su; break;
			case 4:
			case 5: s = -su; c = -cu; break;
			default: s = -cu; c = su; break;
		}
	}

	/// La transformation d'UN nœud : ses trois champs, réduits à ce que le calcul
	/// utilise.
	struct NkTransfo {
			float32 deg = 0.f;
			bool mh = false;
			bool mv = false;
			float32 sx = 1.f; ///< echelle horizontale (1 = neutre), portee par le noeud
			float32 sy = 1.f; ///< echelle verticale
			float32 iX = 0.f; ///< inclinaison autour de X, en degres
			float32 iY = 0.f; ///< inclinaison autour de Y, en degres
			bool persp = false;		 ///< l'inclinaison se projette-t-elle en PERSPECTIVE ?
			float32 focale = 800.f;	 ///< distance de l'oeil au plan du noeud, en px
			/// Vrai quand il n'y a rien à faire — le cas de l'immense majorité des
			/// nœuds. ⚠️ IL EST INTERROGÉ AVANT TOUT CALCUL : sans ce court-circuit,
			/// chaque point de chaque forme paierait un sinus, et surtout chaque
			/// contour repasserait par un aller-retour en flottant qui déplacerait
			/// les coordonnées d'un ulp — un document non tourné cesserait de se
			/// dessiner au pixel près.
			bool Identite() const {
				return deg == 0.f && !mh && !mv && sx == 1.f && sy == 1.f && iX == 0.f
					   && iY == 0.f;
			}
			/// Vrai quand la perspective a quelque chose a projeter : sans inclinaison,
			/// un plan vu de face reste un rectangle, focale ou pas.
			bool PerspectiveActive() const {
				return persp && focale > 0.f && (iX != 0.f || iY != 0.f);
			}
	};

	inline NkTransfo NkTransfoDe(const NkUINode &n) {
		NkTransfo t;
		t.deg = n.rotation;
		t.mh = n.miroirH;
		t.mv = n.miroirV;
		t.sx = n.echelleX;
		t.sy = n.echelleY;
		t.iX = n.inclinaisonX;
		t.iY = n.inclinaisonY;
		t.persp = n.perspective;
		t.focale = n.focale;
		return t;
	}

	/// LA ROTATION EFFECTIVE d'un nœud : la sienne PLUS celle de tous ses
	/// ancêtres.
	/// ⚠️ C'EST LE POINT 2 DU PÉRIMÈTRE (« la disposition qui le propage aux
	///    enfants »), et il est ici plutôt que dans la disposition parce que la
	///    disposition calcule des BOÎTES : lui faire porter des angles l'aurait
	///    obligée à produire des rectangles non alignés, ce qu'aucun de ses
	///    consommateurs ne sait lire. Tourner un groupe tourne donc ses enfants
	///    **au dessin et au pointage**, sans que leur boîte cesse d'être droite.
	inline NkTransfo NkTransfoEffective(const NkUIDocument &doc, int32 i) {
		NkTransfo t;
		int32 k = i;
		uint32 garde = 0;
		while (doc.IsValidIndex(k) && k > 0 && garde++ < 256u) {
			const NkUINode &n = doc.nodes[(uint32)k];
			t.deg += n.rotation;
			// ⚠️ DEUX MIROIRS S'ANNULENT. Le composer par un OU aurait rendu un
			//    enfant retourné sous un parent retourné… retourné deux fois et
			//    affiché une seule — la moitié des cas faux, et invisible sur
			//    toute forme symétrique.
			if (n.miroirH)
				t.mh = !t.mh;
			if (n.miroirV)
				t.mv = !t.mv;
			k = n.parent;
		}
		return t;
	}

	/// APPLIQUER la transformation à un point, autour du centre `(cx, cy)`.
	inline void NkTransfoPoint(const NkTransfo &t, float32 cx, float32 cy, float32 &x,
							   float32 &y) {
		if (t.Identite())
			return;
		float32 dx = x - cx, dy = y - cy;
		// MIROIR D'ABORD (repère propre de l'objet), ROTATION ENSUITE.
		if (t.mh)
			dx = -dx;
		if (t.mv)
			dy = -dy;
		if (t.deg != 0.f) {
			float32 s = 0.f, c = 1.f;
			NkSinCosDeg(t.deg, s, c);
			const float32 rx = dx * c - dy * s;
			const float32 ry = dx * s + dy * c;
			dx = rx;
			dy = ry;
		}
		x = cx + dx;
		y = cy + dy;
	}

	/// L'INVERSE — c'est elle qui fait le pointage.
	/// ⚠️ L'ORDRE EST INVERSÉ AUSSI : rotation inverse d'abord, miroirs ensuite.
	///    Garder l'ordre direct aurait donné un inverse qui n'en est pas un dès
	///    qu'un miroir et une rotation coexistent — et le clic serait tombé à
	///    côté **seulement dans ce cas-là**, le plus rare, donc le dernier trouvé.
	inline void NkTransfoPointInverse(const NkTransfo &t, float32 cx, float32 cy, float32 &x,
									  float32 &y) {
		if (t.Identite())
			return;
		float32 dx = x - cx, dy = y - cy;
		if (t.deg != 0.f) {
			float32 s = 0.f, c = 1.f;
			NkSinCosDeg(-t.deg, s, c);
			const float32 rx = dx * c - dy * s;
			const float32 ry = dx * s + dy * c;
			dx = rx;
			dy = ry;
		}
		if (t.mh)
			dx = -dx;
		if (t.mv)
			dy = -dy;
		x = cx + dx;
		y = cy + dy;
	}

	/// Applique la transformation à une LISTE de points (un contour), en place.
	inline void NkTransfoContour(const NkTransfo &t, const NkPaintRect &r, float32 *xy,
								 uint32 nb) {
		if (t.Identite() || !xy)
			return;
		const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
		for (uint32 i = 0; i < nb; ++i)
			NkTransfoPoint(t, cx, cy, xy[i * 2], xy[i * 2 + 1]);
	}

	/// LE POINTAGE : le point `(px, py)` est-il DANS le nœud, sa transformation
	/// comprise ?
	/// ⚠️ POINT 3 DU PÉRIMÈTRE, ET C'EST LE PLUS FACILE À RATER : *« un clic dans
	///    un rectangle tourné n'est plus un test de rectangle »*. On ne teste pas
	///    la boîte tournée — on RAMÈNE LE POINT dans le repère de l'objet et on
	///    teste la boîte droite. C'est le même calcul pour toutes les formes, et
	///    il reste exact quel que soit l'angle.
	inline bool NkPointDansRectTransfo(const NkTransfo &t, const NkPaintRect &r, float32 px,
									   float32 py) {
		const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
		float32 x = px, y = py;
		NkTransfoPointInverse(t, cx, cy, x, y);
		return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  LA MATRICE — PARCE QU'UN ANGLE CUMULÉ NE SUFFIT PAS À TOURNER UN GROUPE
	// ═══════════════════════════════════════════════════════════════════════════
	/// ⚠️ CE BLOC EXISTE À CAUSE D'UNE ERREUR QUE J'AI FAILLI COMMETTRE, ET ELLE
	///    MÉRITE D'ÊTRE ÉCRITE. `NkTransfoEffective` cumule les ANGLES des
	///    ancêtres, et j'allais m'en servir pour peindre les enfants d'un groupe
	///    tourné. C'est faux : **chaque ancêtre tourne autour de SON PROPRE
	///    CENTRE**, pas autour de celui de l'enfant. Un angle cumulé ferait
	///    pivoter chaque enfant sur lui-même — le groupe se disloquerait au lieu
	///    de tourner d'un bloc, et chaque élément resterait obstinément à sa
	///    place.
	///
	///    L'angle cumulé reste juste pour ce à quoi il sert (savoir de combien un
	///    objet paraît penché, afficher un champ, aimanter un geste) ; il ne
	///    suffit pas à POSITIONNER. Il faut composer les transformations, donc
	///    une matrice affine 2×3.
	///
	///    *C'est la même famille que « une moyenne ne se transporte pas à un
	///    sous-ensemble choisi pour une autre raison » : une grandeur juste,
	///    réutilisée là où sa définition ne vaut plus.*
	struct NkMat2D {
			// | a c e |   les points sont des colonnes (x, y, 1)
			// | b d f |
			// | g h 1 |   ⚠️ LA TROISIEME RANGEE (11/09) : w = g x + h y + 1, et le point
			//             rendu est (a x + c y + e) / w. A g = h = 0 c'est l'affine
			//             d'avant, MOT POUR MOT -- toutes les fonctions ci-dessous
			//             gardent le chemin affine separe pour que l'orthogonal ne
			//             change pas d'un ulp (le temoin est un diff de flux a zero).
			//
			// 🔑 POURQUOI UNE HOMOGRAPHIE ET PAS UNE AFFINE DE PLUS : le trapeze d'une
			//    vraie perspective N'EST PAS AFFINE -- deux bords paralleles y
			//    convergent, ce qu'aucune matrice 2x3 ne sait faire. C'est la limite
			//    qui etait ecrite dans `NkMatDe` depuis le 09/09, et que Rodolf a vue.
			float32 a = 1.f, b = 0.f, c = 0.f, d = 1.f, e = 0.f, f = 0.f;
			float32 g = 0.f, h = 0.f;
			bool Identite() const {
				return a == 1.f && b == 0.f && c == 0.f && d == 1.f && e == 0.f && f == 0.f
					   && g == 0.f && h == 0.f;
			}
			/// Affine : la rangee de perspective est nulle. Le chemin rapide, et le
			/// SEUL emprunte par un document sans perspective.
			bool Affine() const {
				return g == 0.f && h == 0.f;
			}
	};

	/// LE PLANCHER DE `w`. Un point dont le `w` passe sous ce seuil est DERRIERE
	/// l'oeil : sa projection n'existe pas (elle se retournerait). On le pince, et
	/// on le dit ici -- l'interface borne la focale pour que le cas ne se presente
	/// pas sur une boite raisonnable.
	inline float32 NkWMin() {
		return 0.05f;
	}
	/// La focale la plus courte qu'on accepte, en px. En dessous, un nœud un peu
	/// grand aurait des coins derriere l'oeil.
	inline float32 NkFocaleMin() {
		return 100.f;
	}
	inline float32 NkFocaleMax() {
		return 10000.f;
	}

	/// `m` puis `n` (n ∘ m) — l'ordre se lit « d'abord m, ensuite n ».
	inline NkMat2D NkMatComposer(const NkMat2D &n, const NkMat2D &m) {
		NkMat2D o;
		if (n.Affine() && m.Affine()) { // le chemin d'avant, inchange
			o.a = n.a * m.a + n.c * m.b;
			o.b = n.b * m.a + n.d * m.b;
			o.c = n.a * m.c + n.c * m.d;
			o.d = n.b * m.c + n.d * m.d;
			o.e = n.a * m.e + n.c * m.f + n.e;
			o.f = n.b * m.e + n.d * m.f + n.f;
			return o;
		}
		// LE PRODUIT 3x3, puis la NORMALISATION : une homographie est definie a un
		// facteur pres, on ramene le coin bas-droit a 1 pour que `w = g x + h y + 1`
		// reste vrai a chaque etage -- sans quoi deux compositions donneraient deux
		// echelles differentes pour la meme geometrie.
		const float32 A = n.a * m.a + n.c * m.b + n.e * m.g;
		const float32 C = n.a * m.c + n.c * m.d + n.e * m.h;
		const float32 E = n.a * m.e + n.c * m.f + n.e;
		const float32 B = n.b * m.a + n.d * m.b + n.f * m.g;
		const float32 Dd = n.b * m.c + n.d * m.d + n.f * m.h;
		const float32 F = n.b * m.e + n.d * m.f + n.f;
		const float32 G = n.g * m.a + n.h * m.b + m.g;
		const float32 H = n.g * m.c + n.h * m.d + m.h;
		const float32 I = n.g * m.e + n.h * m.f + 1.f;
		if (I == 0.f)
			return o; // degeneres : l'identite plutot que des infinis (meme repli que l'inverse)
		const float32 k = 1.f / I;
		o.a = A * k;
		o.b = B * k;
		o.c = C * k;
		o.d = Dd * k;
		o.e = E * k;
		o.f = F * k;
		o.g = G * k;
		o.h = H * k;
		return o;
	}

	inline void NkMatPoint(const NkMat2D &m, float32 &x, float32 &y) {
		if (m.Identite())
			return;
		const float32 nx = m.a * x + m.c * y + m.e;
		const float32 ny = m.b * x + m.d * y + m.f;
		if (m.Affine()) { // le chemin d'avant, sans une operation de plus
			x = nx;
			y = ny;
			return;
		}
		// LA DIVISION PAR `w` : c'est ELLE la perspective. Tout le reste (les coins
		// d'un rect, les 48 points d'une ellipse, les sommets d'un trace, le
		// pointage par l'inverse) passe par ici et en herite.
		float32 w = m.g * x + m.h * y + 1.f;
		if (w < NkWMin())
			w = NkWMin(); // derriere l'oeil : pince, jamais retourne
		x = nx / w;
		y = ny / w;
	}

	/// L'INVERSE. ⚠️ Elle rend l'identité si le déterminant est nul — ce qui
	/// n'arrive qu'avec une échelle nulle, que ce modèle ne produit pas ; le
	/// repli existe pour que le picking désigne « la boîte droite » plutôt que
	/// de rendre des coordonnées infinies qui feraient disparaître le nœud.
	/// APPLIQUER une matrice a un CONTOUR (suite de x,y).
	/// 🔑 C'est la porte qui manquait au DESSIN : le pointage lisait deja
	///    `NkMatEffective`, le dessin n'appliquait que la transformee PROPRE
	///    du noeud. Les deux lisent desormais la meme composition.
	inline void NkMatContour(const NkMat2D &m, float32 *xy, uint32 nb) {
		if (!xy || m.Identite())
			return;
		for (uint32 i = 0; i < nb; ++i)
			NkMatPoint(m, xy[i * 2u], xy[i * 2u + 1u]);
	}

	inline NkMat2D NkMatInverse(const NkMat2D &m) {
		NkMat2D o;
		if (m.Affine()) { // le chemin d'avant, inchange
			const float32 det = m.a * m.d - m.b * m.c;
			if (det == 0.f)
				return o;
			const float32 k = 1.f / det;
			o.a = m.d * k;
			o.b = -m.b * k;
			o.c = -m.c * k;
			o.d = m.a * k;
			o.e = (m.c * m.f - m.d * m.e) * k;
			o.f = (m.b * m.e - m.a * m.f) * k;
			return o;
		}
		// L'INVERSE D'UNE HOMOGRAPHIE EST UNE HOMOGRAPHIE : l'adjointe de la 3x3,
		// normalisee. C'est ce qui fait que LE POINTAGE SUIT LA PERSPECTIVE sans
		// une ligne de code a lui -- `NkPointDansNoeud` ramene le point dans le
		// repere du nœud et teste la boite droite, exactement comme avant.
		const float32 A = m.d - m.f * m.h;
		const float32 B = m.f * m.g - m.b;
		const float32 G = m.b * m.h - m.d * m.g;
		const float32 C = m.e * m.h - m.c;
		const float32 Dd = m.a - m.e * m.g;
		const float32 H = m.c * m.g - m.a * m.h;
		const float32 E = m.c * m.f - m.e * m.d;
		const float32 F = m.e * m.b - m.a * m.f;
		const float32 I = m.a * m.d - m.b * m.c;
		if (I == 0.f)
			return o;
		const float32 k = 1.f / I;
		o.a = A * k;
		o.b = B * k;
		o.c = C * k;
		o.d = Dd * k;
		o.e = E * k;
		o.f = F * k;
		o.g = G * k;
		o.h = H * k;
		return o;
	}

	// ── LA BORNE DE L'INCLINAISON, ET SA RAISON, AU MEME ENDROIT ─────────
	//
	// ⚠️ ELLE VIT ICI, A COTE DE LA MATRICE, ET PAS DANS LE PANNEAU : ce n'est pas
	//    un gout d'interface, c'est une propriete de la GEOMETRIE. A 90 degres le
	//    cosinus s'annule, le determinant tombe a zero, la forme se reduit a un
	//    TRAIT -- et `NkMatInverse` rend alors l'identite, donc le pointage
	//    designerait la boite droite d'une forme invisible.
	//    *Un reglage qui permet de faire disparaitre une forme sans le dire est un
	//    piege.* Ecrite la, elle se mesure AVEC ce qu'elle protege : l'essai 144
	//    exige que la matrice reste inversible A CETTE borne. La changer pour 90
	//    fait donc rougir l'essai -- la borne et sa raison ne peuvent plus diverger.
	inline float32 NkInclinaisonMax() {
		return 80.f;
	}

	/// La matrice d'UN nœud : ses trois champs, autour du centre `(cx, cy)`.
	inline NkMat2D NkMatDe(const NkTransfo &t, float32 cx, float32 cy) {
		NkMat2D m;
		if (t.Identite())
			return m;
		float32 s = 0.f, c = 1.f;
		if (t.deg != 0.f)
			NkSinCosDeg(t.deg, s, c);
		// miroir d'abord (échelle ±1), rotation ensuite — le même ordre que
		// `NkTransfoPoint`, et pour la même raison.
		// L'echelle du noeud, au signe du miroir : une seule matrice pour le
		// dessin et le pointage (NkMatInverse la retourne telle quelle).
		const float32 sx = (t.mh ? -1.f : 1.f) * t.sx;
		const float32 sy = (t.mv ? -1.f : 1.f) * t.sy;
		m.a = c * sx;
		m.b = s * sx;
		m.c = -s * sy;
		m.d = c * sy;
		// ── L'INCLINAISON, DERIVEE ET NON POSTULEE ────────────────────────────
		//
		// Un point du plan du nœud est (x, y, 0). On l'incline autour de X puis
		// autour de Y, et on regarde le resultat SANS perspective (projection
		// orthographique -- on laisse tomber z) :
		//
		//   apres Rx(a) :  (x,           y*cos a,   y*sin a)
		//   apres Ry(b) :  x' = x*cos b + z*sin b = x*cos b + y*sin a*sin b
		//                  y' = y*cos a
		//
		// Soit exactement une ECHELLE plus un CISAILLEMENT :
		//
		//        | cos b   sin a * sin b |
		//   T =  |   0         cos a     |
		//
		// ⚠️ ET LE CISAILLEMENT N'APPARAIT QUE SI LES DEUX ANGLES SONT NON NULS.
		//    Incliner autour d'un seul axe n'est qu'un ECRASEMENT -- c'est la
		//    verite geometrique d'un quadrilatere PLAT vu sans perspective, et
		//    non une approximation paresseuse. Le trapeze que l'œil attend d'une
		//    vraie perspective n'est PAS affine : il demanderait un `w` au sommet.
		//    *On applique ce qui est vrai, et on ecrit ce qui manque.*
		if (t.iX != 0.f || t.iY != 0.f) {
			float32 sa = 0.f, ca = 1.f, sb = 0.f, cb = 1.f;
			if (t.iX != 0.f)
				NkSinCosDeg(t.iX, sa, ca);
			if (t.iY != 0.f)
				NkSinCosDeg(t.iY, sb, cb);
			// M' = M . T -- l'inclinaison agit DANS le repere du nœud, donc avant
			// le miroir, l'echelle et la rotation, qui la voient comme une forme.
			const float32 ta = cb, tc = sa * sb, td = ca;
			const float32 a2 = m.a * ta, b2 = m.b * ta;
			const float32 c2 = m.a * tc + m.c * td, d2 = m.b * tc + m.d * td;
			m.a = a2;
			m.b = b2;
			m.c = c2;
			m.d = d2;
			// ── LA PERSPECTIVE, DERIVEE ELLE AUSSI (11/09, palier A) ──────────
			//
			// On garde le `z` au lieu de le jeter. Dans le repere CENTRE du nœud :
			//
			//   apres Rx(a) puis Ry(b) :  z' = -x*sin b + y*sin a*cos b
			//
			// L'oeil est a la distance `focale` devant le plan ; un point remonte
			// vers lui d'autant que son `z'` est grand :
			//
			//   w = 1 - z'/focale = 1 + (x*sin b - y*sin a*cos b) / focale
			//
			// et le point rendu est (T·p) / w. C'est exactement la projection
			// orthographique d'au-dessus DIVISEE PAR w -- a focale infinie, w = 1
			// et les deux se confondent. *Ce qui manquait n'etait pas une matrice
			// de plus : c'etait la division.*
			if (t.PerspectiveActive()) {
				const float32 gz = sb / t.focale;
				const float32 hz = -sa * cb / t.focale;
				// la meme mise au centre que l'affine, mais en 3x3 :
				//   p' = C + (L·(p - C)) / (1 + (g,h)·(p - C))
				// on developpe pour rester dans la forme « w = g x + h y + 1 »
				const float32 i0 = 1.f - gz * cx - hz * cy;
				if (i0 != 0.f) {
					const float32 k = 1.f / i0;
					const float32 la = m.a, lb = m.b, lc = m.c, ld = m.d;
					m.a = (la + cx * gz) * k;
					m.c = (lc + cx * hz) * k;
					m.b = (lb + cy * gz) * k;
					m.d = (ld + cy * hz) * k;
					m.e = (cx * i0 - (la * cx + lc * cy)) * k;
					m.f = (cy * i0 - (lb * cx + ld * cy)) * k;
					m.g = gz * k;
					m.h = hz * k;
					return m; // le recentrage est DEJA fait
				}
			}
		}
		// puis on recentre : p' = C + R·S·(p - C)
		m.e = cx - (m.a * cx + m.c * cy);
		m.f = cy - (m.b * cx + m.d * cy);
		return m;
	}

	/// LA MATRICE EFFECTIVE d'un nœud : la sienne, composée avec celles de tous
	/// ses ancêtres, chacune autour de SON PROPRE CENTRE.
	/// ⚠️ L'ORDRE DE COMPOSITION VA DE L'ANCÊTRE VERS L'ENFANT : la transformation
	///    du groupe s'applique APRÈS celle de l'élément, parce qu'elle agit sur le
	///    résultat de celle-ci. Composé à l'envers, un élément déjà tourné dans un
	///    groupe tourné partirait dans une direction que personne ne peut prévoir.
	/// @param lay les rectangles NON transformés (la disposition les calcule
	///        droits — c'est voulu : lui faire porter des angles l'obligerait à
	///        produire des rectangles non alignés, qu'aucun de ses consommateurs
	///        ne sait lire).
	/// `avecPerspective = false` : la MEME matrice, projection ORTHOGONALE -- ce que
	/// le nœud serait sans la fuite. Une seule lectrice aujourd'hui, l'export SVG,
	/// qui n'a pas de perspective dans son format et l'ECRIT (`data-projection`)
	/// plutot que de laisser croire a une silhouette qu'il ne sait pas rendre.
	inline NkMat2D NkMatEffective(const NkUIDocument &doc, const NkLayoutResult &lay, int32 i,
								  bool avecPerspective = true);

	inline NkMat2D NkMatEffective(const NkUIDocument &doc, const NkLayoutResult &lay, int32 i,
								  bool avecPerspective) {
		NkMat2D m;
		// ── LE REFUS PAR AXE, ICI ET NULLE PART AILLEURS ────────────────────
		// Un enfant peut refuser d'heriter un axe de ses ancetres : la rotation
		// (miroirs compris : un miroir est une orientation, pas une taille),
		// l'echelle, la position. Le filtre s'applique en composant les
		// ancetres ; le dessin et le pointage lisent cette matrice-la, donc un
		// refus honore a l'ecran est honore sous la souris, sans second code.
		const NkUINode *moi = doc.IsValidIndex(i) ? &doc.nodes[(uint32)i] : nullptr;
		const bool refR = moi && moi->refusRotation;
		const bool refE = moi && moi->refusEchelle;
		const bool refP = moi && moi->refusPosition;
		int32 k = i;
		uint32 garde = 0;
		while (doc.IsValidIndex(k) && k > 0 && garde++ < 256u) {
			const NkUINode &n = doc.nodes[(uint32)k];
			NkTransfo t = NkTransfoDe(n);
			if (!avecPerspective)
				t.persp = false;
			if (refR) {
				t.deg = 0.f;
				t.mh = false;
				t.mv = false;
			}
			if (refE) {
				t.sx = 1.f;
				t.sy = 1.f;
			}
			if (!t.Identite() && lay.Has(k)) {
				const NkPaintRect r = lay.At(k);
				const NkMat2D mk = NkMatDe(t, r.x + r.w * 0.5f, r.y + r.h * 0.5f);
				m = NkMatComposer(mk, m); // l'ancêtre s'applique APRÈS
			}
			k = n.parent;
		}
		if (refP && moi && lay.Has(i)) {
			// La position ne suit pas les transformees des ancetres : le centre
			// reste ou la mise en page l'a mis (le noeud tourne ou s'agrandit
			// encore autour de ce centre, s'il ne refuse pas ces axes-la).
			const NkPaintRect r = lay.At(i);
			const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
			float32 x = cx, y = cy;
			NkMatPoint(m, x, y);
			m.e += cx - x;
			m.f += cy - y;
		}
		return m;
	}

	/// LE POINTAGE, VERSION MATRICE — celui que la toile utilise.
	inline bool NkPointDansNoeud(const NkUIDocument &doc, const NkLayoutResult &lay, int32 i,
								 float32 px, float32 py) {
		if (!doc.IsValidIndex(i) || !lay.Has(i))
			return false;
		const NkMat2D inv = NkMatInverse(NkMatEffective(doc, lay, i));
		float32 x = px, y = py;
		NkMatPoint(inv, x, y);
		const NkPaintRect r = lay.At(i);
		return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
	}

	/// LE POINTAGE DE LA TOILE, TRANSFORMATIONS COMPRISES.
	/// ⚠️ C'EST LE JUMEAU DE `NkPickNode` (Layout.h) ET IL VIT ICI, PAS LÀ-BAS,
	///    pour une raison de dépendance et non de goût : `Transfo.h` inclut
	///    `Layout.h`, donc l'inverse ferait un cycle. Les deux sont des chemins
	///    frères et **doivent départager de la même façon** — à profondeur égale,
	///    le DERNIER de l'ordre document gagne (`>=`). Cette règle-là a été payée
	///    par le 4e retour de Rodolf (« Aide e-mail » inatteignable sous son
	///    frère) : la réécrire à l'envers ici rendrait le défaut, mais seulement
	///    sur les documents qui portent une rotation.
	/// ⚠️ ET IL SE DÉGRADE EXACTEMENT EN `NkPickNode` quand rien n'est transformé
	///    (`NkMatEffective` rend l'identité, `NkPointDansNoeud` retombe sur le
	///    test de boîte) : un document sans rotation pointe au bit près comme
	///    hier, et les contrôles 40x qui le mesurent gardent leur sens.
	inline int32 NkPickNodeTransfo(const NkUIDocument &doc, const NkLayoutResult &lay, float32 x,
								   float32 y) {
		int32 best = -1, bestDepth = -1;
		for (uint32 i = 0; i < (uint32)doc.nodes.Size(); ++i) {
			if (!lay.Has((int32)i))
				continue;
			// ── VERROUILLE / MASQUE : LE CLIC LE TRAVERSE (vague 2) ──────────
			// ⚠️ ET IL LE TRAVERSE VRAIMENT : le `continue` laisse la boucle se
			//    poursuivre, donc c'est le nœud DERRIERE qui est attrape. Un
			//    `return -1` aurait fait du verrou un TROU dans la toile -- on
			//    verrouille un fond de page, et plus rien au-dessus ne se
			//    selectionne. *Rendre un objet inattrapable, ce n'est pas rendre
			//    sa surface inerte.*
			if (!NkNoeudAttrapable(doc, (int32)i))
				continue;
			if (!NkPointDansNoeud(doc, lay, (int32)i, x, y))
				continue;
			int32 depth = 0;
			for (int32 c = doc.nodes[i].parent; c >= 0; c = doc.nodes[(uint32)c].parent)
				++depth;
			if (depth >= bestDepth) {
				bestDepth = depth;
				best = (int32)i;
			}
		}
		return best;
	}

	/// L'englobant écran d'un nœud, sa transformation effective comprise.
	inline NkPaintRect NkEnglobantEcran(const NkUIDocument &doc, const NkLayoutResult &lay,
										int32 i) {
		const NkPaintRect r = lay.Has(i) ? lay.At(i) : NkPaintRect{0.f, 0.f, 0.f, 0.f};
		const NkMat2D m = NkMatEffective(doc, lay, i);
		if (m.Identite())
			return r;
		float32 xy[8] = {r.x, r.y, r.x + r.w, r.y, r.x + r.w, r.y + r.h, r.x, r.y + r.h};
		for (uint32 k = 0; k < 4; ++k)
			NkMatPoint(m, xy[k * 2], xy[k * 2 + 1]);
		float32 x0 = xy[0], y0 = xy[1], x1 = xy[0], y1 = xy[1];
		for (uint32 k = 1; k < 4; ++k) {
			if (xy[k * 2] < x0)
				x0 = xy[k * 2];
			if (xy[k * 2] > x1)
				x1 = xy[k * 2];
			if (xy[k * 2 + 1] < y0)
				y0 = xy[k * 2 + 1];
			if (xy[k * 2 + 1] > y1)
				y1 = xy[k * 2 + 1];
		}
		return {x0, y0, x1 - x0, y1 - y0};
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  LES POIGNÉES DE ROTATION — LEUR GÉOMÉTRIE EST UN CALCUL, PAS UN DESSIN
	// ═══════════════════════════════════════════════════════════════════════════
	/// ⚠️ CE QUE MONTRE `lunacy_props_11_040102.png`, RELU AU PIXEL : les poignées
	///    de rotation ne sont PAS des carrés de plus sur la boîte. Ce sont quatre
	///    zones **en dehors** des poignées de redimensionnement, en diagonale de
	///    chaque coin, et le curseur y devient une flèche courbe.
	///
	/// ⚠️ ET C'EST CE « EN DEHORS » QUI EST TOUT LE CONTRAT : posées SUR les
	///    coins, elles auraient volé le geste de redimensionnement, qui est cent
	///    fois plus fréquent. La règle se cite : **le coin redimensionne, son
	///    extérieur tourne.**
	/// @param k 0..3 = haut-gauche, haut-droit, bas-droit, bas-gauche.
	inline NkPaintRect NkPoigneeRotation(const NkPaintRect &r, uint32 k, float32 taille) {
		const float32 d = taille; // le décalage vers l'extérieur, en diagonale
		const float32 h = taille * 0.5f;
		switch (k & 3u) {
			case 0: return {r.x - d - h, r.y - d - h, taille, taille};
			case 1: return {r.x + r.w + d - h, r.y - d - h, taille, taille};
			case 2: return {r.x + r.w + d - h, r.y + r.h + d - h, taille, taille};
			default: return {r.x - d - h, r.y + r.h + d - h, taille, taille};
		}
	}
	/// LA TAILLE de la zone attrapable d'une poignee de rotation, en pixels
	/// ECRAN. Elle sert AUX DEUX endroits qui la lisent -- celui qui RECLAME
	/// le clic et celui qui PEINT l'arc. Deux nombres auraient diverge, et la
	/// zone cliquable se serait decollee du dessin sans que rien ne le dise.
	inline float32 NkTaillePoigneeRotation() {
		return 9.f;
	}
	/// ② LA TOLERANCE DES POIGNEES, en pixels ECRAN (independante du zoom) : la zone
	///    saisissable couvre ce qui est dessine, ETENDUE de ce rayon -- arcs de rotation
	///    (distance au centre de l'arc) et poignees de forme (bandes des bords). Rodolf,
	///    04/09 soir : « ce n'est pas mieux de donner une zone de detection un peu grande ? ».
	inline float32 NkTolerancePoignee() {
		return 12.f;
	}
	/// La bande d'un bord pour un noeud de taille `cote` : la tolerance, bornee au tiers du
	/// noeud pour qu'un petit noeud garde un corps a saisir (deplacer).
	inline float32 NkBandeBord(float32 cote) {
		const float32 t = NkTolerancePoignee();
		const float32 tiers = cote / 3.f;
		return tiers < t ? (tiers < 2.f ? 2.f : tiers) : t;
	}
	inline uint32 NkNbPoigneesRotation() {
		return 4u;
	}

	/// L'ANGLE, en degrés horaires, du vecteur centre -> point.
	/// Sert au glisser : l'angle sous la souris moins l'angle au début du geste.
	/// ⚠️ `atan2` n'existe pas ici non plus : approximation polynomiale par
	///    octants, exacte à ~0,1° — bien en deçà du pas d'un degré que le champ
	///    affiche.
	inline float32 NkAngleDeg(float32 cx, float32 cy, float32 px, float32 py) {
		const float32 dx = px - cx, dy = py - cy;
		if (dx == 0.f && dy == 0.f)
			return 0.f;
		const float32 ax = dx < 0.f ? -dx : dx;
		const float32 ay = dy < 0.f ? -dy : dy;
		// atan(z) approché sur [0,1], puis replié par octant.
		const float32 z = (ax > ay) ? (ay / ax) : (ax / ay);
		const float32 z2 = z * z;
		float32 a = z * (0.9998660f
						 + z2 * (-0.3302995f + z2 * (0.1801410f + z2 * (-0.0851330f
																	   + z2 * 0.0208351f))));
		a *= 57.29577951f; // en degrés
		if (ay > ax)
			a = 90.f - a;
		if (dx < 0.f)
			a = 180.f - a;
		if (dy < 0.f)
			a = -a;
		return a;
	}

	/// NORMALISER un angle dans [0, 360) — ce que le champ affiche.
	/// ⚠️ SANS ELLE, TOURNER TROIS FOIS AFFICHERAIT « 1080° » et le fichier
	///    porterait un nombre qui grandit sans fin. Un angle est une direction,
	///    pas un compteur de tours.
	inline float32 NkAngleNormalise(float32 deg) {
		if (deg >= 0.f && deg < 360.f)
			return deg;
		float32 a = deg;
		uint32 garde = 0;
		while (a < 0.f && garde++ < 4096u)
			a += 360.f;
		garde = 0;
		while (a >= 360.f && garde++ < 4096u)
			a -= 360.f;
		return a;
	}

	/// L'ANGLE AIMANTÉ aux quarts de tour quand `maj` est tenue (Lunacy, Figma).
	/// Le pas est 15° — celui de Lunacy, pas un choix maison.
	inline float32 NkAngleAimante(float32 deg, bool maj) {
		if (!maj)
			return deg;
		const float32 pas = 15.f;
		const float32 k = deg / pas;
		const int32 n = (int32)(k >= 0.f ? k + 0.5f : k - 0.5f);
		return (float32)n * pas;
	}

	/// LE RECTANGLE ENGLOBANT D'UN NŒUD TOURNÉ, en espace écran.
	/// ⚠️ IL N'EST PAS ÉGAL À SA BOÎTE : une carte de 300×40 tournée de 45°
	///    occupe un carré bien plus grand. Les consommateurs qui décident d'un
	///    CADRAGE (le survol, la boîte de multi-sélection, l'aimantation) doivent
	///    lire celui-ci, sinon ils dessinent un cadre qui coupe l'objet.
	inline NkPaintRect NkEnglobantTransfo(const NkTransfo &t, const NkPaintRect &r) {
		if (t.Identite())
			return r;
		float32 xy[8] = {r.x, r.y, r.x + r.w, r.y, r.x + r.w, r.y + r.h, r.x, r.y + r.h};
		NkTransfoContour(t, r, xy, 4);
		float32 x0 = xy[0], y0 = xy[1], x1 = xy[0], y1 = xy[1];
		for (uint32 i = 1; i < 4; ++i) {
			if (xy[i * 2] < x0)
				x0 = xy[i * 2];
			if (xy[i * 2] > x1)
				x1 = xy[i * 2];
			if (xy[i * 2 + 1] < y0)
				y0 = xy[i * 2 + 1];
			if (xy[i * 2 + 1] > y1)
				y1 = xy[i * 2 + 1];
		}
		return {x0, y0, x1 - x0, y1 - y0};
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  CE QUE LE PEINTRE SAIT TOURNER — ET CE QU'IL NE SAIT PAS, DIT ICI
	// ═══════════════════════════════════════════════════════════════════════════
	/// ⚠️ MESURE À LA SOURCE (`NkComponentPaint.h`, 01/09), pas une supposition :
	///    `PolygonHex(xy, count, rgba)` prend une LISTE DE POINTS quelconque —
	///    donc toute forme qui passe par un contour se peint tournée sans qu'on
	///    invente une primitive. `Text(rect, ...)` prend un `NkPaintRect`, qui est
	///    **droit par construction** : il n'y a aucun moyen de peindre du texte
	///    tourné avec ce peintre.
	///
	///    LA RÈGLE, DONC, ET ELLE SE DIT À L'UTILISATEUR PLUTÔT QUE DE MENTIR :
	///    un nœud qui porte du texte accepte la rotation dans son MODÈLE (elle
	///    s'enregistre, elle se relit, elle s'annule) et son texte reste DROIT à
	///    l'écran. L'alternative aurait été de refuser le champ sur les textes —
	///    mais alors tourner un groupe qui contient un texte aurait échoué à
	///    moitié, sans qu'on sache pourquoi.
	inline bool NkPeintureSaitTourner(const NkUINode &n) {
		// Depuis la transformee du peintre (NkComponentPaint::PushTransform,
		// poussee par NkDrawDocument), TOUT ce qu'un noeud dessine tourne avec
		// lui : formes, composants, texte (sonde 45d, verifie au pied dans le
		// document de Rodolf). Il n'y a plus de rotation partielle a annoncer.
		(void)n;
		return true;
	}

	/// La phrase qui accompagne une rotation que le peintre ne rendra pas.
	/// **Jamais vide** — même règle que `NkRaisonDeDblClic`.
	inline const char *NkRaisonRotationTexte() {
		return "Rotation enregistrée — ce peintre ne sait pas tourner du texte, le "
			   "libellé reste droit à l'écran.";
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  CE QUI PEUT TOURNER AUJOURD'HUI — ET LE REFUS EST DIT, PAS SUBI
	// ═══════════════════════════════════════════════════════════════════════════
	/// ⚠️ LA DÉCISION LA PLUS IMPORTANTE DE CE LOT, ET ELLE EST DE NE PAS LIVRER.
	///    L'arbitrage de Q42 dit mot pour mot : *« un demi-champ de rotation est
	///    pire que pas de rotation. »* Voici la mesure qui m'a fait m'arrêter :
	///
	///    Le PICKING d'un enfant de groupe tourné est juste (`NkMatEffective`,
	///    cas 14 et 15 : mesuré, éprouvé par mutation). Le RENDU, lui, ne l'est
	///    pas : la disposition calcule des rectangles DROITS pour les enfants, et
	///    le peintre reçoit ces rectangles-là. Un groupe tourné aurait donc des
	///    enfants **qu'on attrape à leur place tournée et qu'on voit à leur place
	///    droite** — l'objet et son ombre à deux endroits différents.
	///
	///    ⚠️ C'EST PIRE QUE « ÇA NE TOURNE PAS ». Un refus s'explique en une
	///       phrase ; un clic qui atterrit à côté de ce qu'on voit se diagnostique
	///       en une heure, et fait douter de tout le reste de l'outil.
	///
	///    LA ROTATION EST DONC OFFERTE SUR LES FEUILLES, ET REFUSÉE-QUI-LE-DIT
	///    sur les nœuds à enfants. Ce n'est pas un manque du modèle : le modèle,
	///    la matrice et le picking sont prêts et mesurés. **Il manque un seul
	///    maillon, et il est nommé** — que le peintre reçoive la matrice
	///    effective de ses ancêtres, ce qui veut dire la faire descendre depuis
	///    `NkDrawNodeTree` jusqu'à chaque branche de `DrawShape`.
	/// 🔴 ELLE REFUSAIT TOUT NŒUD À ENFANTS, ET C'ÉTAIT UN BLOCAGE, PAS UNE
	///    GARDE (Rodolf, 03/09 : *« pas toujours de poignée de rotation
	///    visible »*). **Son objet de travail est `Bouton_Connexion` — un bouton
	///    avec un libellé dedans, donc un nœud à enfants.** La règle interdisait
	///    donc le geste sur l'objet le plus courant de l'application, et il ne
	///    voyait rien : ni poignée, ni raison, puisqu'on ne dessine pas ce qu'on
	///    refuse. *Une règle qui refuse le geste sur l'objet le plus courant
	///    n'est pas une garde, c'est un blocage.*
	///
	/// ⚠️ ET LA MESURE DIT QUE RIEN NE CASSE. Le dessin recurse dans les enfants
	///    SANS transformation du parent (`NkDrawDocument`, fin de fonction) :
	///    le parent se dessine tourné ET se clique tourné (même contour), chaque
	///    enfant se dessine droit ET se clique droit. **Aucun objet n'est à deux
	///    endroits** — c'est ce que craignait l'ancienne raison, et ça n'arrive
	///    pas. Ce qui manque est la rotation D'ENSEMBLE, pas la cohérence.
	///
	/// 🔑 ET LE PRÉCÉDENT ÉTAIT DÉJÀ DANS CE FICHIER : pour le TEXTE, on a
	///    choisi d'enregistrer, de dessiner ce qu'on sait, et de DIRE le reste
	///    (`NkPeintureSaitTourner`) — au lieu de refuser le champ. Le commentaire
	///    d'à côté explique même pourquoi : refuser aurait fait « échouer à
	///    moitié, sans qu'on sache pourquoi ». La même règle vaut ici, et elle
	///    n'avait pas traversé les vingt lignes qui séparent les deux fonctions.
	inline bool NkPeutTourner(const NkUINode &n) {
		// Le refus par axe : un noeud qui refuse la rotation ne tourne ni par
		// ses ancetres (NkMatEffective) ni par lui-meme (poignee, champ).
		return !n.refusRotation;
	}

	/// Vrai si la rotation de `n` emporte TOUT ce qu'on voit à sa place.
	/// Faux = elle est PARTIELLE, et l'interface doit le dire.
	inline bool NkRotationEmporteTout(const NkUINode &n) {
		return n.children.Size() == 0 && NkPeintureSaitTourner(n);
	}

	/// Ce que la rotation ne fera PAS, en toutes lettres. **Jamais vide** quand
	/// `NkRotationEmporteTout` est faux.
	/// ⚠️ Elle NOMME le manque au lieu de le taire : un bouton qui tourne
	///    pendant que son libellé reste droit doit s'expliquer à l'instant où ça
	///    se voit, pas dans une note de version.
	inline const char *NkRaisonRotationPartielle(const NkUINode &n) {
		if (!n.children.Empty() && !NkPeintureSaitTourner(n))
			return "Rotation — le contenu et le texte restent droits (rotation d'ensemble : "
				   "pas encore).";
		if (!n.children.Empty())
			return "Rotation — les éléments contenus restent droits (rotation d'ensemble : "
				   "pas encore).";
		return "Rotation enregistrée — ce peintre ne sait pas tourner du texte, le "
			   "libellé reste droit à l'écran.";
	}

	/// Conservée : d'anciens sites la citent encore.
	inline const char *NkRaisonPasDeRotation() {
		return "Rotation refusée sur ce nœud (refus par axe « R », dans l'inspecteur) : "
			   "ni la sienne, ni celle de ses parents.";
	}

} // namespace nkuidesign
