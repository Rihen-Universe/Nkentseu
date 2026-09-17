// =============================================================================
// NKTexte3D -- « UNE PHRASE -> UN MAILLAGE EDITABLE », EN CONSOLE.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
//
// CE QUE C'EST, ET SURTOUT CE QUE CE N'EST PAS.
//   Ce n'est PAS un modele de langage, et ce n'est pas « texte vers 3D » au sens
//   ou le grand public l'entend. C'est une GRAMMAIRE RESTREINTE : un vocabulaire
//   de six formes et de quatre operateurs, qui se compile en un champ scalaire,
//   duquel gen::SurfaceNets extrait une surface. Le jour ou un modele entraine
//   chez Rihen saura ecrire CETTE grammaire, il remplacera l'analyseur sans
//   qu'une ligne du reste ne bouge -- c'est le meme pari que NkIGenerateur cote
//   image : un contrat etroit, et le producteur derriere lui est interchangeable.
//
//   Constat du 2026-09-17, mesure et non suppose : au moment ou ce fichier est
//   ecrit, « texte -> 3D » n'existait NULLE PART dans le depot. `NkIGenerateur`
//   n'a qu'une methode, et elle prend une IMAGE. `NKGen/NkGen.h:10` annonce le
//   conditionnement texte comme « a venir ». Ceci est donc le premier chemin, et
//   il est volontairement petit.
//
// POURQUOI DU .OBJ ET PAS DU glTF.
//   Il n'existe AUCUN ecrivain glTF dans le depot (`NkUVUnwrap.h:244` le dit,
//   `NkGLTFIO.h:50` et `NkOBJIO.cpp:82` le confirment). Le SEUL ecrivain de
//   maillage de la maison est `gen::SaveMeshObj`. Et cela suffit : `LoadOBJ`
//   remplit la MEME structure que `LoadGLTF` (`NkGLTFMeshData`), et la porte de
//   liste du modeleur accepte « .obj » au meme titre que « .glb »
//   (`NkModelerImport.h:116`). La porte d'edition est donc atteinte sans ecrire
//   un seul octet de glTF.
//
// LES DEUX INSTRUMENTS, ET ILS N'ONT AUCUNE LIGNE COMMUNE.
//   [I] l'ecriture : mon champ scalaire -> gen::SurfaceNets -> gen::SaveMeshObj.
//   [II] la relecture : LoadOBJ -> NkEditMesh::BuildFromIndexed -> NkMeshAnalysis.
//   Le second n'a pas ete ecrit pour ce chantier et ne connait pas ma grammaire.
//   C'est lui qui prononce l'etancheite et la CARACTERISTIQUE D'EULER.
//
// L'ATTENDU EST DERIVE, PAS CHOISI.
//   Sur un maillage FERME et manifold, V - E + F = 2 - 2g, ou g est le nombre
//   d'anses (`NkMeshAnalysis.h`, en-tete de `euler`). Donc :
//       une sphere      -> chi = 2   (g = 0)
//       un tore         -> chi = 0   (g = 1)
//       un cube perce   -> chi = 0   (g = 1)   un trou traversant = une anse
//       deux spheres    -> chi = 4   (2 + 2)
//   Ces quatre valeurs DISTINGUENT : aucun repli, aucun objet par defaut, aucune
//   erreur de grammaire ne peut rendre les quatre a la fois. Un temoin qui ne
//   distingue rien est une decoration.
//
// LA MUTATION, ET ELLE EST DANS LE MEME BINAIRE.
//   `NK_TEXTE3D_MUTE=1` fait ignorer la forme analysee et emet TOUJOURS une
//   sphere. Le cas « un tore » doit alors passer de chi=0 a chi=2 et ROUGIR.
//   Une variable d'environnement plutot que deux constructions : deux binaires
//   peuvent differer par autre chose.
//
// LE ZERO SE PROUVE D'ABORD.
//   Une demande vide, un mot inconnu, une sortie qui n'est pas .obj : chacun
//   REFUSE avec un motif NOMME, code 2, et n'ecrit AUCUN fichier. Jamais un
//   objet par defaut en silence -- « un refus nomme sa cause ».
//
// USAGE
//   NKTexte3D --texte "<phrase>" --out <fichier.obj> [--res N]
//   NKTexte3D --banc [--res N]        code 0 = VERT, 1 = ROUGE, 2 = REFUS
// =============================================================================
#include "NKGen/NkMesh.h"

#include "NKRenderer/Mesh/NkEditMesh.h"
#include "NKRenderer/Mesh/NkMeshAnalysis.h"
#include "NKRenderer/Mesh/NkOBJLoader.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace nkentseu;
using namespace nkentseu::renderer;
namespace gen = nkentseu::ai::gen;

