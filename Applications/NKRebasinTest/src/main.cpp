// =============================================================================
// NKRebasinTest — DEUX RESEAUX ENTRAINES SEPAREMENT, ALIGNES PAR PERMUTATION.
//
// LA QUESTION, POSEE PAR RIHEN : peut-on prendre deux modeles entraines
// independamment et les COMBINER ? La reponse naive est non -- leurs espaces
// internes n'ont aucune raison de coincider, et la moyenne de leurs poids
// donne un reseau qui ne sait plus rien.
//
// MAIS LA RAISON EST PLUS SUBTILE QUE « ILS SONT DIFFERENTS ». Un reseau
// possede des SYMETRIES DE PERMUTATION : echanger deux unites cachees, en
// echangeant du meme coup les poids qui y entrent et ceux qui en sortent, donne
// EXACTEMENT la meme fonction. Deux entrainements independants tombent souvent
// dans le meme creux de la surface de perte, A UNE PERMUTATION PRES. Ils ne
// parlent pas deux langues : ils parlent la meme, dans un ordre different.
//
// CE QUE CETTE EXPERIENCE MESURE (methode : Ainsworth & al., « Git Re-Basin »,
// 2022 -- article librement implementable, cf. docs/SOURCES_TIERCES.md) :
//   1. entrainer A et B depuis des graines DIFFERENTES ;
//   2. interpoler naivement leurs poids -> la BARRIERE (l'effondrement au
//      milieu du chemin) ;
//   3. chercher les permutations qui alignent B sur A ;
//   4. interpoler apres alignement -> la barriere doit S'EFFONDRER.
//
// ---------------------------------------------------------------------------
// LA MARCHE FRANCHIE ICI : PLUSIEURS COUCHES CACHEES.
//
// Avec UNE couche cachee, le probleme est EXACTEMENT soluble : une seule
// permutation a trouver, un probleme d'affectation lineaire que la methode
// hongroise resout de facon optimale. C'est ce que mesurait la version
// precedente, et ce chiffre reste le socle.
//
// Des DEUX couches cachees, ce n'est plus vrai. Le meilleur choix pour la
// couche 1 depend de ce qu'on decide pour la couche 2, et reciproquement : les
// poids d'une couche relient DEUX permutations a la fois. Le probleme joint
// n'est plus une affectation lineaire, et personne ne sait le resoudre
// exactement.
//
// LA METHODE (celle de l'article) : la DESCENTE PAR COORDONNEES. On fige toutes
// les permutations sauf une, ce qui redonne un probleme d'affectation exact
// pour celle-la ; on la resout a l'optimum ; on passe a la suivante ; on
// recommence jusqu'a ce que plus rien ne bouge. Chaque etape ne peut
// qu'augmenter la quantite qu'on maximise -- la somme des produits scalaires
// entre poids de A et poids de B permutes -- donc le procede converge.
//
// CE QU'IL FAUT SURVEILLER, ET QU'ON MESURE ICI : (a) cette quantite doit
// croitre A CHAQUE balayage, jamais decroitre -- une baisse signalerait une
// erreur dans la construction du cout, pas une difficulte du probleme ;
// (b) permuter B ne doit RIEN changer a ce que B calcule, sur TOUTES les
// couches a la fois ; (c) l'optimum atteint depend du point de depart, donc
// rien ne garantit le meilleur alignement possible -- seulement un bon.
//
// DISPOSITION DES POIDS (NkDense) : W est [entree, sortie], b est [1, sortie].
// Permuter l'unite j d'une couche cachee revient donc a permuter la COLONNE j
// de la matrice qui y entre, l'entree j de son biais, et la LIGNE j de la
// matrice qui en sort.
//
// LE GPU EST UN GUICHET UNIQUE : une seule carte, un seul entrainement a la
// fois. Cette experience s'execute donc en CPU par defaut (option --gpu pour
// forcer), afin de pouvoir tourner PENDANT un entrainement long sans lui
// disputer la memoire video.
// =============================================================================
#include "NKTrain/NkTrain.h"
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKData/NkData.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorGpu.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static bool gUseGpu = false;
static int gPass = 0;
static int gFail = 0;

