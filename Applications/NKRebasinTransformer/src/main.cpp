// =============================================================================
// NKRebasinTransformer — la MARCHE 3 : aligner deux TRANSFORMEURS.
// -----------------------------------------------------------------------------
// Ce qui precede (NKRebasinTest) traite des perceptrons : une permutation par
// couche cachee, resolue exactement a une couche, par descente au-dela. Sur un
// transformeur la question change de nature, et il faut d'abord dire OU se
// trouvent reellement les libertes de permutation. Elles ne sont pas la ou on
// les attend.
//
// INVENTAIRE DES SYMETRIES D'UN BLOC TRANSFORMEUR
//
//   (1) LES UNITES CACHEES DU MLP (largeur 4d). Exactement comme dans un
//       perceptron : permuter une unite = permuter la colonne correspondante de
//       la premiere matrice, l'entree du biais, et la LIGNE de la seconde.
//       Libre, et propre a chaque bloc.
//
//   (2) LES TETES D'ATTENTION. Une tete est INDIVISIBLE : ses dimensions
//       participent ensemble a un produit scalaire puis a un softmax, on ne peut
//       pas en deplacer une seule. En revanche on peut echanger des tetes
//       ENTIERES — c'est-a-dire des blocs de `hd` colonnes consecutives dans
//       Wq/Wk/Wv (et leurs biais), avec les blocs de LIGNES correspondants dans
//       Wo. Libre, et propre a chaque bloc.
//
//   (3) LE FLUX RESIDUEL (largeur d). C'est LUI le noeud du probleme. Ce n'est
//       pas une permutation par bloc mais UNE SEULE permutation globale, que
//       devraient subir en meme temps : la table d'embedding, l'embedding
//       positionnel, les gains et decalages de TOUTES les normalisations, les
//       entrees ET sorties de toutes les projections d'attention, les entrees et
//       sorties de tous les MLP, et la tete de sortie. Tout y est couple.
//
// CE QUE CETTE APPLICATION FAIT, ET CE QU'ELLE NE FAIT PAS
//
// Elle aligne (1) et (2), en LAISSANT LE FLUX RESIDUEL INTACT. Ce choix n'est
// pas un renoncement : c'est ce qui rend le probleme EXACTEMENT soluble. Le flux
// residuel etant fige, les permutations de MLP et de tetes se DECOUPLENT
// completement — entre elles et d'un bloc a l'autre — et chacune redevient une
// simple affectation lineaire que la methode hongroise resout a l'optimum. Aucune
// descente par coordonnees, aucun optimum local : sur ce sous-espace de
// symetries, ce qui est mesure ici est LA verite.
//
// La question a laquelle on repond est donc precise : ALIGNER TOUT CE QUI EST
// LIBRE SANS TOUCHER AU FLUX RESIDUEL, EST-CE QUE CELA SUFFIT ? La reponse a
// une valeur, quelle qu'elle soit : si oui, la piste de la combinaison de
// modeles s'ouvre largement ; si non, elle designe le flux residuel comme le
// verrou a faire sauter, et c'est deja un resultat.
//
// GARDE-FOU : permuter ne doit RIEN changer a ce que le modele calcule. La perte
// du modele B permute doit etre EGALE a celle de B, a la precision machine. Sans
// cette verification, une erreur d'indice donnerait une barriere « reduite » qui
// ne signifierait rien.
//
// CPU STRICT : aucun device GPU n'est cree (une seule carte, un entrainement
// long peut l'occuper).
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static int g_pass = 0;
static int g_fail = 0;

static void Verdict(bool ok, const char *quoi) {
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %s\n", ok ? "OK" : "KO", quoi);
}

// ---- Dimensions (petites : tout tourne en CPU) -----------------------------
static const int32 kVocab = 24;
static const int32 kD = 64;
static const int32 kHeads = 4;
static const int32 kLayers = 2;
static const int32 kT = 32;
static const int32 kB = 8;
static const int32 kFF = 4 * kD;
static const int32 kHd = kD / kHeads;

// ---------------------------------------------------------------------------
// METHODE HONGROISE (Kuhn-Munkres) en O(n^3) — affectation de cout MINIMAL.
// On lui passe l'oppose des similarites pour obtenir l'appariement le plus
// ressemblant. Un glouton se tromperait des que deux unites se disputent le
// meme partenaire, et un mauvais alignement ferait conclure a tort que la
// methode ne marche pas.
// ---------------------------------------------------------------------------
static void HungarianMinCost(const NkVector<float64> &cost, int32 n, NkVector<int32> &assign) {
	NkVector<float64> u, v;
	NkVector<int32> p, way;
	u.Resize((usize)n + 1);
	v.Resize((usize)n + 1);
	p.Resize((usize)n + 1);
	way.Resize((usize)n + 1);
	for (int32 i = 0; i <= n; ++i) {
		u[(usize)i] = 0.0;
		v[(usize)i] = 0.0;
		p[(usize)i] = 0;
		way[(usize)i] = 0;
	}
	for (int32 i = 1; i <= n; ++i) {
		p[0] = i;
		int32 j0 = 0;
		NkVector<float64> minv;
		NkVector<bool> used;
		minv.Resize((usize)n + 1);
		used.Resize((usize)n + 1);
		for (int32 j = 0; j <= n; ++j) {
			minv[(usize)j] = 1e300;
			used[(usize)j] = false;
		}
		do {
			used[(usize)j0] = true;
			const int32 i0 = p[(usize)j0];
			float64 delta = 1e300;
			int32 j1 = 0;
			for (int32 j = 1; j <= n; ++j) {
				if (used[(usize)j])
					continue;
				const float64 cur = cost[(usize)((i0 - 1) * n + (j - 1))] - u[(usize)i0] - v[(usize)j];
				if (cur < minv[(usize)j]) {
					minv[(usize)j] = cur;
					way[(usize)j] = j0;
				}
				if (minv[(usize)j] < delta) {
					delta = minv[(usize)j];
					j1 = j;
				}
			}
			for (int32 j = 0; j <= n; ++j) {
				if (used[(usize)j]) {
					u[(usize)p[(usize)j]] += delta;
					v[(usize)j] -= delta;
				} else
					minv[(usize)j] -= delta;
			}
			j0 = j1;
		} while (p[(usize)j0] != 0);
		do {
			const int32 j1 = way[(usize)j0];
			p[(usize)j0] = p[(usize)j1];
			j0 = j1;
		} while (j0);
	}
	assign.Resize((usize)n);
	for (int32 j = 1; j <= n; ++j)
		assign[(usize)(p[(usize)j] - 1)] = j - 1;
}

