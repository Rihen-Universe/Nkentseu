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
//   3. chercher la permutation des unites cachees qui aligne B sur A ;
//   4. interpoler apres alignement -> la barriere doit S'EFFONDRER.
//
// POURQUOI CE RESEAU-LA. Avec UNE seule couche cachee, le probleme est
// EXACTEMENT soluble : il n'y a qu'une permutation a trouver, et c'est un
// probleme d'affectation lineaire que la methode hongroise resout de facon
// optimale -- aucune heuristique, aucune boucle de coordination. On mesure donc
// la VERITE sur ce cas, pas une approximation. C'est le socle a partir duquel
// on saura si la piste vaut d'etre poussee vers les transformeurs, ou l'ordre
// des tetes d'attention, les normalisations et les connexions residuelles
// rendent le probleme ouvert.
//
// DISPOSITION DES POIDS (NkDense) : W est [in, out], b est [1, out]. Permuter
// l'unite cachee j revient donc a permuter la COLONNE j de W1, l'entree j de
// b1, et la LIGNE j de W2.
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
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static const int32 kIn = 784;
static const int32 kHidden = 256;
static const int32 kOut = 10;

// ---------------------------------------------------------------------------
// METHODE HONGROISE (Kuhn-Munkres), variante par chemins augmentants en O(n^3).
//
// Elle rend l'affectation de COUT MINIMAL — donc, en lui passant l'oppose des
// similarites, l'appariement de similarite MAXIMALE. On ne prend pas un
// glouton : deux unites peuvent se disputer le meme partenaire, et le glouton
// se trompe alors sur toute la suite. Ici l'optimum est garanti, ce qui est
// indispensable — sinon un mauvais alignement ferait conclure a tort que la
// methode ne marche pas.
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
				const float64 cur =
					cost[(usize)((i0 - 1) * n + (j - 1))] - u[(usize)i0] - v[(usize)j];
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

