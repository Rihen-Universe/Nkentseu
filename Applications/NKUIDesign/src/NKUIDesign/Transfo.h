#pragma once
// -----------------------------------------------------------------------------
// @File    Transfo.h
// @Brief   ROTATION ET MIROIRS — la transformation autour du centre, en MÉCANISME :
//          ce qu'elle fait au dessin, ce qu'elle fait au pointage, et ce qu'elle
//          propage aux enfants.
// @Author  Rihen
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
			/// Vrai quand il n'y a rien à faire — le cas de l'immense majorité des
			/// nœuds. ⚠️ IL EST INTERROGÉ AVANT TOUT CALCUL : sans ce court-circuit,
			/// chaque point de chaque forme paierait un sinus, et surtout chaque
			/// contour repasserait par un aller-retour en flottant qui déplacerait
			/// les coordonnées d'un ulp — un document non tourné cesserait de se
			/// dessiner au pixel près.
			bool Identite() const {
				return deg == 0.f && !mh && !mv;
			}
	};

	inline NkTransfo NkTransfoDe(const NkUINode &n) {
		NkTransfo t;
		t.deg = n.rotation;
		t.mh = n.miroirH;
		t.mv = n.miroirV;
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
		return n.text.Empty() && !NkComponentDecl::StrEq(n.shape.Data(), "text");
	}

	/// La phrase qui accompagne une rotation que le peintre ne rendra pas.
	/// **Jamais vide** — même règle que `NkRaisonDeDblClic`.
	inline const char *NkRaisonRotationTexte() {
		return "Rotation enregistrée — ce peintre ne sait pas tourner du texte, le "
			   "libellé reste droit à l'écran.";
	}

} // namespace nkuidesign
