// =============================================================================
// NKFp16Test — preuve CPU de l'infrastructure mixed precision (NKAI).
//
// Portée VOLONTAIRE de cette passe (cf Kernel/AI/ROADMAP.md « Restes : mixed
// precision FP16 ») : le GPU (RTX 3070) est OCCUPÉ par un entraînement en cours
// (Palier 6, bridé 1100 MHz) -> AUCUNE exécution GPU ici. Cette app prouve,
// CPU UNIQUEMENT, par assertions réelles (pas de simulation) :
//   (a) le dtype NK_F16 (NkFp16, cf NKTensor/NkFp16.h) : conversions f32<->f16
//       logicielles (round-to-nearest-even, dénormaux, Inf/NaN), intégration
//       tenseur (création/indexation/ops élémentaires via NK_DTYPE_DISPATCH).
//   (b) le loss scaling dynamique (NkGradScaler, NKOptim) sur le chemin
//       autograd CPU EXISTANT (F32) : Scale/Backward/Unscale, backoff sur
//       overflow (Inf/NaN détecté), croissance périodique de l'échelle,
//       équivalence numérique scale/unscale = pas de scaling.
// Zéro STL / NKMemory / NKLogger dans CE fichier (convention des apps de test
// NKAI, cf NKAutogradTest/NKNNTest) : uniquement <cstdio>/<cmath>/<cstring> +
// les bibliothèques NKAI (qui, elles, utilisent NKMemory en interne).
//
// La validation GPU (kernels NkSL half-packed, cf ROADMAP) se fera PLUS TARD,
// entre deux paliers d'entraînement — jamais pendant un run actif.
// =============================================================================
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"
#include "NKTensor/NkDType.h"
#include "NKTensor/NkFp16.h"
#include "NKAutograd/NkVar.h"
#include "NKOptim/NkOptim.h"
#include "NKOptim/NkGradScaler.h"

#include <cstdio>
#include <cmath>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::ai;

static int g_pass = 0, g_fail = 0;

static void Check(const char *name, bool cond, const char *detail = nullptr) {
	(cond ? g_pass : g_fail)++;
	if (detail)
		printf("  [ %s ] %-40s  %s\n", cond ? "OK" : "KO", name, detail);
	else
		printf("  [ %s ] %s\n", cond ? "OK" : "KO", name);
}

// =============================================================================
// (a) dtype f16 — conversions bas niveau
// =============================================================================

