// =============================================================================
// NkTerrainCheck — UNE IMAGE DE HAUTEURS DEVIENT-ELLE UN TERRAIN DONT CHAQUE
//                  SOMMET EST A LA HAUTEUR QUE L'IMAGE DIT ?
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Sederis - Rihen
// =============================================================================
// Quatre criteres, quatre negatifs, quatre mutations.
//
//   (t0) LA CONDITION D'ESSAI ELLE-MEME. Les images sont FABRIQUEES, ecrites sur
//        disque, RELUES depuis le disque, et confrontees a la loi analytique qui
//        les a produites -- AVANT de servir a quoi que ce soit. Un banc a mesure
//        une pente de 10 degres sur un sol parfaitement plat, hier, avec des
//        chiffres parfaitement coherents entre eux. Si le codec PNG ne rend pas
//        au bit ce qu'on lui a donne, TOUT le reste du banc mesure autre chose
//        que ce qu'il annonce.
//
//   (t1) l'image se lit, le maillage se construit, les nombres sont ceux des
//        formules -- ecrites AVANT, dans NkTerrainHeightMap.h.
//        NEGATIF : image 1x1 -> REFUS NOMME (NK_IMAGE_TROP_PETITE).
//
//   (t2) LA HAUTEUR EST CELLE DE L'IMAGE. Le critere central. Sommet par sommet,
//        ecart maximal ecrit.
//        NEGATIF : image noire -> terrain PLAT, ecart 0 AU BIT.
//
//   (t3) les normales suivent la pente. Sur le plan incline exact, la normale
//        attendue est calculee a la main dans le canal, AVANT la mesure.
//        NEGATIF : terrain plat -> toutes les normales exactement (0,1,0).
//
//   (t4) ca se voit. Rendu hors-ecran, comptage des pixels non-fond.
//        ⚠️ LE COMPTEUR PROUVE SON ZERO AVANT TOUT LE RESTE : meme cible, meme
//        effacement, AUCUN trace -> 0 EXACT. Une sonde a rendu 40 pixels sans
//        aucune cible, hier.
//        FOND MAGENTA, jamais noir. CIBLE UNORM, jamais sRGB (le defaut du
//        struct NkOffscreenDesc est sRGB : il est contre nous, on l'ecrit).
//
// SANS FENETRE. Device headless : `NkDeviceInitInfo` sans surface, width=0,
// height=0. Motif lu dans NkMsaaDeviceCheck::Ouvrir et NkMatGraphDemo::Monter.
// Ce banc n'ouvre AUCUNE fenetre -- il n'y a donc aucune fenetre a journaliser,
// et rien a fermer.
//
// APPLICATION et non `tests/` : la politique du workspace desactive l'execution
// des tests unitaires depuis le 12/03. Un banc qui doit prouver quelque chose
// est une application console.
//
// LE JOURNAL EST POSITIONNEL (`NkFormat("{0}")`), jamais variadique. Un printf
// reclamant sept %u pour six arguments affiche un septieme nombre PLAUSIBLE, lu
// sur la pile -- et un nombre plausible se recopie dans un rapport, puis dans
// une decision. La forme positionnelle rend le trou VISIBLE.
// =============================================================================
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkIDevice.h"

#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Mesh/NkTerrainHeightMap.h"
#include "NKRenderer/Shader/NkShaderLibrary.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"

#include "NKImage/Core/NkImage.h"

#include "NKContainers/String/NkFormat.h"
#include "NKLogger/NkLog.h"

#include <math.h>
#include <stdio.h> // fwrite / fflush : voir `Dire` ci-dessous
#include <string.h>

// `NkShaderStage` existe DEUX FOIS : celui du RHI (`NkTypes.h:412`,
// `using NkShaderStage = NkSLStage`) et celui de `renderer/NkShaderBackend.h:61`.
// Sans cet alias, toute mention du nom est ambigue des qu'on ouvre les deux
// espaces de noms. Meme levee que dans NkMaterialSystem.cpp et NkMatGraphDemo.
using RHIStage = ::nkentseu::NkShaderStage;

using namespace nkentseu;
using namespace nkentseu::renderer;

// ─────────────────────────────────────────────────────────────────────────────
//  Comptage des cas. Un binaire qui n'a execute AUCUN cas annonce la meme chose
//  qu'un binaire dont tout passe : le nombre de cas est donc imprime, toujours.
// ─────────────────────────────────────────────────────────────────────────────
static uint32 gCas = 0;
static uint32 gEchecs = 0;
static uint32 gIgnores = 0;

// ─────────────────────────────────────────────────────────────────────────────
//  `Dire` — LE VERDICT DOIT SORTIR LA OU ON REGARDE
//
//  ⚠️ MESURE, PAS SUPPOSITION (14/09). Ce banc ecrivait **0 octet sur stdout ET
//  0 octet sur stderr** : `cmd /c ".\NkTerrainCheck.exe > out.txt 2> err.txt"`
//  laisse les deux fichiers VIDES. Le journal de NKLogger annonce pourtant
//  « sinks console et fichier ajoutes par defaut » (NkLog.h:361) -- quel que
//  soit le chemin qu'emprunte son puits console, il ne traverse pas les poignees
//  redirigees. Resultat : Rodolf lance l'executable, ne voit RIEN, et le verdict
//  n'existe que dans `logs/app.log`, c'est-a-dire ailleurs que la ou il regarde.
//  C'est la meme famille que « un outil qui filtre sans le dire » : rien n'est
//  faux, tout est invisible.
//
//  ⚠️ ET CE N'EST PAS UN RETOUR AU printf. La regle de Rodolf -- « ne pas
//  utiliser directement printf, le systeme definit des loggers » -- vise la
//  mecanique VARIADIQUE NON TYPEE : sept `%u` pour six arguments donnent un
//  septieme nombre plausible lu sur la pile. Ici le formatage reste
//  POSITIONNEL (`NkFormat`, arguments captures PAR LEUR TYPE) ; `fwrite` ne
//  recoit qu'une chaine DEJA FORMEE, sans aucune chaine de format. Le defaut
//  que la regle interdit est irrepresentable par ce chemin.
//
//  Le journal reste ecrit : `logs/app.log` garde la trace, la console porte le
//  verdict. Les deux disent la meme chose, formee une seule fois.
// ─────────────────────────────────────────────────────────────────────────────
template <typename... Args>
static void Dire(const char *format, Args... args) {
	const NkString ligne = NkFormat(format, args...);
	const char *c = ligne.CStr();
	logger.Info("{0}", ligne);
	if (c != nullptr)
		fwrite(c, 1, strlen(c), stdout);
	fputc('\n', stdout);
	// Vide a chaque ligne : un banc qui meurt en cours de route doit laisser
	// derriere lui tout ce qu'il a deja dit, pas un tampon perdu.
	fflush(stdout);
}

