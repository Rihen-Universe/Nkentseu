#pragma once
// -----------------------------------------------------------------------------
// @File    Applications/NK3DModeler/src/NK3DModeler/Viewport/NkCursorWrap.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
//
// NkCursorWrap.h — (b5) LE CURSEUR REBOUCLE AUX BORDS DE LA VUE PENDANT G/R/S.
//
// Comportement Blender : pendant une operation modale (deplacer, tourner,
// redimensionner), le curseur qui sort d'un bord de la region reapparait au bord
// oppose, et la transformation CONTINUE comme si rien ne s'etait passe. Le geste
// n'est donc plus borne par la taille de l'ecran.
//
// ── POURQUOI UN EN-TETE PUR, SANS AUCUNE DEPENDANCE ────────────────────────
//   Parce que la seule chose difficile ici est une ARITHMETIQUE, et qu'une
//   arithmetique se prouve sans fenetre, sans GPU et sans souris. Le
//   deplacement physique du curseur (`SetCursorPos`) est un detail de
//   plateforme qu'on branche apres ; il ne doit pas rendre la regle intestable.
//   Aucun `#include`, pas meme <cmath> : la valeur absolue tient en un ternaire.
//
// ── LE PIEGE, ET C'EST LUI QUI DICTE LA FORME ──────────────────────────────
//   Replacer le curseur ne se voit pas tout de suite. Sous Win32, `SetCursorPos`
//   produit un `WM_MOUSEMOVE` pompe UNE OU DEUX IMAGES plus tard. Si l'on
//   decalait `lastMouseX` au moment du replacement, on fabriquerait le saut
//   INVERSE pendant l'image intermediaire. Il faut donc un report EN ATTENTE,
//   soustrait seulement quand le delta observe le PORTE reellement.
//
// ── L'ATTENDU, DERIVE (canal `modeleur-blender.questions.md`, R16) ─────────
//   Rebouclage par MODULO : `x >= W` -> `x' = x - W` ; `x < 0` -> `x' = x + W`.
//   Le report vaut donc EXACTEMENT `R = x' - x = -W` ou `+W`. C'est cette
//   exactitude qui rend le critere de consommation derivable plutot que regle a
//   la main :
//
//     a l'image ou le replacement est vu, `d = R + r` avec `r` le deplacement
//     REEL. Un geste humain ne traverse pas la moitie de la vue en une image,
//     donc `|r| < W/2`, donc `signe(d) = signe(R)` ET `|d| > W/2`.
//     Reciproquement un geste reel SEUL a `|d| < W/2`.
//     => le critere « meme signe que le report et `|d| > W/2` » separe
//        exactement les deux cas. Delta corrige = `d - R = r`.
//
//   Suite d'epreuve, W = 800, depart x = 795 :
//     image 1 : x=805, d=+10  -> corrige +10, warp vers 5, report -800
//     image 2 : x=7,   d=-798 -> corrige +2, report consomme
//     somme +12 = le deplacement reel. SANS correction : -788.
//
//   Latence (position pompee perimee une image) : 805 / 805 / 8 -> +10, 0, +3.
//   Le 0 ne porte pas le report (signe nul), donc le report est MAINTENU.
//
// ── LA MUTATION, DANS LE MEME BINAIRE ──────────────────────────────────────
//   `disabled` (pose par `NK_WRAP_NOFIX=1`) retire LA SEULE ligne `d -= R` et
//   rien d'autre : le rebouclage est toujours demande, le report toujours pose.
//   Les suites ci-dessus doivent alors rendre le saut. Une mutation qui
//   survivrait dirait que le critere ne teste rien.
//
// ⚠ UNE SEULE FONCTION. Le produit et la sonde appellent CELLE-CI, pas une
//   copie : deux copies divergeraient au premier changement, et la sonde
//   prouverait alors le comportement de la sonde.
// =============================================================================

namespace nkentseu {

	// Au bout de combien d'images un report non consomme est-il abandonne.
	// ── LA CONDITION DE RETRAIT S'ECRIT AVEC LE CONTOURNEMENT ──────────────
	// Si le replacement echoue (fenetre qui perd le curseur, plateforme sans
	// `SetCursorPos`), le report resterait en attente POUR TOUJOURS et
	// corromprait le premier grand geste suivant. 30 images = une demi-seconde
	// a 60 Hz : largement au-dela des une a deux images de latence observees,
	// largement en-deca d'une gene perceptible.
	static const int NK_CURSOR_WRAP_MAX_AGE = 30;