static void TestFp16Conversions() {
	printf("\n-- (a.1) NkFp16 : conversions f32<->f16 (bits), cas connus --\n");

	// Constantes IEEE-754 binary16 bien connues (Wikipedia « Half-precision
	// floating-point format », vérifiable indépendamment).
	Check("1.0f -> 0x3C00", NkF32ToF16Bits(1.0f) == 0x3C00u);
	Check("-1.0f -> 0xBC00", NkF32ToF16Bits(-1.0f) == 0xBC00u);
	Check("2.0f -> 0x4000", NkF32ToF16Bits(2.0f) == 0x4000u);
	Check("0.5f -> 0x3800", NkF32ToF16Bits(0.5f) == 0x3800u);
	Check("0.0f -> 0x0000", NkF32ToF16Bits(0.0f) == 0x0000u);
	Check("-0.0f -> 0x8000", NkF32ToF16Bits(-0.0f) == 0x8000u);
	Check("max normal 65504.0f -> 0x7BFF", NkF32ToF16Bits(65504.0f) == 0x7BFFu);
	Check("plus petit normal 2^-14 -> 0x0400",
		  NkF32ToF16Bits(6.103515625e-05f) == 0x0400u); // 2^-14 exact en float32
	Check("plus petit dénormal 2^-24 -> 0x0001",
		  NkF32ToF16Bits(5.960464477539063e-08f) == 0x0001u); // 2^-24 exact en float32
	Check("plus grand dénormal (1023/1024)*2^-14 -> 0x03FF",
		  NkF32ToF16Bits(6.097555160522461e-05f) == 0x03FFu);

	printf("\n-- (a.2) Inf / NaN --\n");
	volatile float zero = 0.0f;
	const float posInf = 1.0f / zero;
	const float negInf = -1.0f / zero;
	const float qnan = zero / zero;
	Check("+Inf -> bits 0x7C00", NkF32ToF16Bits(posInf) == 0x7C00u);
	Check("-Inf -> bits 0xFC00", NkF32ToF16Bits(negInf) == 0xFC00u);
	Check("NaN -> exposant plein + mantisse != 0", NkFp16IsNan(NkFp16::FromBits(NkF32ToF16Bits(qnan))));
	Check("std::isinf(f16->f32(+Inf))", std::isinf(NkF16BitsToF32(0x7C00u)) && NkF16BitsToF32(0x7C00u) > 0.0f);
	Check("std::isinf(f16->f32(-Inf))", std::isinf(NkF16BitsToF32(0xFC00u)) && NkF16BitsToF32(0xFC00u) < 0.0f);
	Check("std::isnan(f16->f32(NaN))", std::isnan(NkF16BitsToF32(0x7E00u)));

	printf("\n-- (a.3) Dépassement / soupassement (arrondi correct aux bornes) --\n");
	Check("1e30f (dépassement massif) -> Inf", NkF32ToF16Bits(1e30f) == 0x7C00u);
	Check("-1e30f (dépassement massif) -> -Inf", NkF32ToF16Bits(-1e30f) == 0xFC00u);
	Check("1e-30f (soupassement massif) -> 0", NkF32ToF16Bits(1e-30f) == 0x0000u);
	Check("-1e-30f (soupassement massif) -> -0", NkF32ToF16Bits(-1e-30f) == 0x8000u);
	// Bornes IEEE-754 half bien connues : [65504,65520) arrondit vers 65504 ;
	// [65520,+inf) arrondit vers +Inf (65520 = 65504 + moitié du dernier pas 32).
	Check("65519.0f -> 65504 (0x7BFF), pas Inf", NkF32ToF16Bits(65519.0f) == 0x7BFFu);
	Check("65520.0f -> Inf (0x7C00) (bascule exacte)", NkF32ToF16Bits(65520.0f) == 0x7C00u);

	printf("\n-- (a.4) Arrondi au plus proche PAIR (round-to-nearest-even), cas limites exacts --\n");
	// Pile à mi-chemin entre 1.0 (0x3C00, pair) et 1.0009765625 (0x3C01, impair)
	// -> doit arrondir vers le PAIR (0x3C00).
	Check("mi-chemin(1.0, +1ulp) -> arrondi PAIR 0x3C00", NkF32ToF16Bits(1.00048828125f) == 0x3C00u);
	// Pile à mi-chemin entre 0x3C01 (impair) et 0x3C02 (pair) -> doit arrondir
	// vers le PAIR (0x3C02), donc CETTE FOIS vers le haut.
	Check("mi-chemin(+1ulp, +2ulp) -> arrondi PAIR 0x3C02", NkF32ToF16Bits(1.00146484375f) == 0x3C02u);
	// Tie exactement à la frontière dénormal/zéro (2^-25 = moitié du plus petit
	// dénormal) : le pair est 0 (mantisse 0 = paire) -> doit arrondir à 0.
	Check("mi-chemin(0, plus petit dénormal) -> arrondi PAIR 0", NkF32ToF16Bits(2.9802322387695312e-08f) == 0x0000u);

	printf("\n-- (a.5) Round-trip EXACT des dénormaux (f16 dénormal -> f32 -> f16 identique) --\n");
	// Tout binary16 dénormal est représentable SANS perte en binary32 (23 bits
	// de mantisse > 10) : la conversion RETOUR doit reproduire les MÊMES bits,
	// sans dépendre de l'algorithme d'arrondi (c'est un aller-retour lossless).
	bool allDenormExact = true;
	for (uint32 b = 1; b < 1024; ++b) { // 1..1023 : tous les dénormaux positifs
		const float f = NkF16BitsToF32((uint16)b);
		const uint16 back = NkF32ToF16Bits(f);
		if (back != (uint16)b) {
			allDenormExact = false;
			printf("      écart : bits=0x%04X -> f32=%.10g -> bits=0x%04X\n", b, (double)f, back);
			break;
		}
	}
	Check("1..1023 dénormaux : round-trip bit-exact", allDenormExact);

	printf("\n-- (a.6) Round-trip EXACT des normaux + spéciaux (échantillon large) --\n");
	bool allNormExact = true;
	int checked = 0;
	for (uint32 b = 0; b < 0x7C00u; b += 3) { // normaux positifs (évite Inf/NaN >= 0x7C00), pas 3 = échantillon dense
		const float f = NkF16BitsToF32((uint16)b);
		const uint16 back = NkF32ToF16Bits(f);
		++checked;
		if (back != (uint16)b) {
			allNormExact = false;
			printf("      écart : bits=0x%04X -> f32=%.10g -> bits=0x%04X\n", b, (double)f, back);
			break;
		}
	}
	char detail[64];
	snprintf(detail, sizeof(detail), "(%d valeurs testées)", checked);
	Check("0x0000..0x7BFF (normaux+dénormaux+0) : round-trip bit-exact", allNormExact, detail);

	printf("\n-- (a.7) Précision relative (erreur round-trip <= epsilon demi = 2^-10) --\n");
	// Toutes DANS la plage représentable f16 (max normal = 65504) : au-delà, l'erreur
	// RELATIVE n'a plus de sens (la valeur déborde vers Inf par construction, cf a.3).
	const float samples[] = {3.14159265f, 2.71828182f, 0.1f, 123.456f, -9.8765f, 1.0f / 3.0f, 15000.0f};
	const double halfEps = 1.0 / 1024.0; // 2^-10, résolution relative de la mantisse 10 bits
	bool precOk = true;
	for (float s : samples) {
		const float back = NkF16BitsToF32(NkF32ToF16Bits(s));
		const double rel = (s != 0.0f) ? std::fabs(((double)back - (double)s) / (double)s) : std::fabs((double)back);
		// Tolérance = 1 ULP demi-précision + marge (2x) : borne large mais significative,
		// prouve l'ordre de grandeur de précision attendu d'un vrai f16 (pas juste "proche").
		if (rel > halfEps * 2.0) {
			precOk = false;
			printf("      écart relatif trop grand : %.8g -> %.8g (rel=%.6g)\n", (double)s, (double)back, rel);
		}
	}
	Check("échantillon réel : erreur relative <= 2*2^-10", precOk);

	printf("\n-- (a.8) Monotonie (aucune valeur ne « recule » en montant en f32) --\n");
	bool monotone = true;
	float prevF = -100.0f;
	uint16 prevBits = NkF32ToF16Bits(prevF);
	for (int i = 1; i <= 2000; ++i) {
		const float f = -100.0f + (float)i * 0.1f; // -100 .. +100
		const uint16 bits = NkF32ToF16Bits(f);
		const float back = NkF16BitsToF32(bits);
		const float prevBack = NkF16BitsToF32(prevBits);
		if (back < prevBack) {
			monotone = false;
			break;
		}
		prevBits = bits;
		prevF = f;
	}
	(void)prevF;
	Check("séquence croissante -100..100 : f16(x) non décroissant", monotone);
}