// ─────────────────────────────────────────────────────────────────────────────
// 0. LE REFUS. Il nomme sa cause, il va sur stderr, il rend 2, il n'ecrit rien.
// ─────────────────────────────────────────────────────────────────────────────
static int Refus(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	fputs("REFUS : ", stderr);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	fflush(stderr);
	return 2;
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. LA GRAMMAIRE. Six formes, quatre operateurs, et rien d'autre.
// ─────────────────────────────────────────────────────────────────────────────
enum Forme { F_SPHERE = 0, F_CUBE, F_CYLINDRE, F_CONE, F_TORE, F_CAPSULE, F_AUCUNE };

static const char *NomForme(Forme f) {
	switch (f) {
		case F_SPHERE: return "sphere";
		case F_CUBE: return "cube";
		case F_CYLINDRE: return "cylindre";
		case F_CONE: return "cone";
		case F_TORE: return "tore";
		case F_CAPSULE: return "capsule";
		default: return "(aucune)";
	}
}

// ── LA DEMI-HAUTEUR EN Y, ET ELLE EST LUE DANS `SdfPiece`, PAS DEVINEE ──────
// Chaque forme a la sienne, et c'est la cause reelle du defaut du 17/09 : le
// placement utilisait le RAYON ENGLOBANT `s` pour tout le monde, si bien que
// le jour entre deux formes empilees n'etait meme pas constant d'une forme a
// l'autre. Les valeurs ci-dessous sont recopiees de `SdfPiece` ; si l'une y
// change, elle doit changer ici -- c'est une dette, et elle est nommee.
static float DemiHauteurY(Forme f, float s) {
	switch (f) {
		case F_SPHERE: return s;              // rayon
		case F_CUBE: return 0.62f * s;        // demi-cote
		case F_CYLINDRE: return 0.90f * s;    // h
		case F_CONE: return 0.95f * s;        // h
		case F_TORE: return 0.28f * s;        // petit rayon r
		case F_CAPSULE: return 1.00f * s;     // h + r = 0,55 + 0,45
		default: return s;
	}
}

// Les deux facons d'empiler, et elles sont DISTINCTES a la mesure.
enum Liaison { L_AUCUNE = 0, L_CONTACT, L_JOUR };

struct Piece {
		Forme f = F_AUCUNE;
		float cx = 0.f, cy = 0.f, cz = 0.f; // centre
		float s = 1.f;						// echelle (rayon englobant = s)
};

struct Trou {
		float cx = 0.f, cz = 0.f, r = 0.3f; // cylindre traversant, axe Y
};

struct Scene {
		Piece pieces[16];
		uint32 nPieces = 0;
		Trou trous[8];
		uint32 nTrous = 0;
};

// ── Normalisation : minuscules ASCII, accents rabattus, ponctuation en espace ─
// Les accents sont rabattus sur l'UTF-8 (deux octets, 0xC3 0xA9 = e aigu) parce
// que Rodolf ecrit en francais et que « percé » ne doit pas devenir un mot
// inconnu. Une console en cp1252 pourrait envoyer un seul octet : on traite les
// deux cas, et tout octet >= 0x80 non reconnu devient un espace plutot qu'un
// caractere qui casserait la comparaison en silence.
static void Normaliser(const char *in, char *out, size_t cap) {
	size_t j = 0;
	for (size_t i = 0; in[i] && j + 1 < cap; ++i) {
		unsigned char c = (unsigned char)in[i];
		char r = 0;
		if (c == 0xC3 && in[i + 1]) {
			const unsigned char d = (unsigned char)in[i + 1];
			++i;
			if (d >= 0xA0 && d <= 0xA5) r = 'a';
			else if (d >= 0xA8 && d <= 0xAB) r = 'e';
			else if (d >= 0xAC && d <= 0xAF) r = 'i';
			else if (d >= 0xB2 && d <= 0xB6) r = 'o';
			else if (d >= 0xB9 && d <= 0xBC) r = 'u';
			else if (d == 0xA7) r = 'c';
			else r = ' ';
		} else if (c >= 'A' && c <= 'Z') {
			r = (char)(c - 'A' + 'a');
		} else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
			r = (char)c;
		} else {
			r = ' ';
		}
		out[j++] = r;
	}
	out[j] = 0;
}

static bool Est(const char *t, const char *a) {
	return strcmp(t, a) == 0;
}

// Rend la forme d'un mot, ou F_AUCUNE. Les synonymes sont ecrits ici et nulle
// part ailleurs : un vocabulaire disperse est un vocabulaire qu'on ne peut plus
// imprimer a l'utilisateur au moment du refus.
static Forme FormeDuMot(const char *t) {
	if (Est(t, "sphere") || Est(t, "spheres") || Est(t, "boule") || Est(t, "boules")) return F_SPHERE;
	if (Est(t, "cube") || Est(t, "cubes") || Est(t, "boite") || Est(t, "boites") || Est(t, "caisse")) return F_CUBE;
	if (Est(t, "cylindre") || Est(t, "cylindres") || Est(t, "tube") || Est(t, "tubes")) return F_CYLINDRE;
	if (Est(t, "cone") || Est(t, "cones")) return F_CONE;
	if (Est(t, "tore") || Est(t, "tores") || Est(t, "anneau") || Est(t, "anneaux") || Est(t, "donut")) return F_TORE;
	if (Est(t, "capsule") || Est(t, "capsules") || Est(t, "gelule")) return F_CAPSULE;
	return F_AUCUNE;
}

static const char *VOCABULAIRE =
	"formes : sphere/boule, cube/boite, cylindre/tube, cone, tore/anneau/donut, capsule\n"
	"        nombres : un..six ou un chiffre ; tailles : grand, petit\n"
	"        liaisons : « et » (cote a cote), « sur » (empile), « perce »/« avec un trou » (troue)";

static int Nombre(const char *t) {
	if (Est(t, "un") || Est(t, "une") || Est(t, "1")) return 1;
	if (Est(t, "deux") || Est(t, "2")) return 2;
	if (Est(t, "trois") || Est(t, "3")) return 3;
	if (Est(t, "quatre") || Est(t, "4")) return 4;
	if (Est(t, "cinq") || Est(t, "5")) return 5;
	if (Est(t, "six") || Est(t, "6")) return 6;
	return -1;
}