// ---------------------------------------------------------------------------
// Corpus jouet STRUCTURE : des « mots » tires d'un petit lexique, separes par
// un espace. Il y a donc quelque chose a apprendre (quelle lettre suit quelle
// lettre dans un mot, et qu'un mot finit), sans dependre d'un fichier externe.
// ---------------------------------------------------------------------------
static NkVector<int32> gTexte;

static void ConstruireTexte() {
	static const char *kMots[] = {"le", "chat", "dort", "sur", "le", "toit", "et", "la", "pluie",
								  "tombe", "sans", "bruit", "dans", "la", "nuit", "noire"};
	const int nMots = (int)(sizeof(kMots) / sizeof(kMots[0]));
	uint64 rng = 0x2545F4914F6CDD1Dull;
	auto suivant = [&]() {
		rng = rng * 6364136223846793005ull + 1442695040888963407ull;
		return (uint32)(rng >> 33);
	};
	gTexte.Clear();
	for (int i = 0; i < 20000; ++i) {
		const char *m = kMots[suivant() % (uint32)nMots];
		for (const char *p = m; *p; ++p)
			gTexte.PushBack((int32)(*p - 'a' + 1)); // 1..22
		gTexte.PushBack(0);						   // 0 = espace
	}
}

// Lot [B,T] + cible en indices [B*T], tire au hasard dans le texte.
static void FabriquerLot(NkTensor &x, NkTensor &cible, uint64 &rng) {
	x = NkTensor::Zeros(NkShape{(int64)kB, (int64)kT});
	cible = NkTensor::Zeros(NkShape{(int64)kB * kT});
	float *xp = x.DataAs<float>();
	float *cp = cible.DataAs<float>();
	const int64 N = (int64)gTexte.Size();
	for (int b = 0; b < kB; ++b) {
		rng = rng * 6364136223846793005ull + 1442695040888963407ull;
		const int64 off = (int64)((rng >> 33) % (uint32)(N - kT - 2));
		for (int t = 0; t < kT; ++t) {
			xp[b * kT + t] = (float)gTexte[(usize)(off + t)];
			cp[b * kT + t] = (float)gTexte[(usize)(off + t + 1)];
		}
	}
}

// ---------------------------------------------------------------------------
// Acces aux poids : NkGPT::Parameters() rend, DANS CET ORDRE :
//   0  tokEmb [V,d]
//   1  posEmb [T,d]
//   puis par bloc (16 tenseurs) :
//     ln1.gamma, ln1.beta, Wq.W, Wq.b, Wk.W, Wk.b, Wv.W, Wv.b, Wo.W, Wo.b,
//     ln2.gamma, ln2.beta, fc1.W, fc1.b, fc2.W, fc2.b
//   puis lnf.gamma, lnf.beta, head.W [d,V], head.b
// C'est le SEUL lien entre le modele et ce code : s'y fier explicitement, et le
// verifier par les formes.
// ---------------------------------------------------------------------------
static const int kParBloc = 16;

static int IdxBloc(int bloc, int quoi) {
	return 2 + bloc * kParBloc + quoi;
}
// Decalages dans un bloc.
enum { P_LN1_G = 0, P_LN1_B = 1, P_WQ_W = 2, P_WQ_B = 3, P_WK_W = 4, P_WK_B = 5, P_WV_W = 6,
	   P_WV_B = 7, P_WO_W = 8, P_WO_B = 9, P_LN2_G = 10, P_LN2_B = 11, P_FC1_W = 12,
	   P_FC1_B = 13, P_FC2_W = 14, P_FC2_B = 15 };
// Index des quatre derniers tenseurs (apres les blocs).
static int IdxFin(int quoi) {
	return 2 + kLayers * kParBloc + quoi;
}
enum { P_LNF_G = 0, P_LNF_B = 1, P_HEAD_W = 2 };

static bool VerifierFormes(const NkVector<NkVar> &p) {
	if (p.Size() != (usize)(2 + kLayers * kParBloc + 4))
		return false;
	for (int l = 0; l < kLayers; ++l) {
		if (p[(usize)IdxBloc(l, P_FC1_W)].Value().Shape()[1] != kFF)
			return false;
		if (p[(usize)IdxBloc(l, P_FC2_W)].Value().Shape()[0] != kFF)
			return false;
		if (p[(usize)IdxBloc(l, P_WQ_W)].Value().Shape()[1] != kD)
			return false;
	}
	return true;
}

static NkVector<NkTensor> Extraire(const NkVector<NkVar> &p) {
	NkVector<NkTensor> t;
	for (usize i = 0; i < p.Size(); ++i)
		t.PushBack(p[i].Value().ToCPU().Contiguous().Clone());
	return t;
}

// ---------------------------------------------------------------------------
// (1) Permutation des unites cachees du MLP d'un bloc.
//     fc1.W [d,ff] : colonnes ; fc1.b [1,ff] : entrees ; fc2.W [ff,d] : lignes.
// ---------------------------------------------------------------------------
static void CoutMlp(const NkVector<NkTensor> &A, const NkVector<NkTensor> &B, int bloc, NkVector<float64> &cout) {
	cout.Resize((usize)kFF * (usize)kFF);
	for (usize k = 0; k < cout.Size(); ++k)
		cout[k] = 0.0;
	const float *a1 = A[(usize)IdxBloc(bloc, P_FC1_W)].DataAs<float>();
	const float *b1 = B[(usize)IdxBloc(bloc, P_FC1_W)].DataAs<float>();
	const float *ab = A[(usize)IdxBloc(bloc, P_FC1_B)].DataAs<float>();
	const float *bb = B[(usize)IdxBloc(bloc, P_FC1_B)].DataAs<float>();
	const float *a2 = A[(usize)IdxBloc(bloc, P_FC2_W)].DataAs<float>();
	const float *b2 = B[(usize)IdxBloc(bloc, P_FC2_W)].DataAs<float>();
	for (int32 r = 0; r < kD; ++r)
		for (int32 i = 0; i < kFF; ++i) {
			const float64 av = (float64)a1[(int64)r * kFF + i];
			if (av == 0.0)
				continue;
			float64 *dst = &cout[(usize)((int64)i * kFF)];
			const float *rowB = b1 + (int64)r * kFF;
			for (int32 j = 0; j < kFF; ++j)
				dst[j] -= av * (float64)rowB[j];
		}
	for (int32 i = 0; i < kFF; ++i)
		for (int32 j = 0; j < kFF; ++j) {
			float64 s = (float64)ab[i] * (float64)bb[j];
			for (int32 c = 0; c < kD; ++c)
				s += (float64)a2[(int64)i * kD + c] * (float64)b2[(int64)j * kD + c];
			cout[(usize)((int64)i * kFF + j)] -= s;
		}
}