// =============================================================================
// (a) dtype f16 — intégration NkTensor (stockage/création/indexation/ops)
// =============================================================================

static void TestFp16Tensor() {
	printf("\n-- (a.9) NkTensor dtype NK_F16 : création / indexation --\n");

	Check("NkDTypeSize(NK_F16) == 2", NkDTypeSize(NkDType::NK_F16) == 2);
	Check("NkDTypeIsFloat(NK_F16)", NkDTypeIsFloat(NkDType::NK_F16));
	Check("NkDTypeName(NK_F16) == \"f16\"", std::strcmp(NkDTypeName(NkDType::NK_F16), "f16") == 0);

	NkTensor t = NkTensor::Zeros(NkShape{4}, NkDType::NK_F16);
	Check("Zeros f16 : valide", t.IsValid());
	Check("Zeros f16 : dtype", t.DType() == NkDType::NK_F16);
	Check("Zeros f16 : GetItem == 0", t.GetItem(NkShape{0}) == 0.0);

	t.SetItem(NkShape{0}, 1.5);
	t.SetItem(NkShape{1}, -2.25);
	t.SetItem(NkShape{2}, 100.0);
	t.SetItem(NkShape{3}, 0.1);
	Check("SetItem/GetItem f16 : 1.5 exact (représentable pile)", t.GetItem(NkShape{0}) == 1.5);
	Check("SetItem/GetItem f16 : -2.25 exact (représentable pile)", t.GetItem(NkShape{1}) == -2.25);
	Check("SetItem/GetItem f16 : 100.0 exact (entier < 2048, représentable pile)", t.GetItem(NkShape{2}) == 100.0);
	Check("SetItem/GetItem f16 : 0.1 proche (non représentable pile, arrondi attendu)",
		  std::fabs(t.GetItem(NkShape{3}) - 0.1) < 0.001);

	NkTensor f = NkTensor::Full(NkShape{3, 2}, 3.0, NkDType::NK_F16);
	bool fullOk = true;
	for (int64 i = 0; i < 3; ++i)
		for (int64 j = 0; j < 2; ++j)
			if (f.GetItem(NkShape{i, j}) != 3.0)
				fullOk = false;
	Check("Full(3.0) f16 : toutes les cases == 3.0", fullOk);

	NkTensor o = NkTensor::Ones(NkShape{5}, NkDType::NK_F16);
	bool onesOk = true;
	for (int64 i = 0; i < 5; ++i)
		if (o.GetItem(NkShape{i}) != 1.0)
			onesOk = false;
	Check("Ones f16 : toutes les cases == 1.0", onesOk);

	// FromData : buffer CPU de NkFp16 construits directement depuis des float.
	NkFp16 raw[4] = {NkFp16(1.0f), NkFp16(2.0f), NkFp16(-3.5f), NkFp16(0.0f)};
	NkTensor fd = NkTensor::FromData(NkShape{4}, raw, NkDType::NK_F16);
	Check("FromData f16 : élément 0", fd.GetItem(NkShape{0}) == 1.0);
	Check("FromData f16 : élément 2", fd.GetItem(NkShape{2}) == -3.5);

	// Clone (copie profonde) + Contiguous doivent préserver le dtype f16.
	NkTensor clone = fd.Clone();
	Check("Clone f16 : dtype préservé", clone.DType() == NkDType::NK_F16);
	Check("Clone f16 : valeurs identiques", clone.GetItem(NkShape{2}) == -3.5);
	Check("Clone f16 : stockage indépendant (RawData != original)", clone.RawData() != fd.RawData());
}