// L'ANALYSE. Rend le nombre de formes emises ; 0 = aucune forme reconnue, et
// c'est un REFUS chez l'appelant, jamais un objet par defaut.
static uint32 Analyser(const char *phrase, Scene &sc, char *motInconnu, size_t capMot) {
	char norm[1024];
	Normaliser(phrase, norm, sizeof(norm));
	motInconnu[0] = 0;

	int compte = 1;
	float taille = 1.f;
	// ARBITRAGE DE RODOLF, 18/09 : « sur » signifie CONTACT. Qui veut un jour le
	// dit -- « au-dessus de ». Les deux restent mesures separement, pour que la
	// difference entre contact et jour soit une MESURE et non une supposition.
	Liaison liaison = L_AUCUNE;
	bool trouEnAttente = false;
	uint32 motsUtiles = 0;

	// Le curseur de pose : x avance de piece en piece, y monte quand on empile.
	float curX = 0.f, curY = 0.f, dernierR = 0.f, dernierX = 0.f, dernierZ = 0.f;

	// Decoupage A LA MAIN : `strtok_r` n'existe pas sur MSVC (qui nomme la
	// meme fonction `strtok_s`) et `strtok` garde un etat global. Un outil qui
	// ne compile que sur une chaine est un outil qui ment sur sa portee.
	char mot[64];
	size_t pos = 0;
	const size_t nlen = strlen(norm);
	while (pos <= nlen) {
		while (pos < nlen && norm[pos] == ' ')
			++pos;
		size_t fin = pos;
		while (fin < nlen && norm[fin] != ' ')
			++fin;
		if (fin == pos)
			break;
		size_t lm = fin - pos;
		if (lm > sizeof(mot) - 1)
			lm = sizeof(mot) - 1;
		memcpy(mot, norm + pos, lm);
		mot[lm] = 0;
		pos = fin;
		char *t = mot;
		{
		// Mots de liaison sans effet geometrique : on les AVALE explicitement,
		// sinon ils remonteraient comme « mot inconnu » et feraient refuser une
		// phrase parfaitement lisible.
		// ⚠️ « dessus » A ETE RETIRE DE CETTE LISTE le 18/09, et c'est le coeur du
		// correctif. Il y etait, donc il etait avale ICI, donc le test « dessus »
		// ecrit plus bas etait un CORPS MORT que rien n'atteignait. Le banc n'a
		// rien vu : « au-dessus de » posait les formes COTE A COTE, ce qui donne
		// aussi C=2 et chi=4. Un critere topologique ne peut pas distinguer deux
		// solides disjoints EMPILES de deux solides disjoints COTE A COTE.
		// LA LECON : avant d'ajouter un mot au vocabulaire, chercher qui le mange
		// deja. Cette liste est l'UNIQUE autorite sur les mots sans effet.
		if (Est(t, "le") || Est(t, "la") || Est(t, "les") || Est(t, "de") || Est(t, "du") || Est(t, "des") ||
			Est(t, "d") || Est(t, "l") || Est(t, "au") || Est(t, "avec") || Est(t, "a")) {
			++motsUtiles;
			continue;
		}
		if (Est(t, "et")) {
			liaison = L_AUCUNE;
			++motsUtiles;
			continue;
		}
		if (Est(t, "sur")) {
			liaison = L_CONTACT;
			++motsUtiles;
			continue;
		}
		// « au-dessus de » : la normalisation rabat la ponctuation sur des espaces,
		// donc le tiret disparait et le mot arrive comme « dessus ». « au » et « de »
		// restent avales par la liste unique du dessus -- ce test-ci doit donc venir
		// APRES elle dans le fichier mais AVANT qu'elle ne mange « dessus », ce qui
		// se regle en retirant le mot de la liste, pas en ajoutant un second test.
		if (Est(t, "dessus") || Est(t, "audessus")) {
			liaison = L_JOUR;
			++motsUtiles;
			continue;
		}
		if (Est(t, "perce") || Est(t, "percee") || Est(t, "troue") || Est(t, "trouee") || Est(t, "trou") ||
			Est(t, "trous")) {
			// MESURE DU 2026-09-17 : « un cube perce » ne percait RIEN. Ma
			// grammaire ne savait poser le trou que sur la forme SUIVANTE, or
			// en francais l'adjectif SUIT le nom : au moment ou « perce »
			// arrive, le cube est deja emis. Le banc l'a dit en une ligne --
			// chi mesure 2 la ou l'attendu derive valait 0 -- et c'est
			// exactement ce qu'un attendu qui DISTINGUE est cense faire.
			// On pose donc le trou EN ARRIERE s'il y a deja une forme, et en
			// avant sinon (« perce un cube »). Les deux ordres sont acceptes,
			// et l'ANALYSE imprimee dit lequel a ete compris.
			if (sc.nPieces > 0 && sc.nTrous < 8) {
				const Piece &der = sc.pieces[sc.nPieces - 1];
				Trou &h = sc.trous[sc.nTrous++];
				h.cx = der.cx;
				h.cz = der.cz;
				h.r = 0.38f * der.s;
			} else {
				trouEnAttente = true;
			}
			++motsUtiles;
			continue;
		}
		if (Est(t, "grand") || Est(t, "grande") || Est(t, "gros") || Est(t, "grosse")) {
			taille = 1.6f;
			++motsUtiles;
			continue;
		}
		if (Est(t, "petit") || Est(t, "petite")) {
			taille = 0.6f;
			++motsUtiles;
			continue;
		}
		const int n = Nombre(t);
		if (n > 0) {
			compte = n;
			++motsUtiles;
			continue;
		}
		const Forme f = FormeDuMot(t);
		if (f == F_AUCUNE) {
			if (!motInconnu[0]) {
				size_t k = 0;
				for (; t[k] && k + 1 < capMot; ++k)
					motInconnu[k] = t[k];
				motInconnu[k] = 0;
			}
			continue;
		}
		// EMISSION. Cote a cote (X), ou empilees (Y) de DEUX facons :
		//   « sur »          -> CONTACT : les surfaces se touchent, les solides
		//                       fusionnent, C = 1 et chi = 2 ;
		//   « au-dessus de » -> JOUR    : deux solides disjoints, C = 2, chi = 4.
		// L'attendu du banc a ete RE-DERIVE avant que cette ligne ne change :
		// l'ancien chi = 4 du cas « sur » etait satisfait PARCE QUE le defaut
		// etait la.
		for (int k = 0; k < compte && sc.nPieces < 16; ++k) {
			Piece &p = sc.pieces[sc.nPieces++];
			p.f = f;
			p.s = taille;
			if (liaison != L_AUCUNE && sc.nPieces >= 2) {
				// « A SUR B » : c'est A qui est EN HAUT. L'apercu du 17/09 a
				// montre l'inverse -- « un cylindre sur un cube » posait le
				// CUBE au-dessus. Les mots arrivent dans l'ordre A, « sur », B,
				// donc la forme qui suit « sur » descend SOUS la precedente.
				// Aucun chiffre du banc ne voyait cela : deux solides disjoints
				// donnent chi = 4 quel que soit lequel est en haut.
				const Piece &prec = sc.pieces[sc.nPieces - 2];
				const float hA = DemiHauteurY(prec.f, prec.s);
				const float hB = DemiHauteurY(p.f, p.s);
				const float hMin = hA < hB ? hA : hB;
				p.cx = prec.cx;
				p.cz = prec.cz;
				if (liaison == L_CONTACT) {
					// POURQUOI UNE PENETRATION ET PAS UNE TANGENCE EXACTE : a
					// tangence exacte le champ vaut 0 sur tout le plan de
					// contact, et SurfaceNets peut rendre UNE composante ou
					// DEUX selon le hasard de la grille. Le banc serait alors
					// NON DETERMINISTE, ce qui est pire qu'un banc faux : il
					// recompense celui qui s'arrete au premier essai qui
					// l'arrange. 0,20 x la plus petite demi-hauteur fait ~1,8
					// cellule de recouvrement a res=64 sur cylindre/cube, et
					// davantage plus fin -- la BASSE resolution est le cas
					// fragile, c'est donc elle qu'il faut eprouver.
					p.cy = prec.cy - (hA + hB - 0.20f * hMin);
				} else {
					p.cy = prec.cy - (hA + hB + 0.5f * hMin);
				}
				curY = p.cy;
			} else {
				p.cx = curX + (sc.nPieces == 1 ? 0.f : dernierR + taille + 0.8f * taille);
				p.cy = 0.f;
				p.cz = 0.f;
				curX = p.cx;
				curY = 0.f;
			}
			dernierR = taille;
			dernierX = p.cx;
			dernierZ = p.cz;
			liaison = L_AUCUNE;
			if (trouEnAttente && sc.nTrous < 8) {
				Trou &h = sc.trous[sc.nTrous++];
				h.cx = p.cx;
				h.cz = p.cz;
				h.r = 0.38f * taille;
			}
		}
		trouEnAttente = false;
		compte = 1;
		taille = 1.f;
		++motsUtiles;
		}
	}
	(void)motsUtiles;
	return sc.nPieces;
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. LE CHAMP. Distances signees ; l'occupation vaut -distance (SurfaceNets
//    definit « dedans » par champ > iso, avec iso = 0).
// ─────────────────────────────────────────────────────────────────────────────
static float Mx(float a, float b) { return a > b ? a : b; }
static float Mn(float a, float b) { return a < b ? a : b; }

static float SdfPiece(const Piece &p, float x, float y, float z) {
	const float px = x - p.cx, py = y - p.cy, pz = z - p.cz;
	const float s = p.s;
	switch (p.f) {
		case F_SPHERE:
			return sqrtf(px * px + py * py + pz * pz) - s;
		case F_CUBE: {
			const float h = 0.62f * s; // demi-cote : diagonale ~ s, comme le rayon des autres
			const float qx = fabsf(px) - h, qy = fabsf(py) - h, qz = fabsf(pz) - h;
			const float ex = Mx(qx, 0.f), ey = Mx(qy, 0.f), ez = Mx(qz, 0.f);
			return sqrtf(ex * ex + ey * ey + ez * ez) + Mn(Mx(qx, Mx(qy, qz)), 0.f);
		}
		case F_CYLINDRE: {
			const float r = 0.55f * s, h = 0.9f * s;
			const float d = sqrtf(px * px + pz * pz) - r;
			return Mx(d, fabsf(py) - h);
		}
		case F_CONE: {
			// Cone plein, pointe en haut : rayon decroissant lineairement.
			const float h = 0.95f * s, r = 0.75f * s;
			const float t = (py + h) / (2.f * h); // 0 en bas, 1 en haut
			const float rr = r * (1.f - (t < 0.f ? 0.f : (t > 1.f ? 1.f : t)));
			const float d = sqrtf(px * px + pz * pz) - rr;
			return Mx(d, fabsf(py) - h);
		}
		case F_TORE: {
			const float R = 0.68f * s, r = 0.28f * s;
			const float q = sqrtf(px * px + pz * pz) - R;
			return sqrtf(q * q + py * py) - r;
		}
		case F_CAPSULE: {
			const float r = 0.45f * s, h = 0.55f * s;
			const float yy = py - (py > h ? h : (py < -h ? -h : py));
			return sqrtf(px * px + yy * yy + pz * pz) - r;
		}
		default:
			return 1e30f;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. LE MAILLAGE. Champ -> SurfaceNets -> normales -> .OBJ.
// ─────────────────────────────────────────────────────────────────────────────
struct Sortie {
		uint32 sommets = 0, triangles = 0;
		float cell = 0.f;
		uint32 nx = 0, ny = 0, nz = 0;
		float volume = 0.f;	  // signe : > 0 = faces tournees vers l'exterieur
		bool redresse = false; // vrai si le sens des faces a du etre inverse
};

static bool Mailler(const Scene &sc, uint32 res, const char *outPath, Sortie &so, const char **pourquoi) {
	// Boite de la scene, elargie de 4 cellules : une forme qui touche le bord de
	// la grille sort OUVERTE (SurfaceNets ne peut pas relier les cellules
	// manquantes), et l'etancheite mesuree serait fausse pour une raison qui
	// n'a rien a voir avec la forme.
	float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
	for (uint32 i = 0; i < sc.nPieces; ++i) {
		const Piece &p = sc.pieces[i];
		const float c[3] = {p.cx, p.cy, p.cz};
		for (int k = 0; k < 3; ++k) {
			if (c[k] - p.s < mn[k]) mn[k] = c[k] - p.s;
			if (c[k] + p.s > mx[k]) mx[k] = c[k] + p.s;
		}
	}
	float ext = 0.f;
	for (int k = 0; k < 3; ++k)
		if (mx[k] - mn[k] > ext) ext = mx[k] - mn[k];
	if (ext <= 0.f) {
		*pourquoi = "la scene analysee est vide (aucune etendue)";
		return false;
	}
	const float cell = ext / (float)res;
	const float pad = 4.f * cell;
	for (int k = 0; k < 3; ++k) {
		mn[k] -= pad;
		mx[k] += pad;
	}
	const uint32 nx = (uint32)((mx[0] - mn[0]) / cell) + 1;
	const uint32 ny = (uint32)((mx[1] - mn[1]) / cell) + 1;
	const uint32 nz = (uint32)((mx[2] - mn[2]) / cell) + 1;
	if ((double)nx * ny * nz > 40e6) {
		*pourquoi = "grille trop grande (plus de 40 millions de points) : baisse --res";
		return false;
	}

	NkVector<float> champ;
	champ.Resize(nx * ny * nz);
	for (uint32 k = 0; k < nz; ++k)
		for (uint32 j = 0; j < ny; ++j)
			for (uint32 i = 0; i < nx; ++i) {
				const float x = mn[0] + (float)i * cell, y = mn[1] + (float)j * cell, z = mn[2] + (float)k * cell;
				float d = 1e30f;
				for (uint32 q = 0; q < sc.nPieces; ++q)
					d = Mn(d, SdfPiece(sc.pieces[q], x, y, z));
				// Les trous : difference booleenne, max(d, -dTrou). Le cylindre
				// est INFINI en Y (pas de borne) -- un trou borne laisserait une
				// cavite fermee, qui n'est pas une anse et ne changerait pas la
				// caracteristique d'Euler de la meme facon.
				for (uint32 q = 0; q < sc.nTrous; ++q) {
					const float dx = x - sc.trous[q].cx, dz = z - sc.trous[q].cz;
					const float dt = sqrtf(dx * dx + dz * dz) - sc.trous[q].r;
					d = Mx(d, -dt);
				}
				champ[i + nx * (j + ny * k)] = -d; // occupation = -distance
			}

	gen::NkMesh m = gen::SurfaceNets(champ.Data(), nx, ny, nz, 0.f, cell);
	if (m.VertexCount() == 0 || m.TriangleCount() == 0) {
		*pourquoi = "aucune surface extraite : la forme ne traverse pas la grille";
		return false;
	}
	// LE SENS DES FACES : mesure, jamais suppose (2026-09-17).
	// Huit cas du banc etaient verts -- fermes, manifolds, chi juste -- et les
	// huit sortaient RETOURNES. C'est structurel : l'etancheite, le genre et la
	// caracteristique d'Euler sont INVARIANTS par retournement des faces, donc
	// aucun d'eux ne POUVAIT le voir. Ce sont les images qui l'ont trouve, et
	// c'est le volume signe qui l'a prononce.
	// Le meme garde-fou existe cote image (`genia_triposr.py`, « faces
	// retournees (volume negatif) ») : deux producteurs, un seul reflexe.
	double vol6 = 0.0;
	for (uint32 t = 0; t < m.TriangleCount(); ++t) {
		const uint32 ia = m.triangles[t * 3 + 0], ib = m.triangles[t * 3 + 1], ic = m.triangles[t * 3 + 2];
		const double ax = m.positions[ia * 3], ay = m.positions[ia * 3 + 1], az = m.positions[ia * 3 + 2];
		const double bx = m.positions[ib * 3], by = m.positions[ib * 3 + 1], bz = m.positions[ib * 3 + 2];
		const double cx = m.positions[ic * 3], cy = m.positions[ic * 3 + 1], cz = m.positions[ic * 3 + 2];
		vol6 += ax * (by * cz - bz * cy) - ay * (bx * cz - bz * cx) + az * (bx * cy - by * cx);
	}
	so.volume = (float)(vol6 / 6.0);
	const bool sauterLeRedressement = [] {
		const char *mu = getenv("NK_TEXTE3D_MUTE");
		return mu && mu[0] == '2'; // MUTATION 2 : ne pas redresser
	}();
	if (so.volume < 0.f && !sauterLeRedressement) {
		for (uint32 t = 0; t < m.TriangleCount(); ++t) {
			const uint32 tmp = m.triangles[t * 3 + 1];
			m.triangles[t * 3 + 1] = m.triangles[t * 3 + 2];
			m.triangles[t * 3 + 2] = tmp;
		}
		so.volume = -so.volume;
		so.redresse = true;
	}
	gen::ComputeNormals(m);
	if (!gen::SaveMeshObj(outPath, m)) {
		*pourquoi = "ecriture du .obj impossible (chemin ou droits)";
		return false;
	}
	so.sommets = m.VertexCount();
	so.triangles = m.TriangleCount();
	so.cell = cell;
	so.nx = nx;
	so.ny = ny;
	so.nz = nz;
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. LE SECOND INSTRUMENT. Il ne partage aucune ligne avec le premier.
//    Rebasage des indices par sous-mesh : la MEME lecture que l'import du
//    modeleur (global = indices[firstIndex + i] + baseVertex). Sans lui, la
//    face 0 de chaque sous-mesh porterait les sommets 0,1,2.
// ─────────────────────────────────────────────────────────────────────────────
static void IndicesGlobaux(const NkGLTFMeshData &data, NkVector<uint32> &out) {
	out.Clear();
	const uint32 iTotal = (uint32)data.indices.Size();
	for (uint32 s = 0; s < (uint32)data.subMeshes.Size(); s++) {
		const NkSubMesh &sm = data.subMeshes[s];
		for (uint32 i = 0; i < sm.indexCount && sm.firstIndex + i < iTotal; ++i)
			out.PushBack(data.indices[sm.firstIndex + i] + sm.baseVertex);
	}
}

struct Relecture {
		bool lu = false;
		uint32 V = 0, F = 0;
		double volume = 0.0; // signe, calcule sur le fichier RELU
		NkMeshStats st;
};

static bool Relire(const char *path, Relecture &r) {
	NkGLTFMeshData data;
	if (!LoadOBJ(NkString(path), data) || !data.IsValid())
		return false;
	NkVector<uint32> gi;
	IndicesGlobaux(data, gi);
	NkEditMesh m;
	m.BuildFromIndexed(data.vertices.Data(), (uint32)data.vertices.Size(), gi.Data(), (uint32)gi.Size(), false);
	// Le volume signe est recalcule ICI, sur les octets relus du .obj, et non
	// repris de l'ecriture : un chiffre que le producteur se repasse a lui-meme
	// ne mesure que sa propre memoire.
	double v6 = 0.0;
	for (uint32 t = 0; t + 2 < (uint32)gi.Size(); t += 3) {
		const NkVec3f &A = data.vertices[gi[t]].pos;
		const NkVec3f &B = data.vertices[gi[t + 1]].pos;
		const NkVec3f &C = data.vertices[gi[t + 2]].pos;
		v6 += (double)A.x * ((double)B.y * C.z - (double)B.z * C.y) -
			  (double)A.y * ((double)B.x * C.z - (double)B.z * C.x) +
			  (double)A.z * ((double)B.x * C.y - (double)B.y * C.x);
	}
	r.volume = v6 / 6.0;
	r.V = m.VertCount();
	r.F = m.FaceCount();
	r.st = NkMeshAnalysis::Analyze(m);
	r.lu = true;
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. LE BANC.
// ─────────────────────────────────────────────────────────────────────────────
static bool Existe(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	fclose(f);
	return true;
}

static int g_rouge = 0;

static void Attendu(bool ok, const char *quoi, long long att, long long mes) {
	printf("  [%s] %-56s attendu=%lld mesure=%lld\n", ok ? "VERT " : "ROUGE", quoi, att, mes);
	if (!ok)
		++g_rouge;
}

static bool SortieEstObj(const char *out) {
	const size_t n = strlen(out);
	return n > 4 && (strcmp(out + n - 4, ".obj") == 0 || strcmp(out + n - 4, ".OBJ") == 0);
}

// Le geste complet, partage par le mode « une phrase » et par le banc.
static int Produire(const char *texte, const char *out, uint32 res, Sortie &so, Scene &sc) {
	if (!texte || !texte[0])
		return Refus("demande vide : aucun texte. Vocabulaire :\n        %s", VOCABULAIRE);
	if (!out || !SortieEstObj(out))
		return Refus("la sortie doit finir par .obj (gen::SaveMeshObj est le SEUL ecrivain de "
					 "maillage du depot ; aucun ecrivain glTF n'existe) : %s",
					 out ? out : "(null)");
	if (res < 8 || res > 512)
		return Refus("resolution hors bornes : %u (attendu entre 8 et 512)", res);

	char inconnu[64];
	const uint32 n = Analyser(texte, sc, inconnu, sizeof(inconnu));
	if (n == 0)
		return Refus("aucune forme reconnue dans « %s »%s%s%s. Vocabulaire :\n        %s", texte,
					 inconnu[0] ? " (mot inconnu : « " : "", inconnu[0] ? inconnu : "", inconnu[0] ? " »)" : "",
					 VOCABULAIRE);

	// LA MUTATION, dans le meme binaire : toutes les formes deviennent des
	// spheres. Les cas « tore » et « cube perce » doivent alors rougir.
	if (const char *mu = getenv("NK_TEXTE3D_MUTE")) {
		if (mu[0] == '2')
			printf("  MUTATION NK_TEXTE3D_MUTE=2 : le redressement des faces est desactive\n");
		if (mu[0] == '3') {
			// Force le JOUR meme pour « sur ». Le cas contact doit alors rougir
			// (chi mesure 4 la ou l'attendu vaut 2) et le cas « au-dessus de »
			// rester VERT : une mutation qui fait tout rougir ne prouve pas
			// qu'elle vise la bonne chose.
			for (uint32 i = 1; i < sc.nPieces; ++i) {
				const Piece &pr = sc.pieces[i - 1];
				if (sc.pieces[i].cy >= pr.cy) continue; // pose cote a cote : rien a muter
				const float hA = DemiHauteurY(pr.f, pr.s), hB = DemiHauteurY(sc.pieces[i].f, sc.pieces[i].s);
				const float hMin = hA < hB ? hA : hB;
				sc.pieces[i].cy = pr.cy - (hA + hB + 0.5f * hMin);
			}
			printf("  MUTATION NK_TEXTE3D_MUTE=3 : « sur » force au JOUR (contact supprime)\n");
		}
		if (mu[0] == '1') {
			for (uint32 i = 0; i < sc.nPieces; ++i)
				sc.pieces[i].f = F_SPHERE;
			sc.nTrous = 0;
			printf("  MUTATION NK_TEXTE3D_MUTE=1 : toutes les formes forcees en sphere, trous retires\n");
		}
	}

	printf("  ANALYSE : %u forme(s)", n);
	for (uint32 i = 0; i < sc.nPieces; ++i)
		printf(" · %s(s=%.2f en %.2f,%.2f,%.2f)", NomForme(sc.pieces[i].f), sc.pieces[i].s, sc.pieces[i].cx,
			   sc.pieces[i].cy, sc.pieces[i].cz);
	printf(" · %u trou(s)\n", sc.nTrous);

	const char *pourquoi = "raison inconnue";
	if (!Mailler(sc, res, out, so, &pourquoi))
		return Refus("%s", pourquoi);
	printf("  MESURE ecriture : grille %ux%ux%u (pas %.4f) -> sommets=%u triangles=%u volume_signe=%+.5f%s -> %s\n",
		   so.nx, so.ny, so.nz, so.cell, so.sommets, so.triangles, so.volume,
		   so.redresse ? " (faces REDRESSEES)" : "", out);
	return 0;
}

// Un cas du banc : une phrase, une caracteristique d'Euler DERIVEE, et le
// nombre de composantes qui la justifie.
// LA CONVENTION, CITEE, PARCE QU'UN ATTENDU DERIVE D'UNE CONVENTION DOIT CITER
// LA LIGNE QUI LA FIXE. `NkMeshAnalysis.cpp:184` pose
//     genus = (2 - euler) / 2   des que le maillage est ferme et manifold.
// Cette formule suppose UNE SEULE composante. Sur un maillage a C composantes
// fermees, la vraie relation est chi = 2 x (C - G). Pour deux spheres, chi = 4
// et la formule rend genus = -1.
//
// ⚠️ ET C'EST UNE AMBIGUITE DE L'INSTRUMENT, pas une erreur de calcul : l'en-tete
//    de `NkMeshAnalysis.h` annonce -1 comme la valeur « non calculable ». Deux
//    sens pour un meme nombre. Je ne corrige pas NKRenderer ici -- il est
//    partage par cinq applications -- je le NOMME, et mon banc cesse de juger le
//    genre la ou sa condition n'est pas remplie.
//
// Mon premier attendu disait « deux spheres -> genre 0 ». Il etait FAUX, et le
// moteur avait raison : un temoin qui se prononce hors de sa condition de
// validite fabrique du bruit qu'on apprend a ignorer.
static const int32 GENRE_NON_JUGE = -99;

struct Cas {
		const char *texte;
		int32 euler;   // V - E + F attendu, DERIVE de chi = 2 x (C - G)
		int32 genre;   // anses attendues, ou GENRE_NON_JUGE si C > 1
		int32 composantes; // C : ce qui rend l'attendu d'Euler derivable
		const char *pourquoi;
		// SANS CE CHAMP, chi=2 NE PROUVERAIT RIEN sur un cas a deux formes : une
		// grammaire qui raterait le second mot n'emettrait qu'UNE forme, et une
		// forme seule donne aussi chi=2. Le critere ne distinguerait pas « deux
		// solides fusionnes » de « une seule forme comprise ».
		uint32 formes;
		// L'AXE, ET SANS LUI LE CAS « AU-DESSUS » NE PROUVE RIEN. Mesure du 18/09 :
		// « au-dessus de » ne mordait pas, les formes tombaient COTE A COTE, et le
		// banc est reste VERT -- parce que chi=4 vaut pour les deux dispositions.
		// -1 = non juge (une seule forme) · 0 = cote a cote · 1 = empile en Y.
		int32 empileY;
};

static void JouerCas(const Cas &c, uint32 res, const char *dossier) {
	char out[512];
	char nom[64];
	size_t k = 0;
	for (const char *p = c.texte; *p && k + 1 < sizeof(nom); ++p)
		nom[k++] = (*p == ' ') ? '_' : *p;
	nom[k] = 0;
	snprintf(out, sizeof(out), "%s/%s.obj", dossier, nom);
	remove(out); // un fichier qui preexiste rendrait un echec invisible

	printf("\n-- cas « %s »  (%s)\n", c.texte, c.pourquoi);
	Sortie so;
	Scene sc;
	const int r = Produire(c.texte, out, res, so, sc);
	if (r != 0) {
		Attendu(false, "la production reussit", 0, r);
		return;
	}
	Relecture rl;
	if (!Relire(out, rl)) {
		Attendu(false, "LoadOBJ relit ce que SaveMeshObj a ecrit", 1, 0);
		return;
	}
	printf("  MESURE relecture : V(soude)=%u V(brut)=%u E=%u F=%u  bords=%u nonManifold=%u  chi=%d genre=%d "
		   "volume_signe=%+.5f\n",
		   rl.st.verts, rl.st.rawVerts, rl.st.edges, rl.st.faces, rl.st.boundaryEdges, rl.st.nonManifoldEdges,
		   rl.st.euler, rl.st.genus, rl.volume);
	Attendu(rl.F == so.triangles, "chaque triangle ecrit est relu comme une face", (long long)so.triangles,
			(long long)rl.F);
	Attendu(rl.st.boundaryEdges == 0, "maillage FERME (aucune arete de bord)", 0, rl.st.boundaryEdges);
	Attendu(rl.st.nonManifoldEdges == 0, "maillage MANIFOLD (aucune arete a 3 faces)", 0, rl.st.nonManifoldEdges);
	Attendu(sc.nPieces == c.formes, "nombre de formes ANALYSEES (sans quoi chi ne distingue pas)",
			(long long)c.formes, (long long)sc.nPieces);
	if (c.empileY >= 0 && sc.nPieces >= 2) {
		const float dx = sc.pieces[1].cx - sc.pieces[0].cx;
		const float dy = sc.pieces[1].cy - sc.pieces[0].cy;
		const float adx = dx < 0.f ? -dx : dx, ady = dy < 0.f ? -dy : dy;
		printf("  MESURE disposition : dx=%+.3f dy=%+.3f\n", dx, dy);
		Attendu((c.empileY == 1) ? (ady > adx) : (adx > ady),
				c.empileY == 1 ? "les formes sont EMPILEES (|dy| > |dx|)" : "les formes sont COTE A COTE (|dx| > |dy|)",
				1, ((c.empileY == 1) ? (ady > adx) : (adx > ady)) ? 1 : 0);
		if (c.empileY == 1) {
			// L'ECART ATTENDU EST DERIVE de `DemiHauteurY`, la source meme du
			// placement -- pas ecrit en dur : « un attendu en dur se perime ».
			const float hA = DemiHauteurY(sc.pieces[0].f, sc.pieces[0].s);
			const float hB = DemiHauteurY(sc.pieces[1].f, sc.pieces[1].s);
			const float hMin = hA < hB ? hA : hB;
			const float att = (c.euler == 2) ? (hA + hB - 0.20f * hMin) : (hA + hB + 0.50f * hMin);
			const float ecart = ady - att;
			Attendu((ecart < 0.001f && ecart > -0.001f),
					c.euler == 2 ? "ecart vertical = contact (hA + hB - 0,20 hMin)"
								 : "ecart vertical = jour (hA + hB + 0,50 hMin)",
					(long long)(att * 1000.f + 0.5f), (long long)(ady * 1000.f + 0.5f));
		}
	}
	Attendu(rl.st.euler == c.euler, "caracteristique d'Euler V-E+F (derivee)", c.euler, rl.st.euler);
	// LE CRITERE QUE LA TOPOLOGIE NE POUVAIT PAS PORTER. chi, l'etancheite et le
	// genre sont invariants par retournement des faces : il faut une grandeur
	// ORIENTEE, et le volume signe en est une.
	Attendu(rl.volume > 0.0, "faces tournees vers l'EXTERIEUR (volume signe > 0)", 1, rl.volume > 0.0 ? 1 : 0);
	if (c.genre == GENRE_NON_JUGE)
		printf("  [ -- ] %-56s (C=%d composantes : la formule genus=(2-chi)/2 ne s'applique pas)\n",
			   "genre : condition non remplie, donc NON JUGE", c.composantes);
	else
		Attendu(rl.st.genus == c.genre, "genre deduit (nombre d'anses)", c.genre, rl.st.genus);
}

static int Banc(uint32 res, const char *dossier) {
	printf("== NKTexte3D --banc (res=%u, sorties dans %s) ==\n", res, dossier);
	printf("\n[0] LE ZERO D'ABORD : trois refus, chacun NOMME, et AUCUN fichier ecrit.\n");
	{
		char out[512];
		snprintf(out, sizeof(out), "%s/__zero.obj", dossier);
		remove(out);
		Sortie so;
		Scene sc;
		// UNE course, UNE lecture. Appeler Produire() dans les deux arguments
		// d'un meme Attendu ferait tourner DEUX courses et comparerait la
		// seconde a la premiere : un instrument qui fait autre chose que ce
		// qu'il annonce. Idem pour fopen, qui en plus fuyait son descripteur.
		const int r1 = Produire("", out, res, so, sc);
		Attendu(r1 == 2, "demande vide -> refus nomme (code 2)", 2, r1);
		Attendu(!Existe(out), "demande vide -> AUCUN fichier ecrit", 0, Existe(out) ? 1 : 0);

		Scene sc2;
		const int r2 = Produire("un bidule quantique", out, res, so, sc2);
		Attendu(r2 == 2, "mot inconnu -> refus nomme, jamais un objet par defaut", 2, r2);
		Attendu(!Existe(out), "mot inconnu -> AUCUN fichier ecrit", 0, Existe(out) ? 1 : 0);

		char glb[512];
		snprintf(glb, sizeof(glb), "%s/__zero.glb", dossier);
		Scene sc3;
		const int r3 = Produire("une sphere", glb, res, so, sc3);
		Attendu(r3 == 2, "sortie qui n'est pas .obj -> refus nomme", 2, r3);
	}

	printf("\n[1] LES FORMES. L'attendu est DERIVE de V-E+F = 2-2g, pas choisi.\n");
	const Cas cas[] = {
		{"une sphere", 2, 0, 1, "C=1 G=0 : chi = 2x(1-0) = 2", 1, -1},
		{"un tore", 0, 1, 1, "C=1 G=1 : chi = 2x(1-1) = 0", 1, -1},
		{"un cube perce", 0, 1, 1, "un trou traversant EST une anse : C=1 G=1, chi = 0", 1, -1},
		{"une sphere percee", 0, 1, 1, "l'adjectif SUIT le nom, et il doit mordre : C=1 G=1, chi = 0", 1, -1},
		{"deux spheres", 4, GENRE_NON_JUGE, 2, "C=2 G=0 : chi = 2x(2-0) = 4", 2, 0},
		{"un cube", 2, 0, 1, "C=1 G=0 : chi = 2", 1, -1},
		{"un cylindre sur un cube", 2, 0, 1,
		 "CONTACT (arbitrage 18/09) : deux convexes qui s'intersectent = une boule, C=1 G=0, chi = 2", 2, 1},
		{"un cylindre au dessus d un cube", 4, GENRE_NON_JUGE, 2,
		 "JOUR : deux solides disjoints, C=2, chi = 4 -- et ce cas MESURE la difference avec le precedent", 2, 1},
		{"deux tores", 0, GENRE_NON_JUGE, 2, "C=2 G=2 : chi = 2x(2-2) = 0 -- et ce cas DISTINGUE de deux spheres", 2, 0},
	};
	for (uint32 i = 0; i < sizeof(cas) / sizeof(cas[0]); ++i)
		JouerCas(cas[i], res, dossier);

	printf("\n== VERDICT : %s (%d ligne(s) rouge(s)) ==\n", g_rouge == 0 ? "VERT" : "ROUGE", g_rouge);
	if (getenv("NK_TEXTE3D_MUTE"))
		printf("   (course MUTEE : un VERT ici serait le vrai probleme -- les criteres ne testeraient rien)\n");
	return g_rouge == 0 ? 0 : 1;
}

// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char **argv) {
	const char *texte = nullptr;
	const char *out = nullptr;
	const char *dossier = ".";
	uint32 res = 64;
	bool banc = false;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--texte") == 0 && i + 1 < argc) texte = argv[++i];
		else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) out = argv[++i];
		else if (strcmp(argv[i], "--res") == 0 && i + 1 < argc) res = (uint32)atoi(argv[++i]);
		else if (strcmp(argv[i], "--dossier") == 0 && i + 1 < argc) dossier = argv[++i];
		else if (strcmp(argv[i], "--banc") == 0) banc = true;
		else
			return Refus("argument inconnu : %s\nusage : NKTexte3D --texte \"<phrase>\" --out <f.obj> [--res N]\n"
						 "        NKTexte3D --banc [--res N] [--dossier <d>]",
						 argv[i]);
	}
	if (banc)
		return Banc(res, dossier);
	if (!texte && !out)
		return Refus("rien a faire.\nusage : NKTexte3D --texte \"<phrase>\" --out <f.obj> [--res N]\n"
					 "        NKTexte3D --banc [--res N] [--dossier <d>]\n        %s",
					 VOCABULAIRE);

	printf("== NKTexte3D : « %s » -> %s ==\n", texte ? texte : "(rien)", out ? out : "(rien)");
	Sortie so;
	Scene sc;
	const int r = Produire(texte, out, res, so, sc);
	if (r != 0)
		return r;
	// La relecture par l'AUTRE instrument, toujours : un fichier ecrit qu'on ne
	// relit pas est une affirmation, pas une mesure.
	Relecture rl;
	if (!Relire(out, rl)) {
		fprintf(stderr, "REFUS : le .obj a ete ecrit mais LoadOBJ ne le relit pas : %s\n", out);
		return 1;
	}
	printf("  MESURE relecture : V(soude)=%u V(brut)=%u E=%u F=%u  bords=%u nonManifold=%u  chi=%d genre=%d "
		   "volume_signe=%+.5f\n",
		   rl.st.verts, rl.st.rawVerts, rl.st.edges, rl.st.faces, rl.st.boundaryEdges, rl.st.nonManifoldEdges,
		   rl.st.euler, rl.st.genus, rl.volume);
	printf("  %s\n", rl.st.IsClosed() ? "ETANCHE : oui (aucune arete de bord)"
									  : "ETANCHE : NON -- le maillage est ouvert, et je le dis plutot que de le taire");
	printf("  EDITABLE : oui -- entre dans NkEditMesh par la MEME porte que les modeles importes\n");
	return 0;
}