static void AppliquerMlp(NkVector<NkTensor> &B, int bloc, const NkVector<int32> &perm) {
	NkTensor w1 = B[(usize)IdxBloc(bloc, P_FC1_W)].Clone();
	NkTensor b1 = B[(usize)IdxBloc(bloc, P_FC1_B)].Clone();
	NkTensor w2 = B[(usize)IdxBloc(bloc, P_FC2_W)].Clone();
	const float *s1 = w1.DataAs<float>();
	const float *sb = b1.DataAs<float>();
	const float *s2 = w2.DataAs<float>();
	float *d1 = B[(usize)IdxBloc(bloc, P_FC1_W)].DataAs<float>();
	float *db = B[(usize)IdxBloc(bloc, P_FC1_B)].DataAs<float>();
	float *d2 = B[(usize)IdxBloc(bloc, P_FC2_W)].DataAs<float>();
	for (int32 i = 0; i < kFF; ++i) {
		const int32 j = perm[(usize)i];
		for (int32 r = 0; r < kD; ++r)
			d1[(int64)r * kFF + i] = s1[(int64)r * kFF + j];
		db[i] = sb[j];
		for (int32 c = 0; c < kD; ++c)
			d2[(int64)i * kD + c] = s2[(int64)j * kD + c];
	}
}

// ---------------------------------------------------------------------------
// (2) Permutation des TETES ENTIERES d'un bloc.
//     Une tete occupe les colonnes [t*hd, (t+1)*hd) de Wq/Wk/Wv et de leurs
//     biais, et les LIGNES correspondantes de Wo. Elle se deplace d'un bloc.
//     Le contenu INTERNE d'une tete n'est pas touche : c'est bien une tete qu'on
//     echange, pas des unites.
// ---------------------------------------------------------------------------
static void CoutTetes(const NkVector<NkTensor> &A, const NkVector<NkTensor> &B, int bloc, NkVector<float64> &cout) {
	cout.Resize((usize)kHeads * (usize)kHeads);
	for (usize k = 0; k < cout.Size(); ++k)
		cout[k] = 0.0;
	const int projs[3] = {P_WQ_W, P_WK_W, P_WV_W};
	const int biais[3] = {P_WQ_B, P_WK_B, P_WV_B};
	for (int32 ti = 0; ti < kHeads; ++ti)
		for (int32 tj = 0; tj < kHeads; ++tj) {
			float64 s = 0.0;
			for (int q = 0; q < 3; ++q) {
				const float *aw = A[(usize)IdxBloc(bloc, projs[q])].DataAs<float>();
				const float *bw = B[(usize)IdxBloc(bloc, projs[q])].DataAs<float>();
				const float *ab = A[(usize)IdxBloc(bloc, biais[q])].DataAs<float>();
				const float *bb = B[(usize)IdxBloc(bloc, biais[q])].DataAs<float>();
				for (int32 k = 0; k < kHd; ++k) {
					const int32 ca = ti * kHd + k, cb = tj * kHd + k;
					for (int32 r = 0; r < kD; ++r)
						s += (float64)aw[(int64)r * kD + ca] * (float64)bw[(int64)r * kD + cb];
					s += (float64)ab[ca] * (float64)bb[cb];
				}
			}
			const float *ao = A[(usize)IdxBloc(bloc, P_WO_W)].DataAs<float>();
			const float *bo = B[(usize)IdxBloc(bloc, P_WO_W)].DataAs<float>();
			for (int32 k = 0; k < kHd; ++k) {
				const int32 ra = ti * kHd + k, rb = tj * kHd + k;
				for (int32 c = 0; c < kD; ++c)
					s += (float64)ao[(int64)ra * kD + c] * (float64)bo[(int64)rb * kD + c];
			}
			cout[(usize)((int64)ti * kHeads + tj)] = -s;
		}
}

static void AppliquerTetes(NkVector<NkTensor> &B, int bloc, const NkVector<int32> &perm) {
	const int projs[3] = {P_WQ_W, P_WK_W, P_WV_W};
	const int biais[3] = {P_WQ_B, P_WK_B, P_WV_B};
	for (int q = 0; q < 3; ++q) {
		NkTensor w = B[(usize)IdxBloc(bloc, projs[q])].Clone();
		NkTensor b = B[(usize)IdxBloc(bloc, biais[q])].Clone();
		const float *sw = w.DataAs<float>();
		const float *sb = b.DataAs<float>();
		float *dw = B[(usize)IdxBloc(bloc, projs[q])].DataAs<float>();
		float *db = B[(usize)IdxBloc(bloc, biais[q])].DataAs<float>();
		for (int32 ti = 0; ti < kHeads; ++ti) {
			const int32 tj = perm[(usize)ti];
			for (int32 k = 0; k < kHd; ++k) {
				const int32 ca = ti * kHd + k, cb = tj * kHd + k;
				for (int32 r = 0; r < kD; ++r)
					dw[(int64)r * kD + ca] = sw[(int64)r * kD + cb];
				db[ca] = sb[cb];
			}
		}
	}
	NkTensor wo = B[(usize)IdxBloc(bloc, P_WO_W)].Clone();
	const float *so = wo.DataAs<float>();
	float *dof = B[(usize)IdxBloc(bloc, P_WO_W)].DataAs<float>();
	for (int32 ti = 0; ti < kHeads; ++ti) {
		const int32 tj = perm[(usize)ti];
		for (int32 k = 0; k < kHd; ++k)
			for (int32 c = 0; c < kD; ++c)
				dof[(int64)(ti * kHd + k) * kD + c] = so[(int64)(tj * kHd + k) * kD + c];
	}
}