// =============================================================================
// (a) dtype f16 — ops élémentaires CPU de référence (via conversion f32)
// =============================================================================

static void TestFp16Ops() {
	printf("\n-- (a.10) Ops élémentaires CPU sur f16 (référence via float, cf NkFp16.h) --\n");

	float ad[4] = {1.0f, 2.0f, -3.0f, 0.5f};
	float bd[4] = {0.5f, -1.0f, 2.0f, 0.25f};
	NkFp16 ah[4], bh[4];
	for (int i = 0; i < 4; ++i) {
		ah[i] = NkFp16(ad[i]);
		bh[i] = NkFp16(bd[i]);
	}
	NkTensor A = NkTensor::FromData(NkShape{4}, ah, NkDType::NK_F16);
	NkTensor B = NkTensor::FromData(NkShape{4}, bh, NkDType::NK_F16);

	NkTensor add = ops::Add(A, B);
	NkTensor sub = ops::Sub(A, B);
	NkTensor mul = ops::Mul(A, B);
	NkTensor rel = ops::Relu(A);
	Check("Add f16 : dtype préservé", add.DType() == NkDType::NK_F16);

	bool addOk = true, subOk = true, mulOk = true, reluOk = true;
	for (int i = 0; i < 4; ++i) {
		if (std::fabs(add.GetItem(NkShape{i}) - (double)(ad[i] + bd[i])) > 0.01)
			addOk = false;
		if (std::fabs(sub.GetItem(NkShape{i}) - (double)(ad[i] - bd[i])) > 0.01)
			subOk = false;
		if (std::fabs(mul.GetItem(NkShape{i}) - (double)(ad[i] * bd[i])) > 0.01)
			mulOk = false;
		const double expectedRelu = ad[i] > 0.0f ? ad[i] : 0.0f;
		if (std::fabs(rel.GetItem(NkShape{i}) - expectedRelu) > 0.01)
			reluOk = false;
	}
	Check("Add f16 vs référence float (tol 1e-2)", addOk);
	Check("Sub f16 vs référence float (tol 1e-2)", subOk);
	Check("Mul f16 vs référence float (tol 1e-2)", mulOk);
	Check("Relu f16 vs référence float (tol 1e-2)", reluOk);

	NkFp16 sigIn[3] = {NkFp16(-2.f), NkFp16(0.f), NkFp16(2.f)};
	NkTensor sig = ops::Sigmoid(NkTensor::FromData(NkShape{3}, sigIn, NkDType::NK_F16));
	const double s0 = 1.0 / (1.0 + std::exp(2.0));
	const double s1 = 0.5;
	const double s2 = 1.0 / (1.0 + std::exp(-2.0));
	Check("Sigmoid f16 vs référence (tol 1e-2)", std::fabs(sig.GetItem(NkShape{0}) - s0) < 0.01 &&
													 std::fabs(sig.GetItem(NkShape{1}) - s1) < 0.01 &&
													 std::fabs(sig.GetItem(NkShape{2}) - s2) < 0.01);
}

