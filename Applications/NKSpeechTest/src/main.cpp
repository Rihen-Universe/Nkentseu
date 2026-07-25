// =============================================================================
// NKSpeechTest — tests headless du module de parole NKSpeech (NKAI).
// AUCUN device GPU : pur CPU. Sortie via printf (sortie directe console).
// =============================================================================
#include "NKSpeech/NkAudioFeatures.h"
#include "NKSpeech/NkGriffinLim.h"
#include "NKSpeech/NkVoiceSynth.h"
#include "NKSpeech/NkG2P.h"
#include "NKContainers/String/Encoding/NkUTF8.h"

#include <cstdio>

using namespace nkentseu;

// Écrit un WAV PCM16 mono (pour ÉCOUTER la synthèse). RIFF little-endian.
static void WriteWavPcm16(const char *path, const float *samples, int n, int sampleRate) {
	FILE *fp = fopen(path, "wb");
	if (!fp)
		return;
	auto u32 = [&](unsigned v) { fputc(v & 255, fp); fputc((v >> 8) & 255, fp); fputc((v >> 16) & 255, fp); fputc((v >> 24) & 255, fp); };
	auto u16 = [&](unsigned v) { fputc(v & 255, fp); fputc((v >> 8) & 255, fp); };
	const int dataBytes = n * 2;
	fwrite("RIFF", 1, 4, fp); u32(36 + dataBytes); fwrite("WAVE", 1, 4, fp);
	fwrite("fmt ", 1, 4, fp); u32(16); u16(1); u16(1);           // PCM, mono
	u32(sampleRate); u32(sampleRate * 2); u16(2); u16(16);        // byte rate, block align, bits
	fwrite("data", 1, 4, fp); u32(dataBytes);
	for (int i = 0; i < n; ++i) {
		float v = samples[i];
		if (v > 1.0f) v = 1.0f;
		if (v < -1.0f) v = -1.0f;
		int s = (int)(v * 32767.0f);
		u16((unsigned)(s & 0xFFFF));
	}
	fclose(fp);
}

// Construit un NkString UTF-8 depuis une suite de codepoints Unicode (évite tout
// littéral non-ASCII dans ce fichier .cpp : portable quel que soit le charset
// source du compilateur). Utilisé pour les mots ghomala' RÉELS de la démo G2P.
static NkString Utf8FromCodepoints(const uint32 *cps, int n) {
	char buf[128];
	usize p = 0;
	for (int i = 0; i < n; ++i)
		p += encoding::utf8::NkEncodeChar(cps[i], buf + p);
	return NkString(buf, (NkString::SizeType)p);
}

// Affiche une suite de phonèmes G2P (symbole + ton + nasalisation) façon
// "d oh(L) m n y eu(L)". Debug/démo uniquement (labels ASCII, cf. NkG2P::SymbolLabel).
static void PrintPhonemes(const NkVector<ai::NkPhonemeUnit> &ph) {
	for (nk_size i = 0; i < ph.Size(); ++i) {
		const ai::NkPhonemeUnit &u = ph[i];
		printf("%s", ai::NkG2P::SymbolLabel(u.symbol));
		if (u.tone != ai::NkTone::None && u.tone != ai::NkTone::Mid)
			printf("(%s)", ai::NkG2P::ToneLabel(u.tone));
		if (u.nasalized)
			printf("~");
		printf(" ");
	}
}