static void Cas(const char *nom, bool ok, const NkString &detail) {
	++gCas;
	if (!ok)
		++gEchecs;
	Dire("  [{0}] {1:<34} | {2}", NkString(ok ? "OK   " : "ROUGE"), NkString(nom), detail);
}

static void Ignore(const char *nom, const NkString &raison) {
	++gIgnores;
	Dire("  [IGN ] {0:<34} | {1}", NkString(nom), raison);
}

// ─────────────────────────────────────────────────────────────────────────────
//  LES MUTATIONS. Elles s'appliquent AU MAILLAGE PRODUIT, avant les criteres :
//  ce qu'on teste ici, c'est le CONTROLE, pas la production. « Une garde verte
//  peut ne rien garder du tout. »
// ─────────────────────────────────────────────────────────────────────────────
enum class Mutation : uint8 {
	AUCUNE = 0,
	HAUTEUR_PLUS_UN, ///< +1 partout   -> t2 ROUGE, t3 VERT (translation uniforme)
	APLATI,			 ///< y = min      -> t2 ROUGE (sauf D), t3 ROUGE
	TRANSPOSE,		 ///< (i,j)<-(j,i) -> grille CARREE seulement (donc A, pas B ni C)
	LIGNES_INVERSEES, ///< j <- M-1-j  -> le retournement de lignes, LE risque de convention
	VIDE,			 ///< rien soumis  -> t4 doit rendre 0
};

static Mutation gMutation = Mutation::AUCUNE;

static const char *NomMutation(Mutation m) {
	switch (m) {
		case Mutation::AUCUNE:
			return "aucune";
		case Mutation::HAUTEUR_PLUS_UN:
			return "hauteur+1";
		case Mutation::APLATI:
			return "aplati";
		case Mutation::TRANSPOSE:
			return "transpose";
		case Mutation::LIGNES_INVERSEES:
			return "lignes-inversees";
		case Mutation::VIDE:
			return "vide";
	}
	return "?";
}