// =============================================================================
// (b) Loss scaling dynamique — NkGradScaler (chemin autograd CPU F32 existant)
// =============================================================================

static NkTensor Vec1(float v) {
	return NkTensor::FromData(NkShape{1}, &v, NkDType::NK_F32);
}

static void TestGradScalerBasics() {
	printf("\n-- (b.1) NkGradScaler : Scale() multiplie bien la perte --\n");
	optim::NkGradScaler scaler(1024.0f, 2.0f, 0.5f, 2000, 1.0f);
	Check("échelle initiale == 1024", scaler.Scale() == 1024.0f);

	NkVar loss = NkVar::Leaf(Vec1(2.5f), false);
	NkVar scaled = scaler.Scale(loss);
	Check("Scale(2.5) == 2.5*1024 = 2560", std::fabs(scaled.Value().GetItem(NkShape{0}) - 2560.0) < 1e-3);

	printf("\n-- (b.2) HasInfNan : détection directe --\n");
	Check("tenseur fini -> pas d'Inf/NaN", !optim::NkGradScaler::HasInfNan(Vec1(3.14f)));
	volatile float zero = 0.0f;
	NkTensor infT = Vec1(1.0f / zero);
	NkTensor nanT = Vec1(zero / zero);
	Check("tenseur avec +Inf -> détecté", optim::NkGradScaler::HasInfNan(infT));
	Check("tenseur avec NaN -> détecté", optim::NkGradScaler::HasInfNan(nanT));
}

static void TestGradScalerEquivalence() {
	printf("\n-- (b.3) Équivalence numérique : grad(scale puis unscale) == grad(sans scaling) --\n");

	// Graphe A : SANS scaling.
	float wd = 0.8f, bd = 0.1f;
	NkVar wA = NkVar::Leaf(NkTensor::FromData(NkShape{1}, &wd, NkDType::NK_F32), true);
	NkVar bA = NkVar::Leaf(NkTensor::FromData(NkShape{1}, &bd, NkDType::NK_F32), true);
	float xd[4] = {1.0f, 2.0f, 3.0f, -1.0f}, yd[4] = {2.1f, 4.0f, 5.9f, -2.2f};
	NkTensor x4 = NkTensor::FromData(NkShape{4}, xd, NkDType::NK_F32);
	NkTensor y4 = NkTensor::FromData(NkShape{4}, yd, NkDType::NK_F32);

	auto BuildLoss = [&](NkVar &w, NkVar &b) {
		// pred_i = w*x_i + b (Mul/Add autograd CPU exigent mêmes formes -> on matérialise le
		// broadcast manuellement en 4 termes Sum, équivalent mathématique simple et sûr).
		NkVar yv = NkVar::Leaf(y4, false);
		// w/b sont des NkVar scalaires {1} ; Mul/Add (autograd CPU) exigent les mêmes formes
		// -> on construit pred via 4 Mul+Add sur des vues 1-élément de x, puis Concat0 pour
		// recomposer un vecteur [4] SANS jamais quitter l'autograd (pas de broadcast requis).
		NkVar preds[4];
		for (int i = 0; i < 4; ++i) {
			NkVar xi = NkVar::Leaf(x4.Slice(0, i, i + 1).Contiguous(), false);
			preds[i] = autograd::Add(autograd::Mul(w, xi), b);
		}
		NkVar cat01 = autograd::Concat0(preds[0], preds[1]);
		NkVar cat012 = autograd::Concat0(cat01, preds[2]);
		NkVar pred = autograd::Concat0(cat012, preds[3]);
		return autograd::MSE(pred, yv);
	};

	NkVar lossA = BuildLoss(wA, bA);
	lossA.Backward();
	const double gwA = wA.Grad().GetItem(NkShape{0});
	const double gbA = bA.Grad().GetItem(NkShape{0});

	// Graphe B : AVEC scaling (scale=512), puis Unscale().
	float wd2 = 0.8f, bd2 = 0.1f;
	NkVar wB = NkVar::Leaf(NkTensor::FromData(NkShape{1}, &wd2, NkDType::NK_F32), true);
	NkVar bB = NkVar::Leaf(NkTensor::FromData(NkShape{1}, &bd2, NkDType::NK_F32), true);
	optim::NkGradScaler scaler(512.0f, 2.0f, 0.5f, 2000, 1.0f);
	NkVar lossB = BuildLoss(wB, bB);
	NkVar scaledLossB = scaler.Scale(lossB);
	scaledLossB.Backward();
	// AVANT Unscale : le gradient doit être ~512x plus grand (loss scaling brut).
	const double gwB_raw = wB.Grad().GetItem(NkShape{0});
	Check("AVANT Unscale : grad scaled ~= 512 * grad non-scaled",
		  std::fabs(gwB_raw - 512.0 * gwA) < std::fabs(512.0 * gwA) * 1e-3 + 1e-6);

	NkVector<NkVar> params;
	params.PushBack(wB);
	params.PushBack(bB);
	const bool ok = scaler.Unscale(params);
	Check("Unscale() : pas d'overflow -> renvoie true", ok);
	const double gwB = wB.Grad().GetItem(NkShape{0});
	const double gbB = bB.Grad().GetItem(NkShape{0});

	char detail[96];
	snprintf(detail, sizeof(detail), "(sans scaling=%.6f, scale+unscale=%.6f)", gwA, gwB);
	Check("dW : scale(512)+Backward+Unscale == Backward direct", std::fabs(gwA - gwB) < 1e-4, detail);
	snprintf(detail, sizeof(detail), "(sans scaling=%.6f, scale+unscale=%.6f)", gbA, gbB);
	Check("dB : scale(512)+Backward+Unscale == Backward direct", std::fabs(gbA - gbB) < 1e-4, detail);
}