// ---------------------------------------------------------------------------
// (3) LE FLUX RESIDUEL — UNE SEULE permutation, partout a la fois.
//
// C'est la difference de nature avec (1) et (2) : il n'y a pas une permutation
// par bloc, mais UNE permutation de largeur d que doivent subir SIMULTANEMENT
// tous les endroits ou le flux residuel apparait. En voici la liste exhaustive,
// avec l'axe concerne — s'en tenir a cette liste EST le travail : en oublier un
// seul, et le modele permute ne calcule plus la meme chose.
//
//   tokEmb [V,d]      colonnes        posEmb [T,d]      colonnes
//   ln*.gamma/.beta   entrees         Wq/Wk/Wv.W [d,d]  LIGNES (cote entree)
//   Wo.W [d,d]        COLONNES        Wo.b              entrees
//   fc1.W [d,ff]      LIGNES          fc2.W [ff,d]      COLONNES
//   fc2.b             entrees         head.W [d,V]      LIGNES
//
// Ne sont PAS concernes : les biais de Wq/Wk/Wv (cote tetes), fc1.b (cote MLP),
// head.b (cote vocabulaire).
//
// Pourquoi c'est bien une symetrie : LayerNorm calcule moyenne et variance SUR
// cet axe, or les deux sont insensibles a l'ordre — donc LN(Px) = P·LN(x). Et le
// raccourci residuel additionne deux branches permutees de la meme facon.
// ---------------------------------------------------------------------------

// Accumule dans `cout` la similarite -Σ A[..,i]·B[..,j] pour un axe donne.
// `axe` : 0 = lignes (i indexe la 1re dimension), 1 = colonnes.
static void AjouterCout(NkVector<float64> &cout, const NkTensor &A, const NkTensor &B, int axe, int32 n) {
	const float *a = A.DataAs<float>();
	const float *b = B.DataAs<float>();
	const int64 lignes = A.Shape()[0];
	const int64 cols = (A.Shape().Size() > 1) ? A.Shape()[1] : 1;
	if (axe == 1) { // colonnes : i,j parcourent les colonnes, on somme sur les lignes
		for (int64 r = 0; r < lignes; ++r) {
			const float *ra = a + r * cols;
			const float *rb = b + r * cols;
			for (int32 i = 0; i < n; ++i) {
				const float64 av = (float64)ra[i];
				if (av == 0.0)
					continue;
				float64 *dst = &cout[(usize)((int64)i * n)];
				for (int32 j = 0; j < n; ++j)
					dst[j] -= av * (float64)rb[j];
			}
		}
	} else { // lignes : i,j parcourent les lignes, on somme sur les colonnes
		for (int32 i = 0; i < n; ++i) {
			const float *ra = a + (int64)i * cols;
			float64 *dst = &cout[(usize)((int64)i * n)];
			for (int32 j = 0; j < n; ++j) {
				const float *rb = b + (int64)j * cols;
				float64 s = 0.0;
				for (int64 c = 0; c < cols; ++c)
					s += (float64)ra[c] * (float64)rb[c];
				dst[j] -= s;
			}
		}
	}
}

static void CoutResiduel(const NkVector<NkTensor> &A, const NkVector<NkTensor> &B, NkVector<float64> &cout) {
	cout.Resize((usize)kD * (usize)kD);
	for (usize k = 0; k < cout.Size(); ++k)
		cout[k] = 0.0;
	AjouterCout(cout, A[0], B[0], 1, kD); // tokEmb : colonnes
	AjouterCout(cout, A[1], B[1], 1, kD); // posEmb : colonnes
	for (int l = 0; l < kLayers; ++l) {
		const int g[6] = {P_LN1_G, P_LN1_B, P_LN2_G, P_LN2_B, P_WO_B, P_FC2_B};
		for (int k = 0; k < 6; ++k)
			AjouterCout(cout, A[(usize)IdxBloc(l, g[k])], B[(usize)IdxBloc(l, g[k])], 1, kD);
		const int lignes[4] = {P_WQ_W, P_WK_W, P_WV_W, P_FC1_W};
		for (int k = 0; k < 4; ++k)
			AjouterCout(cout, A[(usize)IdxBloc(l, lignes[k])], B[(usize)IdxBloc(l, lignes[k])], 0, kD);
		AjouterCout(cout, A[(usize)IdxBloc(l, P_WO_W)], B[(usize)IdxBloc(l, P_WO_W)], 1, kD);
		AjouterCout(cout, A[(usize)IdxBloc(l, P_FC2_W)], B[(usize)IdxBloc(l, P_FC2_W)], 1, kD);
	}
	AjouterCout(cout, A[(usize)IdxFin(P_LNF_G)], B[(usize)IdxFin(P_LNF_G)], 1, kD);
	AjouterCout(cout, A[(usize)IdxFin(P_LNF_B)], B[(usize)IdxFin(P_LNF_B)], 1, kD);
	AjouterCout(cout, A[(usize)IdxFin(P_HEAD_W)], B[(usize)IdxFin(P_HEAD_W)], 0, kD);
}

// Permute un axe d'un tenseur en place (via une copie).
static void PermuterAxe(NkTensor &T, int axe, const NkVector<int32> &perm, int32 n) {
	NkTensor src = T.Clone();
	const float *s = src.DataAs<float>();
	float *d = T.DataAs<float>();
	const int64 lignes = T.Shape()[0];
	const int64 cols = (T.Shape().Size() > 1) ? T.Shape()[1] : 1;
	if (axe == 1) {
		for (int64 r = 0; r < lignes; ++r)
			for (int32 i = 0; i < n; ++i)
				d[r * cols + i] = s[r * cols + perm[(usize)i]];
	} else {
		for (int32 i = 0; i < n; ++i)
			for (int64 c = 0; c < cols; ++c)
				d[(int64)i * cols + c] = s[(int64)perm[(usize)i] * cols + c];
	}
}

static void AppliquerResiduel(NkVector<NkTensor> &B, const NkVector<int32> &perm) {
	PermuterAxe(B[0], 1, perm, kD);
	PermuterAxe(B[1], 1, perm, kD);
	for (int l = 0; l < kLayers; ++l) {
		const int g[6] = {P_LN1_G, P_LN1_B, P_LN2_G, P_LN2_B, P_WO_B, P_FC2_B};
		for (int k = 0; k < 6; ++k)
			PermuterAxe(B[(usize)IdxBloc(l, g[k])], 1, perm, kD);
		const int lignes[4] = {P_WQ_W, P_WK_W, P_WV_W, P_FC1_W};
		for (int k = 0; k < 4; ++k)
			PermuterAxe(B[(usize)IdxBloc(l, lignes[k])], 0, perm, kD);
		PermuterAxe(B[(usize)IdxBloc(l, P_WO_W)], 1, perm, kD);
		PermuterAxe(B[(usize)IdxBloc(l, P_FC2_W)], 1, perm, kD);
	}
	PermuterAxe(B[(usize)IdxFin(P_LNF_G)], 1, perm, kD);
	PermuterAxe(B[(usize)IdxFin(P_LNF_B)], 1, perm, kD);
	PermuterAxe(B[(usize)IdxFin(P_HEAD_W)], 0, perm, kD);
}