	// L'etat d'UN axe. Deux axes independants : un rebouclage horizontal n'a
	// rien a dire du vertical, et les melanger obligerait a decider ce que
	// « le report est porte » veut dire quand un seul des deux le porte.
	struct NkCursorWrapAxis {
		bool pending = false; // un replacement est demande et pas encore vu
		float pend = 0.f;	  // le report signe, exactement -W ou +W
		int age = 0;		  // images ecoulees depuis la demande
		unsigned wraps = 0;	  // compteur cumule, pour prouver le ZERO d'abord
	};

	struct NkCursorWrapState {
		NkCursorWrapAxis x;
		NkCursorWrapAxis y;
		// MUTATION : retire la correction, garde tout le reste.
		bool disabled = false;
	};

	struct NkCursorWrapOut {
		float dx = 0.f, dy = 0.f;		// deltas CORRIGES, a donner a la modale
		bool warp = false;				// un replacement est demande cette image
		float warpX = 0.f, warpY = 0.f; // position visee, en px VUE (pas ecran)
	};

	inline float NkCursorWrapAbs(float v) { return v < 0.f ? -v : v; }

	// Un axe, une image. Rend le delta corrige ; remplit `warp`/`warpPos` si un
	// rebouclage est declenche.
	//   cur    : position courante sur l'axe, en px VUE
	//   rawD   : delta brut de l'image (cur - precedent)
	//   extent : taille de la vue sur cet axe
	inline float NkCursorWrapAxisStep(NkCursorWrapAxis &a, bool disabled, float cur, float rawD,
									  float extent, bool &warp, float &warpPos) {
		warp = false;
		warpPos = cur;
		// Une vue degeneree (pas encore dimensionnee, panneau replie) n'a pas de
		// bord utile : on ne reboucle pas, et on LAISSE TOMBER toute attente --
		// sinon un report pose avant un redimensionnement se soustrairait plus
		// tard d'une valeur qui n'a plus cours.
		if (!(extent > 2.f)) {
			a.pending = false;
			a.age = 0;
			return rawD;
		}

		if (a.pending) {
			++a.age;
			// Le delta porte-t-il le report ? Critere derive : meme signe, et
			// amplitude superieure a la demi-vue (voir l'en-tete).
			const bool porte = (rawD * a.pend > 0.f) && (NkCursorWrapAbs(rawD) > extent * 0.5f);
			if (porte) {
				a.pending = false;
				a.age = 0;
				// ⚠ LA SEULE LIGNE QUE LA MUTATION RETIRE.
				return disabled ? rawD : (rawD - a.pend);
			}
			if (a.age > NK_CURSOR_WRAP_MAX_AGE) {
				a.pending = false;
				a.age = 0;
			}
			// Tant qu'un report est en attente, on ne redemande PAS de
			// rebouclage : la latence en empilerait deux.
			return rawD;
		}

		if (cur >= extent)
			warpPos = cur - extent;
		else if (cur < 0.f)
			warpPos = cur + extent;
		else
			return rawD;

		warp = true;
		a.pending = true;
		a.pend = warpPos - cur; // exactement -extent ou +extent
		a.age = 0;
		++a.wraps;
		return rawD; // l'image du declenchement garde son delta REEL
	}

	// Les deux axes d'une image. C'est le point d'entree unique du produit et de
	// la sonde.
	inline void NkCursorWrapStep(NkCursorWrapState &st, float curX, float curY, float rawDX,
								 float rawDY, float extentX, float extentY, NkCursorWrapOut &out) {
		bool wx = false, wy = false;
		float px = curX, py = curY;
		out.dx = NkCursorWrapAxisStep(st.x, st.disabled, curX, rawDX, extentX, wx, px);
		out.dy = NkCursorWrapAxisStep(st.y, st.disabled, curY, rawDY, extentY, wy, py);
		out.warp = (wx || wy);
		out.warpX = px;
		out.warpY = py;
	}

	// Sortie de modale : rien ne doit survivre. Un report garde d'une operation
	// a l'autre s'appliquerait au premier geste de la suivante.
	inline void NkCursorWrapReset(NkCursorWrapState &st) {
		st.x.pending = false;
		st.x.age = 0;
		st.y.pending = false;
		st.y.age = 0;
	}

} // namespace nkentseu
