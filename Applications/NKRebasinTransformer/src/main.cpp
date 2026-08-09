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
enum { P_WQ_W = 2, P_WQ_B = 3, P_WK_W = 4, P_WK_B = 5, P_WV_W = 6, P_WV_B = 7,
	   P_WO_W = 8, P_FC1_W = 12, P_FC1_B = 13, P_FC2_W = 14 };

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

int main() {
	setvbuf(stdout, nullptr, _IONBF, 0);
	printf("=== NKRebasinTransformer — aligner deux TRANSFORMEURS ===\n\n");
	printf("  CPU strict : aucun device GPU n'est cree.\n");
	printf("  Modele : vocabulaire %d, T=%d, d=%d, %d tetes (hd=%d), %d couches, MLP %d\n\n", kVocab, kT, kD,
		   kHeads, kHd, kLayers, kFF);
	ConstruireTexte();

	// ---- Deux transformeurs entraines SEPAREMENT --------------------------
	nn::NkGPT A((uint32)kVocab, (uint32)kD, (uint32)kHeads, (uint32)kLayers, (uint32)kT, 11u);
	nn::NkGPT B((uint32)kVocab, (uint32)kD, (uint32)kHeads, (uint32)kLayers, (uint32)kT, 7717u);

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
	entrainer(A, 3u, "A", kPas);
	entrainer(B, 8081u, "B", kPas);

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
	printf("\n     B permute : perte = %.10f   (B d'origine : %.10f)\n", perteBp, perteB);
	Verdict(std::fabs(perteBp - perteB) < 1e-9, "permuter ne change RIEN a ce que B calcule");

	// ---- Interpolation APRES ----------------------------------------------
	printf("\n  -- 3. Interpolation APRES alignement --\n");
	double aligne[5];
	for (int k = 0; k < 5; ++k) {
		aligne[k] = PerteMoyenne(melanger(WA, WBp, kLam[k]), lotsX, lotsY);
		printf("     lambda = %.2f : perte = %.4f\n", (double)kLam[k], aligne[k]);
	}

	// ---- Verdict -----------------------------------------------------------
	const double base = (naif[0] + naif[4]) * 0.5;
	const double barrN = naif[2] - base;   // en PERTE : la barriere MONTE
	const double barrA = aligne[2] - base;
	printf("\n  == RESULTAT ==\n");
	printf("     perte moyenne des extremites : %.4f\n", base);
	printf("     barriere SANS alignement  : +%.4f de perte au milieu\n", barrN);
	printf("     barriere APRES alignement : +%.4f de perte au milieu\n", barrA);
	if (barrN > 1e-6)
		printf("     l'alignement des symetries libres en retire %.1f%%\n", (1.0 - barrA / barrN) * 100.0);

	Verdict(barrN > 0.05, "il y a bien une barriere a franchir sur un transformeur");
	printf("\n  CE QUE CE CHIFFRE DIT.\n");
	if (barrA < 0.5 * barrN)
		printf("  Aligner ce qui est libre SANS toucher au flux residuel suffit a en retirer\n"
			   "  une part importante. La piste s'ouvre.\n");
	else
		printf("  Aligner ce qui est libre ne suffit PAS : l'essentiel de la barriere tient au\n"
			   "  FLUX RESIDUEL, qui n'est pas permutable bloc par bloc mais par UNE SEULE\n"
			   "  permutation globale liant embedding, normalisations, projections et tete de\n"
			   "  sortie. C'est la le verrou, et c'est un resultat en soi : il designe\n"
			   "  precisement ou porter l'effort suivant.\n");
	printf("\n  Ce qui est mesure ici est EXACT (chaque appariement est optimal, et les\n"
		   "  permutations se decouplent des lors que le flux residuel est fige) — mais il\n"
		   "  porte sur un SOUS-ESPACE des symetries, pas sur toutes.\n");

	printf("\n  Resultat : %d OK, %d echec(s)\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