// Quantite maximisee : produit scalaire de TOUS les poids de A avec ceux de B
// permute. Chaque appariement etant optimal a permutations voisines figees, et
// l'identite etant toujours une solution admissible, elle ne peut que croitre —
// une baisse denoncerait une erreur, pas une difficulte.
static float64 ObjectifTotal(const NkVector<NkTensor> &A, const NkVector<NkTensor> &B) {
	float64 t = 0.0;
	for (usize k = 0; k < A.Size(); ++k) {
		const float *a = A[k].DataAs<float>();
		const float *b = B[k].DataAs<float>();
		const int64 n = NkShapeNumel(A[k].Shape());
		for (int64 i = 0; i < n; ++i)
			t += (float64)a[i] * (float64)b[i];
	}
	return t;
}

// ---------------------------------------------------------------------------
// APPARIEMENT PAR LES ACTIVATIONS, et non par les poids.
//
// POURQUOI CETTE SECONDE VOIE. Apparier les POIDS revient a supposer que deux
// unites qui font la meme chose ont des poids qui se ressemblent. Rien ne le
// garantit : deux unites peuvent produire la meme reponse par des chemins
// differents, surtout apres une normalisation qui efface les echelles. La mesure
// des six paires de graines montre d'ailleurs que sur un transformeur, apparier
// les poids ne reduit PAS la barriere.
//
// On apparie donc sur ce que les unites FONT : on fait passer les MEMES donnees
// dans les deux modeles, on releve les activations, et on apparie l'unite i de A
// avec l'unite j de B qui lui repond le plus semblablement. La mesure employee
// est la CORRELATION (activations centrees et reduites) : elle ignore les
// differences d'echelle et de decalage, qui n'ont pas de sens ici puisqu'une
// normalisation les absorbe de toute facon.
// ---------------------------------------------------------------------------

// Centre et reduit chaque COLONNE d'une matrice [N, n] (une colonne = une unite).
static void CentrerReduire(NkTensor &M, int32 n) {
	float *p = M.DataAs<float>();
	const int64 N = M.Shape()[0];
	if (N < 2)
		return;
	for (int32 c = 0; c < n; ++c) {
		double m = 0.0;
		for (int64 r = 0; r < N; ++r)
			m += (double)p[r * n + c];
		m /= (double)N;
		double v = 0.0;
		for (int64 r = 0; r < N; ++r) {
			const double d = (double)p[r * n + c] - m;
			v += d * d;
		}
		const double s = std::sqrt(v / (double)N);
		const double inv = (s > 1e-12) ? 1.0 / s : 0.0;
		for (int64 r = 0; r < N; ++r)
			p[r * n + c] = (float)(((double)p[r * n + c] - m) * inv);
	}
}

// Accumule -Σ_r A[r,i]·B[r,j] (donc -N·correlation apres centrage-reduction).
static void AjouterCorrelation(NkVector<float64> &cout, const NkTensor &A, const NkTensor &B, int32 n) {
	const float *a = A.DataAs<float>();
	const float *b = B.DataAs<float>();
	const int64 N = A.Shape()[0];
	for (int64 r = 0; r < N; ++r) {
		const float *ra = a + r * n;
		const float *rb = b + r * n;
		for (int32 i = 0; i < n; ++i) {
			const float64 av = (float64)ra[i];
			if (av == 0.0)
				continue;
			float64 *dst = &cout[(usize)((int64)i * n)];
			for (int32 j = 0; j < n; ++j)
				dst[j] -= av * (float64)rb[j];
		}
	}
}

// ---------------------------------------------------------------------------
// Perte moyenne d'un jeu de poids arbitraire, sur des lots FIXES (memes lots
// pour tous les points mesures : sinon on comparerait des bruits, pas des
// modeles).
// ---------------------------------------------------------------------------
static double PerteMoyenne(const NkVector<NkTensor> &poids, const NkVector<NkTensor> &lotsX,
						   const NkVector<NkTensor> &lotsY) {
	nn::NkGPT m((uint32)kVocab, (uint32)kD, (uint32)kHeads, (uint32)kLayers, (uint32)kT, 1u);
	NkVector<NkVar> ps;
	m.Parameters(ps);
	for (usize i = 0; i < ps.Size(); ++i)
		ps[i].SetValue(poids[i]);
	double somme = 0.0;
	for (usize k = 0; k < lotsX.Size(); ++k) {
		NkVar logits = m.Forward(lotsX[k]);
		NkVar perte = autograd::SoftmaxCrossEntropyIndexed(logits, NkVar::Leaf(lotsY[k], false));
		somme += perte.Value().GetItem(NkShape{(int64)0});
	}
	return somme / (double)lotsX.Size();
}

