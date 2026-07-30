// =============================================================================
// NKDataTest — vérifie NKData : Dataset synthétique -> DataLoader (shuffle,
// batchs, one-hot), couverture complète, puis chargeur MNIST si disponible
// (variable d'environnement NK_MNIST_DIR).
// =============================================================================
#include "NKData/NkData.h"
#include "NKData/NkTokenizer.h"
#include "NKData/NkSequence.h"
#include "NKData/NkAugment.h"
#include "NKTensor/NkTensor.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::ai;

static int g_pass = 0, g_fail = 0;

static void Check(bool ok, const char *name) {
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %s\n", ok ? "OK" : "KO", name);
}

int main() {
	printf("=== NKDataTest : Dataset + DataLoader (+ MNIST si dispo) ===\n\n");

	// Jeu synthétique : 3 classes x 20 = 60 exemples 2D.
	const uint32 NC = 3, PER = 20, N = NC * PER;
	data::NkDataset ds = data::MakeBlobs(NC, PER, 123u);
	Check(ds.IsValid() && ds.Size() == N && ds.FeatureDim() == 2 && ds.NumClasses() == NC,
		  "Dataset synthétique (60 exemples, 2D, 3 classes)");

	// Histogramme attendu (par classe) depuis le dataset.
	int expected[16] = {0};
	for (uint32 i = 0; i < ds.Size(); ++i)
		expected[ds.Labels()[i]]++;

	// DataLoader : batch=16, shuffle -> 4 lots (dont un partiel de 12).
	const uint32 BS = 16;
	data::NkDataLoader loader(ds, BS, /*shuffle*/ true, /*seed*/ 7u);
	Check(loader.NumBatches() == (N + BS - 1) / BS, "NumBatches = ceil(60/16) = 4");

	// Parcourt tous les lots : compte total + histogramme + validité one-hot.
	int actual[16] = {0};
	uint32 seen = 0;
	bool oneHotOk = true, shapeOk = true;
	for (uint32 b = 0; b < loader.NumBatches(); ++b) {
		data::NkBatch batch = loader.GetBatch(b);
		seen += batch.size;
		if ((uint32)batch.inputs.Shape()[0] != batch.size || (uint32)batch.inputs.Shape()[1] != ds.FeatureDim() ||
			(uint32)batch.targets.Shape()[1] != ds.NumClasses())
			shapeOk = false;
		const float *tgt = batch.targets.Contiguous().DataAs<float>();
		for (uint32 k = 0; k < batch.size; ++k) {
			actual[batch.labels[k]]++;
			// La ligne one-hot doit sommer à 1 et avoir son 1 sur la bonne classe.
			double sum = 0.0;
			int hot = -1;
			for (uint32 c = 0; c < ds.NumClasses(); ++c) {
				float v = tgt[k * ds.NumClasses() + c];
				sum += v;
				if (v > 0.5f)
					hot = (int)c;
			}
			if (hot != batch.labels[k] || sum < 0.999 || sum > 1.001)
				oneHotOk = false;
		}
	}
	Check(seen == N, "Couverture : tous les exemples exactement une fois (60)");
	bool histOk = true;
	for (uint32 c = 0; c < NC; ++c)
		if (actual[c] != expected[c])
			histOk = false;
	Check(histOk, "Histogramme par classe préservé (pas de perte/doublon)");
	Check(shapeOk, "Formes des lots correctes ([B,D] / [B,C])");
	Check(oneHotOk, "One-hot correct (somme=1, sur la bonne classe)");

	// Le shuffle change l'ordre d'une époque à l'autre.
	data::NkBatch b0 = loader.GetBatch(0);
	loader.Shuffle();
	data::NkBatch b0b = loader.GetBatch(0);
	bool changed = false;
	uint32 m = b0.size < b0b.size ? b0.size : b0b.size;
	for (uint32 k = 0; k < m; ++k)
		if (b0.labels[k] != b0b.labels[k]) {
			changed = true;
			break;
		}
	Check(changed, "Shuffle() remélange l'ordre entre deux époques");

	// ------------------------------------------------------------------
	// MNIST (optionnel) : défini NK_MNIST_DIR pointant vers les 4 fichiers IDX
	// décompressés (train-images-idx3-ubyte, train-labels-idx1-ubyte).
	// ------------------------------------------------------------------
	printf("\n-- MNIST (optionnel) --\n");
	const char *dir = getenv("NK_MNIST_DIR");
	if (!dir || !*dir) {
		printf("  (NK_MNIST_DIR non défini : section MNIST ignorée)\n");
	} else {
		char img[1024], lbl[1024];
		snprintf(img, sizeof(img), "%s/train-images-idx3-ubyte", dir);
		snprintf(lbl, sizeof(lbl), "%s/train-labels-idx1-ubyte", dir);
		data::NkDataset mnist = data::LoadMnist(img, lbl);
		if (!mnist.IsValid()) {
			printf("  MNIST introuvable/illisible dans %s (fichiers IDX décompressés ?)\n", dir);
		} else {
			printf("  MNIST chargé : %u exemples, %u features (28x28), %u classes\n", mnist.Size(), mnist.FeatureDim(),
				   mnist.NumClasses());
			data::NkDataLoader ml(mnist, 64, true, 1u);
			printf("  DataLoader MNIST : %u lots de 64\n", ml.NumBatches());
			Check(mnist.FeatureDim() == 784 && mnist.NumClasses() == 10, "MNIST : 784 features, 10 classes");
		}
	}

	// ------------------------------------------------------------------
	// Jalon 2 (2026-07-26) : tokenizer texte générique (BPE, généralisé depuis
	// NKGpt/NkGptCore -> NKData/NkTokenizer.h) + vocabulaire/séquences/padding.
	// ------------------------------------------------------------------
	logger.Info("");
	logger.Info("-- Tokenizer BPE générique (data::NkBpe, ex-NKGpt) : entraînement + round-trip --");
	{
		// Corpus RÉEL (français, petit mais du vrai texte, pas des octets aléatoires).
		NkVector<NkString> corpus;
		corpus.PushBack(NkString("Le design textile camerounais s'inspire des motifs Bamileke et Fang-Beti."));
		corpus.PushBack(NkString("Les uniformes scolaires du secondaire public portent des motifs traditionnels."));
		corpus.PushBack(
			NkString("Les quatre aires culturelles du Cameroun sont Soudano-Sahelienne, Fang-Beti, Sawa, Grassfields."));
		corpus.PushBack(NkString("Le tissage et la teinture font partie du patrimoine textile camerounais."));

		data::NkBpe bpe;
		data::TrainBpe(corpus, /*nMerges*/ 60, bpe);
		Check(bpe.merges.Size() > 0 && bpe.merges.Size() <= 60, "TrainBpe : fusions apprises (1..60)");
		Check(bpe.vocab.Size() == (nk_size)bpe.Base(), "Bpe::Base() == taille du vocabulaire construit");

		// Round-trip EXACT (byte-level BPE : toute séquence d'octets se décode à l'identique,
		// y compris sur un texte qui n'était PAS dans le corpus d'entraînement).
		NkString sample("Motifs Grassfields et Sawa : broderie, indigo, raphia.");
		NkVector<int32> ids;
		bpe.Encode(sample, ids);
		NkString back = data::DecodeAll(bpe, ids);
		Check(back == sample, "Round-trip BPE exact (texte hors corpus d'entraînement)");
		Check(ids.Size() < sample.Size(), "Compression : moins de tokens BPE que d'octets bruts");
		logger.Info("  ({0} octets -> {1} tokens BPE, vocabulaire = {2})", sample.Size(), ids.Size(), bpe.Base());
	}

	logger.Info("-- Vocabulaire mot-à-mot (data::NkVocab) --");
	{
		NkVector<NkString> texts;
		texts.PushBack(NkString("le tisserand tisse le pagne traditionnel"));
		texts.PushBack(NkString("le pagne traditionnel orne la robe"));
		data::NkVocab vocab;
		vocab.BuildFromTexts(texts, /*minFreq*/ 1);
		Check(vocab.Size() > 2, "NkVocab::BuildFromTexts peuple le vocabulaire (> réservés pad/unk)");
		Check(vocab.IdOf(NkString("<pad>")) == data::NkVocab::kPadId, "id de <pad> == kPadId (0)");
		Check(vocab.IdOf(NkString("mot-totalement-inconnu-xyz")) == data::NkVocab::kUnkId, "mot absent -> <unk>");

		NkVector<int32> ids;
		NkString known("le pagne traditionnel");
		vocab.Encode(known, ids);
		NkString back = vocab.DecodeAll(ids);
		Check(back == known, "Round-trip NkVocab exact sur des mots CONNUS");
	}

	logger.Info("-- Séquences + padding (data::PadSequences) : lots de longueur variable --");
	{
		NkVector<NkVector<int32>> seqs;
		NkVector<int32> s0;
		s0.PushBack(5);
		s0.PushBack(6);
		seqs.PushBack(s0);
		NkVector<int32> s1;
		s1.PushBack(7);
		s1.PushBack(8);
		s1.PushBack(9);
		s1.PushBack(10);
		seqs.PushBack(s1);
		NkVector<int32> s2;
		s2.PushBack(1);
		seqs.PushBack(s2);

		data::NkSeqBatch batch = data::PadSequences(seqs, /*padId*/ 0);
		Check(batch.ids.Shape()[0] == 3 && batch.ids.Shape()[1] == 4, "PadSequences : forme [B=3, Tmax=4] (auto)");
		Check(batch.lengths[0] == 2 && batch.lengths[1] == 4 && batch.lengths[2] == 1,
			  "PadSequences : longueurs réelles préservées");

		const float *idp = batch.ids.Contiguous().DataAs<float>();
		const float *mkp = batch.mask.Contiguous().DataAs<float>();
		const int64 T = batch.ids.Shape()[1];
		bool contentOk = (int32)idp[0 * T + 0] == 5 && (int32)idp[0 * T + 1] == 6 && (int32)idp[0 * T + 2] == 0 &&
						 (int32)idp[1 * T + 3] == 10 && (int32)idp[2 * T + 0] == 1;
		bool maskOk = mkp[0 * T + 0] == 1.0f && mkp[0 * T + 2] == 0.0f && mkp[1 * T + 3] == 1.0f && mkp[2 * T + 1] == 0.0f;
		Check(contentOk, "PadSequences : identifiants réels + padId (0) au-delà de chaque longueur");
		Check(maskOk, "PadSequences : masque 1.0 sur token réel / 0.0 sur padding");

		// Troncature explicite (maxLen < longueur d'une séquence) : garde le PRÉFIXE.
		data::NkSeqBatch trunc = data::PadSequences(seqs, 0, /*maxLen*/ 2);
		Check(trunc.ids.Shape()[1] == 2 && trunc.lengths[1] == 2, "PadSequences : troncature à maxLen (préfixe gardé)");
	}

	// ------------------------------------------------------------------
	// Jalon 3 (2026-07-26) : augmentation + découpage train/val/test.
	// ------------------------------------------------------------------
	logger.Info("-- Découpage train/val/test (data::SplitDataset) : pas de fuite --");
	{
		data::NkDataset full = data::MakeBlobs(4, 50, 321u); // 200 exemples
		data::NkSplit split = data::SplitDataset(full, 0.7, 0.15, 0.15, /*seed*/ 13u);
		const uint32 total = split.train.Size() + split.val.Size() + split.test.Size();
		Check(total == full.Size(), "SplitDataset : train+val+test == taille originale (pas de perte)");
		Check(split.train.Size() > split.val.Size() && split.train.Size() > split.test.Size(),
			  "SplitDataset : la part train est la plus grande (~70%)");

		data::NkDataset recombined = data::ConcatDatasets(split.train, data::ConcatDatasets(split.val, split.test));
		Check(recombined.Size() == full.Size() && recombined.FeatureDim() == full.FeatureDim(),
			  "ConcatDatasets : recolle train+val+test à la taille d'origine");
	}

	logger.Info("-- Augmentation IMAGE : miroir horizontal (data::AugmentFlipHorizontal) --");
	{
		// Une image 2x3 connue : ligne0=[1,2,3], ligne1=[4,5,6]. Miroir attendu :
		// ligne0=[3,2,1], ligne1=[6,5,4].
		NkTensor X = NkTensor::Zeros(NkShape{(int64)1, (int64)6});
		float *xp = X.DataAs<float>();
		for (int i = 0; i < 6; ++i)
			xp[i] = (float)(i + 1);
		NkVector<int32> Y;
		Y.PushBack(0);
		data::NkDataset img(X, Y, 1u);
		data::NkDataset flipped = data::AugmentFlipHorizontal(img, /*rows*/ 2, /*cols*/ 3);
		const float *fp = flipped.Features().Contiguous().DataAs<float>();
		bool flipOk = fp[0] == 3.f && fp[1] == 2.f && fp[2] == 1.f && fp[3] == 6.f && fp[4] == 5.f && fp[5] == 4.f;
		Check(flipOk, "AugmentFlipHorizontal : miroir exact sur une image 2x3 connue");

		data::NkDataset combined = data::ConcatDatasets(img, flipped);
		Check(combined.Size() == 2, "ConcatDatasets(original, augmenté) : taille doublée");
	}

	logger.Info("-- Augmentation NUMÉRIQUE : bruit gaussien (data::AugmentGaussianNoise) --");
	{
		data::NkDataset ds = data::MakeBlobs(3, 30, 55u);
		data::NkDataset noisy = data::AugmentGaussianNoise(ds, /*stddev*/ 0.3f, /*seed*/ 99u);
		Check(noisy.Size() == ds.Size() && noisy.FeatureDim() == ds.FeatureDim(), "AugmentGaussianNoise : même forme");
		bool labelsPreserved = true;
		for (uint32 i = 0; i < ds.Size(); ++i)
			if (ds.Labels()[i] != noisy.Labels()[i])
				labelsPreserved = false;
		Check(labelsPreserved, "AugmentGaussianNoise : labels INCHANGÉS (seules les features bougent)");

		const float *op = ds.Features().Contiguous().DataAs<float>();
		const float *np_ = noisy.Features().Contiguous().DataAs<float>();
		double sumAbsDiff = 0.0;
		const nk_size n = (nk_size)ds.Size() * ds.FeatureDim();
		bool anyChanged = false;
		for (nk_size i = 0; i < n; ++i) {
			const double d = (double)op[i] - (double)np_[i];
			sumAbsDiff += (d < 0 ? -d : d);
			if (d != 0.0)
				anyChanged = true;
		}
		const double meanAbsDiff = sumAbsDiff / (double)n;
		Check(anyChanged, "AugmentGaussianNoise : les features ont RÉELLEMENT changé");
		// E[|N(0,stddev)|] = stddev * sqrt(2/pi) ~= 0.239 pour stddev=0.3 : plage large (0.05..0.6)
		// pour éviter toute fragilité, seule la présence d'un bruit du bon ORDRE DE GRANDEUR compte.
		Check(meanAbsDiff > 0.05 && meanAbsDiff < 0.6, "AugmentGaussianNoise : amplitude du bruit plausible (stddev=0.3)");
	}

	logger.Info("-- Augmentation TEXTE : word-dropout sur séquence de tokens (data::AugmentTokenDropout) --");
	{
		NkVector<int32> ids;
		for (int32 i = 1; i <= 20; ++i)
			ids.PushBack(i);

		NkVector<int32> keepAll = data::AugmentTokenDropout(ids, 0.0f, 1u);
		Check(keepAll.Size() == ids.Size(), "AugmentTokenDropout(prob=0) : conserve tous les tokens");

		NkVector<int32> dropAll = data::AugmentTokenDropout(ids, 1.0f, 1u);
		Check(dropAll.Size() == 1, "AugmentTokenDropout(prob=1) : ne renvoie JAMAIS une séquence vide (garde >=1)");

		NkVector<int32> partial = data::AugmentTokenDropout(ids, 0.4f, 7u);
		bool subsequenceOk = partial.Size() > 0 && partial.Size() <= ids.Size();
		int64 lastPos = -1;
		for (uint32 i = 0; subsequenceOk && i < partial.Size(); ++i) {
			int64 pos = -1;
			for (int64 k = 0; k < (int64)ids.Size(); ++k)
				if (ids[(nk_size)k] == partial[i]) {
					pos = k;
					break;
				}
			if (pos <= lastPos) {
				subsequenceOk = false;
				break;
			}
			lastPos = pos;
		}
		Check(subsequenceOk, "AugmentTokenDropout(prob=0.4) : sous-séquence VALIDE (ordre préservé, longueur réduite)");
		logger.Info("  ({0} tokens -> {1} après dropout 40%)", ids.Size(), partial.Size());
	}

	logger.Info("");
	logger.Info("=== Résultat : {0} OK, {1} échec(s) ===", g_pass, g_fail);
	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