static void TestGradScalerOverflowBackoff() {
	printf("\n-- (b.4) Overflow -> backoff (scale recule), gradients NON modifiés --\n");

	float wd = 1.0f;
	NkVar w = NkVar::Leaf(NkTensor::FromData(NkShape{1}, &wd, NkDType::NK_F32), true);
	// Injecte directement un gradient +Inf dans le nœud (simule un overflow réel de
	// backward en demi-précision, sans dépendre d'une arithmétique float qui déborde
	// réellement -> déterministe, reproductible).
	volatile float zero = 0.0f;
	const float posInf = 1.0f / zero;
	w.Node()->grad = Vec1(posInf);

	optim::NkGradScaler scaler(1024.0f, 2.0f, 0.5f, 2000, 1.0f);
	NkVector<NkVar> params;
	params.PushBack(w);

	const bool ok = scaler.Unscale(params);
	Check("Unscale() avec grad +Inf -> renvoie false", !ok);
	Check("scale après overflow == 1024*0.5 = 512", scaler.Scale() == 512.0f);
	Check("OverflowCount() == 1", scaler.OverflowCount() == 1);
	Check("GoodSteps() remis à 0", scaler.GoodSteps() == 0);
	Check("gradient NON modifié (reste +Inf, l'appelant doit le ZeroGrad lui-même)",
		  std::isinf(w.Node()->grad.GetItem(NkShape{0})));

	// Un second overflow doit reculer ENCORE (0.5 * 512 = 256), plancher minScale=1.
	w.Node()->grad = Vec1(posInf);
	scaler.Unscale(params);
	Check("2e overflow : scale == 256", scaler.Scale() == 256.0f);
	Check("OverflowCount() == 2", scaler.OverflowCount() == 2);

	// NaN doit aussi déclencher l'overflow (pas seulement Inf).
	const float qnan = zero / zero;
	w.Node()->grad = Vec1(qnan);
	const bool okNan = scaler.Unscale(params);
	Check("Unscale() avec grad NaN -> renvoie false aussi", !okNan);
}