int main(int argc, char **argv) {
	// Graines parametrables : une mesure unique sur un phenomene bruite ne
	// prouve rien, surtout quand elle surprend. On relance sur plusieurs paires
	// INDEPENDANTES (poids ET tirage des lots differents) avant de conclure.
	//   NKRebasinTransformer.exe [grainePoidsA grainePoidsB graineLotsA graineLotsB]
	const uint32 grA = (argc > 1) ? (uint32)strtoul(argv[1], nullptr, 10) : 11u;
	const uint32 grB = (argc > 2) ? (uint32)strtoul(argv[2], nullptr, 10) : 7717u;
	const uint64 lotA = (argc > 3) ? (uint64)strtoull(argv[3], nullptr, 10) : 3u;
	const uint64 lotB = (argc > 4) ? (uint64)strtoull(argv[4], nullptr, 10) : 8081u;
	setvbuf(stdout, nullptr, _IONBF, 0);
	printf("=== NKRebasinTransformer — aligner deux TRANSFORMEURS ===\n\n");
	printf("  CPU strict : aucun device GPU n'est cree.\n");
	printf("  Modele : vocabulaire %d, T=%d, d=%d, %d tetes (hd=%d), %d couches, MLP %d\n\n", kVocab, kT, kD,
		   kHeads, kHd, kLayers, kFF);
	ConstruireTexte();

	// ---- Deux transformeurs entraines SEPAREMENT --------------------------
	nn::NkGPT A((uint32)kVocab, (uint32)kD, (uint32)kHeads, (uint32)kLayers, (uint32)kT, grA);
	nn::NkGPT B((uint32)kVocab, (uint32)kD, (uint32)kHeads, (uint32)kLayers, (uint32)kT, grB);

	auto entrainer = [&](nn::NkGPT &m, uint64 graineLots, const char *nom, int pas) {
		NkVector<NkVar> ps;
		m.Parameters(ps);
		optim::NkAdam adam(ps, 3e-3f);
		uint64 rng = graineLots;
		double derniere = 0.0;
		for (int s = 1; s <= pas; ++s) {
			NkTensor x, y;
			FabriquerLot(x, y, rng);
			adam.ZeroGrad();
			NkVar logits = m.Forward(x);
			NkVar perte = autograd::SoftmaxCrossEntropyIndexed(logits, NkVar::Leaf(y, false));
			perte.Backward();
			adam.Step();
			derniere = perte.Value().GetItem(NkShape{(int64)0});
			if (s == 1 || s % 100 == 0)
				printf("    %s pas %4d : perte = %.4f\n", nom, s, derniere);
		}
		return derniere;
	};

	printf("  -- Entrainement de deux transformeurs INDEPENDANTS --\n");
	printf("     (graines de poids ET tirage des lots differents)\n");
	const int kPas = 400;
	entrainer(A, lotA, "A", kPas);
	entrainer(B, lotB, "B", kPas);

	NkVector<NkVar> pa, pb;
	A.Parameters(pa);
	B.Parameters(pb);
	Verdict(VerifierFormes(pa), "la disposition des poids est celle attendue");
	if (g_fail) {
		printf("  Abandon : sans certitude sur la disposition, tout le reste serait faux.\n");
		return 1;
	}
	NkVector<NkTensor> WA = Extraire(pa), WB = Extraire(pb);

	// ---- Lots FIXES d'evaluation ------------------------------------------
	NkVector<NkTensor> lotsX, lotsY;
	{
		uint64 rng = 999u;
		for (int k = 0; k < 8; ++k) {
			NkTensor x, y;
			FabriquerLot(x, y, rng);
			lotsX.PushBack(x);
			lotsY.PushBack(y);
		}
	}
	const double perteA = PerteMoyenne(WA, lotsX, lotsY);
	const double perteB = PerteMoyenne(WB, lotsX, lotsY);
	printf("\n     perte A = %.4f   perte B = %.4f\n", perteA, perteB);

	// ---- Interpolation NAIVE ----------------------------------------------
	auto melanger = [&](const NkVector<NkTensor> &X, const NkVector<NkTensor> &Y, float t) {
		NkVector<NkTensor> M;
		for (usize i = 0; i < X.Size(); ++i) {
			NkTensor o = NkTensor::Zeros(X[i].Shape());
			const float *a = X[i].DataAs<float>();
			const float *b = Y[i].DataAs<float>();
			float *d = o.DataAs<float>();
			const int64 n = NkShapeNumel(X[i].Shape());
			for (int64 k = 0; k < n; ++k)
				d[k] = a[k] * (1.f - t) + b[k] * t;
			M.PushBack(o);
		}
		return M;
	};

	printf("\n  -- 1. Interpolation NAIVE (perte ; plus BAS = mieux) --\n");
	static const float kLam[5] = {0.f, 0.25f, 0.5f, 0.75f, 1.f};
	double naif[5];
	for (int k = 0; k < 5; ++k) {
		naif[k] = PerteMoyenne(melanger(WA, WB, kLam[k]), lotsX, lotsY);
		printf("     lambda = %.2f : perte = %.4f\n", (double)kLam[k], naif[k]);
	}

	// ---- Alignement EXACT de ce qui est libre ------------------------------
	printf("\n  -- 2. Alignement des symetries LIBRES (flux residuel intact) --\n");
	NkVector<NkTensor> WBp = Extraire(pb); // copie a permuter
	int32 mlpDeja = 0, tetesDeja = 0;
	for (int l = 0; l < kLayers; ++l) {
		NkVector<float64> cout;
		NkVector<int32> perm;

		CoutMlp(WA, WBp, l, cout);
		HungarianMinCost(cout, kFF, perm);
		for (int32 i = 0; i < kFF; ++i)
			if (perm[(usize)i] == i)
				++mlpDeja;
		AppliquerMlp(WBp, l, perm);

		CoutTetes(WA, WBp, l, cout);
		HungarianMinCost(cout, kHeads, perm);
		for (int32 i = 0; i < kHeads; ++i)
			if (perm[(usize)i] == i)
				++tetesDeja;
		AppliquerTetes(WBp, l, perm);
		printf("     bloc %d : %d unites de MLP et %d tetes appariees (a l'optimum, sans descente)\n", l, kFF,
			   kHeads);
	}
	printf("     deja en place : %d/%d unites de MLP, %d/%d tetes\n", mlpDeja, kFF * kLayers, tetesDeja,
		   kHeads * kLayers);

	// ---- LE GARDE-FOU ------------------------------------------------------
	const double perteBp = PerteMoyenne(WBp, lotsX, lotsY);
	const double ecartBp = std::fabs(perteBp - perteB) / (perteB > 0 ? perteB : 1.0);
	printf("\n     B permute : perte = %.10f   (B d'origine : %.10f, ecart relatif %.1e)\n", perteBp, perteB,
		   ecartBp);
	// Seuil RELATIF a la mesure du float32, comme pour le flux residuel plus bas :
	// permuter change l'ordre des sommations dans chaque produit de matrices et
	// dans LayerNorm. Exiger l'egalite exacte reviendrait a exiger que l'addition
	// flottante soit associative. (Un seuil absolu de 1e-9 passait par CHANCE sur
	// une paire de graines et criait au loup sur les suivantes.)
	Verdict(ecartBp < 1e-5, "permuter ne change RIEN a ce que B calcule");

	// ---- Interpolation APRES (symetries libres seules) --------------------
	printf("\n  -- 3. Interpolation APRES alignement des seules symetries libres --\n");
	double aligne[5];
	for (int k = 0; k < 5; ++k) {
		aligne[k] = PerteMoyenne(melanger(WA, WBp, kLam[k]), lotsX, lotsY);
		printf("     lambda = %.2f : perte = %.4f\n", (double)kLam[k], aligne[k]);
	}

	// ---- 4. On s'attaque au FLUX RESIDUEL ---------------------------------
	// Il n'est plus question d'exactitude ici : la permutation du flux residuel
	// et les permutations locales se contraignent mutuellement (les memes
	// matrices portent les deux), donc on alterne — chaque etape optimale a
	// voisines figees, sans garantie d'optimum global.
	printf("\n  -- 4. Descente ALTERNEE, flux residuel COMPRIS --\n");
	NkVector<NkTensor> WBr = Extraire(pb); // on repart de B intact
	float64 objPrec = ObjectifTotal(WA, WBr);
	printf("     depart : objectif = %.1f\n", objPrec);
	int32 balayages = 0;
	for (int s = 1; s <= 30; ++s) {
		int32 changees = 0;
		++balayages;
		NkVector<float64> cout;
		NkVector<int32> perm;

		CoutResiduel(WA, WBr, cout);
		HungarianMinCost(cout, kD, perm);
		for (int32 i = 0; i < kD; ++i)
			if (perm[(usize)i] != i)
				++changees;
		AppliquerResiduel(WBr, perm);

		for (int l = 0; l < kLayers; ++l) {
			CoutMlp(WA, WBr, l, cout);
			HungarianMinCost(cout, kFF, perm);
			for (int32 i = 0; i < kFF; ++i)
				if (perm[(usize)i] != i)
					++changees;
			AppliquerMlp(WBr, l, perm);

			CoutTetes(WA, WBr, l, cout);
			HungarianMinCost(cout, kHeads, perm);
			for (int32 i = 0; i < kHeads; ++i)
				if (perm[(usize)i] != i)
					++changees;
			AppliquerTetes(WBr, l, perm);
		}

		const float64 obj = ObjectifTotal(WA, WBr);
		printf("     balayage %2d : %4d affectations changees, objectif = %.1f  (%+.1f)\n", s, changees, obj,
			   obj - objPrec);
		if (obj < objPrec - 1e-4) {
			printf("     *** L'OBJECTIF A BAISSE : la liste des axes du flux residuel est incomplete. ***\n");
			++g_fail;
		}
		objPrec = obj;
		if (changees == 0)
			break;
	}
	printf("     convergence en %d balayage(s)\n", balayages);

	// ---- DEUX garde-fous, qui ne testent PAS la meme chose ------------------
	//
	// (a) COMPTABILITE : appliquer une permutation puis son inverse doit rendre
	//     les poids IDENTIQUES AU BIT PRES. Ce test ne fait intervenir aucune
	//     arithmetique, seulement des deplacements : il isole une erreur d'axe
	//     ou d'indice, sans se laisser troubler par le flottant.
	{
		NkVector<NkTensor> essai = Extraire(pb);
		NkVector<int32> pi, piInv;
		pi.Resize((usize)kD);
		piInv.Resize((usize)kD);
		uint64 rng = 424242u;
		for (int32 i = 0; i < kD; ++i)
			pi[(usize)i] = i;
		for (int32 i = kD - 1; i > 0; --i) { // melange de Fisher-Yates
			rng = rng * 6364136223846793005ull + 1442695040888963407ull;
			const int32 j = (int32)((rng >> 33) % (uint32)(i + 1));
			const int32 t = pi[(usize)i];
			pi[(usize)i] = pi[(usize)j];
			pi[(usize)j] = t;
		}
		for (int32 i = 0; i < kD; ++i)
			piInv[(usize)pi[(usize)i]] = i;
		AppliquerResiduel(essai, pi);
		AppliquerResiduel(essai, piInv);
		bool identique = true;
		for (usize k = 0; k < essai.Size() && identique; ++k) {
			const float *a = WB[k].DataAs<float>();
			const float *b = essai[k].DataAs<float>();
			const int64 n = NkShapeNumel(WB[k].Shape());
			for (int64 i = 0; i < n; ++i)
				if (a[i] != b[i]) {
					identique = false;
					break;
				}
		}
		Verdict(identique, "permutation du flux residuel puis son inverse : poids identiques AU BIT PRES");

		// (b) SYMETRIE : une permutation ALEATOIRE du flux residuel ne doit pas
		//     changer ce que le modele calcule. Un axe oublie dans la liste
		//     casserait le modele de facon flagrante, pas au huitieme chiffre.
		NkVector<NkTensor> alea = Extraire(pb);
		AppliquerResiduel(alea, pi);
		const double perteAlea = PerteMoyenne(alea, lotsX, lotsY);
		const double ecartRel = std::fabs(perteAlea - perteB) / (perteB > 0 ? perteB : 1.0);
		printf("\n     permutation ALEATOIRE du flux residuel : perte = %.10f  (ecart relatif %.1e)\n", perteAlea,
			   ecartRel);
		// Seuil a la mesure du float32 : permuter change l'ORDRE des sommations
		// dans LayerNorm et dans chaque produit de matrices, donc le dernier
		// chiffre bouge forcement. Exiger l'egalite exacte serait exiger que
		// l'addition flottante soit associative — elle ne l'est pas.
		Verdict(ecartRel < 1e-5, "le flux residuel est bien une symetrie (a la precision du float32)");
	}

	const double perteBr = PerteMoyenne(WBr, lotsX, lotsY);
	const double ecartBr = std::fabs(perteBr - perteB) / (perteB > 0 ? perteB : 1.0);
	printf("\n     B re-permute : perte = %.10f   (B d'origine : %.10f, ecart relatif %.1e)\n", perteBr, perteB,
		   ecartBr);
	Verdict(ecartBr < 1e-5, "l'alignement complet ne change pas ce que B calcule");

	printf("\n  -- 5. Interpolation APRES alignement COMPLET (par les POIDS) --\n");
	double complet[5];
	for (int k = 0; k < 5; ++k) {
		complet[k] = PerteMoyenne(melanger(WA, WBr, kLam[k]), lotsX, lotsY);
		printf("     lambda = %.2f : perte = %.4f\n", (double)kLam[k], complet[k]);
	}

	// ---- 6. La MEME chose, mais appariee sur les ACTIVATIONS ---------------
	// Meme structure de permutations, meme garde-fou : SEUL le critere change.
	// C'est ce qui rend la comparaison interpretable — si le resultat differe,
	// ce n'est pas parce qu'on a permute autre chose, mais parce qu'on a
	// mesure la ressemblance autrement.
	printf("\n  -- 6. Alignement par les ACTIVATIONS (memes permutations, autre critere) --\n");
	NkVector<NkTensor> WBa = Extraire(pb);
	{
		// Relever les activations des deux modeles sur les MEMES donnees.
		auto relever = [&](nn::NkGPT &m, NkVector<NkTensor> &res, NkVector<NkTensor> &mlp) {
			for (usize k = 0; k < lotsX.Size(); ++k) {
				NkVector<NkTensor> r, h;
				m.ForwardCapture(lotsX[k], &r, &h);
				for (usize i = 0; i < r.Size(); ++i)
					res.PushBack(r[i]);
				for (usize i = 0; i < h.Size(); ++i)
					mlp.PushBack(h[i]);
			}
		};
		NkVector<NkTensor> resA, mlpA, resB, mlpB;
		relever(A, resA, mlpA);
		relever(B, resB, mlpB);
		for (usize i = 0; i < resA.Size(); ++i) {
			CentrerReduire(resA[i], kD);
			CentrerReduire(resB[i], kD);
		}
		for (usize i = 0; i < mlpA.Size(); ++i) {
			CentrerReduire(mlpA[i], kFF);
			CentrerReduire(mlpB[i], kFF);
		}
		printf("     activations relevees : %d points de flux residuel, %d de MLP (par modele)\n",
			   (int)resA.Size(), (int)mlpA.Size());

		NkVector<float64> cout;
		NkVector<int32> perm;

		// Flux residuel : UNE permutation, donc on somme les correlations sur TOUS
		// les points de relevé (entree + sortie de chaque bloc, sur tous les lots).
		cout.Resize((usize)kD * (usize)kD);
		for (usize k = 0; k < cout.Size(); ++k)
			cout[k] = 0.0;
		for (usize i = 0; i < resA.Size(); ++i)
			AjouterCorrelation(cout, resA[i], resB[i], kD);
		HungarianMinCost(cout, kD, perm);
		AppliquerResiduel(WBa, perm);

		// MLP : une permutation par bloc ; on somme sur les lots.
		const int nLots = (int)lotsX.Size();
		for (int l = 0; l < kLayers; ++l) {
			cout.Resize((usize)kFF * (usize)kFF);
			for (usize k = 0; k < cout.Size(); ++k)
				cout[k] = 0.0;
			for (int k = 0; k < nLots; ++k)
				AjouterCorrelation(cout, mlpA[(usize)(k * kLayers + l)], mlpB[(usize)(k * kLayers + l)], kFF);
			HungarianMinCost(cout, kFF, perm);
			AppliquerMlp(WBa, l, perm);
			// Les tetes n'ont pas d'activation propre relevee ici : on garde
			// l'appariement par les poids, qui est exact pour elles.
			CoutTetes(WA, WBa, l, cout);
			HungarianMinCost(cout, kHeads, perm);
			AppliquerTetes(WBa, l, perm);
		}
	}
	const double perteBa = PerteMoyenne(WBa, lotsX, lotsY);
	const double ecartBa = std::fabs(perteBa - perteB) / (perteB > 0 ? perteB : 1.0);
	printf("     B permute (activations) : perte = %.10f  (ecart relatif %.1e)\n", perteBa, ecartBa);
	Verdict(ecartBa < 1e-5, "l'alignement par activations est lui aussi une pure permutation");

	double parAct[5];
	for (int k = 0; k < 5; ++k) {
		parAct[k] = PerteMoyenne(melanger(WA, WBa, kLam[k]), lotsX, lotsY);
		printf("     lambda = %.2f : perte = %.4f\n", (double)kLam[k], parAct[k]);
	}

	// ---- Verdict -----------------------------------------------------------
	const double base = (naif[0] + naif[4]) * 0.5;
	const double barrN = naif[2] - base; // en PERTE : la barriere MONTE
	const double barrA = aligne[2] - base;
	const double barrC = complet[2] - base;
	printf("\n  == RESULTAT ==\n");
	printf("     perte moyenne des extremites          : %.4f\n", base);
	printf("     barriere SANS aucun alignement        : +%.4f\n", barrN);
	printf("     barriere, symetries LIBRES seules     : +%.4f  (%.1f%% retires)\n", barrA,
		   (barrN > 1e-6) ? (1.0 - barrA / barrN) * 100.0 : 0.0);
	printf("     barriere, flux residuel COMPRIS       : +%.4f  (%.1f%% retires)\n", barrC,
		   (barrN > 1e-6) ? (1.0 - barrC / barrN) * 100.0 : 0.0);
	const double barrAct = parAct[2] - base;
	printf("     barriere, appariee sur ACTIVATIONS    : +%.4f  (%.1f%% retires)\n", barrAct,
		   (barrN > 1e-6) ? (1.0 - barrAct / barrN) * 100.0 : 0.0);

	Verdict(barrN > 0.05, "il y a bien une barriere a franchir sur un transformeur");
	// On NE met PAS de verdict sur « l'alignement complet fait-il mieux ? ». Un
	// test doit controler une invariance du code (la permutation est-elle une
	// symetrie, la comptabilite est-elle exacte, l'objectif monte-t-il), pas une
	// attente sur le resultat. Que le flux residuel aide ou nuise, les deux sont
	// des mesures a rapporter, pas des reussites ou des echecs.

	printf("\n  CE QUE CES CHIFFRES DISENT.\n");
	printf("  Les symetries libres (MLP, tetes) sont alignees A L'OPTIMUM : leur mesure est\n"
		   "  exacte. Le flux residuel, lui, est traite par descente alternee — chaque etape\n"
		   "  est optimale a voisines figees, mais rien ne garantit l'optimum global, et le\n"
		   "  resultat depend du point de depart (ici l'identite).\n");
	if (barrC < 0.5 * barrN)
		printf("  => Le flux residuel etait bien le verrou, et le lever change tout.\n");
	else if (barrC < 0.9 * barrN)
		printf("  => Le flux residuel compte, mais le lever ne suffit pas : deux transformeurs\n"
			   "     entrainee separement ne sont PAS le meme modele a une permutation pres.\n"
			   "     Ils different par autre chose que l'ordre de leurs unites.\n");
	else
		printf("  => Meme en traitant TOUT ce qui est permutable, la barriere reste. C'est le\n"
			   "     resultat le plus informatif de la serie : sur un transformeur, la\n"
			   "     permutation n'explique PAS l'ecart entre deux entrainements. La piste de\n"
			   "     la combinaison par simple realignement ne suffira pas — il faudra autre\n"
			   "     chose (re-entrainement court apres empilement, ou fusion guidee par les\n"
			   "     activations plutot que par les poids).\n");

	printf("\n  Resultat : %d OK, %d echec(s)\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