// Applique la mutation courante au maillage. N et M sont necessaires a
// `transpose` : la mutation doit connaitre la grille, pas la deviner.
static void AppliquerMutation(NkEditMesh &m, uint32 N, uint32 M, float32 hauteurMin) {
	if (gMutation == Mutation::AUCUNE || gMutation == Mutation::VIDE)
		return;
	const uint32 nv = (uint32)m.verts.Size();
	if (gMutation == Mutation::HAUTEUR_PLUS_UN) {
		for (uint32 k = 0; k < nv; ++k)
			m.verts[k].pos.y += 1.f;
		return;
	}
	if (gMutation == Mutation::APLATI) {
		for (uint32 k = 0; k < nv; ++k)
			m.verts[k].pos.y = hauteurMin;
		m.RecomputeNormals();
		return;
	}
	if (gMutation == Mutation::LIGNES_INVERSEES) {
		// ⚠️ LA MUTATION QUI MANQUAIT, ET C'EST LA MESURE QUI L'A DIT.
		// J'avais annonce dans le canal que `transpose` rougirait « sur C et sur
		// C SEULE ». Mesure : elle rougit sur A et PAS sur C -- parce que C est
		// 6x10, donc non carree, donc `transpose` la SAUTE. Mon temoin d'ordre
		// n'etait exerce par aucune mutation : une garde verte qui ne gardait
		// rien, exactement ce que le canal demande de debusquer.
		// Le retournement de lignes, lui, est defini sur TOUTE grille, et c'est
		// le risque de convention reel (« la ligne 0 de l'image va en z le plus
		// negatif »). Il rougit sur B et C, et il est VERT sur A -- qui ne
		// depend pas de j et ne peut donc pas voir un retournement de lignes.
		// C'est cette paire qui prouve que C sert a quelque chose.
		NkVector<float32> tmp;
		tmp.Resize(nv);
		for (uint32 k = 0; k < nv; ++k)
			tmp[k] = m.verts[k].pos.y;
		for (uint32 j = 0; j < M; ++j)
			for (uint32 i = 0; i < N; ++i)
				m.verts[j * N + i].pos.y = tmp[(M - 1u - j) * N + i];
		m.RecomputeNormals();
		return;
	}
	if (gMutation == Mutation::TRANSPOSE) {
		// Seulement definissable si la grille est carree ; sinon on ne mute pas,
		// et on le DIT plutot que de muter a moitie.
		if (N != M)
			return;
		NkVector<float32> tmp;
		tmp.Resize(nv);
		for (uint32 k = 0; k < nv; ++k)
			tmp[k] = m.verts[k].pos.y;
		for (uint32 j = 0; j < M; ++j)
			for (uint32 i = 0; i < N; ++i)
				m.verts[j * N + i].pos.y = tmp[i * N + j];
		m.RecomputeNormals();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  (t0) LES IMAGES D'ESSAI : fabriquees, ECRITES, RELUES, puis CONFRONTEES.
// ─────────────────────────────────────────────────────────────────────────────
typedef uint8 (*LoiNiveau)(uint32 i, uint32 j, uint32 N, uint32 M);

static uint8 LoiPenteX(uint32 i, uint32 j, uint32 N, uint32 M) {
	(void)j;
	(void)N;
	(void)M;
	return (uint8)(i * 16u); // 0,16,...,128 pour N = 9
}
static uint8 LoiMarche(uint32 i, uint32 j, uint32 N, uint32 M) {
	(void)i;
	(void)N;
	(void)M;
	return (uint8)(j < 4u ? 0u : 255u);
}
static uint8 LoiTousDifferents(uint32 i, uint32 j, uint32 N, uint32 M) {
	(void)M;
	return (uint8)(j * N + i); // 0..59 pour 6x10 : tous distincts
}
static uint8 LoiNoire(uint32 i, uint32 j, uint32 N, uint32 M) {
	(void)i;
	(void)j;
	(void)N;
	(void)M;
	return 0u;
}

// Fabrique, ecrit, relit, confronte. Rend `false` si la relecture ne redonne pas
// EXACTEMENT ce qui a ete ecrit : dans ce cas, aucun critere en aval n'a de sens.
static bool PoserImage(const char *chemin, uint32 N, uint32 M, LoiNiveau loi, NkImage &out) {
	NkImage img = NkImage::Create(N, M, NkImagePixelFormat::NK_GRAY8, 0u);
	if (!img.IsValid()) {
		Cas(chemin, false, NkString("NkImage::Create a echoue"));
		return false;
	}
	for (uint32 j = 0; j < M; ++j)
		for (uint32 i = 0; i < N; ++i) {
			const uint8 v = loi(i, j, N, M);
			img.SetPixel((int32)i, (int32)j, math::NkColor(v, v, v, 255));
		}
	if (!img.SavePNG(chemin)) {
		Cas(chemin, false, NkString("SavePNG a echoue"));
		return false;
	}
	// ⚠️ ON RELIT DEPUIS LE DISQUE. Reutiliser l'objet en memoire mesurerait le
	// tableau qu'on vient de remplir, jamais le chemin que le terrain empruntera.
	if (!out.Load(chemin, 0)) {
		Cas(chemin, false, NkString("relecture impossible"));
		return false;
	}
	if (out.Width() != (int32)N || out.Height() != (int32)M) {
		Cas(chemin, false,
			NkFormat("relu {0}x{1}, ecrit {2}x{3}", out.Width(), out.Height(), (int32)N, (int32)M));
		return false;
	}
	// Confrontation AU BIT avec la loi analytique.
	uint32 divergents = 0;
	int32 ecartMax = 0;
	const int32 bpp = out.BytesPP();
	for (uint32 j = 0; j < M; ++j) {
		const uint8 *ligne = out.RowPtr((int32)j);
		for (uint32 i = 0; i < N; ++i) {
			const int32 lu = (int32)ligne[(usize)i * (usize)bpp];
			const int32 attendu = (int32)loi(i, j, N, M);
			const int32 e = lu > attendu ? lu - attendu : attendu - lu;
			if (e != 0)
				++divergents;
			if (e > ecartMax)
				ecartMax = e;
		}
	}
	const bool ok = (divergents == 0u);
	Cas(chemin, ok,
		NkFormat("{0}x{1}, canaux={2}, pixels divergents={3}/{4}, ecart max={5} (attendu 0)", (int32)N, (int32)M,
				 out.Channels(), divergents, N * M, ecartMax));
	return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
//  (t1)(t2)(t3) : un cas complet sur une image.
// ─────────────────────────────────────────────────────────────────────────────
struct ResultatTerrain {
		bool construit = false;
		NkEditMesh mesh;
		uint32 N = 0, M = 0;
		NkVector<float32> niveaux;
};

static bool ConstruireEtVerifier(const char *nom, const NkImage &img, const NkTerrainParams &p,
								 ResultatTerrain &r) {
	uint32 N = 0, M = 0;
	NkTerrainStatut st = NkTerrainLireNiveaux(img, p, r.niveaux, N, M);
	if (st != NkTerrainStatut::NK_OK) {
		Cas(nom, false, NkFormat("lecture des niveaux : {0}", NkString(NkTerrainStatutNom(st))));
		return false;
	}
	r.N = N;
	r.M = M;

	st = NkTerrainDepuisHeightMap(img, p, r.mesh);
	if (st != NkTerrainStatut::NK_OK) {
		Cas(nom, false, NkFormat("construction : {0}", NkString(NkTerrainStatutNom(st))));
		return false;
	}
	r.construit = true;

	// ── (t1) LES NOMBRES ────────────────────────────────────────────────
	const uint32 attSommets = NkTerrainNbSommets(N, M);
	const uint32 attQuads = NkTerrainNbQuads(N, M);
	const uint32 attTriangles = NkTerrainNbTriangles(N, M);
	const uint32 attAretes = NkTerrainNbAretes(N, M);

	const uint32 nbSommets = r.mesh.VertCount();
	const uint32 nbFaces = r.mesh.FaceCount();
	r.mesh.RebuildEdges();
	const uint32 nbAretes = r.mesh.EdgeCount();

	NkVector<NkVertex3D> tv;
	NkVector<uint32> ti;
	NkVector<NkEmId> tf;
	r.mesh.Triangulate(tv, ti, tf);
	const uint32 nbTriangles = (uint32)(ti.Size() / 3u);

	Cas(NkFormat("{0} / t1 nombres", NkString(nom)).CStr(),
		nbSommets == attSommets && nbFaces == attQuads && nbTriangles == attTriangles && nbAretes == attAretes,
		NkFormat("sommets {0}/{1} . quads {2}/{3} . triangles {4}/{5} . aretes {6}/{7}", nbSommets, attSommets,
				 nbFaces, attQuads, nbTriangles, attTriangles, nbAretes, attAretes));

	// Le CONTRAT 1:1 de Triangulate, verifie plutot que cru : c'est lui qui
	// rend le critere central lisible sommet par sommet.
	Cas(NkFormat("{0} / t1 contrat 1:1", NkString(nom)).CStr(), (uint32)tv.Size() == nbSommets,
		NkFormat("Triangulate rend {0} sommets, le maillage en a {1}", (uint32)tv.Size(), nbSommets));

	return true;
}

// LE critere. Renvoie l'ecart maximal ; `bitExact` dit si tous les ecarts sont
// nuls AU BIT.
static void VerifierHauteurs(const char *nom, const ResultatTerrain &r, const NkTerrainParams &p,
							 bool exigerBitExact) {
	double ecartMax = 0.0;
	uint32 divergentsBit = 0;
	uint32 pireI = 0, pireJ = 0;
	for (uint32 j = 0; j < r.M; ++j) {
		for (uint32 i = 0; i < r.N; ++i) {
			const uint32 k = j * r.N + i;
			const float32 attendu = NkTerrainHauteurDepuisNiveau(p, r.niveaux[k]);
			const float32 lu = r.mesh.verts[k].pos.y;
			if (lu != attendu)
				++divergentsBit;
			const double e = (double)lu - (double)attendu;
			const double a = e < 0.0 ? -e : e;
			if (a > ecartMax) {
				ecartMax = a;
				pireI = i;
				pireJ = j;
			}
		}
	}
	const bool ok = exigerBitExact ? (divergentsBit == 0u) : (ecartMax <= 1e-6);
	Cas(NkFormat("{0} / t2 HAUTEUR", NkString(nom)).CStr(), ok,
		NkFormat("ecart max={0} au pixel ({1},{2}) . sommets differant AU BIT={3}/{4} . exigence={5}",
				 (float32)ecartMax, pireI, pireJ, divergentsBit, r.N * r.M,
				 NkString(exigerBitExact ? "0 au bit" : "<= 1e-6")));
}

// Geometrie : les positions XZ doivent etre celles des formules, et l'ordre des
// sommets doit etre celui de l'image. Un retournement de lignes se voit ici.
static void VerifierGeometrie(const char *nom, const ResultatTerrain &r, const NkTerrainParams &p) {
	const float32 x0 = p.centre ? -0.5f * (float32)(r.N - 1u) * p.pasX : 0.f;
	const float32 z0 = p.centre ? -0.5f * (float32)(r.M - 1u) * p.pasZ : 0.f;
	double ecartMax = 0.0;
	for (uint32 j = 0; j < r.M; ++j)
		for (uint32 i = 0; i < r.N; ++i) {
			const uint32 k = j * r.N + i;
			const double ex = (double)(x0 + (float32)i * p.pasX);
			const double ez = (double)(z0 + (float32)j * p.pasZ);
			const double dx = (double)r.mesh.verts[k].pos.x - ex;
			const double dz = (double)r.mesh.verts[k].pos.z - ez;
			const double a = (dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz);
			if (a > ecartMax)
				ecartMax = a;
		}
	Cas(NkFormat("{0} / t1 geometrie XZ", NkString(nom)).CStr(), ecartMax == 0.0,
		NkFormat("ecart max sur x et z = {0} (attendu 0 : aucune de ces valeurs n'est calculee, "
				 "elles sont posees)",
				 (float32)ecartMax));
}

// (t3) Les normales.
static void VerifierNormales(const char *nom, const ResultatTerrain &r, float32 nx, float32 ny, float32 nz,
							 bool exigerBitExact) {
	// ⚠️ DEUX QUESTIONS, DEUX CRITERES. Le premier jet mesurait la DIRECTION et
	// la LONGUEUR d'un seul geste : `acos(n . attendue)` avec `n` non normalisee
	// vaut `acos(|n| cos t)`, donc une normale de longueur 0.9999998 produit un
	// ecart angulaire de 0.036 deg meme quand la direction est PARFAITE. Un
	// instrument qui melange deux grandeurs accuse la mauvaise.
	// ⚠️ L'ANGLE SE MESURE PAR `atan2(|a x b|, a.b)`, JAMAIS PAR `acos(a.b)`.
	// C'est un DEFAUT DE CE BANC, mesure et corrige le 14/09 : `acos` est mal
	// conditionne au voisinage de 1 — une erreur eps sur le cosinus devient
	// sqrt(2 eps) sur l'angle. Les 81 normales du plan incline etaient
	// BIT-IDENTIQUES a l'attendue (`differant AU BIT = 0/81`) et le banc
	// annoncait pourtant « 0.00886653 deg d'ecart » : la longueur valait
	// 1 - 1.2e-8, et sqrt(2 * 1.2e-8) = 1.55e-4 rad = 0.00886 deg. Le chiffre
	// etait exact, coherent, reproductible -- et il accusait le terrain d'un
	// defaut qui appartenait a l'instrument. `atan2` de la norme du produit
	// vectoriel reste precis jusqu'a zero.
	double pireAngleDeg = -1.0;
	uint32 divergentsBit = 0;
	double lenMin = 1e30, lenMax = -1e30;
	uint32 pireK = 0;
	NkVec3f pireN = {0.f, 0.f, 0.f};
	// L'attendue est renormalisee EN DOUBLE : les litteraux float32 du site
	// d'appel ne sont pas exactement unitaires, et cette erreur-la n'a rien a
	// voir avec le terrain.
	const double el = sqrt((double)nx * (double)nx + (double)ny * (double)ny + (double)nz * (double)nz);
	const double ex = (double)nx / el, ey = (double)ny / el, ez = (double)nz / el;
	const uint32 nv = (uint32)r.mesh.verts.Size();
	for (uint32 k = 0; k < nv; ++k) {
		const NkVec3f n = r.mesh.verts[k].normal;
		if (n.x != nx || n.y != ny || n.z != nz)
			++divergentsBit;
		const double len =
			sqrt((double)n.x * (double)n.x + (double)n.y * (double)n.y + (double)n.z * (double)n.z);
		if (len < lenMin)
			lenMin = len;
		if (len > lenMax)
			lenMax = len;
		const double inv = (len > 1e-12) ? (1.0 / len) : 0.0;
		const double ax = (double)n.x * inv, ay = (double)n.y * inv, az = (double)n.z * inv;
		const double d = ax * ex + ay * ey + az * ez;
		const double cx = ay * ez - az * ey, cy = az * ex - ax * ez, cz = ax * ey - ay * ex;
		const double cl = sqrt(cx * cx + cy * cy + cz * cz);
		const double ang = atan2(cl, d) * 57.29577951308232;
		if (ang > pireAngleDeg) {
			pireAngleDeg = ang;
			pireK = k;
			pireN = n;
		}
	}
	const bool okDir = exigerBitExact ? (divergentsBit == 0u) : (pireAngleDeg < 0.01);
	// ⚠️ LE PIRE SOMMET EST NOMME, pas seulement son ecart. Un ecart sans son
	// site ne se diagnostique pas : on ne sait pas s'il est au BORD (donc une
	// question de voisinage) ou au MILIEU (donc une question de calcul).
	Cas(NkFormat("{0} / t3 NORMALES direction", NkString(nom)).CStr(), okDir,
		NkFormat("attendue=({0},{1},{2}) . ecart angulaire max (APRES normalisation)={3} deg au sommet "
				 "#{4} = pixel ({5},{6}), n=({7},{8},{9}) . differant AU BIT={10}/{11} . exigence={12}",
				 nx, ny, nz, (float32)pireAngleDeg, pireK, pireK % r.N, pireK / r.N, pireN.x, pireN.y,
				 pireN.z, divergentsBit, nv, NkString(exigerBitExact ? "0 au bit" : "< 0.01 deg")));
	// La longueur est un critere A PART : `NkEditMesh::Vert::normal` est cense
	// etre unitaire. S'il ne l'est pas, ce n'est pas une tolerance a elargir,
	// c'est un fait a nommer.
	const double ecartLen = (1.0 - lenMin > lenMax - 1.0) ? (1.0 - lenMin) : (lenMax - 1.0);
	Cas(NkFormat("{0} / t3 NORMALES unitaires", NkString(nom)).CStr(), ecartLen < 1e-5,
		NkFormat("|n| min={0} max={1} . ecart a 1 = {2} (exigence < 1e-5)", (float32)lenMin, (float32)lenMax,
				 (float32)ecartLen));
}

// ─────────────────────────────────────────────────────────────────────────────
//  (t4) CA SE VOIT — et le compteur prouve son zero d'abord.
// ─────────────────────────────────────────────────────────────────────────────
static const char *const kVertexNkSL = R"NKSL(
// Le sommet arrive DEJA EN ESPACE DE CLIP (w = 1) : la projection est faite au
// processeur. Aucune matrice ne traverse donc ce nuanceur, et aucun defaut de
// matrice ne peut se faire passer pour un defaut de terrain.
// ⚠️ LES NOMS D'ATTRIBUTS SONT IMPOSES : le generateur HLSL deduit la SEMANTIQUE
// du NOM (table kSemanticRules, NkSLCodeGenHLSLStructs.cpp:28-38). « apos » ->
// POSITION, « anormal » -> NORMAL. Tout autre nom retomberait sur
// TEXCOORD<location> et le layout declare cote C++ cesserait de correspondre,
// SANS AUCUN MESSAGE.
@location(0) in vec3 aPos;
@location(1) in vec3 aNormal;

@location(0) out vec3 vNormal;

@stage(vertex)
@entry
void main() {
    vNormal     = aNormal;
    gl_Position = vec4(aPos, 1.0);
}
)NKSL";

static const char *const kFragmentNkSL = R"NKSL(
@binding(set=0, binding=0) uniform NkTerrainUBO { vec4 lumiere; } U;

@location(0) in vec3 vNormal;
@location(0) out vec4 fragColor;

@stage(fragment)
@entry
void main() {
    vec3  L = normalize(U.lumiere.xyz);
    float d = max(dot(normalize(vNormal), L), 0.0);
    // ⚠️ LE CANAL ROUGE EST FORCE A ZERO. Le fond est magenta (255,0,255) : avec
    // r = 0, aucun pixel du terrain ne peut coincider avec le fond, quelle que
    // soit la lumiere. Le compteur devient exact au lieu d'etre probable.
    fragColor = vec4(0.0, 0.25 + 0.75 * d, 0.0, 1.0);
}
)NKSL";

struct SommetRendu {
		float32 pos[3];
		float32 nrm[3];
};

struct UboLumiere {
		float32 lumiere[4] = {0.4f, 0.8f, 0.3f, 0.f};
};

struct Scene {
		NkIDevice *device = nullptr;
		NkTextureLibrary texLib;
		NkShaderLibrary shaders;
		NkOffscreenTarget cible;
		NkBufferHandle ubo;
		NkDescSetHandle setLayout;
		NkDescSetHandle set;
		NkPipelineHandle pipe;
		uint32 largeur = 256, hauteur = 256;

		bool Monter();
		void Demonter();
		// `sommets == nullptr` => AUCUN trace. C'est la preuve du zero.
		// `outNuances`, quand il est fourni, recoit le nombre de VALEURS DE VERT
		// DISTINCTES parmi les pixels non-fond. C'est ce qui distingue « le
		// terrain est peint » de « les normales arrivent au nuanceur ».
		bool RendreEtCompter(const SommetRendu *sommets, uint32 nbSommets, const uint32 *indices,
							 uint32 nbIndices, uint32 &outNonFond, const char *capture,
							 uint32 *outNuances = nullptr);
};

bool Scene::Monter() {
	NkDeviceInitInfo di;
	di.api = NkGraphicsApi::NK_GFX_API_DX11;
	di.width = 0; // pas de surface -> headless
	di.height = 0;
	device = NkDeviceFactory::Create(di);
	if (!device || !device->IsValid())
		return false;
	// TEMOIN D'IDENTITE : on LIT l'API du peripherique obtenu au lieu de croire
	// celle qu'on a demandee (correctif de NkMsaaVulkanCheck du 27/08).
	if (device->GetApi() != NkGraphicsApi::NK_GFX_API_DX11)
		return false;
	if (texLib.Init(device, nullptr) != NkRResult::NK_OK)
		return false;
	if (!shaders.Init(device, device->GetApi(), /*useNkSL=*/true))
		return false;

	NkOffscreenDesc od;
	od.width = largeur;
	od.height = hauteur;
	od.hasDepth = true;
	// ⚠️ UNORM, PAS sRGB. Le DEFAUT du struct est NK_RGBA8_SRGB : il est contre
	// nous. OpenGL n'encode pas comme les trois autres dorsaux, et l'attendu
	// cesserait d'etre calculable.
	od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
	od.readable = true;
	od.readback = true;
	od.name = NkString("NkTerrainCheck");
	if (!cible.Init(device, &texLib, od))
		return false;

	UboLumiere u;
	ubo = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(UboLumiere)));
	if (!ubo.IsValid())
		return false;
	device->WriteBuffer(ubo, &u, sizeof(u));

	NkDescriptorSetLayoutDesc ld;
	ld.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, RHIStage::NK_ALL_GRAPHICS);
	setLayout = device->CreateDescriptorSetLayout(ld);
	set = device->AllocateDescriptorSet(setLayout);
	NkDescriptorWrite w{};
	w.set = set;
	w.binding = 0;
	w.type = NkDescriptorType::NK_UNIFORM_BUFFER;
	w.buffer = ubo;
	w.bufferRange = sizeof(UboLumiere);
	device->UpdateDescriptorSets(&w, 1);

	::nkentseu::NkShaderHandle prog =
		shaders.CompileVF(NkString(kVertexNkSL), NkString(kFragmentNkSL), NkString("nkterrain"));
	::nkentseu::NkShaderHandle rhi = shaders.GetRHIHandle(prog);
	if (!rhi.IsValid())
		return false;

	NkGraphicsPipelineDesc pd;
	pd.shader = rhi;
	pd.vertexLayout.AddBinding(0, (uint32)sizeof(SommetRendu))
		.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
		.AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT, 12, "NORMAL", 0);
	// Aucun cull : le sens d'enroulement n'est PAS ce que (t4) mesure, et un cull
	// mal oriente rendrait une image entierement magenta sans le moindre message
	// -- on lirait « le terrain ne se voit pas » pour un defaut de convention.
	// L'enroulement, lui, est cale sur CreatePlaneMesh dans le .cpp du terrain.
	pd.rasterizer.cullMode = NkCullMode::NK_NONE;
	pd.depthStencil = NkDepthStencilDesc::Default();
	pd.renderPass = cible.GetRP();
	pd.descriptorSetLayouts.PushBack(setLayout);
	pd.debugName = "NkTerrainCheck";
	pipe = device->CreateGraphicsPipeline(pd);
	return pipe.IsValid();
}

