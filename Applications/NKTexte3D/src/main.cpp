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

// L'OPERATEUR : ce qu'une piece fait a ce qui la precede.
enum Op { OP_UNION = 0, OP_DIFF, OP_INTER };

struct Piece {
		Forme f = F_AUCUNE;
		float cx = 0.f, cy = 0.f, cz = 0.f; // centre
		float s = 1.f;						// echelle (rayon englobant = s)
		// ── CE QUI MANQUAIT, ET C'ETAIT LE VERROU (mesure du 17/09) ─────────
		// « un bonhomme de neige » rendait « un tore sur un petit cylindre »
		// avec tous les compteurs verts. Le defaut n'etait pas le vocabulaire
		// des formes -- six suffisent pour une chaise -- c'est que la grammaire
		// NE SAVAIT PAS PLACER : un seul axe d'empilement, une echelle isotrope,
		// aucune rotation, un seul trou vertical par forme.
		//
		// Les valeurs par defaut sont NEUTRES : sx=sy=sz=1, rotation nulle,
		// union. Le chemin de la grammaire de mots ne voit donc AUCUN
		// changement, et les neuf cas du banc doivent rendre les memes comptes.
		float sx = 1.f, sy = 1.f, sz = 1.f; // echelle PAR AXE (un cylindre plat = un disque)
		float rx = 0.f, ry = 0.f, rz = 0.f; // rotation en degres
		Op op = OP_UNION;
		char nom[32] = {0}; // le nom de la partie dans le document (pour les refus et le critere)
};

struct Trou {
		float cx = 0.f, cz = 0.f, r = 0.3f; // cylindre traversant, axe Y
};