int main() {
	printf("=== NKSpeechTest — parole from-scratch (NKAI, headless) ===\n\n");

	int nbOk = 0, nbTotal = 0;

	{
		++nbTotal;
		const bool ok = ai::NkAudioFeatures::SelfTest();
		printf("[ %s ] NkAudioFeatures : MFCC/log-Mel (sinus 1kHz -> bon canal Mel, deterministe, silence fini)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}

	{
		++nbTotal;
		const bool ok = ai::NkGriffinLim::SelfTest();
		printf("[ %s ] NkGriffinLim : vocodeur (spectrogramme magnitude -> onde, phase iterative ; "
			   "magnitude reconstruite fidele + energie preservee)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}

	{
		++nbTotal;
		const bool ok = ai::NkG2P::SelfTest();
		printf("[ %s ] NkG2P : G2P rule-based fr/en/bbj (mots REELS ghomala' verifies "
			   "lamba-africa.com \"dommnye\"=impasse/\"letee\"=solide + NT \"Ngkhenye\" + apostrophe glottale)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}

	// Demo lisible du G2P : mots REELS bbj (voir NkG2P.h SOURCES) -> phonemes.
	{
		printf("\n-- NkG2P demo (bbj = ghomala', mots reels sources verifiees) --\n");

		// "dɔ̀mnyə̀" = « impasse » (lamba-africa.com, lexique deja utilise par le projet).
		const uint32 cpDomnye[] = {'d', 0x0254, 0x0300, 'm', 'n', 'y', 0x0259, 0x0300};
		NkString wDomnye = Utf8FromCodepoints(cpDomnye, 8);
		printf("  \"dommnye\" (bbj, = \"impasse\" en fr) -> ");
		PrintPhonemes(ai::NkG2P::ToPhonemesBbj(wDomnye));
		printf("\n");

		// "lɛtə̌" = « solide » (lamba-africa.com).
		const uint32 cpLetee[] = {'l', 0x025B, 't', 0x0259, 0x030C};
		NkString wLetee = Utf8FromCodepoints(cpLetee, 5);
		printf("  \"letee\" (bbj, = \"solide\" en fr) -> ");
		PrintPhonemes(ai::NkG2P::ToPhonemesBbj(wLetee));
		printf("\n");

		// "Ŋkhənyə" — premier mot de Matio 1:1 (Nouveau Testament ghomala', corpus reel
		// du projet AI/corpus/lamba/bbj_ghomala_nt.txt). Traduction non revalidee ici :
		// on ne teste que la transcription G2P, pas le sens.
		const uint32 cpNkhenye[] = {0x014A, 'k', 'h', 0x0259, 'n', 'y', 0x0259};
		NkString wNkhenye = Utf8FromCodepoints(cpNkhenye, 7);
		printf("  \"Ngkhenye\" (bbj, mot reel du NT, Matio 1:1) -> ");
		PrintPhonemes(ai::NkG2P::ToPhonemesBbj(wNkhenye));
		printf("\n");

		// "gʉ'" — occlusive glottale finale (forme attestee dans le NT, ex. Matio 1:18).
		const uint32 cpGu[] = {'g', 0x0289, '\''};
		NkString wGu = Utf8FromCodepoints(cpGu, 3);
		printf("  \"gu'\" (bbj, apostrophe = occlusive glottale finale) -> ");
		PrintPhonemes(ai::NkG2P::ToPhonemesBbj(wGu));
		printf("\n");

		printf("  \"bonjour\" (fr) -> ");
		PrintPhonemes(ai::NkG2P::ToPhonemesFr(NkString("bonjour")));
		printf("\n  \"maison\" (fr) -> ");
		PrintPhonemes(ai::NkG2P::ToPhonemesFr(NkString("maison")));
		printf("\n  \"church\" (en) -> ");
		PrintPhonemes(ai::NkG2P::ToPhonemesEn(NkString("church")));
		printf("\n  \"think\" (en) -> ");
		PrintPhonemes(ai::NkG2P::ToPhonemesEn(NkString("think")));
		printf("\n\n");
	}

	{
		++nbTotal;
		const bool ok = ai::NkVoiceSynth::SelfTest();
		printf("[ %s ] NkVoiceSynth : synthese par formants (voyelle 'a' -> energie autour de F1/F2)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}

	// Synthese AUDIBLE (a ecouter).
	{
		// Sequence de voyelles a e i o u (u = /y/ francais) + le "ou" a la fin pour comparer.
		auto voyelles = []() {
			NkVector<ai::NkPhone> seq;
			const char v[6] = {'a', 'e', 'i', 'o', 'u', 'w'}; // 'w' = "ou" /u/
			for (int i = 0; i < 6; ++i) {
				seq.PushBack(ai::NkVoiceSynth::Vowel(v[i], 300.0f));
				ai::NkPhone sil;
				sil.gain = 0.0f;
				sil.durationMs = 70.0f;
				seq.PushBack(sil);
			}
			return seq;
		};

		struct Voix {
			const char *file;
			ai::NkVoiceSynthConfig cfg;
		};
		Voix voix[4] = {
			{"nkvoice_homme.wav", ai::NkVoiceSynthConfig::Homme()},
			{"nkvoice_femme.wav", ai::NkVoiceSynthConfig::Femme()},
			{"nkvoice_enfant.wav", ai::NkVoiceSynthConfig::Enfant()},
			{"nkvoice_geant.wav", ai::NkVoiceSynthConfig::Geant()},
		};
		NkVector<ai::NkPhone> seq = voyelles();
		for (int k = 0; k < 4; ++k) {
			NkVector<float32> wav = ai::NkVoiceSynth::Synthesize(seq, voix[k].cfg);
			if (wav.Size() > 0) {
				WriteWavPcm16(voix[k].file, wav.Data(), (int)wav.Size(), voix[k].cfg.sampleRate);
				printf("       -> %s (%d ech., F0=%.0f Hz) : a e i o u [u=/y/] ou\n", voix[k].file,
					   (int)wav.Size(), voix[k].cfg.f0);
			}
		}

		// MOTS (consonnes + voyelles) : "papa", "maman", "salu", "lili".
		struct Mot {
			const char *file;
			const char *phon;
		};
		Mot mots[4] = {
			{"nkmot_papa.wav", "p a p a"},
			{"nkmot_maman.wav", "m a m a~"},
			{"nkmot_salu.wav", "s a l u"},
			{"nkmot_lili.wav", "l i l i"},
		};
		ai::NkVoiceSynthConfig cfgH = ai::NkVoiceSynthConfig::Homme();
		for (int k = 0; k < 4; ++k) {
			NkVector<float32> wav = ai::NkVoiceSynth::Speak(mots[k].phon, cfgH);
			if (wav.Size() > 0) {
				WriteWavPcm16(mots[k].file, wav.Data(), (int)wav.Size(), cfgH.sampleRate);
				printf("       -> %s (\"%s\")\n", mots[k].file, mots[k].phon);
			}
		}
	}

	printf("\n=== Resultat : %d/%d suites OK ===\n", nbOk, nbTotal);
	return (nbOk == nbTotal) ? 0 : 1;
}