void Scene::Demonter() {
	cible.Shutdown();
	shaders.Shutdown();
	texLib.Shutdown();
	if (device)
		NkDeviceFactory::Destroy(device);
	device = nullptr;
}

bool Scene::RendreEtCompter(const SommetRendu *sommets, uint32 nbSommets, const uint32 *indices,
							uint32 nbIndices, uint32 &outNonFond, const char *capture, uint32 *outNuances) {
	outNonFond = 0;
	if (outNuances)
		*outNuances = 0;
	NkBufferHandle vbo, ibo;
	const bool trace = (sommets != nullptr && nbSommets > 0u && indices != nullptr && nbIndices > 0u);
	if (trace) {
		vbo = device->CreateBuffer(NkBufferDesc::Vertex((uint64)nbSommets * sizeof(SommetRendu), sommets));
		ibo = device->CreateBuffer(NkBufferDesc::Index((uint64)nbIndices * sizeof(uint32), indices));
		if (!vbo.IsValid() || !ibo.IsValid())
			return false;
	}

	NkICommandBuffer *cmd = device->CreateCommandBuffer();
	if (!cmd || !cmd->Begin())
		return false;
	// FOND MAGENTA OPAQUE, jamais noir : un fond noir se confond avec un objet
	// noir, et le compteur dirait « rien » pour « tout, mais sombre ».
	// BeginCapture pose lui-meme la barriere, l'effacement, la passe, le viewport
	// et le ciseau ; EndCapture remet la texture en SHADER_READ, etat que
	// ReadbackPixels SUPPOSE.
	cible.BeginCapture(cmd, /*clearColor=*/true, NkVec4f{1.f, 0.f, 1.f, 1.f}, /*clearDepth=*/true);
	if (trace) {
		cmd->BindGraphicsPipeline(pipe);
		cmd->BindDescriptorSet(set, 0);
		cmd->BindVertexBuffer(0, vbo);
		cmd->BindIndexBuffer(ibo, NkIndexFormat::NK_UINT32);
		cmd->DrawIndexed(nbIndices);
	}
	cible.EndCapture(cmd);
	cmd->End();
	device->Submit(&cmd, 1);
	device->WaitIdle();

	NkVector<uint8> px;
	px.Resize(largeur * hauteur * 4u);
	if (!cible.ReadbackPixels(px.Data()))
		return false;

	uint8 vus[256];
	memset(vus, 0, sizeof(vus));
	for (uint32 k = 0; k < largeur * hauteur; ++k) {
		const uint8 r = px[k * 4u + 0u], g = px[k * 4u + 1u], b = px[k * 4u + 2u];
		if (!(r == 255u && g == 0u && b == 255u)) {
			++outNonFond;
			vus[g] = 1u;
		}
	}
	if (outNuances) {
		uint32 n = 0;
		for (uint32 g = 0; g < 256u; ++g)
			n += vus[g];
		*outNuances = n;
	}

	// La capture PNG est POUR L'OEIL DE RODOLF, pas pour le verdict. Le banc ne
	// juge AUCUNE image : « une ressource qui n'arrive pas ne change pas l'image
	// tant que personne ne la lit » -- un temoin par capture est structurellement
	// aveugle a toute une classe de defauts. Le verdict, lui, est le comptage.
	if (capture != nullptr)
		cible.Capture(capture);

	if (trace) {
		device->DestroyBuffer(vbo);
		device->DestroyBuffer(ibo);
	}
	return true;
}