static void TestGradScalerGrowth() {
	printf("\n-- (b.5) Croissance périodique de l'échelle après N pas propres --\n");

	float wd = 1.0f;
	NkVar w = NkVar::Leaf(NkTensor::FromData(NkShape{1}, &wd, NkDType::NK_F32), true);
	optim::NkGradScaler scaler(100.0f, 4.0f, 0.5f, /*growthInterval*/ 3, 1.0f);
	NkVector<NkVar> params;
	params.PushBack(w);

	bool allOk = true;
	for (int i = 0; i < 3; ++i) {
		w.Node()->grad = Vec1(0.01f * (float)(i + 1)); // gradient fini, petit
		const bool ok = scaler.Unscale(params);
		if (!ok)
			allOk = false;
	}
	Check("3 pas propres sans overflow -> tous true", allOk);
	Check("après growthInterval=3 pas propres : scale == 100*4 = 400", scaler.Scale() == 400.0f);
	Check("compteur de pas propres remis à 0 après croissance", scaler.GoodSteps() == 0);
}

// Entraînement bout-en-bout MINIMAL (mêmes briques que NKAutogradTest/NKNNTest) :
// prouve que Scale/Backward/Unscale/Step composent correctement avec NkAdam et que
// la perte DESCEND, exactement comme sans scaling (le scaling doit être numériquement
// transparent quand aucun overflow ne survient).
static void TestGradScalerTrainingLoop() {
	printf("\n-- (b.6) Boucle d'entraînement (Adam + GradScaler) : la perte descend --\n");

	float wd = 0.0f, bd = 0.0f;
	NkVar w = NkVar::Leaf(NkTensor::FromData(NkShape{1}, &wd, NkDType::NK_F32), true);
	NkVar b = NkVar::Leaf(NkTensor::FromData(NkShape{1}, &bd, NkDType::NK_F32), true);
	float xd[4] = {1.0f, 2.0f, 3.0f, -1.0f}, yd[4] = {2.0f, 4.0f, 6.0f, -2.0f}; // y = 2x exact
	NkTensor x4 = NkTensor::FromData(NkShape{4}, xd, NkDType::NK_F32);
	NkTensor y4 = NkTensor::FromData(NkShape{4}, yd, NkDType::NK_F32);

	NkVector<NkVar> params;
	params.PushBack(w);
	params.PushBack(b);
	optim::NkAdam opt(params, 0.1f);
	optim::NkGradScaler scaler(256.0f, 2.0f, 0.5f, 2000, 1.0f);

	double firstLoss = -1.0, lastLoss = -1.0;
	for (int step = 0; step < 50; ++step) {
		opt.ZeroGrad();
		NkVar xv = NkVar::Leaf(x4, false);
		NkVar yv = NkVar::Leaf(y4, false);
		NkVar preds[4];
		for (int i = 0; i < 4; ++i) {
			NkVar xi = NkVar::Leaf(x4.Slice(0, i, i + 1).Contiguous(), false);
			preds[i] = autograd::Add(autograd::Mul(w, xi), b);
		}
		NkVar cat01 = autograd::Concat0(preds[0], preds[1]);
		NkVar cat012 = autograd::Concat0(cat01, preds[2]);
		NkVar pred = autograd::Concat0(cat012, preds[3]);
		NkVar loss = autograd::MSE(pred, yv);
		(void)xv;

		const double lv = loss.Value().GetItem(NkShape{0});
		if (step == 0)
			firstLoss = lv;
		lastLoss = lv;

		NkVar scaledLoss = scaler.Scale(loss);
		scaledLoss.Backward();
		const bool stepOk = scaler.Unscale(params);
		if (stepOk)
			opt.Step();
		// (si overflow : on saute Step(), comme AMP standard — n'arrive pas ici, gradients petits)
	}

	printf("      perte : %.6f -> %.6f (échelle finale = %.1f)\n", firstLoss, lastLoss, scaler.Scale());
	Check("la perte DESCEND avec Adam+GradScaler (comme sans scaling)", lastLoss < firstLoss * 0.5);
	Check("w converge vers ~2.0", std::fabs(w.Value().GetItem(NkShape{0}) - 2.0) < 0.3);
	Check("aucun overflow sur ce run (gradients petits, well-scaled)", scaler.OverflowCount() == 0);
}

// =============================================================================
int main() {
	printf("=== NKFp16Test : mixed precision CPU-only (GPU occupé par un entraînement) ===\n");
	printf("    (a) dtype f16 (NkFp16)  (b) loss scaling dynamique (NkGradScaler)\n");

	TestFp16Conversions();
	TestFp16Tensor();
	TestFp16Ops();
	TestGradScalerBasics();
	TestGradScalerEquivalence();
	TestGradScalerOverflowBackoff();
	TestGradScalerGrowth();
	TestGradScalerTrainingLoop();

	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