int main() {
	printf("=== NKRebasinTest — aligner deux reseaux par permutation ===\n\n");
	printf("  Question : deux reseaux entraines SEPAREMENT peuvent-ils etre combines ?\n");
	printf("  Hypothese : ils occupent le meme creux, a une PERMUTATION pres.\n\n");

	NkTensorGpu &gpu = NkTensorGpu::Get();
	const bool useGpu = gpu.IsAvailable();
	printf("  GPU compute : %s (%s)\n", useGpu ? "OUI" : "NON", gpu.BackendName());

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
	printf("  MNIST : %u exemples d'entrainement, %u de test\n\n", mnist.Size(), test.Size());

	// ---- Entrainer DEUX modeles depuis des graines differentes -------------
	// Les graines different sur les DEUX couches ET sur le melange des lots :
	// deux modeles qui ne different que par l'initialisation restent trop
	// proches, et la barriere serait faible pour de mauvaises raisons.
	struct Model {
			nn::NkDense m1, m2;
			double accTest = 0.0;
	};
	Model A{nn::NkDense(kIn, kHidden, 11u), nn::NkDense(kHidden, kOut, 22u), 0.0};
	Model B{nn::NkDense(kIn, kHidden, 7717u), nn::NkDense(kHidden, kOut, 9931u), 0.0};

	auto trainOne = [&](Model &M, uint32 shuffleSeed, const char *nom) {
		data::NkDataLoader loader(mnist, 128, true, shuffleSeed);
		NkVector<NkVar> params;
		M.m1.Parameters(params);
		M.m2.Parameters(params);
		if (useGpu)
			for (uint32 i = 0; i < params.Size(); ++i)
				params[i].SetValue(params[i].Value().ToGPU());
		auto fwd = [&](const NkVar &x) {
			NkVar xg = useGpu ? NkVar::Leaf(x.Value().ToGPU(), false) : x;
			return M.m2.Forward(nn::Relu(M.m1.Forward(xg)));
		};
		optim::NkAdam adam(params, 0.001f);
		for (int e = 1; e <= 3; ++e) {
			train::EpochStats st = train::TrainEpoch(fwd, adam, loader);
			printf("    %s epoque %d : perte %.4f  exactitude %.2f%%\n", nom, e, st.loss,
				   st.acc * 100.0);
		}
		data::NkDataLoader tl(test, 128, false, 1u);
		M.accTest = train::Accuracy(fwd, tl);
		printf("    %s : TEST = %.2f%%\n\n", nom, M.accTest * 100.0);
	};

	printf("  -- Entrainement de deux reseaux INDEPENDANTS --\n");
	trainOne(A, 3u, "A");
	trainOne(B, 8081u, "B");

	// ---- Evaluer un jeu de poids arbitraire --------------------------------
	// On construit un modele jetable et on y INJECTE les poids a tester : c'est
	// la seule facon de mesurer un point du chemin d'interpolation, qui n'est
	// le resultat d'aucun entrainement.
	NkTensor w1A = ToCpu(A.m1.Weight()), b1A = ToCpu(A.m1.Bias());
	NkTensor w2A = ToCpu(A.m2.Weight()), b2A = ToCpu(A.m2.Bias());
	NkTensor w1B = ToCpu(B.m1.Weight()), b1B = ToCpu(B.m1.Bias());
	NkTensor w2B = ToCpu(B.m2.Weight()), b2B = ToCpu(B.m2.Bias());

	auto evalMix = [&](const NkTensor &w1, const NkTensor &b1, const NkTensor &w2,
					   const NkTensor &b2) -> double {
		nn::NkDense e1(kIn, kHidden, 1u), e2(kHidden, kOut, 1u);
		NkVector<NkVar> ps;
		e1.Parameters(ps);
		e2.Parameters(ps);
		// L'ORDRE de Parameters() est (W, b) par couche — le meme que celui
		// dans lequel on les a lus. S'y fier est le seul lien entre les deux.
		ps[0].SetValue(useGpu ? w1.ToGPU() : w1);
		ps[1].SetValue(useGpu ? b1.ToGPU() : b1);
		ps[2].SetValue(useGpu ? w2.ToGPU() : w2);
		ps[3].SetValue(useGpu ? b2.ToGPU() : b2);
		auto fwd = [&](const NkVar &x) {
			NkVar xg = useGpu ? NkVar::Leaf(x.Value().ToGPU(), false) : x;
			return e2.Forward(nn::Relu(e1.Forward(xg)));
		};
		data::NkDataLoader tl(test, 128, false, 1u);
		return train::Accuracy(fwd, tl);
	};

	auto lerp = [](const NkTensor &x, const NkTensor &y, float32 t) {
		NkTensor o = NkTensor::Zeros(x.Shape());
		const float *a = x.DataAs<float>();
		const float *b = y.DataAs<float>();
		float *d = o.DataAs<float>();
		const int64 n = NkShapeNumel(x.Shape());
		for (int64 i = 0; i < n; ++i)
			d[i] = a[i] * (1.f - t) + b[i] * t;
		return o;
	};

	static const float32 kLambda[5] = {0.f, 0.25f, 0.5f, 0.75f, 1.f};

	printf("  -- 1. Interpolation NAIVE (sans alignement) --\n");
	double naive[5];
	for (int k = 0; k < 5; ++k) {
		const float32 t = kLambda[k];
		naive[k] = evalMix(lerp(w1A, w1B, t), lerp(b1A, b1B, t), lerp(w2A, w2B, t),
						   lerp(b2A, b2B, t));
		printf("     lambda = %.2f : %.2f%%\n", (double)t, naive[k] * 100.0);
	}

	// ---- Chercher la permutation ------------------------------------------
	// COUT D'APPARIER l'unite i de A avec l'unite j de B : la SIMILARITE de
	// tout ce qui touche cette unite -- ses poids d'entree (colonne de W1), son
	// biais, et ses poids de sortie (ligne de W2). Maximiser la somme de ces
	// produits scalaires, c'est exactement maximiser le produit scalaire des
	// deux jeux de poids une fois B permute, donc rendre l'interpolation aussi
	// droite que possible.
	printf("\n  -- 2. Recherche de la permutation des %d unites cachees --\n", kHidden);
	NkVector<float64> cost;
	cost.Resize((usize)kHidden * (usize)kHidden);
	{
		const float *pw1A = w1A.DataAs<float>();
		const float *pw1B = w1B.DataAs<float>();
		const float *pb1A = b1A.DataAs<float>();
		const float *pb1B = b1B.DataAs<float>();
		const float *pw2A = w2A.DataAs<float>();
		const float *pw2B = w2B.DataAs<float>();
		for (int32 i = 0; i < kHidden; ++i) {
			for (int32 j = 0; j < kHidden; ++j) {
				float64 s = 0.0;
				// W1 est [in, hidden] : l'unite occupe une COLONNE.
				for (int32 r = 0; r < kIn; ++r)
					s += (float64)pw1A[(int64)r * kHidden + i] *
						 (float64)pw1B[(int64)r * kHidden + j];
				s += (float64)pb1A[i] * (float64)pb1B[j];
				// W2 est [hidden, out] : l'unite occupe une LIGNE.
				for (int32 c = 0; c < kOut; ++c)
					s += (float64)pw2A[(int64)i * kOut + c] * (float64)pw2B[(int64)j * kOut + c];
				// Cout = OPPOSE de la similarite : la methode minimise.
				cost[(usize)((int64)i * kHidden + j)] = -s;
			}
		}
	}
	NkVector<int32> perm;
	HungarianMinCost(cost, kHidden, perm);
	// Combien d'unites etaient DEJA a leur place ? Si beaucoup l'etaient, les
	// deux reseaux seraient trop semblables et l'experience ne prouverait rien.
	int32 fixed = 0;
	for (int32 i = 0; i < kHidden; ++i)
		if (perm[(usize)i] == i)
			++fixed;
	printf("     permutation trouvee (optimale) — %d unites sur %d etaient deja alignees\n", fixed,
		   kHidden);

	// ---- Appliquer la permutation a B --------------------------------------
	NkTensor w1Bp = NkTensor::Zeros(w1B.Shape());
	NkTensor b1Bp = NkTensor::Zeros(b1B.Shape());
	NkTensor w2Bp = NkTensor::Zeros(w2B.Shape());
	{
		const float *sw1 = w1B.DataAs<float>();
		const float *sb1 = b1B.DataAs<float>();
		const float *sw2 = w2B.DataAs<float>();
		float *dw1 = w1Bp.DataAs<float>();
		float *db1 = b1Bp.DataAs<float>();
		float *dw2 = w2Bp.DataAs<float>();
		for (int32 i = 0; i < kHidden; ++i) {
			const int32 j = perm[(usize)i]; // l'unite i de A correspond a j de B
			for (int32 r = 0; r < kIn; ++r)
				dw1[(int64)r * kHidden + i] = sw1[(int64)r * kHidden + j];
			db1[i] = sb1[j];
			for (int32 c = 0; c < kOut; ++c)
				dw2[(int64)i * kOut + c] = sw2[(int64)j * kOut + c];
		}
	}

	// VERIFICATION INDISPENSABLE : permuter ne doit RIEN changer a la fonction.
	// Si l'exactitude de B bouge apres permutation, c'est que l'application est
	// fausse — et tout le reste de la mesure ne voudrait plus rien dire.
	const double accBperm = evalMix(w1Bp, b1Bp, w2Bp, b2B);
	printf("     B permute : %.2f%%  (B d'origine : %.2f%%) — l'ecart doit etre NUL\n",
		   accBperm * 100.0, B.accTest * 100.0);

	printf("\n  -- 3. Interpolation APRES alignement --\n");
	double aligned[5];
	for (int k = 0; k < 5; ++k) {
		const float32 t = kLambda[k];
		aligned[k] = evalMix(lerp(w1A, w1Bp, t), lerp(b1A, b1Bp, t), lerp(w2A, w2Bp, t),
							 lerp(b2A, b2B, t));
		printf("     lambda = %.2f : %.2f%%\n", (double)t, aligned[k] * 100.0);
	}

	// ---- Verdict ------------------------------------------------------------
	// LA BARRIERE = de combien le milieu du chemin tombe sous la droite qui
	// joint les deux extremites. C'est LE chiffre : une barriere nulle veut
	// dire qu'il existe un chemin droit, sans vallee, entre les deux reseaux.
	const double endAvg = (naive[0] + naive[4]) * 0.5;
	const double barrNaive = endAvg - naive[2];
	const double barrAlign = endAvg - aligned[2];
	printf("\n  == RESULTAT ==\n");
	printf("     exactitude moyenne des extremites : %.2f%%\n", endAvg * 100.0);
	printf("     barriere SANS alignement : %.2f points\n", barrNaive * 100.0);
	printf("     barriere APRES alignement : %.2f points\n", barrAlign * 100.0);
	if (barrNaive > 0.0001)
		printf("     la permutation retire %.1f%% de la barriere\n",
			   (1.0 - barrAlign / barrNaive) * 100.0);

	const bool permIsExact = std::fabs(accBperm - B.accTest) < 1e-9;
	const bool lowered = barrAlign < barrNaive;
	printf("\n  [%s] permuter ne change pas la fonction de B\n", permIsExact ? " OK " : "FAIL");
	printf("  [%s] l'alignement abaisse la barriere\n", lowered ? " OK " : "FAIL");
	printf("\n  Ce que ce chiffre DIT, et ce qu'il ne dit pas : il porte sur UNE couche\n"
		   "  cachee, ou la permutation est exactement soluble. Rien ici ne prouve que\n"
		   "  la meme chose tiendra sur un transformeur -- c'est la question ouverte.\n");

	gpu.Shutdown();
	return (permIsExact && lowered) ? 0 : 1;
}