static void Verdict(bool ok, const char *quoi) {
	if (ok)
		++gPass;
	else
		++gFail;
	printf("  [%s] %s\n", ok ? " OK " : "FAIL", quoi);
}

// ---------------------------------------------------------------------------
// METHODE HONGROISE (Kuhn-Munkres), variante par chemins augmentants en O(n^3).
//
// Elle rend l'affectation de COUT MINIMAL — donc, en lui passant l'oppose des
// similarites, l'appariement de similarite MAXIMALE. On ne prend pas un
// glouton : deux unites peuvent se disputer le meme partenaire, et le glouton
// se trompe alors sur toute la suite. Ici l'optimum est garanti, ce qui est
// indispensable — sinon un mauvais alignement ferait conclure a tort que la
// methode ne marche pas. Avec plusieurs couches, c'est aussi ce qui garantit
// que chaque pas de la descente par coordonnees est bien optimal A SON TOUR.
//
// `cost` est une matrice n x n en ligne-majeur. `assign[i]` recoit la colonne
// attribuee a la ligne i.
// ---------------------------------------------------------------------------
static void HungarianMinCost(const NkVector<float64> &cost, int32 n, NkVector<int32> &assign) {
	// Potentiels (u sur les lignes, v sur les colonnes) et p[j] = ligne
	// affectee a la colonne j. Les indices partent a 1 : la case 0 sert de
	// sentinelle au parcours, c'est la forme classique de l'algorithme.
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
				} else {
					minv[(usize)j] -= delta;
				}
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

// Copie CPU d'un parametre, quel que soit l'endroit ou il reside.
static NkTensor ToCpu(const NkVar &v) {
	// ToCPU rend *this quand le tenseur y est deja : pas de test a ecrire, donc
	// pas de test a se tromper.
	return v.Value().ToCPU();
}

// ---------------------------------------------------------------------------
// Un reseau de profondeur quelconque : tailles = [entree, cache..., sortie].
// ---------------------------------------------------------------------------
struct Reseau {
		NkVector<int32> tailles;
		NkVector<nn::NkDense> couches;
		NkVector<NkTensor> W, b; // copies CPU, remplies par Extraire()
		double accTest = 0.0;

		int32 NbCouches() const {
			return (int32)couches.Size();
		}

		int32 NbCachees() const {
			return (int32)couches.Size() - 1;
		}

		void Construire(const NkVector<int32> &t, uint32 graine) {
			tailles = t;
			couches.Clear();
			for (usize i = 0; i + 1 < t.Size(); ++i)
				couches.PushBack(nn::NkDense((uint32)t[i], (uint32)t[i + 1], graine + (uint32)i * 1013u));
		}

		void Extraire() {
			W.Clear();
			b.Clear();
			for (usize i = 0; i < couches.Size(); ++i) {
				W.PushBack(ToCpu(couches[i].Weight()));
				b.PushBack(ToCpu(couches[i].Bias()));
			}
		}
};

// Evalue un jeu de poids ARBITRAIRE (celui d'aucun entrainement : un point du
// chemin d'interpolation, ou un B permute). On injecte les poids dans un
// reseau jetable de meme forme.
static double Evaluer(const NkVector<int32> &tailles, const NkVector<NkTensor> &W, const NkVector<NkTensor> &b,
					  const data::NkDataset &test) {
	Reseau e;
	e.Construire(tailles, 1u);
	NkVector<NkVar> ps;
	for (usize i = 0; i < e.couches.Size(); ++i)
		e.couches[i].Parameters(ps);
	// L'ORDRE de Parameters() est (W, b) par couche — le meme que celui dans
	// lequel on les a lus. S'y fier est le seul lien entre les deux.
	for (usize i = 0; i < e.couches.Size(); ++i) {
		ps[i * 2 + 0].SetValue(gUseGpu ? W[i].ToGPU() : W[i]);
		ps[i * 2 + 1].SetValue(gUseGpu ? b[i].ToGPU() : b[i]);
	}
	auto fwd = [&](const NkVar &x) {
		NkVar h = gUseGpu ? NkVar::Leaf(x.Value().ToGPU(), false) : x;
		for (usize i = 0; i < e.couches.Size(); ++i) {
			h = e.couches[i].Forward(h);
			if (i + 1 < e.couches.Size())
				h = nn::Relu(h);
		}
		return h;
	};
	data::NkDataLoader tl(test, 128, false, 1u);
	return train::Accuracy(fwd, tl);
}