// ── La projection, faite au processeur ──────────────────────────────────────
// Orthographique : w reste a 1, donc la position ecrite dans le tampon EST la
// position de clip. Deux consequences voulues :
//   - aucune matrice ne traverse le nuanceur, donc aucun defaut de matrice ne
//     peut se faire passer pour un defaut de terrain ;
//   - en vue de DESSUS, l'empreinte du terrain est un RECTANGLE PLEIN dont la
//     couverture en pixels est CALCULABLE. C'est ce qui remplace un « > 0 » par
//     un attendu derive des parametres du banc.
struct Vue {
		float32 f[3]; // direction de visee
		float32 up[3];
		float32 k; // fraction du carre NDC occupee par la boite du terrain
};

static void Normaliser(float32 v[3]) {
	const float32 l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	if (l > 1e-8f) {
		v[0] /= l;
		v[1] /= l;
		v[2] /= l;
	}
}
static void Croix(const float32 a[3], const float32 b[3], float32 o[3]) {
	o[0] = a[1] * b[2] - a[2] * b[1];
	o[1] = a[2] * b[0] - a[0] * b[2];
	o[2] = a[0] * b[1] - a[1] * b[0];
}

static void Projeter(const NkVector<NkVertex3D> &tv, const Vue &vue, NkVector<SommetRendu> &out) {
	float32 f[3] = {vue.f[0], vue.f[1], vue.f[2]};
	Normaliser(f);
	float32 up[3] = {vue.up[0], vue.up[1], vue.up[2]};
	float32 r[3];
	Croix(f, up, r);
	Normaliser(r);
	float32 u[3];
	Croix(r, f, u);
	Normaliser(u);

	const uint32 n = (uint32)tv.Size();
	NkVector<float32> vx, vy, vd;
	vx.Resize(n);
	vy.Resize(n);
	vd.Resize(n);
	float32 xmin = 1e30f, xmax = -1e30f, ymin = 1e30f, ymax = -1e30f, dmin = 1e30f, dmax = -1e30f;
	for (uint32 i = 0; i < n; ++i) {
		const NkVec3f p = tv[i].pos;
		vx[i] = p.x * r[0] + p.y * r[1] + p.z * r[2];
		vy[i] = p.x * u[0] + p.y * u[1] + p.z * u[2];
		vd[i] = p.x * f[0] + p.y * f[1] + p.z * f[2];
		if (vx[i] < xmin)
			xmin = vx[i];
		if (vx[i] > xmax)
			xmax = vx[i];
		if (vy[i] < ymin)
			ymin = vy[i];
		if (vy[i] > ymax)
			ymax = vy[i];
		if (vd[i] < dmin)
			dmin = vd[i];
		if (vd[i] > dmax)
			dmax = vd[i];
	}
	const float32 hx = (xmax - xmin) > 1e-8f ? 0.5f * (xmax - xmin) : 1.f;
	const float32 hy = (ymax - ymin) > 1e-8f ? 0.5f * (ymax - ymin) : 1.f;
	const float32 cx = 0.5f * (xmin + xmax), cy = 0.5f * (ymin + ymax);
	const float32 dr = (dmax - dmin) > 1e-8f ? (dmax - dmin) : 1.f;

	out.Resize(n);
	for (uint32 i = 0; i < n; ++i) {
		SommetRendu s;
		s.pos[0] = (vx[i] - cx) / hx * vue.k;
		s.pos[1] = (vy[i] - cy) / hy * vue.k;
		// Profondeur dans [0.05, 0.95] : a l'interieur de l'intervalle des DEUX
		// conventions ([0,1] DirectX et [-1,1] OpenGL), donc jamais ecretee par
		// l'une ni par l'autre. La comparaison est NK_LESS et l'effacement de
		// profondeur est 1.0 : le plus PROCHE (vd petit) gagne.
		s.pos[2] = 0.05f + 0.90f * ((vd[i] - dmin) / dr);
		s.nrm[0] = tv[i].normal.x;
		s.nrm[1] = tv[i].normal.y;
		s.nrm[2] = tv[i].normal.z;
		out[i] = s;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char **argv) {
	// `Pattern()` est GLOBAL et PERSISTANT : pose une seule fois, pas par ligne.
	logger.Pattern("%v");

	for (int a = 1; a < argc; ++a) {
		if (strncmp(argv[a], "--mutation=", 11) == 0) {
			const char *m = argv[a] + 11;
			if (strcmp(m, "hauteur+1") == 0)
				gMutation = Mutation::HAUTEUR_PLUS_UN;
			else if (strcmp(m, "aplati") == 0)
				gMutation = Mutation::APLATI;
			else if (strcmp(m, "transpose") == 0)
				gMutation = Mutation::TRANSPOSE;
			else if (strcmp(m, "lignes-inversees") == 0)
				gMutation = Mutation::LIGNES_INVERSEES;
			else if (strcmp(m, "vide") == 0)
				gMutation = Mutation::VIDE;
			else {
				Dire("mutation inconnue : {0}", NkString(m));
				return 2;
			}
		}
	}

	Dire("== NkTerrainCheck -- une image de hauteurs devient-elle un terrain ? ==");
	Dire("   mutation en cours : {0}", NkString(NomMutation(gMutation)));
	Dire("");

	// ── (t0) LA CONDITION D'ESSAI ───────────────────────────────────────
	Dire("-- (t0) les images d'essai : fabriquees, ECRITES, RELUES, confrontees --");
	NkImage imgA, imgB, imgC, imgD, imgE;
	bool condOk = true;
	condOk &= PoserImage("nkterrain_A_pente_9x9.png", 9, 9, LoiPenteX, imgA);
	condOk &= PoserImage("nkterrain_B_marche_6x10.png", 6, 10, LoiMarche, imgB);
	condOk &= PoserImage("nkterrain_C_tousdiff_6x10.png", 6, 10, LoiTousDifferents, imgC);
	condOk &= PoserImage("nkterrain_D_noire_5x5.png", 5, 5, LoiNoire, imgD);
	condOk &= PoserImage("nkterrain_E_unpixel_1x1.png", 1, 1, LoiNoire, imgE);
	if (!condOk) {
		Dire("");
		Dire("-- LA CONDITION D'ESSAI N'EST PAS SAINE. Tout critere en aval mesurerait");
		Dire("   autre chose que ce qu'il annonce. Le banc s'arrete ici, et c'est un ECHEC.");
		Dire("== {0} cas, {1} rouges ==", gCas, gEchecs);
		return 1;
	}
	Dire("");

	// ── Les parametres, ecrits une fois : tous les attendus en DERIVENT ──
	// hauteurParNiveau = 1/32 : puissance de deux, donc toute hauteur attendue
	// est exacte en float32 et le negatif « 0 au bit » reste atteignable.
	NkTerrainParams p;
	p.pasX = 1.f;
	p.pasZ = 1.f;
	p.hauteurMin = 0.f;
	p.hauteurParNiveau = 1.f / 32.f;

	// ── (t1) NEGATIF : 1x1 -> refus nomme ───────────────────────────────
	Dire("-- (t1) negatif : une image 1x1 --");
	{
		NkEditMesh vide;
		const NkTerrainStatut st = NkTerrainDepuisHeightMap(imgE, p, vide);
		Cas("1x1 -> refus nomme", st == NkTerrainStatut::NK_IMAGE_TROP_PETITE,
			NkFormat("statut={0} (attendu NK_IMAGE_TROP_PETITE) . maillage laisse a {1} sommets (attendu 0)",
					 NkString(NkTerrainStatutNom(st)), vide.VertCount()));
	}
	Dire("");

	// ── A : le plan incline exact ───────────────────────────────────────
	Dire("-- A : plan incline exact 9x9, niveau = i*16, pas 1/32 -> hauteur = i*0.5 --");
	ResultatTerrain rA;
	if (ConstruireEtVerifier("A", imgA, p, rA)) {
		AppliquerMutation(rA.mesh, rA.N, rA.M, p.hauteurMin);
		VerifierGeometrie("A", rA, p);
		VerifierHauteurs("A", rA, p, /*exigerBitExact=*/true);
		// La normale attendue, calculee A LA MAIN et ECRITE dans le canal AVANT
		// la mesure : pente s = (16 niveaux * 1/32) / pasX = 0.5.
		//   n = (-s, 1, 0) / sqrt(1 + s*s) = (-0.4472135955, 0.8944271910, 0)
		VerifierNormales("A", rA, -0.4472135955f, 0.8944271910f, 0.f, /*exigerBitExact=*/false);
	}
	Dire("");

	// ── B : la marche ───────────────────────────────────────────────────
	Dire("-- B : marche 6x10, niveau = 0 si j<4 sinon 255 --");
	ResultatTerrain rB;
	if (ConstruireEtVerifier("B", imgB, p, rB)) {
		AppliquerMutation(rB.mesh, rB.N, rB.M, p.hauteurMin);
		VerifierGeometrie("B", rB, p);
		VerifierHauteurs("B", rB, p, /*exigerBitExact=*/true);
		// Temoin de MARCHE : le nombre de sommets bas et hauts est calculable.
		uint32 bas = 0, haut = 0;
		const float32 hHaut = NkTerrainHauteurDepuisNiveau(p, 255.f);
		for (uint32 k = 0; k < rB.mesh.verts.Size(); ++k) {
			if (rB.mesh.verts[k].pos.y == p.hauteurMin)
				++bas;
			else if (rB.mesh.verts[k].pos.y == hHaut)
				++haut;
		}
		Cas("B / t2 marche au bon endroit", bas == 24u && haut == 36u,
			NkFormat("sommets bas={0} (attendu 24 = 6 col x 4 lignes) . hauts={1} (attendu 36) . "
					 "hauteur haute={2}",
					 bas, haut, hHaut));
	}
	Dire("");

	// ── C : tous les pixels differents — le temoin d'ORDRE ──────────────
	Dire("-- C : 6x10, niveau = j*6+i, 60 valeurs TOUTES DIFFERENTES (temoin d'ordre) --");
	ResultatTerrain rC;
	if (ConstruireEtVerifier("C", imgC, p, rC)) {
		AppliquerMutation(rC.mesh, rC.N, rC.M, p.hauteurMin);
		VerifierGeometrie("C", rC, p);
		VerifierHauteurs("C", rC, p, /*exigerBitExact=*/true);
	}
	Dire("");

	// ── D : NEGATIF, image noire -> terrain plat, 0 au bit ──────────────
	Dire("-- D : NEGATIF, image noire 5x5 -> terrain PARFAITEMENT plat --");
	ResultatTerrain rD;
	if (ConstruireEtVerifier("D", imgD, p, rD)) {
		AppliquerMutation(rD.mesh, rD.N, rD.M, p.hauteurMin);
		VerifierGeometrie("D", rD, p);
		VerifierHauteurs("D", rD, p, /*exigerBitExact=*/true);
		VerifierNormales("D", rD, 0.f, 1.f, 0.f, /*exigerBitExact=*/true);
	}
	Dire("");

	// ── (t4) CA SE VOIT ─────────────────────────────────────────────────
	Dire("-- (t4) ca se voit : rendu hors-ecran, comptage des pixels non-fond --");
	Scene sc;
	if (!sc.Monter()) {
		Ignore("(t4) montage hors-ecran",
			   NkString("aucun peripherique DX11 headless, ou nuanceur/pipeline refuse. "
						"AUCUN pixel n'a ete mesure : ce n'est NI un succes NI un echec."));
		sc.Demonter();
	} else {
		// ⚠️ LE ZERO, AVANT TOUT LE RESTE. Meme cible, meme effacement, AUCUN
		// trace. Une sonde a rendu 40 pixels sans aucune cible, hier.
		uint32 zero = 0xFFFFFFFFu;
		const bool luZero = sc.RendreEtCompter(nullptr, 0, nullptr, 0, zero, nullptr);
		Cas("(t4) ZERO du compteur", luZero && zero == 0u,
			NkFormat("scene SANS terrain : pixels non-fond = {0} (attendu 0, EXACT) . relecture={1}", zero,
					 NkString(luZero ? "ok" : "KO")));

		if (luZero && zero == 0u && rA.construit) {
			NkVector<NkVertex3D> tv;
			NkVector<uint32> ti;
			NkVector<NkEmId> tf;
			rA.mesh.Triangulate(tv, ti, tf);

			// Passe 1 : VUE DE DESSUS. L'empreinte est un rectangle plein, donc
			// la couverture est DERIVEE des parametres du banc, pas reglee :
			//   cote en pixels = k * largeur ; attendu = (k*W) * (k*H).
			Vue dessus;
			dessus.f[0] = 0.f;
			dessus.f[1] = -1.f;
			dessus.f[2] = 0.f;
			// `up` ne peut pas etre (0,1,0) ici : il serait colineaire a la visee
			// et le produit vectoriel s'effondrerait. (0,0,-1) place la ligne 0 de
			// l'image (z le plus negatif) EN HAUT de l'image rendue.
			dessus.up[0] = 0.f;
			dessus.up[1] = 0.f;
			dessus.up[2] = -1.f;
			dessus.k = 0.75f;

			NkVector<SommetRendu> sr;
			Projeter(tv, dessus, sr);
			const bool muteVide = (gMutation == Mutation::VIDE);
			uint32 vus = 0xFFFFFFFFu;
			const bool lu = muteVide ? sc.RendreEtCompter(nullptr, 0, nullptr, 0, vus, nullptr)
									 : sc.RendreEtCompter(sr.Data(), (uint32)sr.Size(), ti.Data(),
														  (uint32)ti.Size(), vus, "nkterrain_vue_dessus.png");
			const uint32 cote = (uint32)(dessus.k * (float32)sc.largeur + 0.5f);
			const uint32 attendu = cote * cote;
			const uint32 ecart = vus > attendu ? vus - attendu : attendu - vus;
			const uint32 tolerance = attendu / 100u; // 1 %
			if (muteVide) {
				Cas("(t4) mutation vide -> 0", lu && vus == 0u,
					NkFormat("pixels non-fond = {0} (attendu 0) . relecture={1}", vus,
							 NkString(lu ? "ok" : "KO")));
			} else {
				Cas("(t4) vue de dessus", lu && ecart <= tolerance,
					NkFormat("pixels non-fond = {0} . attendu {1} = ({2} px)^2 derive de k={3} et "
							 "largeur={4} . ecart={5} (tolerance {6} = 1%)",
							 vus, attendu, cote, dessus.k, sc.largeur, ecart, tolerance));
			}

			// Passe 2 : VUE OBLIQUE. Elle montre le RELIEF. Son nombre de pixels
			// depend du cadrage, donc il est IMPRIME et non juge : un attendu en
			// pixels serait un reglage deguise en propriete.
			if (!muteVide) {
				Vue oblique;
				oblique.f[0] = 0.5f;
				oblique.f[1] = -0.7f;
				oblique.f[2] = 0.5f;
				oblique.up[0] = 0.f;
				oblique.up[1] = 1.f;
				oblique.up[2] = 0.f;
				oblique.k = 0.85f;
				NkVector<SommetRendu> so;
				Projeter(tv, oblique, so);
				uint32 vus2 = 0, nuances2 = 0;
				const bool lu2 =
					sc.RendreEtCompter(so.Data(), (uint32)so.Size(), ti.Data(), (uint32)ti.Size(), vus2,
									   "nkterrain_A_oblique.png", &nuances2);
				// ⚠️ CE QUE CETTE IMAGE NE PROUVE PAS, ET IL FAUT LE DIRE.
				// A est un plan EXACT : toutes ses normales sont identiques, donc
				// sa capture est UNIFORMEMENT verte -- c'est correct, et c'est
				// justement pourquoi elle ne temoigne de rien sur les normales.
				// Une chaine de normales entierement cassee donnerait aussi un
				// aplat uniforme, d'une autre teinte. L'attendu ici est donc
				// EXACTEMENT UNE nuance, et le temoin des normales est la passe
				// suivante, sur B.
				Cas("(t4) A oblique : 1 nuance (plan exact)", lu2 && vus2 > 0u && nuances2 == 1u,
					NkFormat("pixels non-fond = {0} (chiffre imprime, pas juge : il depend du cadrage) . "
							 "nuances de vert = {1} (attendu 1 : un plan exact n'a qu'une normale)",
							 vus2, nuances2));

				// ── LE TEMOIN DES NORMALES : B, LA MARCHE ───────────────────
				// B a deux plateaux et une falaise, donc des normales qui
				// DIFFERENT. Si les normales n'arrivaient pas au nuanceur, ou si
				// le terrain etait plat, on retomberait sur UNE seule nuance. Le
				// critere est donc « au moins 2 », derive de la forme de B et non
				// d'un reglage.
				if (rB.construit) {
					NkVector<NkVertex3D> tvB;
					NkVector<uint32> tiB;
					NkVector<NkEmId> tfB;
					rB.mesh.Triangulate(tvB, tiB, tfB);
					NkVector<SommetRendu> soB;
					Projeter(tvB, oblique, soB);
					uint32 vusB = 0, nuancesB = 0;
					const bool luB =
						sc.RendreEtCompter(soB.Data(), (uint32)soB.Size(), tiB.Data(), (uint32)tiB.Size(),
										   vusB, "nkterrain_B_marche_oblique.png", &nuancesB);
					Cas("(t4) B oblique : le RELIEF se voit", luB && vusB > 0u && nuancesB >= 2u,
						NkFormat("pixels non-fond = {0} . nuances de vert = {1} (attendu >= 2 : la marche a "
								 "deux plateaux et une falaise, donc des normales differentes)",
								 vusB, nuancesB));
				} else {
					Ignore("(t4) B oblique", NkString("l'image B n'a pas produit de maillage."));
				}
			}
		} else if (!rA.construit) {
			Ignore("(t4) terrain", NkString("l'image A n'a pas produit de maillage : rien a rendre."));
		}
		sc.Demonter();
	}

	Dire("");
	Dire("== {0} cas executes, {1} ROUGES, {2} ignores . mutation={3} ==", gCas, gEchecs, gIgnores,
				NkString(NomMutation(gMutation)));
	if (gIgnores > 0u)
		// ASCII pur : le puits console ne transporte pas l'UTF-8 (mesure -- les
		// tirets cadratins sortaient « - » et les guillemets « ? »). Mieux vaut
		// ecrire ce qui sera lu que laisser des caracteres se perdre en chemin.
		Dire("   /!\\ un IGNORE n'est pas un vert : la question n'a pas ete posee.");
	return gEchecs == 0u ? 0 : 1;
}