struct Scene {
		Piece pieces[64]; // 16 ne suffit plus : une chaise fait 6 parties, et le
						  // format prevoit des scenes plus riches
		uint32 nPieces = 0;
		Trou trous[8];
		uint32 nTrous = 0;
		float lissage = 0.f; // union LISSE : les jonctions se fondent sur ce rayon
		// ── SOLIDAIRE OU INDEPENDANT : UNE DECISION D'AUTEUR ────────────────
		// true  : UN objet, N groupes nommes  -> un CORPS (le bras bouge par les
		//         poids des os, pas parce qu'il est un objet separe)
		// false : N objets nommes             -> une VOITURE (roues et portes
		//         bougent independamment)
		// Defaut : false, pour ne rien changer aux documents existants.
		bool solidaire = false;
		char nomScene[64] = {0};
		// La relation « pose_sur » declaree dans le document, conservee APRES le
		// placement : c'est elle que le critere document -> geometrie relit.
		// -1 = aucune. Sans elle, on ne pourrait verifier que le calcul contre
		// lui-meme -- ce qui ne prouve rien.
		int32 poseSur[64];
		uint32 nRelations = 0;
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
static float SdfFormeLocale(Forme f, float px, float py, float pz, float s);
static float Mx(float a, float b) { return a > b ? a : b; }
static float Mn(float a, float b) { return a < b ? a : b; }

// LE SDF D'UNE PIECE PLACEE. Trois transformations, dans cet ordre et pas un
// autre : on ramene le point dans le repere LOCAL de la piece (translation,
// puis rotation INVERSE, puis division par l'echelle), on evalue la forme, puis
// on remultiplie.
//
// ⚠️ POURQUOI LA MULTIPLICATION FINALE PAR min(sx,sy,sz) ET PAS PAR AUTRE CHOSE.
// Diviser les coordonnees par une echelle ANISOTROPE casse la propriete de
// distance : le champ obtenu n'est plus 1-Lipschitz, et un mailleur qui
// interpole entre deux echantillons se tromperait de position de surface. Le
// multiplier par le PLUS PETIT facteur rend une borne INFERIEURE de la vraie
// distance -- ce qui est exactement ce qu'il faut : le signe est juste partout,
// et pres de la surface l'erreur tend vers zero. Une borne superieure, elle,
// ferait rater des traversees de cellule.
static float SdfPiece(const Piece &p, float x, float y, float z) {
	float px = x - p.cx, py = y - p.cy, pz = z - p.cz;
	// Rotation INVERSE (angles en degres, ordre X puis Y puis Z a l'aller, donc
	// Z puis Y puis X au retour).
	if (p.rx != 0.f || p.ry != 0.f || p.rz != 0.f) {
		const float k = 3.14159265358979f / 180.f;
		const float a = -p.rz * k, b = -p.ry * k, c = -p.rx * k;
		float t;
		t = px * cosf(a) - py * sinf(a);
		py = px * sinf(a) + py * cosf(a);
		px = t;
		t = px * cosf(b) + pz * sinf(b);
		pz = -px * sinf(b) + pz * cosf(b);
		px = t;
		t = py * cosf(c) - pz * sinf(c);
		pz = py * sinf(c) + pz * cosf(c);
		py = t;
	}
	float kmin = 1.f;
	if (p.sx != 1.f || p.sy != 1.f || p.sz != 1.f) {
		px /= (p.sx != 0.f ? p.sx : 1e-6f);
		py /= (p.sy != 0.f ? p.sy : 1e-6f);
		pz /= (p.sz != 0.f ? p.sz : 1e-6f);
		kmin = Mn(p.sx, Mn(p.sy, p.sz));
		if (kmin <= 0.f)
			kmin = 1e-6f;
	}
	const float s = p.s;
	const float dLocal = SdfFormeLocale(p.f, px, py, pz, s);
	return dLocal * kmin;
}

// La forme NUE, dans son repere local. Separee de `SdfPiece` pour que le
// placement et la forme soient deux questions distinctes -- c'est ce qui permet
// d'ajouter une transformation sans relire six formules.
static float SdfFormeLocale(Forme f, float px, float py, float pz, float s) {
	switch (f) {
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


// =============================================================================
// LE DOCUMENT DE SCENE (.nkscene) -- LA PIECE QUI MANQUAIT ENTRE LA PHRASE ET
// LA GEOMETRIE.
//
// POURQUOI IL EXISTE. Le 17/09, « un bonhomme de neige » a rendu « un tore sur
// un petit cylindre » avec TOUS les compteurs verts, et Rodolf a refuse le
// resultat : « ce n'est pas admissible ». La cause n'etait pas le modele -- il
// avait repondu 18 fois sur 18 -- c'est qu'il n'y avait RIEN entre la phrase et
// le maillage. Sans reference intermediaire, « le maillage est ferme » ne dit
// rien de « c'est un bonhomme de neige », et aucun critere ne pouvait le dire.
//
// ⚠️ DEUX FIDELITES, ET ON NE LES CONFOND JAMAIS :
//   - document -> geometrie : MECANIQUE, verifiable, c'est ce que ce format
//     debloque et ce que `--verifier` mesure ;
//   - phrase -> document : HUMAINE. Que « trois spheres empilees » soit une
//     bonne description d'un bonhomme de neige, c'est RODOLF qui le dit en
//     lisant le document. On ne fabrique aucun critere qui pretendrait la
//     mesurer : ce serait un vert de plus et rien de vrai.
//
// Le format est specifie dans `Tools/Genia/FORMAT_SCENE.md`, et trois documents
// ecrits A LA MAIN vivent dans `Tools/Genia/scenes/` -- ils ont ete ecrits AVANT
// que le modele n'en produise un seul.
// =============================================================================

static bool NkScMot(const char *&c, char *out, size_t cap) {
	while (*c == ' ' || *c == '\t' || *c == '\r')
		++c;
	if (!*c || *c == '\n')
		return false;
	size_t j = 0;
	while (*c && *c != ' ' && *c != '\t' && *c != '\r' && *c != '\n') {
		if (j + 1 < cap)
			out[j++] = *c;
		++c;
	}
	out[j] = 0;
	return j > 0;
}

static float NkScFlottant(const char *&c, bool &ok) {
	char m[64];
	if (!NkScMot(c, m, sizeof(m))) {
		ok = false;
		return 0.f;
	}
	ok = true;
	return (float)atof(m);
}

// Retrouve une partie par son nom. -1 si inconnue -- et l'appelant REFUSE en
// nommant le nom introuvable, il n'invente jamais une relation par defaut.
static int32 NkScPartie(const Scene &sc, const char *nom) {
	for (uint32 i = 0; i < sc.nPieces; ++i)
		if (strcmp(sc.pieces[i].nom, nom) == 0)
			return (int32)i;
	return -1;
}

// La demi-hauteur REELLE d'une piece placee, echelle par axe comprise. C'est
// elle qui fait le contact de `pose_sur`, et elle se lit dans `DemiHauteurY`,
// qui recopie deja les constantes de `SdfFormeLocale` (dette nommee au R5.4).
static float NkScDemiHauteur(const Piece &p) {
	return DemiHauteurY(p.f, p.s) * p.sy;
}

// Lit le document. Rend le nombre de parties, ou 0 avec un motif NOMME. Un
// document mal forme ne produit JAMAIS un objet par defaut.
static uint32 LireScene(const char *chemin, Scene &sc, char *pourquoi, size_t capPourquoi) {
	pourquoi[0] = 0;
	FILE *f = fopen(chemin, "rb");
	if (!f) {
		snprintf(pourquoi, capPourquoi, "document introuvable : %s", chemin);
		return 0;
	}
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n <= 0 || n > 1 << 20) {
		fclose(f);
		snprintf(pourquoi, capPourquoi, "document vide ou demesure (%ld octets) : %s", n, chemin);
		return 0;
	}
	char *buf = (char *)malloc((size_t)n + 1);
	if (!buf) {
		fclose(f);
		snprintf(pourquoi, capPourquoi, "memoire insuffisante pour lire %s", chemin);
		return 0;
	}
	size_t lu = fread(buf, 1, (size_t)n, f);
	buf[lu] = 0;
	fclose(f);

	for (uint32 i = 0; i < 64; ++i)
		sc.poseSur[i] = -1;

	uint32 ligne = 0;
	const char *c = buf;
	while (*c) {
		++ligne;
		const char *finLigne = c;
		while (*finLigne && *finLigne != '\n')
			++finLigne;
		char mot[64];
		const char *q = c;
		bool aMot = NkScMot(q, mot, sizeof(mot));
		// Une ligne vide ou un commentaire : on avance, sans rien dire.
		if (aMot && mot[0] != '#') {
			if (strcmp(mot, "assemblage") == 0) {
				char v[64];
				if (!NkScMot(q, v, sizeof(v))) {
					snprintf(pourquoi, capPourquoi, "ligne %u : « assemblage » attend solidaire ou independant", ligne);
					free(buf);
					return 0;
				}
				if (strcmp(v, "solidaire") == 0)
					sc.solidaire = true;
				else if (strcmp(v, "independant") == 0)
					sc.solidaire = false;
				else {
					snprintf(pourquoi, capPourquoi,
							 "ligne %u : assemblage inconnu « %s » (solidaire pour un corps, independant pour un vehicule)",
							 ligne, v);
					free(buf);
					return 0;
				}
			} else if (strcmp(mot, "scene") == 0) {
				char v[64];
				if (NkScMot(q, v, sizeof(v)))
					snprintf(sc.nomScene, sizeof(sc.nomScene), "%s", v);
			} else if (strcmp(mot, "demande") == 0) {
				// Metadonnees : conservees pour la lecture humaine, sans effet
				// geometrique. Les avaler EXPLICITEMENT, sinon elles
				// remonteraient comme directives inconnues.
			} else if (strcmp(mot, "lissage") == 0) {
				bool ok = false;
				const float v = NkScFlottant(q, ok);
				if (!ok || v < 0.f) {
					snprintf(pourquoi, capPourquoi, "ligne %u : « lissage » attend un nombre >= 0", ligne);
					free(buf);
					return 0;
				}
				sc.lissage = v;
			} else if (strcmp(mot, "partie") == 0) {
				if (sc.nPieces >= 64) {
					snprintf(pourquoi, capPourquoi, "ligne %u : plus de 64 parties", ligne);
					free(buf);
					return 0;
				}
				Piece &pc = sc.pieces[sc.nPieces];
				pc = Piece();
				char nom[64];
				if (!NkScMot(q, nom, sizeof(nom))) {
					snprintf(pourquoi, capPourquoi, "ligne %u : « partie » attend un nom", ligne);
					free(buf);
					return 0;
				}
				if (NkScPartie(sc, nom) >= 0) {
					snprintf(pourquoi, capPourquoi, "ligne %u : deux parties portent le nom « %s »", ligne, nom);
					free(buf);
					return 0;
				}
				snprintf(pc.nom, sizeof(pc.nom), "%s", nom);
				int32 relation = -1;
				bool sousDessous = false;
				float dx = 0.f, dy = 0.f, dz = 0.f;
				bool aForme = false, aCentre = false;
				char clef[64];
				while (NkScMot(q, clef, sizeof(clef))) {
					if (clef[0] == '#')
						break;
					bool ok = false;
					if (strcmp(clef, "forme") == 0) {
						char nf[64];
						if (!NkScMot(q, nf, sizeof(nf))) {
							snprintf(pourquoi, capPourquoi, "ligne %u : « forme » attend un nom", ligne);
							free(buf);
							return 0;
						}
						char norm[64];
						Normaliser(nf, norm, sizeof(norm));
						pc.f = FormeDuMot(norm);
						if (pc.f == F_AUCUNE) {
							snprintf(pourquoi, capPourquoi,
									 "ligne %u : forme inconnue « %s ». Les six : sphere, cube, cylindre, cone, tore, capsule",
									 ligne, nf);
							free(buf);
							return 0;
						}
						aForme = true;
					} else if (strcmp(clef, "taille") == 0) {
						pc.sx = NkScFlottant(q, ok);
						if (ok) pc.sy = NkScFlottant(q, ok);
						if (ok) pc.sz = NkScFlottant(q, ok);
						if (!ok || pc.sx <= 0.f || pc.sy <= 0.f || pc.sz <= 0.f) {
							snprintf(pourquoi, capPourquoi,
									 "ligne %u : « taille » attend trois nombres STRICTEMENT positifs", ligne);
							free(buf);
							return 0;
						}
					} else if (strcmp(clef, "rotation") == 0) {
						pc.rx = NkScFlottant(q, ok);
						if (ok) pc.ry = NkScFlottant(q, ok);
						if (ok) pc.rz = NkScFlottant(q, ok);
						if (!ok) {
							snprintf(pourquoi, capPourquoi, "ligne %u : « rotation » attend trois angles", ligne);
							free(buf);
							return 0;
						}
					} else if (strcmp(clef, "centre") == 0) {
						pc.cx = NkScFlottant(q, ok);
						if (ok) pc.cy = NkScFlottant(q, ok);
						if (ok) pc.cz = NkScFlottant(q, ok);
						if (!ok) {
							snprintf(pourquoi, capPourquoi, "ligne %u : « centre » attend trois nombres", ligne);
							free(buf);
							return 0;
						}
						aCentre = true;
					} else if (strcmp(clef, "decale") == 0) {
						dx = NkScFlottant(q, ok);
						if (ok) dy = NkScFlottant(q, ok);
						if (ok) dz = NkScFlottant(q, ok);
						if (!ok) {
							snprintf(pourquoi, capPourquoi, "ligne %u : « decale » attend trois nombres", ligne);
							free(buf);
							return 0;
						}
					} else if (strcmp(clef, "pose_sur") == 0 || strcmp(clef, "pose_sous") == 0 ||
							   strcmp(clef, "aligne_sur") == 0) {
						char autre[64];
						if (!NkScMot(q, autre, sizeof(autre))) {
							snprintf(pourquoi, capPourquoi, "ligne %u : « %s » attend un nom de partie", ligne, clef);
							free(buf);
							return 0;
						}
						const int32 k = NkScPartie(sc, autre);
						if (k < 0) {
							// ⚠️ On REFUSE au lieu de placer a l'origine. Une
							// relation vers une partie inconnue placerait
							// silencieusement la piece au centre du monde, et le
							// document dirait une chose que la geometrie ne fait
							// pas -- exactement ce qu'on repare.
							snprintf(pourquoi, capPourquoi,
									 "ligne %u : « %s %s » -- aucune partie de ce nom n'est declaree AVANT celle-ci",
									 ligne, clef, autre);
							free(buf);
							return 0;
						}
						relation = k;
						if (strcmp(clef, "aligne_sur") == 0)
							relation = -2 - k; // aligne : x/z seulement
						sousDessous = (strcmp(clef, "pose_sous") == 0);
					} else if (strcmp(clef, "op") == 0) {
						char o[64];
						if (!NkScMot(q, o, sizeof(o))) {
							snprintf(pourquoi, capPourquoi, "ligne %u : « op » attend union, difference ou intersection", ligne);
							free(buf);
							return 0;
						}
						if (strcmp(o, "union") == 0) pc.op = OP_UNION;
						else if (strcmp(o, "difference") == 0) pc.op = OP_DIFF;
						else if (strcmp(o, "intersection") == 0) pc.op = OP_INTER;
						else {
							snprintf(pourquoi, capPourquoi,
									 "ligne %u : operateur inconnu « %s » (union, difference, intersection)", ligne, o);
							free(buf);
							return 0;
						}
					} else {
						snprintf(pourquoi, capPourquoi,
								 "ligne %u : directive inconnue « %s » (forme, taille, rotation, centre, decale, "
								 "pose_sur, pose_sous, aligne_sur, op)",
								 ligne, clef);
						free(buf);
						return 0;
					}
				}
				if (!aForme) {
					snprintf(pourquoi, capPourquoi, "ligne %u : la partie « %s » n'a pas de forme", ligne, pc.nom);
					free(buf);
					return 0;
				}
				// ── LE PLACEMENT, ET C'EST ICI QUE « pose_sur » PREND SON SENS ──
				// Le dessous de cette piece touche le dessus de l'autre : la
				// distance entre centres vaut la somme des demi-hauteurs REELLES
				// (echelle par axe comprise), moins une penetration.
				//
				// ⚠️ LA PENETRATION N'EST PAS UN CONFORT. A tangence exacte le
				// champ vaut 0 sur tout le plan de contact, et SurfaceNets peut
				// rendre UNE composante ou DEUX selon le hasard de la grille --
				// un banc non deterministe, « pire qu'un instrument faux ».
				// C'est la meme derivation qu'au R4.3, appliquee au document.
				if (relation >= 0) {
					const Piece &a = sc.pieces[relation];
					const float hA = NkScDemiHauteur(a), hB = NkScDemiHauteur(pc);
					const float pen = 0.20f * Mn(hA, hB);
					pc.cx = a.cx;
					pc.cz = a.cz;
					// « pose_sous » est le SYMETRIQUE exact, pas un decalage
					// bricole : le dessus de cette piece touche le dessous de
					// l'autre. Sans lui, poser quatre pieds sous une assise
					// obligeait a decaler apres coup -- et le pied TRAVERSAIT
					// l'assise, ce que l'apercu a montre et qu'aucun chiffre
					// n'avait vu.
					pc.cy = sousDessous ? (a.cy - hA - hB + pen) : (a.cy + hA + hB - pen);
					sc.poseSur[sc.nPieces] = relation;
					++sc.nRelations;
				} else if (relation <= -2) {
					const Piece &a = sc.pieces[-2 - relation];
					pc.cx = a.cx;
					pc.cz = a.cz;
				} else if (!aCentre) {
					// Ni relation ni centre : la piece reste a l'origine, et
					// c'est LEGITIME pour la premiere partie d'une scene.
				}
				pc.cx += dx;
				pc.cy += dy;
				pc.cz += dz;
				++sc.nPieces;
			} else {
				snprintf(pourquoi, capPourquoi,
						 "ligne %u : directive inconnue « %s » (scene, demande, partie, lissage, assemblage)", ligne, mot);
				free(buf);
				return 0;
			}
		}
		c = (*finLigne == '\n') ? finLigne + 1 : finLigne;
	}
	free(buf);
	if (sc.nPieces == 0) {
		snprintf(pourquoi, capPourquoi, "le document ne declare AUCUNE partie : %s", chemin);
		return 0;
	}
	// Une scene faite UNIQUEMENT de soustractions ne produit rien : on le dit,
	// plutot que de rendre un maillage vide avec un motif vague en aval.
	bool auMoinsUnAdditif = false;
	for (uint32 i = 0; i < sc.nPieces; ++i)
		if (sc.pieces[i].op != OP_DIFF)
			auMoinsUnAdditif = true;
	if (!auMoinsUnAdditif) {
		snprintf(pourquoi, capPourquoi, "le document ne contient que des soustractions : rien a mailler");
		return 0;
	}
	return sc.nPieces;
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. LE MAILLAGE. Champ -> SurfaceNets -> normales -> .OBJ.
// ─────────────────────────────────────────────────────────────────────────────
struct Sortie {
		uint32 sommets = 0, triangles = 0;
		float cell = 0.f;
		// ⚠️ L'ORIGINE DE LA GRILLE, ET ELLE MANQUAIT. Le maillage produit part du
		// coin de la grille, PAS du repere du document : sur le personnage, le
		// document place le torse en (0,0,0) et le maillage sort entre
		// (0,343 ; 0,037 ; 0,194) et (1,085 ; 2,25 ; 0,714). Mon critere de
		// presence comparait donc DEUX REPERES INCOMPATIBLES et rendait « 7
		// parties absentes sur 7 » -- un faux rouge total, sur un maillage juste.
		// On remonte la valeur que `Mailler` possede deja, au lieu de la
		// redériver : une valeur redérivée peut diverger de l'originale.
		float ox = 0.f, oy = 0.f, oz = 0.f;
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
		// ⚠️ Une piece SOUSTRAITE n'agrandit pas la scene : compter sa boite
		// ferait grossir la grille sans raison, donc baisser la resolution
		// effective la ou il y a de la matiere. C'est le genre de detail qui
		// degrade un resultat sans qu'aucun critere ne bouge.
		if (p.op == OP_DIFF)
			continue;
		// Le rayon englobant apres echelle par axe ET rotation : la rotation ne
		// change pas un rayon, l'echelle si. On prend le plus grand facteur.
		const float r = p.s * Mx(p.sx, Mx(p.sy, p.sz));
		const float c[3] = {p.cx, p.cy, p.cz};
		for (int k = 0; k < 3; ++k) {
			if (c[k] - r < mn[k]) mn[k] = c[k] - r;
			if (c[k] + r > mx[k]) mx[k] = c[k] + r;
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
	so.ox = mn[0];
	so.oy = mn[1];
	so.oz = mn[2];
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
				for (uint32 q = 0; q < sc.nPieces; ++q) {
					const float dq = SdfPiece(sc.pieces[q], x, y, z);
					switch (sc.pieces[q].op) {
						case OP_DIFF:
							// Retirer : max(d, -dq). C'est ce qui creuse un verre
							// ou entaille -- la grammaire de mots ne savait poser
							// qu'un trou VERTICAL par forme.
							d = Mx(d, -dq);
							break;
						case OP_INTER:
							d = Mx(d, dq);
							break;
						default:
							// UNION LISSE (polynomiale) quand un rayon est demande.
							// A rayon nul, elle vaut EXACTEMENT min(a,b) : le
							// chemin existant ne bouge donc pas d'un bit, et c'est
							// verifiable -- les neuf cas du banc doivent rendre les
							// memes comptes.
							if (sc.lissage > 0.f && d < 1e29f) {
								const float k = sc.lissage;
								float h = 0.5f + 0.5f * (dq - d) / k;
								h = h < 0.f ? 0.f : (h > 1.f ? 1.f : h);
								d = (dq * (1.f - h) + d * h) - k * h * (1.f - h);
							} else {
								d = Mn(d, dq);
							}
							break;
					}
				}
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
static int Emettre(const Scene &sc, const char *out, uint32 res, Sortie &so);

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

	return Emettre(sc, out, res, so);
}

// L'EMISSION : une scene devient un maillage. Elle est SEPAREE de l'analyse
// parce que deux entrees y arrivent desormais -- la phrase (grammaire de mots)
// et le DOCUMENT (.nkscene). Les faire passer par le meme producteur est ce qui
// garantit qu'on ne compare pas deux chemins differents : une seule autorite sur
// « comment une scene devient un maillage ».
static int Emettre(const Scene &sc, const char *out, uint32 res, Sortie &so) {
	printf("  ANALYSE : %u forme(s)", sc.nPieces);
	for (uint32 i = 0; i < sc.nPieces; ++i) {
		const Piece &q = sc.pieces[i];
		printf(" · %s%s%s(s=%.2f", q.nom[0] ? q.nom : "", q.nom[0] ? "=" : "", NomForme(q.f), q.s);
		if (q.sx != 1.f || q.sy != 1.f || q.sz != 1.f)
			printf(" ech %.2f,%.2f,%.2f", q.sx, q.sy, q.sz);
		if (q.rx != 0.f || q.ry != 0.f || q.rz != 0.f)
			printf(" rot %.0f,%.0f,%.0f", q.rx, q.ry, q.rz);
		printf(" en %.2f,%.2f,%.2f%s)", q.cx, q.cy, q.cz,
			   q.op == OP_DIFF ? " SOUSTRAITE" : (q.op == OP_INTER ? " INTERSECTEE" : ""));
	}
	printf(" · %u trou(s)", sc.nTrous);
	if (sc.lissage > 0.f)
		printf(" · lissage %.3f", sc.lissage);
	printf("\n");

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

// ─────────────────────────────────────────────────────────────────────────────
// 5. LE BANC DE VARIETE -- `gen::SurfaceNets` SEUL, sans ma grammaire.
//
// POURQUOI IL EXISTE SEPAREMENT. La dette du 18/09 dit : sur sept resolutions
// eprouvees, seules 32 et 64 rendent un maillage sans arete non-manifold. Mesure
// faite a travers ma grammaire -- donc incapable de dire si la faute est au
// mailleur ou a la facon dont je construis mes champs. Ce banc appelle
// `gen::SurfaceNets` DIRECTEMENT, sur des champs poses a la main.
//
// L'HYPOTHESE, ET ELLE EST TESTABLE. Le SurfaceNets naif pose UN sommet par
// cellule. Quand la surface traverse une cellule en DEUX nappes disjointes -- le
// cas d'ecole etant deux coins DIAGONALEMENT OPPOSES solides -- ce sommet unique
// est partage par des quads appartenant a des nappes differentes, et l'arete qui
// les joint se retrouve portee par plus de deux faces.
//
// Si c'est vrai, alors un champ portant UNE cellule ambigue doit produire au
// moins une arete non-manifold, et un champ sans cellule ambigue jamais. Les
// deux sont mesures ci-dessous, le NEGATIF en premier.
// ─────────────────────────────────────────────────────────────────────────────
static void CasVariete(const char *nom, const NkVector<float> &champ, uint32 nx, uint32 ny, uint32 nz,
					   bool attenduSain, const char *pourquoi, const char *dossier) {
	char out[512];
	snprintf(out, sizeof(out), "%s/__variete_%s.obj", dossier, nom);
	remove(out); // un fichier qui preexiste rendrait un echec invisible

	printf("\n-- variete « %s »  (%s)\n", nom, pourquoi);
	gen::NkMesh m = gen::SurfaceNets(champ.Data(), nx, ny, nz, 0.f, 1.f);
	printf("  MESURE brute : grille %ux%ux%u -> sommets=%u triangles=%u\n", nx, ny, nz, m.VertexCount(),
		   m.TriangleCount());
	if (m.VertexCount() == 0 || m.TriangleCount() == 0) {
		Attendu(false, "SurfaceNets produit une surface (sinon rien ne se mesure)", 1, 0);
		return;
	}
	gen::ComputeNormals(m);
	if (!gen::SaveMeshObj(out, m)) {
		Attendu(false, "gen::SaveMeshObj ecrit le fichier", 1, 0);
		return;
	}
	Relecture rl;
	if (!Relire(out, rl)) {
		Attendu(false, "LoadOBJ relit ce que SaveMeshObj a ecrit", 1, 0);
		return;
	}
	printf("  MESURE relecture : V=%u E=%u F=%u  bords=%u nonManifold=%u chi=%d\n", rl.st.verts, rl.st.edges,
		   rl.st.faces, rl.st.boundaryEdges, rl.st.nonManifoldEdges, rl.st.euler);
	// ── POURQUOI LE CRITERE N'EST PAS « AUCUNE ARETE NON-MANIFOLD » ──────────
	// Premiere version de ce banc, 18/09 : elle ne comptait que les aretes, et
	// le cas DIAGONAL est sorti VERT -- 0 arete non-manifold. L'instrument
	// mesurait la grandeur d'a cote. Une cellule ambigue produit un SOMMET
	// non-manifold : deux nappes jointes par un POINT. Chaque arete y reste
	// portee par exactement deux faces, donc le compteur d'aretes ne peut PAS
	// le voir -- il est invariant par le defaut cherche.
	// Ce qui l'a denonce est un NOMBRE IMPOSSIBLE : V-E+F = 15-36+24 = 3, alors
	// qu'une surface fermee orientable a chi = 2-2g, donc TOUJOURS PAIR.
	// `NkMeshStats` n'a pas de compteur de sommets non-manifold ; la parite de
	// chi en tient lieu, et elle est derivee, pas choisie.
	const bool pair = (rl.st.euler % 2) == 0;
	const bool sain = (rl.st.nonManifoldEdges == 0) && pair;
	Attendu(sain == attenduSain,
			attenduSain ? "surface SAINE (0 arete non-manifold ET chi pair)"
						: "surface PATHOLOGIQUE, comme attendu (le defaut est reproduit)",
			attenduSain ? 1 : 0, sain ? 1 : 0);
}

static int BancVariete(const char *dossier) {
	printf("== NKTexte3D --banc-variete (gen::SurfaceNets SEUL, champs synthetiques) ==\n");

	// [0] LE ZERO D'ABORD : une sphere lisse, aucune cellule ambigue possible.
	{
		const uint32 n = 34;
		NkVector<float> c;
		c.Resize(n * n * n);
		const float ctr = 16.5f, r = 12.f;
		for (uint32 k = 0; k < n; ++k)
			for (uint32 j = 0; j < n; ++j)
				for (uint32 i = 0; i < n; ++i) {
					const float dx = (float)i - ctr, dy = (float)j - ctr, dz = (float)k - ctr;
					c[i + n * (j + n * k)] = r - sqrtf(dx * dx + dy * dy + dz * dz);
				}
		CasVariete("sphere", c, n, n, n, true, "champ lisse : la surface traverse chaque cellule en UNE nappe",
				   dossier);
	}

	// [1] CONTROLE : deux voxels LOIN l'un de l'autre. Deux composantes, mais
	// aucune cellule ne voit les deux -- donc aucune ambiguite. Ce cas existe
	// pour que le cas suivant ne puisse pas etre explique par « il y a deux
	// morceaux » : ici aussi il y en a deux, et il doit rester manifold.
	{
		const uint32 n = 10;
		NkVector<float> c;
		c.Resize(n * n * n);
		for (uint32 i = 0; i < c.Size(); ++i)
			c[i] = -1.f;
		c[2 + n * (2 + n * 2)] = 1.f;
		c[7 + n * (7 + n * 7)] = 1.f;
		CasVariete("deux_voxels_loin", c, n, n, n, true, "deux composantes, AUCUNE cellule ne voit les deux",
				   dossier);
	}

	// [2] LE POSITIF DERIVE : deux voxels DIAGONALEMENT adjacents. La cellule
	// (3,3,3) a pour coins (3..4)^3 : elle contient donc (3,3,3) et (4,4,4),
	// deux coins DIAGONALEMENT OPPOSES. C'est la configuration ambigue, et elle
	// est la SEULE difference avec le cas [1].
	{
		const uint32 n = 10;
		NkVector<float> c;
		c.Resize(n * n * n);
		for (uint32 i = 0; i < c.Size(); ++i)
			c[i] = -1.f;
		c[3 + n * (3 + n * 3)] = 1.f;
		c[4 + n * (4 + n * 4)] = 1.f;
		CasVariete("diagonale", c, n, n, n, true,
				   "deux coins DIAGONALEMENT opposes dans la MEME cellule : un sommet pour deux nappes", dossier);
	}

	// [3] DIAGONAUX SUR UNE FACE (et non par un coin). Les deux voxels
	// partagent une ARETE de grille. C'est l'autre configuration ambigue
	// classique, et elle est distincte de [2] : il faut les separer, sinon on
	// attribue a l'une ce que fait l'autre.
	{
		const uint32 n = 10;
		NkVector<float> c;
		c.Resize(n * n * n);
		for (uint32 i = 0; i < c.Size(); ++i)
			c[i] = -1.f;
		c[3 + n * (3 + n * 3)] = 1.f;
		c[4 + n * (4 + n * 3)] = 1.f;
		CasVariete("diagonale_face", c, n, n, n, true,
				   "deux coins diagonaux d'une MEME FACE : ambiguite de face, distincte du coin", dossier);
	}

	// [4] DAMIER : le pire cas possible, une cellule ambigue partout. S'il
	// existe une configuration qui produit des ARETES non-manifold, elle est
	// ici.
	{
		const uint32 n = 12;
		NkVector<float> c;
		c.Resize(n * n * n);
		for (uint32 k = 0; k < n; ++k)
			for (uint32 j = 0; j < n; ++j)
				for (uint32 i = 0; i < n; ++i) {
					const bool dedans = i >= 3 && i <= 8 && j >= 3 && j <= 8 && k >= 3 && k <= 8;
					c[i + n * (j + n * k)] = (dedans && ((i + j + k) % 2) == 0) ? 1.f : -1.f;
				}
		CasVariete("damier", c, n, n, n, true, "une cellule ambigue PARTOUT : le pire cas constructible",
				   dossier);
	}

	printf("\n== VERDICT VARIETE : %s (%d ligne(s) rouge(s)) ==\n", g_rouge == 0 ? "VERT" : "ROUGE", g_rouge);
	return g_rouge == 0 ? 0 : 1;
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

// =============================================================================
// LE CRITERE DOCUMENT -> GEOMETRIE. C'est lui que le document rend possible, et
// c'est la reponse a « un bonhomme de neige qui rend un tore sur un cylindre
// avec tous les compteurs verts ».
//
// LE 17/09 J'AI ECRIT : « je n'ajoute pas un T5, il n'existe pas de critere
// automatique honnete pour ça ». C'etait vrai TANT QUE LA SEULE REFERENCE ETAIT
// LA PHRASE. Avec un document, il en existe un, et il est DERIVE : un document
// qui declare trois parties empilees affirme une chose VERIFIABLE sur la
// geometrie -- trois renflements le long de l'axe vertical, dans cet ordre.
//
// ⚠️ CE QU'IL MESURE, ET CE QU'IL NE MESURE PAS. Il juge la fidelite MECANIQUE
// (document -> geometrie). Il ne dit RIEN de la fidelite HUMAINE (la phrase
// est-elle bien decrite par ce document) -- c'est Rodolf qui en juge, en lisant
// le document, et aucun chiffre ne le remplacera.
//
// ⚠️ ET IL NE LIT PAS LES POSITIONS CALCULEES. Verifier le placement contre les
// centres que le placement vient d'ecrire, ce serait comparer un nombre a
// lui-meme -- « un critere qui compare un nombre a lui-meme ne peut pas
// echouer ». Il mesure donc le MAILLAGE RELU depuis le disque, par un chemin
// qui ne partage rien avec le calcul de placement.
// =============================================================================

// Le profil du rayon le long de l'axe vertical : pour chaque tranche en Y, le
// rayon maximal de la matiere. Trois spheres empilees font TROIS bosses ; un
// tore sur un cylindre en fait DEUX.
static uint32 CompterRenflements(const NkVector<NkVertex3D> &verts, uint32 tranches, float *hauteurs,
								 float *rayons, uint32 capHauteurs) {
	if (verts.Size() == 0 || tranches < 4)
		return 0;
	float ymin = 1e30f, ymax = -1e30f, cx = 0.f, cz = 0.f;
	for (uint32 i = 0; i < (uint32)verts.Size(); ++i) {
		const NkVec3f &q = verts[i].pos;
		ymin = q.y < ymin ? q.y : ymin;
		ymax = q.y > ymax ? q.y : ymax;
		cx += q.x;
		cz += q.z;
	}
	cx /= (float)verts.Size();
	cz /= (float)verts.Size();
	if (ymax <= ymin)
		return 0;
	NkVector<float> rayon;
	rayon.Resize(tranches);
	for (uint32 t = 0; t < tranches; ++t)
		rayon[t] = 0.f;
	for (uint32 i = 0; i < (uint32)verts.Size(); ++i) {
		const NkVec3f &q = verts[i].pos;
		int32 t = (int32)(((q.y - ymin) / (ymax - ymin)) * (float)(tranches - 1) + 0.5f);
		t = t < 0 ? 0 : (t >= (int32)tranches ? (int32)tranches - 1 : t);
		const float dx = q.x - cx, dz = q.z - cz;
		const float r = sqrtf(dx * dx + dz * dz);
		if (r > rayon[t])
			rayon[t] = r;
	}
	// Un maximum local est un renflement. On exige une PROEMINENCE : sans elle,
	// le bruit de la grille creerait des bosses la ou il n'y a qu'une paroi
	// droite -- « un critere qui accepte une difference imperceptible ne mesure
	// pas ce qu'il annonce ».
	float rMax = 0.f;
	for (uint32 t = 0; t < tranches; ++t)
		if (rayon[t] > rMax)
			rMax = rayon[t];
	const float proeminence = 0.06f * rMax;
	uint32 n = 0;
	for (uint32 t = 1; t + 1 < tranches; ++t) {
		if (rayon[t] < rayon[t - 1] || rayon[t] < rayon[t + 1])
			continue;
		// Descendre a gauche et a droite jusqu'a retrouver une vallee assez
		// profonde : c'est ce qui distingue une vraie bosse d'une ondulation.
		float creuxG = rayon[t], creuxD = rayon[t];
		for (int32 k = (int32)t - 1; k >= 0; --k) {
			if (rayon[k] > rayon[t])
				break;
			if (rayon[k] < creuxG)
				creuxG = rayon[k];
		}
		for (uint32 k = t + 1; k < tranches; ++k) {
			if (rayon[k] > rayon[t])
				break;
			if (rayon[k] < creuxD)
				creuxD = rayon[k];
		}
		const float creux = creuxG > creuxD ? creuxG : creuxD;
		if (rayon[t] - creux >= proeminence) {
			if (n < capHauteurs) {
				hauteurs[n] = ymin + ((float)t / (float)(tranches - 1)) * (ymax - ymin);
				rayons[n] = rayon[t];
			}
			++n;
			// Ne pas recompter la meme bosse sur plusieurs tranches voisines.
			while (t + 1 < tranches && rayon[t + 1] >= rayon[t] - 1e-6f)
				++t;
		}
	}
	return n;
}

// Rend le nombre de lignes ROUGES. 0 = la geometrie est fidele au document.
static int VerifierContreDocument(const Scene &sc, const char *cheminObj, float soCell, float soOx,
								  float soOy, float soOz) {
	printf("\n[D] LA GEOMETRIE EST-ELLE FIDELE AU DOCUMENT ? (fidelite MECANIQUE)\n");
	printf("    ⚠ la fidelite HUMAINE -- « ce document decrit-il bien la demande » -- n'est PAS\n");
	printf("      mesuree ici, et ne le sera jamais : elle se lit, elle ne se compte pas.\n");
	NkGLTFMeshData data;
	if (!LoadOBJ(NkString(cheminObj), data) || !data.IsValid()) {
		printf("  [ROUGE] le maillage ecrit n'est pas relisible : rien ne peut se verifier\n");
		return 1;
	}
	int rouge = 0;

	// ── L'ATTENDU, DERIVE DU DOCUMENT ET DE RIEN D'AUTRE ────────────────────
	// Les parties ADDITIVES reliees par « pose_sur » forment une pile. Le
	// document en declare le nombre ; la geometrie doit le montrer.
	uint32 additives = 0;
	for (uint32 i = 0; i < sc.nPieces; ++i)
		if (sc.pieces[i].op != OP_DIFF)
			++additives;
	uint32 empilees = 0;
	for (uint32 i = 0; i < sc.nPieces; ++i)
		if (sc.poseSur[i] >= 0 && sc.pieces[i].op != OP_DIFF)
			++empilees;
	// Une pile de N relations touche N+1 parties -- mais seulement si les
	// parties empilees le sont toutes SUR LE MEME AXE. Une chaise empile quatre
	// pieds sur la meme assise, decales en x et z : ce n'est pas une pile.
	uint32 surMemeAxe = 0;
	for (uint32 i = 0; i < sc.nPieces; ++i) {
		if (sc.poseSur[i] < 0 || sc.pieces[i].op == OP_DIFF)
			continue;
		const Piece &a = sc.pieces[sc.poseSur[i]], &b = sc.pieces[i];
		const float dx = b.cx - a.cx, dz = b.cz - a.cz;
		const float dy = b.cy - a.cy;
		if (fabsf(dy) > fabsf(dx) && fabsf(dy) > fabsf(dz))
			++surMemeAxe;
	}

	NkVector<NkVertex3D> &V = data.vertices;
	float hauteurs[64], rayons[64];
	const uint32 renflements = CompterRenflements(V, 48, hauteurs, rayons, 64);
	printf("  DOCUMENT : %u partie(s) additive(s), %u relation(s) « pose_sur », dont %u sur le meme axe vertical\n",
		   additives, empilees, surMemeAxe);
	printf("  GEOMETRIE : %u renflement(s) mesure(s) le long de l'axe vertical (%u sommets relus)\n", renflements,
		   (uint32)V.Size());

	// ⚠️ LE CRITERE NE SE PRONONCE QUE LA OU IL A UN SENS. Une pile verticale
	// de N+1 parties doit montrer N+1 renflements. Hors de ce cas -- parties
	// decalees, soustractions, piece unique -- il se TAIT : « un temoin qui se
	// prononce hors de sa condition de validite fabrique du bruit qu'on apprend
	// a ignorer ».
	// ── LA CONDITION DU CRITERE, ET ELLE EST DERIVEE DE LA GEOMETRIE ────────
	// Un renflement est un MAXIMUM LOCAL du rayon le long de l'axe. Seules les
	// formes dont le rayon culmine en produisent un : sphere, capsule, tore. Un
	// cone s'elargit jusqu'a sa base et s'arrete ; un cylindre est constant ; un
	// cube aussi. Compter des renflements sur eux, c'est mesurer autre chose.
	bool toutesCulminent = true;
	for (uint32 i = 0; i < sc.nPieces; ++i) {
		if (sc.pieces[i].op == OP_DIFF)
			continue;
		const Forme f = sc.pieces[i].f;
		if (f != F_SPHERE && f != F_CAPSULE && f != F_TORE)
			toutesCulminent = false;
	}
	if (surMemeAxe >= 1 && surMemeAxe == empilees && additives == surMemeAxe + 1 && toutesCulminent) {
		const uint32 attendu = additives;
		const bool ok = (renflements == attendu);
		printf("  [%s] pile verticale : %u renflement(s) attendu(s), %u mesure(s)\n", ok ? "VERT " : "ROUGE", attendu,
			   renflements);
		if (!ok) {
			++rouge;
			printf("          -> LE DOCUMENT DECRIT UN OBJET QUE LA GEOMETRIE NE MONTRE PAS.\n");
		}
		// ── L'ORDRE DES LARGEURS, ET POURQUOI CE CRITERE-CI ET PAS L'AUTRE ──
		// Mon premier critere disait « les renflements sont ordonnes de bas en
		// haut ». Il ne pouvait PAS echouer : `CompterRenflements` parcourt les
		// tranches du bas vers le haut, donc ses hauteurs sont croissantes PAR
		// CONSTRUCTION. C'est la mutation NK_SCENE_MUTE=2 -- empiler vers le bas
		// -- qui l'a revele en restant VERTE. « La mutation n'a pas valide le
		// temoin, elle a revele que je n'en avais pas. »
		//
		// Celui-ci compare deux grandeurs REELLEMENT distinctes : l'ordre des
		// RAYONS mesures sur le maillage, et l'ordre des LARGEURS declarees dans
		// le document. Un bonhomme de neige declare 1,00 / 0,70 / 0,45 ; sorti a
		// l'envers, son plus gros renflement est en haut, et le critere rougit.
		if (renflements == additives && renflements >= 2 && renflements <= 64) {
			// Les largeurs declarees, du bas vers le haut : on suit la chaine
			// des relations depuis la partie qui n'en a aucune.
			float largeur[64];
			uint32 nl = 0;
			int32 courant = -1;
			for (uint32 i = 0; i < sc.nPieces && nl == 0; ++i)
				if (sc.poseSur[i] < 0 && sc.pieces[i].op != OP_DIFF)
					courant = (int32)i;
			while (courant >= 0 && nl < 64) {
				const Piece &q = sc.pieces[courant];
				largeur[nl++] = q.s * Mx(q.sx, q.sz);
				int32 suivant = -1;
				for (uint32 i = 0; i < sc.nPieces && suivant < 0; ++i)
					if (sc.poseSur[i] == courant && sc.pieces[i].op != OP_DIFF)
						suivant = (int32)i;
				courant = suivant;
			}
			if (nl == renflements) {
				uint32 accords = 0, paires = 0;
				for (uint32 k = 1; k < nl; ++k) {
					const bool decDoc = (largeur[k] < largeur[k - 1]);
					const bool decGeo = (rayons[k] < rayons[k - 1]);
					++paires;
					if (decDoc == decGeo)
						++accords;
				}
				const bool ok = (accords == paires);
				printf("  [%s] l'ordre des largeurs suit le document (%u paire(s) sur %u)\n", ok ? "VERT " : "ROUGE",
					   accords, paires);
				printf("          document :");
				for (uint32 k = 0; k < nl; ++k)
					printf(" %.2f", largeur[k]);
				printf("   |   geometrie :");
				for (uint32 k = 0; k < renflements && k < 64; ++k)
					printf(" %.2f", rayons[k]);
				printf("\n");
				if (!ok) {
					++rouge;
					printf("          -> L'OBJET N'EST PAS ORIENTE COMME LE DOCUMENT LE DIT.\n");
				}
			} else {
				printf("  [ -- ] ordre des largeurs NON APPLICABLE (la chaine de relations n'est pas lineaire)\n");
			}
		}
	} else {
		printf("  [ -- ] critere de pile NON APPLICABLE ici : %s -- il se TAIT\n",
			   !toutesCulminent ? "au moins une partie n'a pas de rayon qui CULMINE (cone, cylindre, cube)"
								: "parties decalees, soustraites, ou pile non lineaire");
	}

	// ── LES COMPOSANTES CONNEXES : LE CRITERE QUI ATTRAPE UNE PARTIE DETACHEE ─
	// DERIVE du document et de rien d'autre : si toutes les parties additives
	// sont reliees entre elles par des relations, l'objet est d'un seul tenant,
	// donc la geometrie doit montrer UNE composante. Si K parties n'ont aucune
	// relation, elles peuvent former jusqu'a K groupes.
	//
	// ⚠️ C'EST L'IMAGE QUI A COMMANDE CE CRITERE. La chaise etait verte, et son
	// dossier FLOTTAIT. chi = 4 le disait -- chi = 2(C - G) donne C = 2 -- et je
	// ne l'avais pas lu. Un chiffre juste qu'on ne lit pas ne vaut pas mieux
	// qu'un chiffre absent.
	{
		// Union-find sur les sommets, par les aretes des triangles du maillage
		// RELU : aucun code commun avec le placement.
		NkVector<uint32> parent;
		const uint32 nv = (uint32)V.Size();
		parent.Resize(nv);
		for (uint32 i = 0; i < nv; ++i)
			parent[i] = i;
		// Les sommets coincidents doivent etre soudes AVANT, sinon deux faces
		// qui se touchent passeraient pour disjointes.
		// (LoadOBJ rend deja des indices partages : on relie par les triangles.)
		struct F {
				static uint32 Trouver(NkVector<uint32> &p, uint32 a) {
					while (p[a] != a) {
						p[a] = p[p[a]];
						a = p[a];
					}
					return a;
				}
		};
		for (uint32 sm = 0; sm < (uint32)data.subMeshes.Size(); ++sm) {
			const NkSubMesh &S = data.subMeshes[sm];
			for (uint32 i = 0; i + 2 < S.indexCount; i += 3) {
				const uint32 a = data.indices[S.firstIndex + i] + S.baseVertex;
				const uint32 b = data.indices[S.firstIndex + i + 1] + S.baseVertex;
				const uint32 c = data.indices[S.firstIndex + i + 2] + S.baseVertex;
				if (a >= nv || b >= nv || c >= nv)
					continue;
				const uint32 ra = F::Trouver(parent, a), rb = F::Trouver(parent, b), rc = F::Trouver(parent, c);
				if (ra != rb)
					parent[rb] = ra;
				const uint32 ra2 = F::Trouver(parent, a), rc2 = F::Trouver(parent, c);
				if (ra2 != rc2)
					parent[rc2] = ra2;
			}
		}
		uint32 composantes = 0;
		for (uint32 i = 0; i < nv; ++i)
			if (F::Trouver(parent, i) == i)
				++composantes;
		// L'attendu : le nombre de GROUPES de parties additives reliees.
		uint32 racines = 0;
		for (uint32 i = 0; i < sc.nPieces; ++i)
			if (sc.pieces[i].op != OP_DIFF && sc.poseSur[i] < 0)
				++racines;
		const uint32 attenduC = racines == 0 ? 1 : racines;
		const bool okC = (composantes == attenduC);
		printf("  [%s] composantes connexes : %u attendue(s) (le document relie %u partie(s) en %u groupe(s)), "
			   "%u mesuree(s)\n",
			   okC ? "VERT " : "ROUGE", attenduC, additives, attenduC, composantes);
		if (!okC) {
			++rouge;
			printf("          -> UNE PARTIE EST DETACHEE, ou deux qui devaient l'etre se touchent.\n");
		}
	}

	// ── CHAQUE PARTIE DECLAREE A-T-ELLE DE LA MATIERE ? ─────────────────────
	// ⚠️ CE CRITERE MANQUAIT, ET C'EST L'IMAGE QUI L'A DIT. Le document declarait
	// une « antenne » de rayon reel 0,009 alors que le pas de grille valait
	// 0,0203 : plus fine que la moitie d'une cellule, elle n'a pas ete maillee
	// DU TOUT -- et aucun critere n'a rougi. Le compte de composantes ne la
	// voyait pas (elle n'est pas une racine), le compte de parties ne juge que le
	// document. Une partie DECLAREE pouvait donc disparaitre en silence.
	//
	// Le rayon de recherche est DERIVE de la piece : son rayon englobant plus une
	// cellule de tolerance. Il n'est pas choisi.
	{
		uint32 absentes = 0;
		char nomsAbsents[256];
		nomsAbsents[0] = 0;
		for (uint32 i = 0; i < sc.nPieces; ++i) {
			const Piece &q = sc.pieces[i];
			if (q.op == OP_DIFF)
				continue; // une piece soustraite ne DOIT pas laisser de matiere
			// ⚠️ ON EVALUE LE SDF DE LA PIECE, PAS UNE SPHERE ENGLOBANTE.
			// Premiere version : « un sommet existe-t-il a moins de r + 2 cellules
			// du centre ? ». Elle rendait VERT pour l'antenne -- parce que des
			// sommets de la TETE, voisine, tombaient dans ce rayon genereux. Le
			// critere ne distinguait pas la piece de ce qui l'entoure, donc il ne
			// pouvait pas echouer : « un critere qui ne peut pas distinguer ce
			// qu'il pretend distinguer » ne mesure rien.
			//
			// Le test juste : un sommet qui appartient a la piece P est sur la
			// SURFACE de P, donc son SDF y vaut ~0. Un sommet de la tete, lui, est
			// loin de la surface de l'antenne. La tolerance est UNE CELLULE, et
			// elle est derivee : c'est la precision avec laquelle le mailleur peut
			// placer un sommet.
			bool trouve = false;
			for (uint32 k = 0; k < (uint32)V.Size() && !trouve; ++k) {
				const NkVec3f w{V[k].pos.x + soOx, V[k].pos.y + soOy, V[k].pos.z + soOz};
				const float d = SdfPiece(q, w.x, w.y, w.z);
				if (d <= soCell && d >= -soCell)
					trouve = true;
			}
			if (!trouve) {
				++absentes;
				if (q.nom[0]) {
					const size_t l = strlen(nomsAbsents);
					snprintf(nomsAbsents + l, sizeof(nomsAbsents) - l, "%s%s", l ? ", " : "", q.nom);
				}
			}
		}
		const bool ok = (absentes == 0);
		printf("  [%s] chaque partie declaree a de la MATIERE dans le maillage : %u absente(s)%s%s\n",
			   ok ? "VERT " : "ROUGE", absentes, absentes ? " -> " : "", absentes ? nomsAbsents : "");
		if (!ok) {
			++rouge;
			printf("          -> UNE PARTIE DECLAREE N'EXISTE PAS DANS LA GEOMETRIE (trop fine pour la grille ?).\n");
		}
	}

	// ── LE CRITERE QUI PORTE SUR LA SORTIE, ET NON SUR LE DOCUMENT ──────────
	// ⚠️ Le critere des composantes connexes est VRAI et INSUFFISANT : trois
	// composantes ANONYMES et trois composantes NOMMEES donnent le meme chiffre.
	// Il est donc invariant par le defaut que Rodolf a vu dans Blender -- un seul
	// objet « nkgen_shape », impossible a selectionner partie par partie.
	// Celui-ci relit le fichier PAR LA VRAIE PORTE et compte les sous-maillages
	// NOMMES, puis verifie que leurs noms sont ceux du document.
	{
		uint32 attendus = 0;
		for (uint32 i = 0; i < sc.nPieces; ++i)
			if (sc.pieces[i].op != OP_DIFF)
				++attendus;
		const uint32 lus = (uint32)data.subMeshes.Size();
		printf("  SORTIE   : %u sous-maillage(s) relu(s) par NkOBJLoader, %u partie(s) additive(s) declaree(s)\n",
			   lus, attendus);
		// Les noms du document se retrouvent-ils dans la sortie ?
		uint32 nommes = 0;
		for (uint32 i = 0; i < sc.nPieces; ++i) {
			if (sc.pieces[i].op == OP_DIFF || !sc.pieces[i].nom[0])
				continue;
			for (uint32 k = 0; k < (uint32)data.subMeshes.Size(); ++k) {
				if (strcmp(data.subMeshes[k].name.CStr(), sc.pieces[i].nom) == 0) {
					++nommes;
					break;
				}
			}
		}
		const bool ok = (lus >= attendus) && (nommes == attendus);
		Attendu(ok, "chaque partie declaree est un objet NOMME dans le fichier produit", attendus, nommes);
		if (!ok)
			printf("          -> LE FICHIER N'EST PAS UTILISABLE PARTIE PAR PARTIE (un seul objet ?).\n");
	}

	// Le compte de parties, lui, se verifie toujours -- contre le DOCUMENT, pas
	// contre la phrase.
	const bool okN = (sc.nPieces > 0);
	printf("  [%s] le document declare au moins une partie\n", okN ? "VERT " : "ROUGE");
	if (!okN)
		++rouge;

	printf("  VERDICT DOCUMENT : %s (%d rouge)\n", rouge == 0 ? "VERT" : "ROUGE", rouge);
	return rouge;
}


// =============================================================================
// LES PRIMITIVES CONSTRUITES, ET NON EXTRAITES.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
//
// LE CONSTAT QUI A IMPOSE CE CHEMIN. Le document rendait 36 596 sommets et
// 73 180 faces -- 100 % de TRIANGLES, ZERO quad, ZERO UV -- pour SIX primitives.
// Rodolf, en mode edition : « il faut tout faire pour avoir un maillage moins
// dense, beaucoup plus propre, qui represente bien toutes les formes, les
// contours, les bosses, les creux, les normales, sans pour autant avoir autant
// de vertices », et « le mieux, ce sont les quads ».
//
// LA CAUSE ETAIT STRUCTURELLE : je faisais passer les primitives par un CHAMP DE
// DENSITE puis par marching cubes, comme s'il fallait les DECOUVRIR. Or je les
// connais ANALYTIQUEMENT. Une capsule ne s'extrait pas -- elle se CONSTRUIT.
//
// ⚠️ ET CE N'EST PAS LE CHANTIER DE RETOPOLOGIE. Celui-la resout le cas GENERAL :
// reprendre un maillage quelconque deja extrait et le requadranguler selon la
// courbure. Ici il n'y a rien a reprendre : il y a une equation. C'est le cas
// facile, et il ne doit pas consommer leur solveur.
//
// CE QUE CE CHEMIN NE SAIT PAS FAIRE, ET C'EST ECRIT AVANT : les BOOLEENS. Une
// soustraction ou une union LISSE exigent un champ, c'est leur definition meme.
// Les parties en union simple passent ici ; le reste garde l'ancien chemin.
//
// LES SOMMETS SONT SUR LA SURFACE PAR CONSTRUCTION : l'ecart au SDF doit etre
// NUL, pas « petit ». Ce n'est pas une tolerance, c'est une identite -- et c'est
// le critere [1] du R29.2.
// =============================================================================

struct NkQuadMesh {
		NkVector<NkVec3f> pos;
		NkVector<NkVec3f> nor; // normales ANALYTIQUES, jamais moyennees
		NkVector<NkVec2f> uv;
		NkVector<uint32> quads; // 4 indices par quad
		NkVector<uint32> tris;  // 3 indices par triangle (pôles uniquement)
		char nom[32] = {0};
};

// Applique l'echelle par axe, la rotation et la translation d'une piece a un
// point ET a sa normale. ⚠️ La normale ne se transforme PAS comme un point sous
// une echelle anisotrope : elle se divise par l'echelle au lieu d'y etre
// multipliee. L'oublier donne des normales fausses sur toute forme aplatie --
// et elles se voient a l'eclairage, pas dans un compteur.
static void NkPrimPlacer(const Piece &p, NkVec3f &q, NkVec3f &n) {
	q.x *= p.sx;
	q.y *= p.sy;
	q.z *= p.sz;
	n.x /= (p.sx != 0.f ? p.sx : 1e-6f);
	n.y /= (p.sy != 0.f ? p.sy : 1e-6f);
	n.z /= (p.sz != 0.f ? p.sz : 1e-6f);
	if (p.rx != 0.f || p.ry != 0.f || p.rz != 0.f) {
		const float k = 3.14159265358979f / 180.f;
		const float a = p.rx * k, b = p.ry * k, c = p.rz * k;
		NkVec3f *v[2] = {&q, &n};
		for (int i = 0; i < 2; i++) {
			float x = v[i]->x, y = v[i]->y, z = v[i]->z, t;
			t = y * cosf(a) - z * sinf(a);
			z = y * sinf(a) + z * cosf(a);
			y = t;
			t = x * cosf(b) + z * sinf(b);
			z = -x * sinf(b) + z * cosf(b);
			x = t;
			t = x * cosf(c) - y * sinf(c);
			y = x * sinf(c) + y * cosf(c);
			x = t;
			v[i]->x = x;
			v[i]->y = y;
			v[i]->z = z;
		}
	}
	const float ln = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
	if (ln > 0.f) {
		n.x /= ln;
		n.y /= ln;
		n.z /= ln;
	}
	q.x += p.cx;
	q.y += p.cy;
	q.z += p.cz;
}

static void NkPrimAjouter(NkQuadMesh &m, const Piece &p, NkVec3f q, NkVec3f n, float u, float v) {
	NkPrimPlacer(p, q, n);
	m.pos.PushBack(q);
	m.nor.PushBack(n);
	m.uv.PushBack(NkVec2f{u, v});
}

// Une grille (A+1) x (S+1) de sommets -> A x S quads. La couture est DEDOUBLEE
// (S+1 colonnes au lieu de S) : sans cela, les UV sauteraient de 1 a 0 sur la
// derniere colonne et la texture se replierait sur toute la largeur.
static void NkPrimGrille(NkQuadMesh &m, uint32 base, uint32 A, uint32 S, bool polesTri) {
	for (uint32 a = 0; a < A; ++a) {
		for (uint32 s = 0; s < S; ++s) {
			const uint32 i0 = base + a * (S + 1) + s;
			const uint32 i1 = i0 + 1;
			const uint32 i2 = i0 + (S + 1) + 1;
			const uint32 i3 = i0 + (S + 1);
			const bool poleHaut = polesTri && a == 0;
			const bool poleBas = polesTri && a == A - 1;
			if (poleHaut) {
				// Au pole, deux sommets de la rangee coincident : un quad y serait
				// DEGENERE. On emet un triangle -- c'est une propriete de la
				// parametrisation spherique, pas un choix de confort.
				m.tris.PushBack(i0);
				m.tris.PushBack(i2);
				m.tris.PushBack(i3);
			} else if (poleBas) {
				m.tris.PushBack(i0);
				m.tris.PushBack(i1);
				m.tris.PushBack(i2);
			} else {
				m.quads.PushBack(i0);
				m.quads.PushBack(i1);
				m.quads.PushBack(i2);
				m.quads.PushBack(i3);
			}
		}
	}
}

// Construit la primitive d'une piece. `A` anneaux, `S` segments.
static bool NkPrimConstruire(const Piece &p, uint32 A, uint32 S, NkQuadMesh &m) {
	const float PI = 3.14159265358979f;
	const uint32 base = (uint32)m.pos.Size();
	const float s = p.s;
	switch (p.f) {
		case F_SPHERE: {
			for (uint32 a = 0; a <= A; ++a) {
				const float th = PI * (float)a / (float)A; // 0 = pole haut
				for (uint32 j = 0; j <= S; ++j) {
					const float ph = 2.f * PI * (float)j / (float)S;
					const NkVec3f n{sinf(th) * cosf(ph), cosf(th), sinf(th) * sinf(ph)};
					NkPrimAjouter(m, p, NkVec3f{n.x * s, n.y * s, n.z * s}, n, (float)j / (float)S,
								  1.f - (float)a / (float)A);
				}
			}
			NkPrimGrille(m, base, A, S, true);
			return true;
		}
		case F_CAPSULE: {
			// Les memes constantes que `SdfFormeLocale` : r = 0,45 s et h = 0,55 s.
			// ⚠️ Elles sont RECOPIEES, et c'est la dette deja nommee pour
			// `DemiHauteurY` : si l'une bouge la-bas, elle doit bouger ici.
			const float r = 0.45f * s, h = 0.55f * s;
			const uint32 Ac = A / 3 > 1 ? A / 3 : 2;   // anneaux par calotte
			const uint32 Am = A - 2 * Ac > 1 ? A - 2 * Ac : 2; // anneaux du tube
			for (uint32 a = 0; a <= Ac + Am + Ac; ++a) {
				float y, rr, ny, nr;
				if (a <= Ac) { // calotte haute
					const float th = 0.5f * PI * (float)a / (float)Ac;
					y = h + r * cosf(th);
					rr = r * sinf(th);
					ny = cosf(th);
					nr = sinf(th);
				} else if (a <= Ac + Am) { // tube
					const float t = (float)(a - Ac) / (float)Am;
					y = h - 2.f * h * t;
					rr = r;
					ny = 0.f;
					nr = 1.f;
				} else { // calotte basse
					const float th = 0.5f * PI * (float)(a - Ac - Am) / (float)Ac;
					y = -h - r * sinf(th);
					rr = r * cosf(th);
					ny = -sinf(th);
					nr = cosf(th);
				}
				for (uint32 j = 0; j <= S; ++j) {
					const float ph = 2.f * PI * (float)j / (float)S;
					const NkVec3f n{nr * cosf(ph), ny, nr * sinf(ph)};
					NkPrimAjouter(m, p, NkVec3f{rr * cosf(ph), y, rr * sinf(ph)}, n, (float)j / (float)S,
								  1.f - (float)a / (float)(Ac + Am + Ac));
				}
			}
			NkPrimGrille(m, base, Ac + Am + Ac, S, true);
			return true;
		}
		case F_CYLINDRE: {
			const float r = 0.55f * s, h = 0.9f * s;
			// Paroi : A anneaux de quads, AUCUN triangle.
			for (uint32 a = 0; a <= A; ++a) {
				const float y = h - 2.f * h * (float)a / (float)A;
				for (uint32 j = 0; j <= S; ++j) {
					const float ph = 2.f * PI * (float)j / (float)S;
					const NkVec3f n{cosf(ph), 0.f, sinf(ph)};
					NkPrimAjouter(m, p, NkVec3f{r * cosf(ph), y, r * sinf(ph)}, n, (float)j / (float)S,
								  1.f - (float)a / (float)A);
				}
			}
			NkPrimGrille(m, base, A, S, false);
			// Les deux disques, en eventail : un disque n'a pas de quadrangulation
			// naturelle, et une grille y creerait des quads degeneres au centre.
			for (int32 k = 0; k < 2; ++k) {
				const float y = k == 0 ? h : -h;
				const NkVec3f n{0.f, k == 0 ? 1.f : -1.f, 0.f};
				const uint32 c = (uint32)m.pos.Size();
				NkPrimAjouter(m, p, NkVec3f{0.f, y, 0.f}, n, 0.5f, 0.5f);
				for (uint32 j = 0; j <= S; ++j) {
					const float ph = 2.f * PI * (float)j / (float)S;
					NkPrimAjouter(m, p, NkVec3f{r * cosf(ph), y, r * sinf(ph)}, n, 0.5f + 0.5f * cosf(ph),
								  0.5f + 0.5f * sinf(ph));
				}
				for (uint32 j = 0; j < S; ++j) {
					m.tris.PushBack(c);
					m.tris.PushBack(k == 0 ? c + 1 + j : c + 2 + j);
					m.tris.PushBack(k == 0 ? c + 2 + j : c + 1 + j);
				}
			}
			return true;
		}
		case F_TORE: {
			// ⚠️ LE TORE EST LE CAS PARFAIT : il se parametre SANS AUCUN POLE,
			// donc 100 % de quads et ZERO triangle. C'est pourquoi le critere [2]
			// l'exige sur lui et pas sur une sphere.
			const float R = 0.68f * s, r = 0.28f * s;
			for (uint32 a = 0; a <= A; ++a) {
				const float th = 2.f * PI * (float)a / (float)A;
				for (uint32 j = 0; j <= S; ++j) {
					const float ph = 2.f * PI * (float)j / (float)S;
					const NkVec3f n{cosf(th) * cosf(ph), sinf(th), cosf(th) * sinf(ph)};
					const NkVec3f q{(R + r * cosf(th)) * cosf(ph), r * sinf(th), (R + r * cosf(th)) * sinf(ph)};
					NkPrimAjouter(m, p, q, n, (float)j / (float)S, (float)a / (float)A);
				}
			}
			NkPrimGrille(m, base, A, S, false);
			return true;
		}
		case F_CUBE: {
			const float h = 0.62f * s;
			const NkVec3f nz[6] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
			const NkVec3f ux[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {1, 0, 0}};
			const NkVec3f uy[6] = {{0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
			const uint32 N = S > 1 ? S : 1;
			for (int32 fi = 0; fi < 6; ++fi) {
				const uint32 b = (uint32)m.pos.Size();
				for (uint32 a = 0; a <= N; ++a) {
					for (uint32 j = 0; j <= N; ++j) {
						const float u = -1.f + 2.f * (float)j / (float)N;
						const float v = -1.f + 2.f * (float)a / (float)N;
						const NkVec3f q{(nz[fi].x + ux[fi].x * u + uy[fi].x * v) * h,
										(nz[fi].y + ux[fi].y * u + uy[fi].y * v) * h,
										(nz[fi].z + ux[fi].z * u + uy[fi].z * v) * h};
						NkPrimAjouter(m, p, q, nz[fi], (float)j / (float)N, (float)a / (float)N);
					}
				}
				NkPrimGrille(m, b, N, N, false);
			}
			return true;
		}
		case F_CONE: {
			const float h = 0.95f * s, r = 0.75f * s;
			for (uint32 a = 0; a <= A; ++a) {
				const float t = (float)a / (float)A; // 0 = pointe
				const float y = h - 2.f * h * t;
				const float rr = r * t;
				for (uint32 j = 0; j <= S; ++j) {
					const float ph = 2.f * PI * (float)j / (float)S;
					// La normale d'un cone : la pente est constante.
					const float k = 1.f / sqrtf(1.f + (r / (2.f * h)) * (r / (2.f * h)));
					const NkVec3f n{k * cosf(ph), k * (r / (2.f * h)), k * sinf(ph)};
					NkPrimAjouter(m, p, NkVec3f{rr * cosf(ph), y, rr * sinf(ph)}, n, (float)j / (float)S, 1.f - t);
				}
			}
			NkPrimGrille(m, base, A, S, true);
			{
				const NkVec3f n{0.f, -1.f, 0.f};
				const uint32 c = (uint32)m.pos.Size();
				NkPrimAjouter(m, p, NkVec3f{0.f, -h, 0.f}, n, 0.5f, 0.5f);
				for (uint32 j = 0; j <= S; ++j) {
					const float ph = 2.f * PI * (float)j / (float)S;
					NkPrimAjouter(m, p, NkVec3f{r * cosf(ph), -h, r * sinf(ph)}, n, 0.5f + 0.5f * cosf(ph),
								  0.5f + 0.5f * sinf(ph));
				}
				for (uint32 j = 0; j < S; ++j) {
					m.tris.PushBack(c);
					m.tris.PushBack(c + 2 + j);
					m.tris.PushBack(c + 1 + j);
				}
			}
			return true;
		}
		default:
			return false;
	}
}

// Ecrit un .obj avec QUADS, UV et NORMALES, un objet nomme par partie.
static bool NkPrimEcrireObj(const char *chemin, const NkVector<NkQuadMesh> &parties, bool solidaire,
							const char *nomScene, uint32 &outQuads, uint32 &outTris, uint32 &outSommets) {
	FILE *f = fopen(chemin, "wb");
	if (!f)
		return false;
	fprintf(f, "# NKTexte3D -- primitives CONSTRUITES analytiquement, pas extraites.\n");
	fprintf(f, "# AUTEUR : TEUGUIA TADJUIDJE Rodolf Sederis - Rihen\n");
	fprintf(f, "# Quads, UV et normales ANALYTIQUES. Un « o <nom> » par partie declaree.\n");
	uint32 base = 1;
	outQuads = outTris = outSommets = 0;
	for (uint32 k = 0; k < (uint32)parties.Size(); ++k) {
		const NkQuadMesh &m = parties[k];
		for (uint32 i = 0; i < (uint32)m.pos.Size(); ++i)
			fprintf(f, "v %.6f %.6f %.6f\n", m.pos[i].x, m.pos[i].y, m.pos[i].z);
		for (uint32 i = 0; i < (uint32)m.uv.Size(); ++i)
			fprintf(f, "vt %.6f %.6f\n", m.uv[i].x, m.uv[i].y);
		for (uint32 i = 0; i < (uint32)m.nor.Size(); ++i)
			fprintf(f, "vn %.6f %.6f %.6f\n", m.nor[i].x, m.nor[i].y, m.nor[i].z);
		// SOLIDAIRE : un seul « o », puis un « g » par partie. NkOBJLoader lit
		// les deux (l. 339 et 347) et rend un NkSubMesh NOMME dans les deux cas :
		// les parties restent donc identifiables A L'INTERIEUR du modele.
		if (solidaire) {
			if (k == 0)
				fprintf(f, "o %s\n", nomScene && nomScene[0] ? nomScene : "modele");
			fprintf(f, "g %s\n", m.nom[0] ? m.nom : "partie");
		} else {
			fprintf(f, "o %s\n", m.nom[0] ? m.nom : "partie");
		}
		for (uint32 i = 0; i + 3 < (uint32)m.quads.Size() + 1; i += 4) {
			const uint32 a = m.quads[i] + base, b = m.quads[i + 1] + base;
			const uint32 c = m.quads[i + 2] + base, d = m.quads[i + 3] + base;
			fprintf(f, "f %u/%u/%u %u/%u/%u %u/%u/%u %u/%u/%u\n", a, a, a, b, b, b, c, c, c, d, d, d);
			++outQuads;
		}
		for (uint32 i = 0; i + 2 < (uint32)m.tris.Size() + 1; i += 3) {
			const uint32 a = m.tris[i] + base, b = m.tris[i + 1] + base, c = m.tris[i + 2] + base;
			fprintf(f, "f %u/%u/%u %u/%u/%u %u/%u/%u\n", a, a, a, b, b, b, c, c, c);
			++outTris;
		}
		base += (uint32)m.pos.Size();
		outSommets += (uint32)m.pos.Size();
	}
	fclose(f);
	return true;
}

// Le chemin ANALYTIQUE complet : construire, mesurer, ecrire. Rend le nombre de
// lignes rouges.
static int NkPrimProduire(const Scene &sc, const char *out, uint32 A, uint32 S) {
	printf("== NKTexte3D : primitives CONSTRUITES (%u anneaux x %u segments) -> %s ==\n", A, S, out);
	NkVector<NkQuadMesh> parties;
	for (uint32 i = 0; i < sc.nPieces; ++i) {
		const Piece &p = sc.pieces[i];
		if (p.op != OP_UNION)
			continue;
		NkQuadMesh m;
		snprintf(m.nom, sizeof(m.nom), "%s", p.nom[0] ? p.nom : "partie");
		if (!NkPrimConstruire(p, A, S, m)) {
			printf("  REFUS : forme non constructible analytiquement : %s\n", NomForme(p.f));
			return 1;
		}
		parties.PushBack(m);
	}
	if (parties.Size() == 0) {
		printf("  REFUS : aucune partie en union simple (les booleens exigent le chemin du champ)\n");
		return 1;
	}

	uint32 nq = 0, nt = 0, nv = 0;
	if (!NkPrimEcrireObj(out, parties, sc.solidaire, sc.nomScene, nq, nt, nv)) {
		printf("  REFUS : ecriture impossible : %s\n", out);
		return 1;
	}
	printf("  MESURE : %u sommets · %u QUADS · %u triangles · %u partie(s) nommee(s)\n", nv, nq, nt,
		   (uint32)parties.Size());
	printf("  ASSEMBLAGE : %s\n", sc.solidaire
									   ? "SOLIDAIRE -- un objet, un groupe « g » par partie (un CORPS : le bras bouge par les os)"
									   : "INDEPENDANT -- un objet « o » par partie (un VEHICULE : les pieces bougent seules)");

	int rouge = 0;

	// ── [1] FIDELITE : les sommets sont SUR la surface, l'ecart doit etre NUL ──
	// Ce n'est pas une tolerance, c'est une identite : le sommet est calcule
	// depuis l'equation de la surface. 1e-5 laisse passer l'erreur du float, rien
	// de plus.
	{
		float pire = 0.f;
		uint32 ip = 0;
		for (uint32 k = 0; k < (uint32)parties.Size(); ++k) {
			uint32 pi = 0, n = 0;
			for (uint32 i = 0; i < sc.nPieces; ++i)
				if (sc.pieces[i].op == OP_UNION && n++ == k)
					pi = i;
			for (uint32 v = 0; v < (uint32)parties[k].pos.Size(); ++v) {
				const NkVec3f &q = parties[k].pos[v];
				float d = SdfPiece(sc.pieces[pi], q.x, q.y, q.z);
				if (d < 0.f)
					d = -d;
				if (d > pire) {
					pire = d;
					ip = k;
				}
			}
		}
		const bool ok = pire <= 1e-5f;
		printf("  [%s] fidelite : ecart MAXIMAL a la surface analytique = %.3e (partie %u)\n",
			   ok ? "VERT " : "ROUGE", (double)pire, ip);
		if (!ok) {
			++rouge;
			printf("          -> un sommet n'est PAS sur la surface : l'equation et la construction divergent.\n");
		}
	}

	// ── [2] QUADS : zero triangle attendu sur un TORE ou un CYLINDRE ──────────
	{
		uint32 triAttendus = 0;
		for (uint32 i = 0; i < sc.nPieces; ++i) {
			if (sc.pieces[i].op != OP_UNION)
				continue;
			switch (sc.pieces[i].f) {
				case F_TORE: break;                              // aucun pole : 0 triangle
				case F_CUBE: break;                              // six grilles : 0 triangle
				case F_SPHERE: triAttendus += 2 * S; break;      // deux poles
				case F_CAPSULE: triAttendus += 2 * S; break;     // deux calottes
				case F_CYLINDRE: triAttendus += 2 * S; break;    // deux disques en eventail
				case F_CONE: triAttendus += S + S; break;        // pointe + disque
				default: break;
			}
		}
		const bool ok = (nt == triAttendus);
		printf("  [%s] quads : %u quads, %u triangles (attendus %u : poles et disques seulement)\n",
			   ok ? "VERT " : "ROUGE", nq, nt, triAttendus);
		if (!ok)
			++rouge;
		const float ratio = (nq + nt) ? 100.f * (float)nq / (float)(nq + nt) : 0.f;
		printf("           proportion de quads : %.1f %%\n", (double)ratio);
	}

	// ── [3] DENSITE : le compte suit la multiplication, ce n'est pas une estimation
	printf("  [INFO ] densite reglable : %u anneaux x %u segments\n", A, S);

	// ── [4] NORMALES : unitaires, et analytiques (jamais moyennees) ───────────
	{
		float pire = 0.f;
		for (uint32 k = 0; k < (uint32)parties.Size(); ++k)
			for (uint32 v = 0; v < (uint32)parties[k].nor.Size(); ++v) {
				const NkVec3f &n = parties[k].nor[v];
				const float l = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
				const float e = l > 1.f ? l - 1.f : 1.f - l;
				if (e > pire)
					pire = e;
			}
		const bool ok = pire <= 1e-4f;
		printf("  [%s] normales : ecart maximal a la norme unite = %.3e\n", ok ? "VERT " : "ROUGE", (double)pire);
		if (!ok)
			++rouge;
	}

	// ── [5] UV : presentes, et dans [0,1] ────────────────────────────────────
	{
		uint32 nuv = 0, hors = 0;
		for (uint32 k = 0; k < (uint32)parties.Size(); ++k)
			for (uint32 v = 0; v < (uint32)parties[k].uv.Size(); ++v) {
				++nuv;
				const NkVec2f &t = parties[k].uv[v];
				if (t.x < -1e-4f || t.x > 1.0001f || t.y < -1e-4f || t.y > 1.0001f)
					++hors;
			}
		const bool ok = (nuv == nv) && (hors == 0);
		printf("  [%s] UV : %u coordonnees pour %u sommets, %u hors de [0,1]\n", ok ? "VERT " : "ROUGE", nuv, nv, hors);
		if (!ok)
			++rouge;
	}

	// ── [6] LES NOMS, RELUS PAR LA VRAIE PORTE ───────────────────────────────
	{
		NkGLTFMeshData data;
		const bool lu = LoadOBJ(NkString(out), data) && data.IsValid();
		uint32 nommes = 0;
		if (lu)
			for (uint32 k = 0; k < (uint32)parties.Size(); ++k)
				for (uint32 j = 0; j < (uint32)data.subMeshes.Size(); ++j)
					if (strcmp(data.subMeshes[j].name.CStr(), parties[k].nom) == 0) {
						++nommes;
						break;
					}
		// ⚠️ LE CRITERE PORTE SUR CE QUI EST VRAI, ET IL DIFFERE SELON L'ASSEMBLAGE.
		// MESURE du 19/09 : `NkOBJLoader` coupe le sous-mesh sur « o » mais PAS
		// sur « g » -- et il le dit lui-meme en commentaire : « FRONTIERE DE
		// MODEL. Ne coupe PAS le sous-mesh ici [...] la coupe sur o est un
		// changement de comportement, elle vient a part ».
		//
		// Donc un corps SOLIDAIRE (un « o », N « g ») revient a UN sous-mesh chez
		// nous. Ce n'est PAS un defaut du fichier : dans Blender un « g » devient
		// un VERTEX GROUP -- exactement le mecanisme du skinning, donc la bonne
		// forme pour un corps qu'on va rigger. C'est NOTRE lecteur qui ne sait pas
		// encore les lire, et c'est une navette, pas un defaut a corriger ici.
		if (sc.solidaire) {
			const bool okS = lu && (uint32)data.subMeshes.Size() >= 1;
			printf("  [%s] SOLIDAIRE : %u sous-mesh relu(s) (attendu 1) -- les parties vivent dans les « g »\n",
				   okS ? "VERT " : "ROUGE", lu ? (uint32)data.subMeshes.Size() : 0);
			printf("           ⚠ NkOBJLoader ne coupe pas sur « g » : navette pour l'agent du moteur.\n");
			printf("             Dans Blender ces groupes deviennent des VERTEX GROUPS -- ce qu'il faut pour rigger.\n");
			if (!okS)
				++rouge;
		} else {
			const bool ok = lu && nommes == (uint32)parties.Size();
			printf("  [%s] INDEPENDANT : noms relus par NkOBJLoader : %u / %u\n", ok ? "VERT " : "ROUGE", nommes,
				   (uint32)parties.Size());
			if (!ok)
				++rouge;
		}
	}

	printf("  VERDICT PRIMITIVES : %s (%d rouge)\n", rouge == 0 ? "VERT" : "ROUGE", rouge);
	return rouge;
}

// ── REECRIRE LE .OBJ AVEC UN OBJET NOMME PAR PARTIE ─────────────────────────
// Rend le nombre de groupes ecrits, 0 si echec. Le fichier d'entree est celui
// que `gen::SaveMeshObj` vient d'ecrire ; on le relit et on le remplace.
//
// ⚠️ LES SOMMETS NE BOUGENT PAS D'UN BIT. On ne fait que REGROUPER des faces :
// une sortie qui deplacerait un sommet ne serait plus la meme geometrie, et le
// critere des composantes connexes deviendrait un mensonge.
static uint32 ReecrireAvecNoms(const char *chemin, const Scene &sc, float ox, float oy, float oz) {
	NkGLTFMeshData data;
	if (!LoadOBJ(NkString(chemin), data) || !data.IsValid())
		return 0;
	// Les pieces ADDITIVES seulement : une piece soustraite ne laisse pas de
	// surface qui lui appartienne.
	NkVector<uint32> idxAdd;
	for (uint32 i = 0; i < sc.nPieces; ++i)
		if (sc.pieces[i].op != OP_DIFF)
			idxAdd.PushBack(i);
	if (idxAdd.Size() == 0)
		return 0;

	const uint32 nTri = (uint32)data.indices.Size() / 3;
	NkVector<uint32> partieDe;
	partieDe.Resize(nTri);
	for (uint32 t = 0; t < nTri; ++t) {
		const NkVec3f &a = data.vertices[data.indices[t * 3 + 0]].pos;
		const NkVec3f &b = data.vertices[data.indices[t * 3 + 1]].pos;
		const NkVec3f &c = data.vertices[data.indices[t * 3 + 2]].pos;
		const float cx = (a.x + b.x + c.x) / 3.f + ox;
		const float cy = (a.y + b.y + c.y) / 3.f + oy;
		const float cz = (a.z + b.z + c.z) / 3.f + oz;
		uint32 best = idxAdd[0];
		float bestD = 1e30f;
		for (uint32 k = 0; k < (uint32)idxAdd.Size(); ++k) {
			float d = SdfPiece(sc.pieces[idxAdd[k]], cx, cy, cz);
			if (d < 0.f)
				d = -d;
			if (d < bestD) {
				bestD = d;
				best = idxAdd[k];
			}
		}
		partieDe[t] = best;
	}

	FILE *f = fopen(chemin, "wb");
	if (!f)
		return 0;
	fprintf(f, "# NKTexte3D -- maillage genere depuis un DOCUMENT DE SCENE.\n");
	fprintf(f, "# AUTEUR : TEUGUIA TADJUIDJE Rodolf Sederis - Rihen\n");
	fprintf(f, "# Un « o <nom> » par PARTIE DECLAREE : c'est ce qui rend chaque partie\n");
	fprintf(f, "# selectionnable et animable dans Blender ou dans le modeleur.\n");
	for (uint32 i = 0; i < (uint32)data.vertices.Size(); ++i)
		fprintf(f, "v %.6f %.6f %.6f\n", data.vertices[i].pos.x, data.vertices[i].pos.y, data.vertices[i].pos.z);
	uint32 groupes = 0;
	for (uint32 k = 0; k < (uint32)idxAdd.Size(); ++k) {
		const uint32 pi = idxAdd[k];
		uint32 n = 0;
		for (uint32 t = 0; t < nTri; ++t)
			if (partieDe[t] == pi)
				++n;
		if (n == 0)
			continue; // une partie sans aucune face ne produit pas de groupe VIDE
		fprintf(f, "o %s\n", sc.pieces[pi].nom[0] ? sc.pieces[pi].nom : "partie");
		++groupes;
		for (uint32 t = 0; t < nTri; ++t) {
			if (partieDe[t] != pi)
				continue;
			fprintf(f, "f %u %u %u\n", data.indices[t * 3 + 0] + 1, data.indices[t * 3 + 1] + 1,
					data.indices[t * 3 + 2] + 1);
		}
	}
	fclose(f);
	return groupes;
}

int main(int argc, char **argv) {
	const char *texte = nullptr;
	const char *scene = nullptr;
	bool verifier = false;
	bool primitives = false;
	uint32 anneaux = 16, segments = 24;
	const char *out = nullptr;
	const char *dossier = ".";
	uint32 res = 64;
	bool banc = false;
	bool bancVariete = false;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--texte") == 0 && i + 1 < argc) texte = argv[++i];
		else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) out = argv[++i];
		else if (strcmp(argv[i], "--res") == 0 && i + 1 < argc) res = (uint32)atoi(argv[++i]);
		else if (strcmp(argv[i], "--dossier") == 0 && i + 1 < argc) dossier = argv[++i];
		else if (strcmp(argv[i], "--banc") == 0) banc = true;
		else if (strcmp(argv[i], "--banc-variete") == 0) bancVariete = true;
		else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) scene = argv[++i];
		else if (strcmp(argv[i], "--verifier") == 0) verifier = true;
		else if (strcmp(argv[i], "--primitives") == 0) primitives = true;
		else if (strcmp(argv[i], "--anneaux") == 0 && i + 1 < argc) anneaux = (uint32)atoi(argv[++i]);
		else if (strcmp(argv[i], "--segments") == 0 && i + 1 < argc) segments = (uint32)atoi(argv[++i]);
		else
			return Refus("argument inconnu : %s\nusage : NKTexte3D --texte \"<phrase>\" --out <f.obj> [--res N]\n"
						 "        NKTexte3D --scene <doc.nkscene> --out <f.obj> [--verifier] [--res N]\n"
						 "        NKTexte3D --banc [--res N] [--dossier <d>]",
						 argv[i]);
	}
	if (bancVariete)
		return BancVariete(dossier);
	if (banc)
		return Banc(res, dossier);
	// ── LE DOCUMENT DE SCENE : le chemin qui REPARE le defaut du 17/09 ──────
	if (scene) {
		if (texte)
			return Refus("--texte et --scene ensemble : deux sources pour une meme scene. Choisis-en une.");
		if (!out || !SortieEstObj(out))
			return Refus("la sortie doit finir par .obj : %s", out ? out : "(null)");
		if (res < 8 || res > 512)
			return Refus("resolution hors bornes : %u (attendu entre 8 et 512)", res);
		printf("== NKTexte3D : document « %s » -> %s ==\n", scene, out);
		Scene sc;
		char pourquoi[512];
		if (LireScene(scene, sc, pourquoi, sizeof(pourquoi)) == 0)
			return Refus("%s", pourquoi);

		// ── LE CHEMIN ANALYTIQUE : CONSTRUIRE AU LIEU D'EXTRAIRE ────────────
		// Il s'AJOUTE au chemin du champ, il ne le remplace pas : les booleens
		// et l'union lisse exigent un champ, c'est leur definition meme. Le banc
		// a neuf cas continue donc de passer par l'ancien chemin, et ses comptes
		// ne doivent pas bouger d'un sommet.
		if (primitives) {
			if (anneaux < 3 || anneaux > 256 || segments < 3 || segments > 256)
				return Refus("anneaux et segments doivent etre entre 3 et 256 (recus %u et %u)", anneaux, segments);
			for (uint32 i = 0; i < sc.nPieces; ++i)
				if (sc.pieces[i].op != OP_UNION)
					return Refus("« %s » utilise un operateur booleen : le chemin analytique ne sait pas le faire.\n"
								 "        Retire --primitives pour passer par le champ (marching cubes).",
								 sc.pieces[i].nom[0] ? sc.pieces[i].nom : "une partie");
			if (sc.lissage > 0.f)
				return Refus("le document demande un lissage (%.3f) : il exige un champ.\n"
							 "        Retire --primitives, ou retire le lissage.",
							 (double)sc.lissage);
			return NkPrimProduire(sc, out, anneaux, segments) == 0 ? 0 : 1;
		}
		// ── LES MUTATIONS DU DOCUMENT, DANS LE MEME BINAIRE ─────────────────
		// Sans elles, le vert du critere ne prouve rien : « une mutation qui
		// survit dit : ce critere ne teste rien ». Deux constructions separees
		// pourraient differer par autre chose ; une variable d'environnement
		// isole la seule cause testee.
		if (const char *mu = getenv("NK_SCENE_MUTE")) {
			if (mu[0] == '1') {
				// Noyer les jonctions : les parties se fondent, les renflements
				// disparaissent. Le COMPTE doit tomber, donc le critere rougir.
				sc.lissage = 0.60f;
				printf("  MUTATION NK_SCENE_MUTE=1 : lissage force a 0,60 (les jonctions se noient)\n");
			}
			if (mu[0] == '2') {
				// Empiler VERS LE BAS : le compte de renflements reste juste, mais
				// l'ordre s'inverse. Une mutation CIBLEE -- elle doit rougir UN
				// critere et laisser l'autre vert, sinon elle ne prouve pas sa cible.
				for (uint32 i = 0; i < sc.nPieces; ++i)
					if (sc.poseSur[i] >= 0)
						sc.pieces[i].cy = sc.pieces[sc.poseSur[i]].cy - (sc.pieces[i].cy - sc.pieces[sc.poseSur[i]].cy);
				printf("  MUTATION NK_SCENE_MUTE=2 : « pose_sur » empile VERS LE BAS\n");
			}
			if (mu[0] == '3') {
				// Ignorer l'echelle par axe : tout redevient isotrope. C'est
				// exactement l'etat d'AVANT ce lot -- le pied plat redevient un
				// cylindre, la tige fine un gros tube.
				for (uint32 i = 0; i < sc.nPieces; ++i)
					sc.pieces[i].sx = sc.pieces[i].sy = sc.pieces[i].sz = 1.f;
				printf("  MUTATION NK_SCENE_MUTE=3 : echelle par axe ignoree (etat d'avant le lot A)\n");
			}
		}
		Sortie so;
		const int r = Emettre(sc, out, res, so);
		if (r != 0)
			return r;
		// ── LES NOMS, ET C'EST TOUT L'INTERET DU DOCUMENT ───────────────────
		const uint32 groupes = ReecrireAvecNoms(out, sc, so.ox, so.oy, so.oz);
		printf("  NOMS     : %u objet(s) nomme(s) ecrit(s) dans le .obj\n", groupes);

		int rougeDoc = 0;
		if (verifier)
			rougeDoc = VerifierContreDocument(sc, out, so.cell, so.ox, so.oy, so.oz);
		Relecture rl;
		if (!Relire(out, rl)) {
			fprintf(stderr, "REFUS : le .obj a ete ecrit mais LoadOBJ ne le relit pas : %s\n", out);
			return 1;
		}
		printf("  MESURE relecture : V(soude)=%u E=%u F=%u  bords=%u nonManifold=%u  chi=%d volume_signe=%+.5f\n",
			   rl.st.verts, rl.st.edges, rl.st.faces, rl.st.boundaryEdges, rl.st.nonManifoldEdges, rl.st.euler,
			   rl.volume);
		if (rl.st.nonManifoldEdges > 0 || (rl.st.euler % 2) != 0) {
			fprintf(stderr, "REFUS : le maillage produit n'est pas une surface saine "
							"(nonManifold=%u, chi=%d) -- le garde-fou refuse de l'ecrire comme valide\n",
					rl.st.nonManifoldEdges, rl.st.euler);
			return 1;
		}
		return rougeDoc == 0 ? 0 : 1;
	}

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

	// ââ LE GARDE-FOU DE VARIETE, ET IL JUGE LE PRODUIT, PAS LE REGLAGE âââ
	// Mesure du 18/09 : `gen::SurfaceNets` pose UN sommet par cellule, et sur une
	// cellule AMBIGUE (deux coins diagonalement opposes) ce sommet unique sert a
	// DEUX nappes. Selon que l'ambiguite porte sur un COIN ou sur une FACE, cela
	// donne un SOMMET non-manifold (chi devient IMPAIR) ou une ARETE
	// non-manifold. La mesure de la cause est dans `--banc-variete`.
	//
	// POURQUOI ON NE REFUSE PAS « LES RESOLUTIONS NON EPROUVEES ». Une liste
	// blanche {32, 64} serait fausse dans les DEUX sens : elle interdirait des
	// resolutions saines, et elle autoriserait 32 ou 64 sur une FORME qui, elle,
	// produit une cellule ambigue. La resolution n'est pas la grandeur qui
	// decide -- la sante du maillage produit l'est, et on vient de la mesurer.
	//
	// chi IMPAIR est le critere qui attrape le cas du COIN : une surface fermee
	// orientable a chi = 2 - 2g, donc toujours PAIR. Un compteur d'aretes seul
	// ne peut pas le voir : il est invariant par ce defaut-la.
	{
		const bool pair = (rl.st.euler % 2) == 0;
		if (rl.st.nonManifoldEdges > 0 || !pair) {
			remove(out); // ne JAMAIS laisser un objet casse sur le disque
			fprintf(stderr,
					"REFUS : le maillage produit n'est pas une surface saine, et il a ete EFFACE.\n"
					"        aretes non-manifold = %u (attendu 0)%s\n"
					"        caracteristique d'Euler = %d%s\n"
					"        CAUSE : gen::SurfaceNets pose un seul sommet par cellule ; sur une cellule\n"
					"        ambigue (deux coins diagonalement opposes) ce sommet sert a deux nappes.\n"
					"        CE QUI MARCHE AUJOURD'HUI : --res 32 et --res 64 sur les formes du banc.\n"
					"        Vois `NKTexte3D --banc-variete` pour la mesure de la cause.\n",
					rl.st.nonManifoldEdges, rl.st.nonManifoldEdges > 0 ? " <- ambiguite de FACE" : "",
					rl.st.euler, pair ? "" : " <- IMPAIR : impossible pour une surface fermee (ambiguite de COIN)");
			return 3;
		}
	}
	printf("  VARIETE : saine (0 arete non-manifold, chi pair)\n");
	printf("  EDITABLE : oui -- entre dans NkEditMesh par la MEME porte que les modeles importes\n");
	return 0;
}