static NkTensor Lerp(const NkTensor &x, const NkTensor &y, float32 t) {
	NkTensor o = NkTensor::Zeros(x.Shape());
	const float *a = x.DataAs<float>();
	const float *bb = y.DataAs<float>();
	float *d = o.DataAs<float>();
	const int64 n = NkShapeNumel(x.Shape());
	for (int64 i = 0; i < n; ++i)
		d[i] = a[i] * (1.f - t) + bb[i] * t;
	return o;
}

// ---------------------------------------------------------------------------
// LA QUANTITE MAXIMISEE : somme des produits scalaires entre les poids de A et
// ceux de B une fois permutes. C'est elle qui doit croitre a chaque balayage.
// Interpoler entre deux jeux de poids d'autant plus « paralleles » que ce
// produit est grand, c'est exactement ce qui aplanit la vallee entre eux.
// ---------------------------------------------------------------------------
static float64 Objectif(const Reseau &A, const Reseau &B, const NkVector<NkVector<int32>> &perms) {
	const int32 L = A.NbCouches();
	float64 total = 0.0;
	for (int32 l = 0; l < L; ++l) {
		const int32 nIn = A.tailles[(usize)l];
		const int32 nOut = A.tailles[(usize)(l + 1)];
		const float *pa = A.W[(usize)l].DataAs<float>();
		const float *pb = B.W[(usize)l].DataAs<float>();
		const float *ba = A.b[(usize)l].DataAs<float>();
		const float *bb = B.b[(usize)l].DataAs<float>();
		for (int32 r = 0; r < nIn; ++r) {
			const int32 rB = (l > 0) ? perms[(usize)(l - 1)][(usize)r] : r;
			for (int32 c = 0; c < nOut; ++c) {
				const int32 cB = (l + 1 < L) ? perms[(usize)l][(usize)c] : c;
				total += (float64)pa[(int64)r * nOut + c] * (float64)pb[(int64)rB * nOut + cB];
			}
		}
		for (int32 c = 0; c < nOut; ++c) {
			const int32 cB = (l + 1 < L) ? perms[(usize)l][(usize)c] : c;
			total += (float64)ba[c] * (float64)bb[cB];
		}
	}
	return total;
}

// Applique les permutations a B : sortie dans Wp/bp.
static void AppliquerPermutations(const Reseau &B, const NkVector<NkVector<int32>> &perms, NkVector<NkTensor> &Wp,
								  NkVector<NkTensor> &bp) {
	const int32 L = B.NbCouches();
	Wp.Clear();
	bp.Clear();
	for (int32 l = 0; l < L; ++l) {
		const int32 nIn = B.tailles[(usize)l];
		const int32 nOut = B.tailles[(usize)(l + 1)];
		NkTensor w = NkTensor::Zeros(B.W[(usize)l].Shape());
		NkTensor bi = NkTensor::Zeros(B.b[(usize)l].Shape());
		const float *sw = B.W[(usize)l].DataAs<float>();
		const float *sb = B.b[(usize)l].DataAs<float>();
		float *dw = w.DataAs<float>();
		float *db = bi.DataAs<float>();
		for (int32 r = 0; r < nIn; ++r) {
			const int32 rB = (l > 0) ? perms[(usize)(l - 1)][(usize)r] : r;
			for (int32 c = 0; c < nOut; ++c) {
				const int32 cB = (l + 1 < L) ? perms[(usize)l][(usize)c] : c;
				dw[(int64)r * nOut + c] = sw[(int64)rB * nOut + cB];
			}
		}
		for (int32 c = 0; c < nOut; ++c) {
			const int32 cB = (l + 1 < L) ? perms[(usize)l][(usize)c] : c;
			db[c] = sb[cB];
		}
		Wp.PushBack(w);
		bp.PushBack(bi);
	}
}

// ---------------------------------------------------------------------------
// DESCENTE PAR COORDONNEES : une couche cachee a la fois, jusqu'a immobilite.
// ---------------------------------------------------------------------------
static int32 AlignerParPermutations(const Reseau &A, const Reseau &B, NkVector<NkVector<int32>> &perms,
									int32 maxBalayages, bool bavard) {
	const int32 L = A.NbCouches();
	const int32 K = L - 1; // nombre de couches cachees

	// Depart : l'identite. C'est le point de depart de l'article, et il a le
	// merite d'etre reproductible.
	perms.Clear();
	for (int32 l = 0; l < K; ++l) {
		NkVector<int32> p;
		const int32 n = A.tailles[(usize)(l + 1)];
		p.Resize((usize)n);
		for (int32 i = 0; i < n; ++i)
			p[(usize)i] = i;
		perms.PushBack(p);
	}

	float64 objPrec = Objectif(A, B, perms);
	if (bavard)
		printf("     depart (identite) : objectif = %.1f\n", objPrec);

	int32 balayages = 0;
	for (int32 s = 1; s <= maxBalayages; ++s) {
		int32 changees = 0;
		++balayages;
		for (int32 l = 0; l < K; ++l) {
			const int32 n = A.tailles[(usize)(l + 1)]; // taille de la couche cachee l
			const int32 nIn = A.tailles[(usize)l];
			const int32 nNext = A.tailles[(usize)(l + 2)];

			// Similarite entre l'unite i de A et l'unite j de B, a permutations
			// VOISINES FIGEES : ce qui entre (aligne par perms[l-1]) + le biais +
			// ce qui sort (aligne par perms[l+1]).
			NkVector<float64> cout;
			cout.Resize((usize)n * (usize)n);
			for (usize k = 0; k < cout.Size(); ++k)
				cout[k] = 0.0;

			{
				const float *wA = A.W[(usize)l].DataAs<float>();
				const float *wB = B.W[(usize)l].DataAs<float>();
				for (int32 r = 0; r < nIn; ++r) {
					const int32 rB = (l > 0) ? perms[(usize)(l - 1)][(usize)r] : r;
					const float *rowA = wA + (int64)r * n;
					const float *rowB = wB + (int64)rB * n;
					for (int32 i = 0; i < n; ++i) {
						const float64 a = (float64)rowA[i];
						if (a == 0.0)
							continue;
						float64 *dst = &cout[(usize)((int64)i * n)];
						for (int32 j = 0; j < n; ++j)
							dst[j] -= a * (float64)rowB[j]; // cout = -similarite
					}
				}
			}
			{
				const float *bA = A.b[(usize)l].DataAs<float>();
				const float *bB = B.b[(usize)l].DataAs<float>();
				for (int32 i = 0; i < n; ++i)
					for (int32 j = 0; j < n; ++j)
						cout[(usize)((int64)i * n + j)] -= (float64)bA[i] * (float64)bB[j];
			}
			{
				const float *wA = A.W[(usize)(l + 1)].DataAs<float>();
				const float *wB = B.W[(usize)(l + 1)].DataAs<float>();
				for (int32 i = 0; i < n; ++i) {
					for (int32 j = 0; j < n; ++j) {
						float64 s2 = 0.0;
						for (int32 c = 0; c < nNext; ++c) {
							const int32 cB = (l + 2 < L) ? perms[(usize)(l + 1)][(usize)c] : c;
							s2 += (float64)wA[(int64)i * nNext + c] * (float64)wB[(int64)j * nNext + cB];
						}
						cout[(usize)((int64)i * n + j)] -= s2;
					}
				}
			}

			NkVector<int32> nouvelle;
			HungarianMinCost(cout, n, nouvelle);
			for (int32 i = 0; i < n; ++i)
				if (nouvelle[(usize)i] != perms[(usize)l][(usize)i])
					++changees;
			perms[(usize)l] = nouvelle;
		}

		const float64 obj = Objectif(A, B, perms);
		if (bavard)
			printf("     balayage %d : %d affectations changees, objectif = %.1f  (%+.1f)\n", s, changees, obj,
				   obj - objPrec);
		// L'objectif ne doit JAMAIS baisser : chaque sous-probleme est resolu a
		// l'optimum a permutations voisines figees, donc au pire il ne bouge pas.
		if (obj < objPrec - 1e-6) {
			printf("     *** L'OBJECTIF A BAISSE (%.6f -> %.6f) : la construction du cout est fausse. ***\n",
				   objPrec, obj);
			++gFail;
		}
		objPrec = obj;
		if (changees == 0)
			break;
	}
	return balayages;
}

// ---------------------------------------------------------------------------
// Une experience complete pour une architecture donnee.
// ---------------------------------------------------------------------------
struct Resultat {
		double barriereNaive = 0.0;
		double barriereAlignee = 0.0;
		double accMilieuNaif = 0.0;
		double accMilieuAligne = 0.0;
		double accExtremites = 0.0;
		int32 balayages = 0;
		int32 dejaAlignees = 0;
		int32 totalCachees = 0;
		bool permExacte = false;
};

static Resultat Experience(const NkVector<int32> &tailles, const data::NkDataset &mnist, const data::NkDataset &test,
						   int epoques) {
	Resultat R;
	printf("\n=================================================================\n");
	printf("  ARCHITECTURE : ");
	for (usize i = 0; i < tailles.Size(); ++i)
		printf("%d%s", tailles[i], (i + 1 < tailles.Size()) ? " -> " : "");
	printf("   (%d couche%s cachee%s)\n", (int)tailles.Size() - 2, (tailles.Size() > 3) ? "s" : "",
		   (tailles.Size() > 3) ? "s" : "");
	printf("=================================================================\n");

	Reseau A, B;
	A.Construire(tailles, 11u);
	B.Construire(tailles, 7717u);

	auto entrainer = [&](Reseau &M, uint32 melange, const char *nom) {
		data::NkDataLoader loader(mnist, 128, true, melange);
		NkVector<NkVar> params;
		for (usize i = 0; i < M.couches.Size(); ++i)
			M.couches[i].Parameters(params);
		if (gUseGpu)
			for (uint32 i = 0; i < params.Size(); ++i)
				params[i].SetValue(params[i].Value().ToGPU());
		auto fwd = [&](const NkVar &x) {
			NkVar h = gUseGpu ? NkVar::Leaf(x.Value().ToGPU(), false) : x;
			for (usize i = 0; i < M.couches.Size(); ++i) {
				h = M.couches[i].Forward(h);
				if (i + 1 < M.couches.Size())
					h = nn::Relu(h);
			}
			return h;
		};
		optim::NkAdam adam(params, 0.001f);
		for (int e = 1; e <= epoques; ++e) {
			train::EpochStats st = train::TrainEpoch(fwd, adam, loader);
			printf("    %s epoque %d : perte %.4f  exactitude %.2f%%\n", nom, e, st.loss, st.acc * 100.0);
		}
		data::NkDataLoader tl(test, 128, false, 1u);
		M.accTest = train::Accuracy(fwd, tl);
		printf("    %s : TEST = %.2f%%\n", nom, M.accTest * 100.0);
		M.Extraire();
	};

	printf("\n  -- Entrainement de deux reseaux INDEPENDANTS --\n");
	entrainer(A, 3u, "A");
	entrainer(B, 8081u, "B");

	static const float32 kLambda[5] = {0.f, 0.25f, 0.5f, 0.75f, 1.f};

	printf("\n  -- 1. Interpolation NAIVE (sans alignement) --\n");
	double naif[5];
	for (int k = 0; k < 5; ++k) {
		NkVector<NkTensor> Wm, bm;
		for (int32 l = 0; l < A.NbCouches(); ++l) {
			Wm.PushBack(Lerp(A.W[(usize)l], B.W[(usize)l], kLambda[k]));
			bm.PushBack(Lerp(A.b[(usize)l], B.b[(usize)l], kLambda[k]));
		}
		naif[k] = Evaluer(tailles, Wm, bm, test);
		printf("     lambda = %.2f : %.2f%%\n", (double)kLambda[k], naif[k] * 100.0);
	}

	printf("\n  -- 2. Alignement par descente sur les %d couche(s) cachee(s) --\n", A.NbCachees());
	NkVector<NkVector<int32>> perms;
	R.balayages = AlignerParPermutations(A, B, perms, 20, true);

	R.dejaAlignees = 0;
	R.totalCachees = 0;
	for (usize l = 0; l < perms.Size(); ++l) {
		R.totalCachees += (int32)perms[l].Size();
		for (usize i = 0; i < perms[l].Size(); ++i)
			if (perms[l][i] == (int32)i)
				++R.dejaAlignees;
	}
	printf("     convergence en %d balayage(s) ; %d unites sur %d etaient deja a leur place\n", R.balayages,
		   R.dejaAlignees, R.totalCachees);

	NkVector<NkTensor> Wp, bp;
	AppliquerPermutations(B, perms, Wp, bp);

	// VERIFICATION INDISPENSABLE : permuter ne doit RIEN changer a la fonction,
	// et cela vaut pour TOUTES les couches simultanement. Si l'exactitude de B
	// bouge apres permutation, l'application est fausse et tout le reste de la
	// mesure ne veut plus rien dire.
	const double accBperm = Evaluer(tailles, Wp, bp, test);
	R.permExacte = std::fabs(accBperm - B.accTest) < 1e-12;
	printf("     B permute : %.4f%%  (B d'origine : %.4f%%) — l'ecart doit etre NUL\n", accBperm * 100.0,
		   B.accTest * 100.0);

	printf("\n  -- 3. Interpolation APRES alignement --\n");
	double aligne[5];
	for (int k = 0; k < 5; ++k) {
		NkVector<NkTensor> Wm, bm;
		for (int32 l = 0; l < A.NbCouches(); ++l) {
			Wm.PushBack(Lerp(A.W[(usize)l], Wp[(usize)l], kLambda[k]));
			bm.PushBack(Lerp(A.b[(usize)l], bp[(usize)l], kLambda[k]));
		}
		aligne[k] = Evaluer(tailles, Wm, bm, test);
		printf("     lambda = %.2f : %.2f%%\n", (double)kLambda[k], aligne[k] * 100.0);
	}

	// LA BARRIERE = de combien le milieu du chemin tombe sous la droite qui
	// joint les deux extremites.
	R.accExtremites = (naif[0] + naif[4]) * 0.5;
	R.accMilieuNaif = naif[2];
	R.accMilieuAligne = aligne[2];
	R.barriereNaive = R.accExtremites - naif[2];
	R.barriereAlignee = R.accExtremites - aligne[2];

	printf("\n  == RESULTAT ==\n");
	printf("     exactitude moyenne des extremites : %.2f%%\n", R.accExtremites * 100.0);
	printf("     barriere SANS alignement  : %.2f points (milieu %.2f%%)\n", R.barriereNaive * 100.0,
		   naif[2] * 100.0);
	printf("     barriere APRES alignement : %.2f points (milieu %.2f%%)\n", R.barriereAlignee * 100.0,
		   aligne[2] * 100.0);
	if (R.barriereNaive > 0.0001)
		printf("     la permutation retire %.1f%% de la barriere\n",
			   (1.0 - R.barriereAlignee / R.barriereNaive) * 100.0);

	Verdict(R.permExacte, "permuter ne change pas la fonction de B (toutes couches)");
	Verdict(R.barriereAlignee < R.barriereNaive, "l'alignement abaisse la barriere");
	return R;
}

int main(int argc, char **argv) {
	// Cette experience dure des dizaines de minutes en CPU. Rediriger sa sortie
	// vers un fichier la rend, par defaut, entierement tamponnee : on ne voit
	// RIEN avant la fin, et un run qui piétine est indiscernable d'un run qui
	// avance. On desactive donc le tampon.
	setvbuf(stdout, nullptr, _IONBF, 0);

	for (int i = 1; i < argc; ++i)
		if (strcmp(argv[i], "--gpu") == 0)
			gUseGpu = true;

	printf("=== NKRebasinTest — aligner deux reseaux par permutation ===\n\n");
	printf("  Question : deux reseaux entraines SEPAREMENT peuvent-ils etre combines ?\n");
	printf("  Hypothese : ils occupent le meme creux, a une PERMUTATION pres.\n\n");

	// Par defaut on ne TOUCHE PAS au GPU : une seule carte, et un entrainement
	// long peut l'occuper. Creer un second device Vulkan ne renverrait aucune
	// erreur — il deborderait simplement en memoire systeme et rendrait n'importe
	// quoi. Mieux vaut du CPU lent qu'un resultat faux.
	if (gUseGpu) {
		NkTensorGpu &gpu = NkTensorGpu::Get();
		gUseGpu = gpu.IsAvailable();
		printf("  GPU compute : %s (%s)\n", gUseGpu ? "OUI" : "NON", gpu.BackendName());
	} else {
		printf("  GPU compute : NON (volontaire — le GPU reste libre pour un entrainement).\n");
		printf("                Passer --gpu pour le forcer.\n");
	}

	const char *env = getenv("NK_MNIST_DIR");
	const char *base = (env && *env) ? env : "D:/Projets/2026/Nkentseu/Nkentseu/Datasets/mnist";
	char img[1024], lbl[1024];
	snprintf(img, sizeof(img), "%s/train-images-idx3-ubyte", base);
	snprintf(lbl, sizeof(lbl), "%s/train-labels-idx1-ubyte", base);
	data::NkDataset mnist = data::LoadMnist(img, lbl);
	if (!mnist.IsValid()) {
		printf("  MNIST introuvable dans %s. Abandon.\n", base);
		return 2;
	}
	char timg[1024], tlbl[1024];
	snprintf(timg, sizeof(timg), "%s/t10k-images-idx3-ubyte", base);
	snprintf(tlbl, sizeof(tlbl), "%s/t10k-labels-idx1-ubyte", base);
	data::NkDataset test = data::LoadMnist(timg, tlbl);
	if (!test.IsValid()) {
		printf("  Jeu de test introuvable. Abandon : la barriere se mesure sur des\n"
			   "  donnees jamais vues, sinon elle ne dit rien.\n");
		return 2;
	}
	printf("  MNIST : %u exemples d'entrainement, %u de test\n", mnist.Size(), test.Size());

	const int epoques = 3;
	NkVector<Resultat> resultats;
	NkVector<NkString> noms;

	{
		NkVector<int32> t;
		t.PushBack(784);
		t.PushBack(256);
		t.PushBack(10);
		resultats.PushBack(Experience(t, mnist, test, epoques));
		noms.PushBack(NkString("784-256-10       (1 cachee, exactement soluble)"));
	}
	{
		NkVector<int32> t;
		t.PushBack(784);
		t.PushBack(256);
		t.PushBack(256);
		t.PushBack(10);
		resultats.PushBack(Experience(t, mnist, test, epoques));
		noms.PushBack(NkString("784-256-256-10   (2 cachees, descente)"));
	}
	{
		NkVector<int32> t;
		t.PushBack(784);
		t.PushBack(256);
		t.PushBack(256);
		t.PushBack(256);
		t.PushBack(10);
		resultats.PushBack(Experience(t, mnist, test, epoques));
		noms.PushBack(NkString("784-256x3-10     (3 cachees, descente)"));
	}

	printf("\n\n=================================================================\n");
	printf("  SYNTHESE — barriere d'interpolation, en points d'exactitude\n");
	printf("=================================================================\n");
	printf("  %-46s %9s %9s %8s\n", "architecture", "sans", "apres", "retire");
	for (usize i = 0; i < resultats.Size(); ++i) {
		const Resultat &R = resultats[i];
		const double retire = (R.barriereNaive > 0.0001) ? (1.0 - R.barriereAlignee / R.barriereNaive) * 100.0 : 0.0;
		printf("  %-46s %8.2f %9.2f %7.1f%%  (%d balayages)\n", noms[i].CStr(), R.barriereNaive * 100.0,
			   R.barriereAlignee * 100.0, retire, R.balayages);
	}

	printf("\n  CE QUE CES CHIFFRES DISENT, ET CE QU'ILS NE DISENT PAS.\n");
	printf("  Avec une couche cachee, l'alignement est OPTIMAL : le chiffre est la verite.\n");
	printf("  Au-dela, la descente par coordonnees ne rend qu'un optimum LOCAL, dependant\n");
	printf("  du point de depart (ici l'identite) : un meilleur alignement peut exister.\n");
	printf("  Et rien de tout ceci ne porte encore sur un transformeur — une tete\n");
	printf("  d'attention ne se permute pas unite par unite, elle est indivisible.\n");

	printf("\n  Resultat : %d OK, %d echecs\n", gPass, gFail);
	if (gUseGpu)
		NkTensorGpu::Get().Shutdown();
	return gFail == 0 ? 0 : 1;
}
